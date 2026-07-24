#include "types.h"
#include "string.h"
#include "sysreg.h"
#include "mmu.h"
#include "memory.h"
#include "mm.h"
#include "page.h"
#include "buddy.h"

extern uint64_t __attribute__((visibility("hidden"))) __init_pgd[PTE_ENTRIES];

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

static uint64_t *mmu_get_ntable(uint64_t *table, uint64_t idx)
{
	uint64_t phys;

	if (!mmu_entry_populated(table[idx])) {
		struct page *page = buddy_alloc_pages(PAGE_SIZE);
		void *vaddr;

		MMU_BUGON(page == NULL);
		vaddr = page_to_virt(page);
		memset(vaddr, 0, PAGE_SIZE);
		phys = __VA_PA__((uint64_t)vaddr);
		table[idx] = PTE_TABLE(phys);
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
			table[idx] = PTE_PAGE(pa, attr);
		} else if (chunk == size && (va & (size - 1)) == 0 && (pa & (size - 1)) == 0) {
			table[idx] = PTE_BLOCK(pa, attr);
		} else {
			uint64_t *ntable = mmu_get_ntable(table, idx);
			mmu_map_range(ntable, va, va + chunk, pa,
				      level + 1, attr);
		}
		va += chunk;
		pa += chunk;
	}
}

void mmu_sync(void)
{
	__asm__ volatile("dsb sy" ::: "memory");
	__asm__ volatile("tlbi vmalle1" ::: "memory");
	__asm__ volatile("dsb sy" ::: "memory");
	__asm__ volatile("isb" ::: "memory");
}

void mmu_map(uint64_t va, uint64_t pa, uint64_t size, uint64_t attr)
{
	mmu_map_range(__init_pgd, va, va + size, pa, PGD_LEVEL, attr);
	mmu_sync();
}

/*
 * Runtime region mapping (called by drivers after paging_init).
 * Maps [phys, phys+size) as Device-nGnRnE in the kernel page table and
 * returns the kernel virtual address.
 */
void *mmu_ioremap(uint64_t pa, uint64_t size)
{
	uint64_t va = __PA_VA__(pa);

	mmu_map(va, pa, size, MMU_REGION_DEVICE);
	mmu_sync();
	return (void *)va;
}

void mmu_switch_pgd(uint64_t pgd)
{
	sys_reg_write(TTBR1_EL1, pgd);
	__asm__ volatile("isb" ::: "memory");
	__asm__ volatile("tlbi vmalle1" ::: "memory");
	__asm__ volatile("dsb ish" ::: "memory");
	__asm__ volatile("isb" ::: "memory");
}

void mmu_disable_ttbr0(void)
{
	uint64_t tcr = sys_reg_read(TCR_EL1);
	tcr |= (1ULL << 7);		/* EPD0: disable TTBR0 walks */
	sys_reg_write(TCR_EL1, tcr);
	__asm__ volatile("isb" ::: "memory");
	sys_reg_write(TTBR0_EL1, 0);
	__asm__ volatile("isb" ::: "memory");
	mmu_sync();
}
