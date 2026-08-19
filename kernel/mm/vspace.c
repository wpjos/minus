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

	vs = kzalloc(sizeof(struct vspace));
	if (!vs)
		return NULL;

	dlist_init(&vs->vregion_list);
	vs->mmap_base = USER_MMAP_BASE;

	pgd = kzalloc_pages(PAGE_SIZE);
	if (!pgd) {
		kfree(vs);
		return NULL;
	}

	vs->pgd = pgd;
	return vs;
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
 * User protection flags (VM_*) to PTE attributes.  Writable-non-exec
 * gets the stack/device attributes; the W+X combination is the one
 * legacy loader mapping that stays RW-at-EL0, PXN (EL0-executable only).
 */
static uint64_t user_prot_to_attr(uint32_t flags)
{
	if (flags & VM_WRITE) {
		if (flags & VM_EXEC)
			return PTE_ATTR_NORMAL | PTE_SH_INNER |
			       PTE_AP_RW_ANY | PTE_AF | PTE_PXN;
		return MMU_REGION_USER_STACK;
	}

	if (flags & VM_EXEC)
		return MMU_REGION_USER_CODE;

	return MMU_REGION_USER_RO;
}

int vspace_map_page(struct vspace *vs, uint64_t uva, uint32_t flags,
		    void **kva)
{
	struct vregion *vr;
	struct page *page;
	uint64_t attr;

	if (!vs || !vs->pgd || !kva)
		return -EINVAL;
	if ((uva & (PAGE_SIZE - 1)) != 0)
		return -EINVAL;

	page = buddy_alloc_pages(PAGE_SIZE);
	if (!page)
		return -ENOMEM;

	vr = kmalloc(sizeof(*vr));
	if (!vr) {
		buddy_free_pages(page);
		return -ENOMEM;
	}
	memset(vr, 0, sizeof(*vr));

	attr = user_prot_to_attr(flags);

	vr->start = uva;
	vr->end = uva + PAGE_SIZE;
	vr->flags = flags;
	vr->page = page;

	mmu_map(vs->pgd, uva, page_to_phy(page), PAGE_SIZE, attr);
	dlist_add_tail(&vs->vregion_list, &vr->node);

	*kva = page_to_virt(page);
	memset(*kva, 0, PAGE_SIZE);
	return 0;
}

/*
 * Activate @next's page table on TTBR0_EL1.  Called from mm's
 * switch-notification callback inside the context switch, so every
 * thread runs with its own user mappings from its first instruction.
 */
void switch_vspace(struct vspace *prev, struct vspace *next)
{
	if (next == prev)
		return;

	if (next)
		mmu_switch_pgd(TTBR0_EL1,
			       __VA_PA__((uintptr_t)next->pgd));
	else
		mmu_clear_ttbr0();
}

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
