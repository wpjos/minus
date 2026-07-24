#include "types.h"
#include "string.h"
#include "sysreg.h"
#include "mmu.h"
#include "memory.h"
#include "mm.h"
#include "page.h"

extern uint64_t __attribute__((visibility("hidden"))) __early_init_pgd[PTE_ENTRIES];
extern uint64_t __attribute__((visibility("hidden"))) __early_idmap_pgd[PTE_ENTRIES];
extern uint64_t __attribute__((visibility("hidden"))) __init_pgd[PTE_ENTRIES];
extern uint64_t __attribute__((visibility("hidden"))) g_level_size[];
extern uint64_t __attribute__((visibility("hidden"))) g_level_shift[];

/* Fixmap slots used while building page tables after the MMU is on. */
#define FIXMAP_SLOT_PGTBL	0
#define FIXMAP_SLOT_PUD		1
#define FIXMAP_SLOT_PMD		2
#define FIXMAP_SLOT_PTE		3

static bool g_early_mmu_on = false;

/*
 * Simple bump allocator used while building the initial page tables with the
 * MMU off.  It hands out physical addresses from the region passed by the
 * boot assembly code, which sits just after the kernel image.
 */
struct early_allocator {
	uint64_t start;
	uint64_t curr;
	uint64_t end;
} early_ator;

static void early_mmu_reserve_ator(void)
{
	extern char __attribute__((visibility("hidden"))) __early_mmu_pool_start[];
	extern char __attribute__((visibility("hidden"))) __early_mmu_pool_end[];
	early_ator.start = (uint64_t)__early_mmu_pool_start;
	early_ator.end = (uint64_t)__early_mmu_pool_end;
	early_ator.curr = (uint64_t)__early_mmu_pool_start;
}

static uint64_t early_mmu_reserve_alloc(uint64_t size)
{
	uint64_t aligned = ALIGN_UP(early_ator.curr, size);

	MMU_BUGON(aligned < early_ator.curr);
	MMU_BUGON(aligned + size > early_ator.end);

	early_ator.curr = aligned + size;
	return aligned;
}

static uint64_t early_mmu_alloc_page(void)
{
	if (g_early_mmu_on)
		return (uint64_t)memblock_alloc_aligned(PAGE_SIZE, PAGE_SIZE);
	return early_mmu_reserve_alloc(PAGE_SIZE);
}

/*
 * Pre-MMU walker: returns a physical pointer into the page tables for @va.
 * Valid only while the MMU is off.
 */
static uint64_t *early_mmu_get_pte(uint64_t va)
{
	uint64_t *table = __early_init_pgd;

	for (int level = PGD_LEVEL; level < PTE_LEVEL; level++) {
		uint64_t shift = g_level_shift[level];
		uint64_t idx = (va >> shift) & (PTE_ENTRIES - 1);
		table = (uint64_t *)(table[idx] & PTE_PHYS_MASK);
	}

	return &table[(va >> PTE_SHIFT) & (PTE_ENTRIES - 1)];
}

/*
 * Pre-MMU fixmap helpers.  These patch __early_init_pgd using physical
 * pointers and are used only to set up the FIXMAP_PGTBL self-mapping before
 * the MMU is enabled.
 */
static void early_mmu_set_fixmap(uint64_t va, uint64_t pa, uint64_t attr)
{
	uint64_t *pte = early_mmu_get_pte(va);
	*pte = PTE_PAGE(pa, attr);
	mmu_sync();
}

static uint64_t early_mmu_fixmap_va(int slot)
{
	return FIXMAP_ADDR + ((uint64_t)slot << PAGE_SHIFT);
}

/*
 * Post-MMU fixmap helpers.  FIXMAP_PGTBL always maps the level-3 fixmap PTE
 * page itself, so writing slot N in that page changes the mapping for slot N.
 */
static uint64_t *early_mmu_fixmap_pte(int slot)
{
	uint64_t *pte_page = (uint64_t *)FIXMAP_PGTBL;
	return &pte_page[slot];
}

static uint64_t early_mmu_fixmap_set(uint64_t pa, uint32_t level)
{
	uint32_t slot = FIXMAP_SLOT_PGTBL + level;
	uint64_t *pte = early_mmu_fixmap_pte(slot);
	uint64_t val = PTE_PAGE(pa, MMU_REGION_NORMAL);

	*pte = val;
	__asm__ volatile("dc cvac, %0" : : "r"(pte) : "memory");
	__asm__ volatile("dsb sy" ::: "memory");
	mmu_sync();
	return early_mmu_fixmap_va(slot);
}

/*
 * Page-table page lookup/allocation.  The returned pointer is always usable
 * in the current execution mode:
 *   - MMU off: physical pointer
 *   - MMU on: fixmap virtual address
 */
static uint64_t *early_mmu_get_ntable(uint64_t *table, uint64_t idx,
				      uint32_t level)
{
	uint64_t phys;
	uint64_t vaddr;
	bool allocated = false;

	if (!mmu_entry_populated(table[idx])) {
		phys = early_mmu_alloc_page();
		table[idx] = PTE_TABLE(phys);
		allocated = true;
	} else {
		phys = table[idx] & PTE_PHYS_MASK;
	}

	if (g_early_mmu_on)
		vaddr = early_mmu_fixmap_set(phys, level);
	else
		vaddr = phys;

	if (allocated) {
		memset((void *)vaddr, 0, PAGE_SIZE);
	}

	return (uint64_t *)vaddr;
}

static inline uint64_t early_mmu_atsel(uint64_t va) {
	uint64_t par;
	__asm__ volatile (
		"at  s1e1r, %1\n\t"
		"dsb sy\n\t"
		"mrs %0, par_el1"
		: "=r" (par)
		: "r" (va)
		: "memory"
	);
	if (par & 0x1) {
		return 0;
	}
	return (par & 0xFFFFFFFFF000ULL) | (va & 0xFFF);
}

static void early_mmu_map_range(uint64_t *table, uint64_t vstart,
				uint64_t vend, uint64_t pa,
				uint32_t level, uint64_t attr)
{
	uint64_t va = vstart;
	uint64_t size = g_level_size[level];
	uint64_t shift = g_level_shift[level];

	while (va < vend) {
		uint64_t idx = (va >> shift) & (PTE_ENTRIES - 1);
		uint64_t offset = va & (size - 1);
		uint64_t chunk = size - offset;

		if (chunk > vend - va)
			chunk = vend - va;

		if (level == PTE_LEVEL) {
			MMU_BUGON(chunk != size);
			table[idx] = PTE_PAGE(pa, attr);
		} else if (chunk == size && (va & (size - 1)) == 0 && (pa & (size - 1)) == 0) {
			table[idx] = PTE_BLOCK(pa, attr);
		} else {
			uint64_t *ntable = early_mmu_get_ntable(table, idx, level + 1);
			early_mmu_map_range(ntable, va, va + chunk, pa,
					    level + 1, attr);
		}
		va += chunk;
		pa += chunk;
	}
}

void early_mmu_map(uint64_t *table, uint64_t va, uint64_t pa,
		   uint64_t size, uint64_t attr)
{
	early_mmu_map_range(table, va, va + size, pa, PGD_LEVEL, attr);
}

void early_mmu_init(void)
{
	extern char __attribute__((visibility("hidden"))) __bss_start[], __bss_end[];
	extern char __attribute__((visibility("hidden"))) __image_start[], __image_end[];
	uint64_t image_start_pa = (uint64_t)__image_start;
	uint64_t image_start_va = __PA_VA__(image_start_pa);
	uint64_t image_size = (uint64_t)(__image_end - __image_start);
	uint64_t bss_size = (uint64_t)(__bss_end - __bss_start);
	uint64_t fixmap_pte_page;
	uint64_t sctlr;

	memset(__bss_start, 0, bss_size);
	memset(__early_init_pgd, 0, PAGE_SIZE);
	memset(__early_idmap_pgd, 0, PAGE_SIZE);

	early_mmu_reserve_ator();

	/*
	 * Identity-map the kernel physical load region so execution survives
	 * the MMU-enable moment while the CPU is still at a physical PC.
	 */
	early_mmu_map(__early_idmap_pgd, image_start_pa, image_start_pa,
		      image_size, MMU_REGION_NORMAL);
	/*
	 * Map the kernel at its linked high virtual address.
	 * This is the mapping the kernel uses once it jumps to VA space.
	 */
	early_mmu_map(__early_init_pgd, image_start_va, image_start_pa,
		      image_size, MMU_REGION_NORMAL);

	early_mmu_map(__early_init_pgd, FIXMAP_ADDR, 0,
		      FIXMAP_SIZE, MMU_REGION_NORMAL);

	/*
	 * Create a self-referential fixmap slot: FIXMAP_PGTBL maps the level-3
	 * PTE page that governs the fixmap area itself.  After the MMU is on this
	 * lets software rewrite any fixmap PTE without needing another mapping.
	 */
	fixmap_pte_page = ((uint64_t)early_mmu_get_pte(FIXMAP_ADDR))
			  & PTE_PHYS_MASK;
	early_mmu_set_fixmap(FIXMAP_PGTBL, fixmap_pte_page,
			     MMU_REGION_NORMAL);

	/*
	 * Ensure all page table writes are visible and any stale TLB entries are
	 * flushed before configuring the MMU.
	 */
	mmu_sync();

	/* Configure translation regime for 48-bit VA */
	sys_reg_write(MAIR_EL1, MMU_MAIR_VAL);
	sys_reg_write(TCR_EL1, MMU_TCR_VAL);
	sys_reg_write(TTBR0_EL1, (uint64_t)__early_idmap_pgd);
	sys_reg_write(TTBR1_EL1, (uint64_t)__early_init_pgd);
	__asm__ volatile("isb" ::: "memory");

	/* Enable MMU, D-cache and I-cache */
	sctlr = sys_reg_read(SCTLR_EL1);
	sctlr |= MMU_SCTLR_ENABLE;
	sys_reg_write(SCTLR_EL1, sctlr);
	__asm__ volatile("isb" ::: "memory");

	/* Invalidate the entire instruction cache for the new regime */
	__asm__ volatile("ic iallu" ::: "memory");
	__asm__ volatile("dsb sy" ::: "memory");
	__asm__ volatile("isb" ::: "memory");

	g_early_mmu_on = true;
}
