#include "page.h"
#include "string.h"
#include "buddy.h"
#include "printk.h"

struct free_area g_free_area[BUDDY_MAX_ORDER];

static size_t get_order(size_t size)
{
	size_t order = 0;

	size = PAGE_ALIGN(size) >> PAGE_SHIFT;
	while ((1 << order) < size && order < BUDDY_MAX_ORDER) {
		order++;
	}
	return order;
}

static void buddy_add_to_list(struct page *page, size_t order)
{
	g_free_area[order].nr_free++;
	page->order = order;
	page->in_buddy = 1;
	dlist_add(&g_free_area[order].free_list, &page->dnode);
}

static void buddy_del_from_list(struct page *page)
{
	g_free_area[page->order].nr_free--;
	page->order = 0;
	page->in_buddy = 0;
	dlist_del(&page->dnode);
}

static bool buddy_exist(struct page *buddy, size_t order)
{
	if (buddy->in_buddy && buddy->order == order) {
		return true;
	}
	return false;
}

static void buddy_add_page(size_t pfn, size_t order)
{
	struct page *page;
	struct page *buddy;
	size_t buddy_pfn;

	while (order < BUDDY_MAX_ORDER - 1) {
		/*
		 pfn = 2 ^ n * (idx)
		 buddy = 2 ^ n * (idx + 1)
		 pfn ^ buddy = 2 ^ n * [idx ^ (idx + 1)]
		 */
		page = pfn_to_page(pfn);
		buddy_pfn = (1 << order) ^ pfn;
		buddy = pfn_to_page(buddy_pfn);
		if (!buddy_exist(buddy, order)) {
			break;
		}

		buddy_del_from_list(buddy);

		pfn = pfn & buddy_pfn;
		order++;
	}
	page = pfn_to_page(pfn);
	buddy_add_to_list(page, order);
}

static size_t pfn_order_align(size_t pfn)
{
	size_t order = 0;

	while (((pfn >> order) & 1) == 0 && order < BUDDY_MAX_ORDER - 1)
		order++;
	return order;
}

static size_t order_max_pages(size_t nr_pages)
{
	size_t order = BUDDY_MAX_ORDER - 1;

	while ((1UL << order) > nr_pages)
		order--;
	return order;
}

void buddy_free_pages_range(uint64_t base, uint64_t size)
{
	uint64_t start = ALIGN_UP(base, PAGE_SIZE);
	uint64_t end = ALIGN_DOWN(base + size, PAGE_SIZE);
	size_t pfn, nr_pages;

	if (end <= start)
		return;

	pfn = start >> PAGE_SHIFT;
	nr_pages = (end - start) >> PAGE_SHIFT;

	while (nr_pages) {
		size_t order = pfn_order_align(pfn);
		size_t max_order = order_max_pages(nr_pages);

		if (order > max_order)
			order = max_order;

		buddy_add_page(pfn, order);
		pfn += 1UL << order;
		nr_pages -= 1UL << order;
	}
}

static void buddy_split(struct page *page, size_t l_order, size_t h_order)
{
	while (h_order > l_order) {
		h_order--;
		buddy_add_to_list(&page[1 << h_order], h_order);
	}
}

struct page *buddy_alloc_pages(size_t size)
{
	size_t order = get_order(size);
	struct page *page = NULL;
	struct dlist_node *dnode;
	size_t h_order = order;
	while (h_order < BUDDY_MAX_ORDER && g_free_area[h_order].nr_free == 0) {
		h_order++;
	}
	if (h_order < BUDDY_MAX_ORDER) {
		dnode = g_free_area[h_order].free_list.next;
		page = container_of(dnode, struct page, dnode);
		buddy_del_from_list(page);
		buddy_split(page, order, h_order);
		page->order = order;
	}
	return page;
}

void buddy_free_pages(struct page *page)
{
	size_t pfn = page_to_pfn(page);
	size_t order = page->order;

	buddy_add_page(pfn, order);
}

void buddy_stat(void)
{
	size_t free_pages = 0;
	for (size_t i = 0; i < BUDDY_MAX_ORDER; i++) {
		free_pages += (1 << i) * g_free_area[i].nr_free;
	}
	printk("==============buddy stat begin=============\n");
	printk("|| free pages = %u\n", (unsigned)free_pages);
	printk("==============buddy stat end===============\n");
}

void buddy_init(void)
{
	memset(&g_free_area, 0, sizeof(g_free_area));
	for (size_t i = 0; i < BUDDY_MAX_ORDER; i++) {
		dlist_init(&g_free_area[i].free_list);
	}
}
