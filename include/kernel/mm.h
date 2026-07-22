#ifndef __MM_H__
#define __MM_H__

#include "types.h"

void memblock_init(void);
void *memblock_alloc_aligned(uint64_t size, uint64_t align);
void memblock_map_all(uint64_t *pgd);
void memblock_free_to_buddy(void);
void early_mm_reserve(uint64_t base, uint64_t size);

uint64_t memblock_mem_start(void);
uint64_t memblock_mem_end(void);

void mm_init(void);

void *kmalloc(size_t size);
void kfree(void *objp);

#endif
