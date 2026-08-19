#ifndef __VSPACE_H__
#define __VSPACE_H__

#include "types.h"
#include "dlist.h"
#include "mm_types.h"

/* Forward declaration: struct page is defined in kernel/mm/page.h. */
struct page;

/* Default userspace mmap base: 1 TiB, below 48-bit VA limit, above normal app space. */
#define USER_MMAP_BASE	0x1000000000ULL

/*
 * Virtual region.
 * For now each vregion backs exactly one 4 KB page; the model can later be
 * extended to multi-page regions by replacing/page with an array/tree.
 */
struct vregion {
	struct dlist_node node;		/* linked into vspace.vregion_list */
	uint64_t start;			/* inclusive virtual start */
	uint64_t end;			/* exclusive virtual end */
	uint32_t flags;			/* VM_READ | VM_WRITE | VM_EXEC */
	struct page *page;		/* backing physical page */
};

/*
 * User address space descriptor.  Holds the user page table root and the
 * list of user vregions.  Kernel stacks are NOT part of a vspace: they
 * belong to core's kthreads.
 */
struct vspace {
	uint64_t *pgd;			/* virtual address of TTBR0 page table root */
	uint64_t ustack_top;		/* current user stack top (sp_el0) */
	uint64_t mmap_base;		/* next free user VA for mmap-style allocations */
	struct dlist_node vregion_list;	/* list of vregion */
};

/* Allocate and initialize a vspace, including its page table root. */
struct vspace *vspace_alloc(void);

/* Free all vregions (and their backing pages), the page table, and @vs. */
void vspace_free(struct vspace *vs);

/* Look up the vregion containing @addr and return its backing page + offset. */
struct page *vspace_find_page(struct vspace *vs, uintptr_t addr,
			      size_t *offset);

/*
 * Map a contiguous physical memory region into the user address space.
 * Allocates a contiguous user VA range of @size bytes starting at @vs's
 * mmap_base, creates one vregion per page, and maps each page into @vs->pgd.
 * On success stores the user virtual address in @uva and returns 0.
 */
int vspace_map_contig_phys(struct vspace *vs, uint64_t phys, size_t size,
			   uint32_t flags, uint64_t *uva);

/*
 * Back @uva (page aligned) with a freshly allocated zeroed page mapped
 * with user attributes for @flags, and return its kernel alias in @kva
 * so the caller can copy into it.  On failure of anything but the page
 * allocation itself the partial mapping is left behind - destroy the
 * whole vspace to reclaim it.
 */
int vspace_map_page(struct vspace *vs, uint64_t uva, uint32_t flags,
		    void **kva);

/*
 * Switch the user page table (TTBR0_EL1) from @prev to @next.
 * Either argument may be NULL to represent "no user address space" (kernel
 * thread / idle), in which case TTBR0 is pointed at an empty page table.
 * Called from mm's switch-notification callback, so address-space
 * activation happens inside the context switch itself.
 */
void switch_vspace(struct vspace *prev, struct vspace *next);

#endif /* __VSPACE_H__ */
