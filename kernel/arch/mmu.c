#include "types.h"
#include "string.h"
#include "sysreg.h"
#include "mmu.h"
#include "memory.h"
#include "mm.h"
#include "page.h"
#include "buddy.h"
#include "printk.h"
#include "cache.h"

extern uint64_t __init_pgd[PTE_ENTRIES];

const uint64_t g_level_size[] = {
	1ULL << PGD_SHIFT,
	1ULL << PUD_SHIFT,
	1ULL << PMD_SHIFT,
	1ULL << PTE_SHIFT,
};
const uint64_t g_level_shift[] = {
	PGD_SHIFT,
	PUD_SHIFT,
	PMD_SHIFT,
	PTE_SHIFT,
};

static inline void mmu_populate(uint64_t *entry, uint64_t val)
{
	*entry = val;
	dsb_ish();
}

static uint64_t *mmu_get_ntable(uint64_t *table, uint64_t idx)
{
	uint64_t phys;

	if (!mmu_entry_populated(table[idx])) {
		void *vaddr = kzalloc_pages(PAGE_SIZE);

		MMU_BUGON(vaddr == NULL);
		phys = __VA_PA__((uint64_t)vaddr);
		mmu_populate(&table[idx], PTE_TABLE(phys));
		return vaddr;
	}

	phys = table[idx] & PTE_PHYS_MASK;
	return (uint64_t *)__PA_VA__(phys);
}

static void mmu_map_range(uint64_t *table, uint64_t vstart,
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
			mmu_populate(&table[idx], PTE_PAGE(pa, attr));
		} else if (chunk == size && (pa & (size - 1)) == 0) {
			mmu_populate(&table[idx], PTE_BLOCK(pa, attr));
		} else {
			uint64_t *ntable = mmu_get_ntable(table, idx);
			mmu_map_range(ntable, va, va + chunk, pa,
				      level + 1, attr);
		}
		va += chunk;
		pa += chunk;
	}
}

void mmu_map(uint64_t *pgd, uint64_t va, uint64_t pa, uint64_t size, uint64_t attr)
{
	uint64_t vstart = ALIGN_DOWN(va, PAGE_SIZE);
	uint64_t vend = ALIGN_UP(va + size, PAGE_SIZE);
	uint64_t pstart = pa - (va - vstart);

	/*
	 * Page tables can only describe whole pages.  Expand the requested
	 * range to page boundaries and shift the physical start by the same
	 * amount so that @va still maps to @pa.  The adjusted physical start
	 * must itself be page aligned.
	 */
	MMU_BUGON((pstart & (PAGE_SIZE - 1)) != 0);

	mmu_map_range(pgd, vstart, vend, pstart, PGD_LEVEL, attr);
}

/*
 * Runtime region mapping (called by drivers after paging_init).
 * Maps [phys, phys+size) as Device-nGnRnE in the kernel page table and
 * returns the kernel virtual address.
 */
void *mmu_ioremap(uint64_t pa, uint64_t size)
{
	MMU_BUGON((pa & (PAGE_SIZE - 1)) != 0);

	uint64_t vstart = __PA_VA__(pa);
	uint64_t vend = ALIGN_UP(vstart + size, PAGE_SIZE);

	mmu_map_range(__init_pgd, vstart, vend, pa, PGD_LEVEL, MMU_REGION_DEVICE);

	return (void *)vstart;
}

/*
 * Reserved empty page table used when no user address space is active.
 * Lives in BSS, so it is zeroed before any C code runs.
 */
static uint64_t __attribute__((aligned(PAGE_SIZE))) g_zero_pgd[PTE_ENTRIES];

void mmu_clear_ttbr0(void)
{
	mmu_switch_pgd(TTBR0_EL1, __VA_PA__((uint64_t)g_zero_pgd));
}
