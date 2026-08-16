#include "vspace.h"
#include "page.h"
#include "buddy.h"
#include "mm.h"
#include "string.h"
#include "mmu.h"
#include "errno.h"

struct vspace *vspace_alloc(void)
{
	struct vspace *vs;
	void *pgd;
	void *kstack;

	vs = kzalloc(sizeof(struct vspace));
	if (!vs)
		return NULL;

	dlist_init(&vs->vregion_list);
	vs->mmap_base = USER_MMAP_BASE;

	pgd = kzalloc_pages(PAGE_SIZE);
	if (!pgd)
		goto fail_vs;

	kstack = kzalloc_pages(PAGE_SIZE);
	if (!kstack)
		goto fail_pgd;

	vs->pgd = pgd;
	vs->kstack_top = (uint64_t)kstack + PAGE_SIZE;
	return vs;

fail_pgd:
	kfree_pages(pgd);
fail_vs:
	kfree(vs);
	return NULL;
}

void vspace_free(struct vspace *vs)
{
	struct dlist_node *node, *next;
	struct vregion *vr;

	if (!vs)
		return;

	node = vs->vregion_list.next;
	while (node != &vs->vregion_list) {
		next = node->next;
		vr = container_of(node, struct vregion, node);
		if (vr->page)
			buddy_free_pages(vr->page);
		kfree(vr);
		node = next;
	}

	if (vs->pgd)
		kfree_pages(vs->pgd);
	if (vs->kstack_top)
		kfree_pages((void *)(vs->kstack_top - PAGE_SIZE));

	kfree(vs);
}

/*
 * vspace_map_contig_phys - map a contiguous physical region into user space.
 *
 * Creates one vregion per page and maps each page into vs->pgd with @flags.
 * The VA range is taken from vs->mmap_base and mmap_base is advanced.
 * The physical region must be page aligned and the size is rounded up.
 */
int vspace_map_contig_phys(struct vspace *vs, uint64_t phys, size_t size,
			   uint32_t flags, uint64_t *uva)
{
	uint64_t va;
	size_t nr_pages;
	size_t i;
	uint64_t attr;

	if (!vs || !vs->pgd || !uva)
		return -EINVAL;

	if ((phys & (PAGE_SIZE - 1)) != 0)
		return -EINVAL;

	size = PAGE_ALIGN(size);
	nr_pages = size >> PAGE_SHIFT;
	if (nr_pages == 0)
		return -EINVAL;

	va = vs->mmap_base;
	vs->mmap_base += size;

	if (flags & VM_DEVICE)
		attr = MMU_REGION_USER_DEVICE;
	else if (flags & VM_WRITE)
		attr = MMU_REGION_USER_STACK;
	else if (flags & VM_EXEC)
		attr = MMU_REGION_USER_CODE;
	else
		attr = MMU_REGION_USER_RO;

	for (i = 0; i < nr_pages; i++) {
		struct vregion *vr;
		struct page *page;
		uint64_t paddr = phys + (i << PAGE_SHIFT);
		uint64_t vaddr = va + (i << PAGE_SHIFT);

		page = pfn_to_page(paddr >> PAGE_SHIFT);

		vr = (struct vregion *)kmalloc(sizeof(*vr));
		if (!vr)
			return -ENOMEM;
		memset(vr, 0, sizeof(*vr));

		vr->start = vaddr;
		vr->end = vaddr + PAGE_SIZE;
		vr->flags = flags;
		vr->page = page;

		mmu_map(vs->pgd, vaddr, paddr, PAGE_SIZE, attr);
		dlist_add_tail(&vs->vregion_list, &vr->node);
	}

	*uva = va;
	return 0;
}

/*
 * vspace_find_page - find the vregion containing @addr and return its backing
 * page plus the offset within that page.
 */
struct page *vspace_find_page(struct vspace *vs, uintptr_t addr,
			      size_t *offset)
{
	struct dlist_node *node;
	struct vregion *vr;

	if (!vs)
		return NULL;

	node = vs->vregion_list.next;
	while (node != &vs->vregion_list) {
		vr = container_of(node, struct vregion, node);
		if (addr >= vr->start && addr < vr->end) {
			*offset = addr - vr->start;
			return vr->page;
		}
		node = node->next;
	}

	return NULL;
}
