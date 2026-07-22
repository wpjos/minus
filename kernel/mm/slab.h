#ifndef __SLAB_H__
#define __SLAB_H__

#include "types.h"

struct kmem_cache;

struct kmem_cache *kmem_cache_create(const char *name, size_t size, size_t align);
void kmem_cache_destroy(struct kmem_cache *cachep);
void *kmem_cache_alloc(struct kmem_cache *cachep);
void kmem_cache_free(struct kmem_cache *cachep, void *objp);

void slab_init(void);

#endif
