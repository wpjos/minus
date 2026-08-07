#ifndef __ARCH_CACHE_H__
#define __ARCH_CACHE_H__

#include "types.h"

static inline void dsb_ish(void)
{
	__asm__ volatile("dsb ish" ::: "memory");
}

static inline void dc_civac(uintptr_t va)
{
	__asm__ volatile("dc civac, %0" : : "r"(va) : "memory");
}

static inline void ic_ivau(uintptr_t va)
{
	__asm__ volatile("ic ivau, %0" : : "r"(va) : "memory");
}

static inline void flush_dcache_icache_range(void *start, size_t len)
{
	uintptr_t va = (uintptr_t)start;
	uintptr_t end = va + len;

	/* Ensure all writes to the range are visible before cache maintenance. */
	dsb_ish();

	while (va < end) {
		dc_civac(va);
		va += 64;
	}
	dsb_ish();

	va = (uintptr_t)start;
	while (va < end) {
		ic_ivau(va);
		va += 64;
	}
	dsb_ish();
	__asm__ volatile("isb" ::: "memory");
}

#endif /* __ARCH_CACHE_H__ */
