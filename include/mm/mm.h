#ifndef __MM_H__
#define __MM_H__

#include "types.h"

void memblock_init(void);
void *memblock_alloc_aligned(uint64_t size, uint64_t align);
void memblock_map_all(uint64_t *pgd);
void memblock_free_to_buddy(void);

uint64_t memblock_mem_start(void);
uint64_t memblock_mem_end(void);

void mm_init(void);

void *kmalloc(size_t size);
void *kzalloc(size_t size);
void kfree(void *objp);

/* Allocate physically contiguous pages and zero them. */
void *kzalloc_pages(size_t size);
/* Free pages allocated by kzalloc_pages/buddy_alloc_pages from a virtual address. */
void kfree_pages(void *vaddr);

#endif
