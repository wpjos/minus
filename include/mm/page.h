#ifndef __PAGE_H
#define __PAGE_H

#include "types.h"
#include "dlist.h"
#include "memory.h"

struct page {
	struct dlist_node dnode;
	size_t order: 4;
	size_t in_buddy: 1;
	size_t in_slab: 1;
};

#define PAGE_ALIGN(x)		(((uintptr_t)x + PAGE_SIZE - 1) & PAGE_MASK)

/*
 * Linear page-table assumptions with a fixed VA/PA offset.
 * The kernel is linked at high virtual addresses but loaded at low physical
 * addresses; LOAD_OFFSET (from memory.h) is used for runtime conversion.
 */

extern struct page *g_mem_pages;
#define phy_to_pfn(paddr)	((size_t)(paddr >> PAGE_SHIFT))
#define pfn_to_phy(pfn)		((pfn) << PAGE_SHIFT)

extern size_t g_pfn_offset;
#define pfn_to_page(pfn)	(&g_mem_pages[(pfn) - g_pfn_offset])
#define page_to_pfn(page)	((size_t)((page) - g_mem_pages + g_pfn_offset))

#define page_to_phy(page)	(pfn_to_phy(page_to_pfn(page)))
#define phy_to_page(paddr)	(pfn_to_page(phy_to_pfn(paddr)))

#define virt_to_page(vaddr)	(phy_to_page(__VA_PA__(vaddr)))
#define page_to_virt(page)	((void *)__PA_VA__(page_to_phy(page)))
#endif
