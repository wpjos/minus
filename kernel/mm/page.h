#ifndef __PAGE_H
#define __PAGE_H

#include "types.h"
#include "dlist.h"

struct page {
	struct dlist_node dnode;
	size_t order: 4;
	size_t in_buddy: 1;
	size_t in_slab: 1;
};

#define PAGE_BITS		(12UL)
#define PAGE_SIZE		(1UL << PAGE_BITS)
#define PAGE_MASK		(~(PAGE_SIZE - 1))
#define PAGE_ALIGN(x)		(((uintptr_t)x + PAGE_SIZE - 1) & PAGE_MASK)

/*
 * Linear page-table assumptions (MMU off / identity mapped).
 * PHYS_OFFSET is platform specific; default to qemu-virt RAM base.
 */
#ifndef PHYS_OFFSET
#define PHYS_OFFSET		0x40000000UL
#endif

#ifndef __VA_PA__
#define __VA_PA__(x)		((unsigned long)(x))
#endif
#ifndef __PA_VA__
#define __PA_VA__(x)		((unsigned long)(x))
#endif

extern struct page *g_mem_pages;
#define phy_to_pfn(paddr)	((size_t)(paddr >> PAGE_BITS))
#define pfn_to_phy(pfn)		((pfn) << PAGE_BITS)

#define PFN_OFFSET		(phy_to_pfn(PHYS_OFFSET))
#define pfn_to_page(pfn)	(&g_mem_pages[pfn - PFN_OFFSET])
#define page_to_pfn(page)	((size_t)(page - g_mem_pages + PFN_OFFSET))

#define page_to_phy(page)	(pfn_to_phy(page_to_pfn(page)))
#define phy_to_page(paddr)	(pfn_to_page(phy_to_pfn(paddr)))

#define virt_to_page(vaddr)	(phy_to_page(__VA_PA__(vaddr)))
#define page_to_virt(page)	((void *)__PA_VA__(page_to_phy(page)))
#endif
