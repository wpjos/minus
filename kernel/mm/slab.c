#include "types.h"
#include "string.h"
#include "buddy.h"
#include "slab.h"
#include "printk.h"

#define SLAB_MAGIC_END		((uint32_t)-1)
#define KMALLOC_MIN_SHIFT	3	/* 8 bytes */
#define KMALLOC_MAX_SHIFT	11	/* 2048 bytes */
#define KMALLOC_MAX_SIZE	(1UL << KMALLOC_MAX_SHIFT)
#define KMALLOC_CACHES_NUM	(KMALLOC_MAX_SHIFT - KMALLOC_MIN_SHIFT + 1)

struct slab {
	struct dlist_node node;
	struct kmem_cache *cachep;
	uint32_t free;
	uint32_t inuse;
	void *s_mem;
};

struct kmem_cache {
	const char *name;
	uint32_t obj_size;
	uint32_t align;
	uint32_t num;
	struct dlist_node slabs_partial;
	struct dlist_node slabs_full;
	struct dlist_node slabs_free;
};

static struct kmem_cache kmalloc_caches[KMALLOC_CACHES_NUM];

static void kmem_cache_init(struct kmem_cache *cachep, const char *name,
			    uint32_t size, uint32_t align)
{
	dlist_init(&cachep->slabs_partial);
	dlist_init(&cachep->slabs_full);
	dlist_init(&cachep->slabs_free);
	cachep->name = name;
	cachep->num = 0;
	cachep->align = align ? align : 8;
	cachep->obj_size = ALIGN_UP(size, cachep->align);

	uint32_t desc_size = ALIGN_UP(sizeof(struct slab), cachep->align);
	if (desc_size + cachep->obj_size > PAGE_SIZE) {
		cachep->num = 0;
	} else {
		cachep->num = (PAGE_SIZE - desc_size) / cachep->obj_size;
	}
}

static struct slab *kmem_cache_grow(struct kmem_cache *cachep)
{
	struct page *page;
	struct slab *slab;
	void *s_mem;
	uint32_t i;
	uint32_t *fp;

	if (cachep->num == 0)
		return NULL;

	page = buddy_alloc_pages(PAGE_SIZE);
	if (!page)
		return NULL;

	page->in_slab = 1;

	slab = (struct slab *)page_to_virt(page);
	s_mem = (void *)ALIGN_UP((uintptr_t)slab + sizeof(struct slab), cachep->align);

	slab->cachep = cachep;
	slab->free = 0;
	slab->inuse = 0;
	slab->s_mem = s_mem;

	for (i = 0; i < cachep->num; i++) {
		fp = (uint32_t *)(s_mem + i * cachep->obj_size);
		*fp = i + 1;
	}
	*fp = SLAB_MAGIC_END;

	dlist_add(&cachep->slabs_free, &slab->node);

	return slab;
}

struct kmem_cache *kmem_cache_create(const char *name, uint32_t size, uint32_t align)
{
	struct page *page;
	struct kmem_cache *cachep;

	if (size == 0)
		return NULL;

	page = buddy_alloc_pages(PAGE_SIZE);
	if (!page)
		return NULL;

	cachep = (struct kmem_cache *)page_to_virt(page);
	kmem_cache_init(cachep, name, size, align);
	if (cachep->num == 0) {
		buddy_free_pages(page);
		return NULL;
	}

	return cachep;
}

static void kmem_cache_free_slabs(struct dlist_node *head)
{
	struct dlist_node *dnode, *next;
	struct slab *slab;
	struct page *page;

	dnode = head->next;
	while (dnode != head) {
		next = dnode->next;
		slab = container_of(dnode, struct slab, node);
		dlist_del(dnode);
		page = virt_to_page(slab);
		page->in_slab = 0;
		buddy_free_pages(page);
		dnode = next;
	}
}

void kmem_cache_destroy(struct kmem_cache *cachep)
{
	if (!cachep)
		return;

	kmem_cache_free_slabs(&cachep->slabs_partial);
	kmem_cache_free_slabs(&cachep->slabs_full);
	kmem_cache_free_slabs(&cachep->slabs_free);

	buddy_free_pages(virt_to_page(cachep));
}

void *kmem_cache_alloc(struct kmem_cache *cachep)
{
	struct slab *slab;
	uint32_t idx;

	if (!cachep || cachep->num == 0)
		return NULL;

	if (dlist_empty(&cachep->slabs_partial)) {
		if (dlist_empty(&cachep->slabs_free)) {
			slab = kmem_cache_grow(cachep);
			if (!slab)
				return NULL;
		} else {
			slab = container_of(cachep->slabs_free.next, struct slab, node);
		}
		dlist_del(&slab->node);
		dlist_add(&cachep->slabs_partial, &slab->node);
	} else {
		slab = container_of(cachep->slabs_partial.next, struct slab, node);
	}

	idx = slab->free;
	slab->free = *(uint32_t *)(slab->s_mem + idx * cachep->obj_size);
	slab->inuse++;

	if (slab->free == SLAB_MAGIC_END) {
		dlist_del(&slab->node);
		dlist_add(&cachep->slabs_full, &slab->node);
	}

	return slab->s_mem + idx * cachep->obj_size;
}

void kmem_cache_free(struct kmem_cache *cachep, void *objp)
{
	struct page *page;
	struct slab *slab;
	uint32_t idx;

	if (!objp)
		return;

	page = virt_to_page(objp);
	slab = (struct slab *)page_to_virt(page);

	idx = (uint32_t)((objp - slab->s_mem) / cachep->obj_size);
	*(uint32_t *)objp = slab->free;
	slab->free = idx;
	slab->inuse--;

	if (slab->inuse == 0) {
		dlist_del(&slab->node);
		page->in_slab = 0;
		buddy_free_pages(page);
	} else if (slab->inuse + 1 == cachep->num) {
		/* was full, now partial */
		dlist_del(&slab->node);
		dlist_add(&cachep->slabs_partial, &slab->node);
	}
}

static inline uint32_t kmalloc_index(uint32_t size)
{
	uint32_t order = 0;
	uint32_t s = (size - 1) >> KMALLOC_MIN_SHIFT;

	while (s) {
		s >>= 1;
		order++;
	}
	return order;
}

void *kmalloc(uint32_t size)
{
	uint32_t idx;

	if (size == 0)
		return NULL;
	if (size > KMALLOC_MAX_SIZE) {
		struct page *page = buddy_alloc_pages(size);
		return page ? page_to_virt(page) : NULL;
	}

	idx = kmalloc_index(size);
	return kmem_cache_alloc(&kmalloc_caches[idx]);
}

void kfree(void *objp)
{
	struct page *page;

	if (!objp)
		return;

	page = virt_to_page(objp);
	if (page->in_slab) {
		struct slab *slab = (struct slab *)page_to_virt(page);
		kmem_cache_free(slab->cachep, objp);
	} else {
		buddy_free_pages(page);
	}
}

void slab_init(void)
{
	static const char *names[KMALLOC_CACHES_NUM] = {
		"kmalloc-8", "kmalloc-16", "kmalloc-32", "kmalloc-64",
		"kmalloc-128", "kmalloc-256", "kmalloc-512",
		"kmalloc-1024", "kmalloc-2048"
	};

	for (uint32_t i = 0, j = KMALLOC_MIN_SHIFT; i < KMALLOC_CACHES_NUM; i++, j++) {
		kmem_cache_init(&kmalloc_caches[i], names[i], 1 << j, 8);
	}
}
