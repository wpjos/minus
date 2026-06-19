#ifndef __MM_H__
#define __MM_H__

#include "types.h"

void early_mm_init(void);
void *early_mm_alloc(uint64_t size);
void early_mm_reserve(uint64_t base, uint64_t size);

#endif
