#ifndef __MM_H__
#define __MM_H__

#include "types.h"

void early_mm_init(void);
void *early_mm_alloc_aligned(uint64_t size, uint64_t align);
void early_mm_reserve(uint64_t base, uint64_t size);
void early_mm_stat(void);

uint64_t early_mm_memory_start(void);
uint64_t early_mm_memory_end(void);
int early_mm_memory_region_count(void);
void early_mm_memory_region(int idx, uint64_t *base, uint64_t *size);

void paging_init(void);

#endif
