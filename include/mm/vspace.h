#ifndef __VSPACE_H__
#define __VSPACE_H__

#include "types.h"
#include "dlist.h"

/* Forward declaration: struct page is defined in kernel/mm/page.h. */
struct page;

/* Vregion permission flags. */
#define VM_READ		(1UL << 0)
#define VM_WRITE	(1UL << 1)
#define VM_EXEC		(1UL << 2)

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
 * Virtual address space descriptor for a task. Holds the user page table root,
 * stack pointers, and the list of user vregions.
 */
struct vspace {
	uint64_t *pgd;			/* virtual address of TTBR0 page table root */
	uint64_t ustack_top;		/* current user stack top (sp_el0) */
	uint64_t kstack_top;		/* initial SP_EL1 */
	uint64_t mmap_base;		/* next free user VA for mmap-style allocations */
	struct dlist_node vregion_list;	/* list of vregion */
};

/* Allocate and initialize a vspace, including its page table and kernel stack. */
struct vspace *vspace_alloc(void);

/* Free all vregions (and their backing pages), page table, kernel stack, and @vs. */
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

#endif /* __VSPACE_H__ */
