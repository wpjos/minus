#include "vma.h"
#include "page.h"
#include "buddy.h"
#include "mm.h"
#include "string.h"
#include "mmu.h"
#include "errno.h"

struct mm_struct *vma_alloc_init(void)
{
	struct mm_struct *mm = kmalloc(sizeof(struct mm_struct));
	if (mm != NULL) {
		memset(mm, 0, sizeof(*mm));
		dlist_init(&mm->vma_list);
		mm->mmap_base = USER_MMAP_BASE;
	}
	return mm;
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

	if (mm->pgd) {
		kfree_pages(mm->pgd);
	}
	if (mm->kstack_top) {
		kfree_pages((void *)(mm->kstack_top - PAGE_SIZE));
	}
}

/*
 * vma_map_contig_phys - map a contiguous physical region into user space.
 *
 * Creates one VMA per page and maps each page into mm->pgd with @flags.
 * The VA range is taken from mm->mmap_base and mmap_base is advanced.
 * The physical region must be page aligned and the size is rounded up.
 */
int vma_map_contig_phys(struct mm_struct *mm, uint64_t phys, size_t size,
			uint32_t flags, uint64_t *uva)
{
	uint64_t va;
	size_t nr_pages;
	size_t i;
	uint64_t attr;

	if (!mm || !mm->pgd || !uva)
		return -EINVAL;

	if ((phys & (PAGE_SIZE - 1)) != 0)
		return -EINVAL;

	size = PAGE_ALIGN(size);
	nr_pages = size >> PAGE_SHIFT;
	if (nr_pages == 0)
		return -EINVAL;

	va = mm->mmap_base;
	mm->mmap_base += size;

	if (flags & VM_WRITE)
		attr = MMU_REGION_USER_STACK;
	else if (flags & VM_EXEC)
		attr = MMU_REGION_USER_CODE;
	else
		attr = MMU_REGION_USER_RO;

	for (i = 0; i < nr_pages; i++) {
		struct vm_area_struct *vma;
		struct page *page;
		uint64_t paddr = phys + (i << PAGE_SHIFT);
		uint64_t vaddr = va + (i << PAGE_SHIFT);

		page = pfn_to_page(paddr >> PAGE_SHIFT);

		vma = (struct vm_area_struct *)kmalloc(sizeof(*vma));
		if (!vma)
			return -ENOMEM;
		memset(vma, 0, sizeof(*vma));

		vma->start = vaddr;
		vma->end = vaddr + PAGE_SIZE;
		vma->flags = flags;
		vma->page = page;

		mmu_map(mm->pgd, vaddr, paddr, PAGE_SIZE, attr);
		dlist_add_tail(&mm->vma_list, &vma->node);
	}

	*uva = va;
	return 0;
}

/*
 * vma_find_page - find the VMA containing @addr and return its backing page
 * plus the offset within that page.
 */
struct page *vma_find_page(struct mm_struct *mm, uintptr_t addr,
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
