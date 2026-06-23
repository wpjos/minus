#include "mm.h"
#include "page.h"
#include "buddy.h"
#include "mmu.h"
#include "memory.h"
#include "string.h"
#include "printk.h"

struct page *g_mem_pages;
size_t g_pfn_offset;

static void paging_panic(const char *msg)
{
	printk("paging_init panic: %s\n", msg);
	while (1);
}

void paging_init(void)
{
	uint64_t mem_start = early_mm_memory_start();
	uint64_t mem_end = early_mm_memory_end();
	uint64_t total_pages = (mem_end - mem_start) >> PAGE_SHIFT;
	uint64_t pages_phys;
	struct page *pages_va;
	int i, cnt;

	if (mem_end <= mem_start)
		paging_panic("no memory");

	g_pfn_offset = mem_start >> PAGE_SHIFT;

	pages_phys = (uint64_t)early_mm_alloc_aligned(
			total_pages * sizeof(struct page), PAGE_SIZE);
	if (!pages_phys)
		paging_panic("cannot allocate g_mem_pages");

	/* Map all of RAM into the high virtual address space. */
	mmu_map_blocks(__PA_VA__(mem_start), mem_start,
		      mem_end - mem_start, MMU_REGION_NORMAL);
	mmu_sync();

	/* Now that RAM is mapped, access the page array through its high VA. */
	pages_va = (struct page *)__PA_VA__(pages_phys);
	g_mem_pages = pages_va;
	memset(g_mem_pages, 0, total_pages * sizeof(struct page));

	/* Hand every still-available region to the buddy allocator. */
	cnt = early_mm_memory_region_count();
	for (i = 0; i < cnt; i++) {
		uint64_t base, size;
		size_t pfn, npages, j;

		early_mm_memory_region(i, &base, &size);
		pfn = base >> PAGE_SHIFT;
		npages = size >> PAGE_SHIFT;
		for (j = 0; j < npages; j++) {
			struct page *page = pfn_to_page(pfn + j);
			buddy_free_pages(page);
		}
	}

	buddy_stat();

	/*
	 * From this point on runtime page-table expansions must allocate through
	 * the buddy allocator instead of the early allocator.
	 */
	mmu_set_paging_ready();

	/* Kernel now runs entirely from TTBR1 high VAs. */
	mmu_disable_ttbr0();
}
