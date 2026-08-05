#ifndef __VMA_H__
#define __VMA_H__

#include "types.h"
#include "dlist.h"

/* Forward declaration: struct page is defined in kernel/mm/page.h. */
struct page;

/* VMA permission flags. */
#define VM_READ		(1UL << 0)
#define VM_WRITE	(1UL << 1)
#define VM_EXEC		(1UL << 2)

/*
 * Virtual memory area.
 * For now each VMA backs exactly one 4 KB page; the model can later be
 * extended to multi-page regions by replacing/page with an array/tree.
 */
struct vm_area_struct {
	struct dlist_node node;		/* linked into mm_struct.vma_list */
	uint64_t start;			/* inclusive virtual start */
	uint64_t end;			/* exclusive virtual end */
	uint32_t flags;		/* VM_READ | VM_WRITE | VM_EXEC */
	struct page *page;		/* backing physical page */
};

/*
 * Memory descriptor for a task. Holds the user page table root, stack
 * pointers, and the list of user VMAs.
 */
struct mm_struct {
	uint64_t *pgd;			/* virtual address of TTBR0 page table root */
	uint64_t ustack_top;		/* current user stack top (sp_el0) */
	uint64_t kstack_top;		/* initial SP_EL1 */
	struct dlist_node vma_list;	/* list of vm_area_struct */
};

/* Initialize the VMA subsystem for a freshly allocated mm_struct. */
struct mm_struct *vma_alloc_init(void);

/* Free all VMAs (and their backing pages) attached to @mm. */
void vma_free_all(struct mm_struct *mm);

/* Look up the VMA containing @addr and return its backing page + offset. */
struct page *vma_find_page(struct mm_struct *mm, uintptr_t addr,
			   size_t *offset);

#endif /* __VMA_H__ */
