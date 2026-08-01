#ifndef __CACHE_H__
#define __CACHE_H__

static inline void dsb_ish(void)
{
	__asm__ volatile("dsb ish" ::: "memory");
}

static inline void isb(void)
{
	__asm__ volatile("isb" ::: "memory");
}

static inline void tlbi_vmall(void)
{
	__asm__ volatile("tlbi vmalle1is"::: "memory");
}

static inline void tlbi_va(uintptr_t va)
{
	__asm__ volatile("tlbi vae1is, %0"::"r"(va >> 12): "memory");
}

#endif
