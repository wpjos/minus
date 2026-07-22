#include "mm.h"
#include "page.h"
#include "buddy.h"
#include "slab.h"
#include "mmu.h"
#include "memory.h"
#include "string.h"

struct page *g_mem_pages;
size_t g_pfn_offset;

extern uint64_t __init_pgd[PTE_ENTRIES];

static void create_kernel_pgtable(uint64_t *pgd)
{
	extern char __text_start[], __init_end[];
	extern char __dtb_start[], __dtb_end[];
	extern char __pgtable_start[], __pgtable_end[];
	extern char __rodata_start[], __rodata_end[];
	extern char __data_start[], __data_end[];
	extern char __bss_start[], __bss_end[];

#define CREATE_PTABLE(pgtable, start, end, attr) do {		\
	uint64_t _s = ALIGN_DOWN((uint64_t)(start), PAGE_SIZE);	\
	uint64_t _e = ALIGN_UP((uint64_t)(end), PAGE_SIZE);		\
	early_mmu_map((pgtable), _s, __VA_PA__(_s), _e - _s, (attr)); \
} while (0)

	CREATE_PTABLE(pgd, __text_start, __init_end, MMU_REGION_NORMAL_RO);
	CREATE_PTABLE(pgd, __dtb_start, __dtb_end, MMU_REGION_NORMAL_RO | PTE_PXN | PTE_UXN);
	CREATE_PTABLE(pgd, __pgtable_start, __pgtable_end, MMU_REGION_NORMAL_XN | PTE_UXN);
	CREATE_PTABLE(pgd, __rodata_start, __rodata_end, MMU_REGION_NORMAL_RO);
	CREATE_PTABLE(pgd, __data_start, __data_end, MMU_REGION_NORMAL_XN | PTE_UXN);
	CREATE_PTABLE(pgd, __bss_start, __bss_end, MMU_REGION_NORMAL_XN | PTE_UXN);
#undef MAP_ALIGNED

}

static void create_memblock_pgtable(uint64_t *pgd)
{
	memblock_map_all(__init_pgd);
}

static void switch_pgd(void)
{
	memset(__init_pgd, 0, PAGE_SIZE);
	create_kernel_pgtable(__init_pgd);
	create_memblock_pgtable(__init_pgd);

	/* TTBR1_EL1 holds the physical base address of the kernel PGD. */
	mmu_switch_pgd(__VA_PA__((uint64_t)__init_pgd));
}

static void early_ptable_free_to_buddy(void)
{
	extern char __early_pgtable_start[], __early_pgtable_end[];

	buddy_free_pages_range(__VA_PA__(__early_pgtable_start),
			       __early_pgtable_end - __early_pgtable_start);
}

static void page_env_prepare(void)
{
	uint64_t pages_pa;
	uint64_t total_pages;

	g_pfn_offset = memblock_mem_start() >> PAGE_SHIFT;
	total_pages = (memblock_mem_end() - memblock_mem_start()) >> PAGE_SHIFT;
	pages_pa = (uint64_t)memblock_alloc_aligned(
			total_pages * sizeof(struct page), PAGE_SIZE);
	g_mem_pages = (struct page *)__PA_VA__(pages_pa);
	memset(g_mem_pages, 0, total_pages * sizeof(struct page));
}

static void reclaim_mem_to_buddy(void)
{
	buddy_init();
	memblock_free_to_buddy();
	early_ptable_free_to_buddy();
	buddy_stat();
}

void mm_init(void)
{
	memblock_init();
	switch_pgd();
	page_env_prepare();
	reclaim_mem_to_buddy();
	slab_init();
	mmu_disable_ttbr0();
}
