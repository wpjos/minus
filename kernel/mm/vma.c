#include "vma.h"
#include "page.h"
#include "buddy.h"
#include "mm.h"
#include "string.h"

void vma_init_mm(struct mm_struct *mm)
{
	dlist_init(&mm->vma_list);
}

void vma_free_all(struct mm_struct *mm)
{
	struct dlist_node *node, *next;
	struct vm_area_struct *vma;

	if (!mm)
		return;

	node = mm->vma_list.next;
	while (node != &mm->vma_list) {
		next = node->next;
		vma = container_of(node, struct vm_area_struct, node);
		if (vma->page)
			buddy_free_pages(vma->page);
		kfree(vma);
		node = next;
	}
}

/*
 * vma_find_page - find the VMA containing @addr and return its backing page
 * plus the offset within that page.
 */
struct page *vma_find_page(struct mm_struct *mm, uint64_t addr,
			   size_t *offset)
{
	struct dlist_node *node;
	struct vm_area_struct *vma;

	if (!mm)
		return NULL;

	node = mm->vma_list.next;
	while (node != &mm->vma_list) {
		vma = container_of(node, struct vm_area_struct, node);
		if (addr >= vma->start && addr < vma->end) {
			*offset = addr - vma->start;
			return vma->page;
		}
		node = node->next;
	}

	return NULL;
}
