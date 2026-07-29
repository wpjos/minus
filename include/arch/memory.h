#ifndef __ARCH_MEMORY_H__
#define __ARCH_MEMORY_H__

#ifdef __LINKER__
#define ULL(x)			(x)
#else
#define ULL(x)			(x##ULL)
#endif

#define VA_BITS			48
#define VA_MAX			ULL(~0)
#define VA_SIZE			(ULL(1) << VA_BITS)
#define VA_START		(VA_MAX - VA_SIZE + ULL(1))

#define PAGE_SHIFT		12
#define PAGE_SIZE		(ULL(1) << PAGE_SHIFT)
#define PAGE_MASK		(~(PAGE_SIZE - ULL(1)))
/*
 * Kernel link base (top 1/4 of the 48-bit VA space).
 * Computed from VA_BITS so it stays non-fixed; ULL is stripped when used
 * in the linker script (ld does not accept the ULL suffix).
 */
#define VIRT_LOAD_BASE		(VA_MAX - (VA_SIZE >> 2) + ULL(1))
#define TEXT_OFFSET		0x080000
#define VIRT_LOAD_OFFSET	(VIRT_LOAD_BASE + TEXT_OFFSET)

#define VIRT_HIGH_ADDR		(VA_MAX - (ULL(1) << 30) + ULL(1))

/* FIXMAP_ADDR must 2M align */
#define FIXMAP_ADDR		(VIRT_HIGH_ADDR)
#define FIXMAP_SIZE		(ULL(1) << 16)
#define FIXMAP_PGTBL		(FIXMAP_ADDR)
#define FIXMAP_PUD		(FIXMAP_ADDR + PAGE_SIZE * 1)
#define FIXMAP_PMD		(FIXMAP_ADDR + PAGE_SIZE * 2)
#define FIXMAP_PTE		(FIXMAP_ADDR + PAGE_SIZE * 3)

/*
 * Physical-memory pool reserved after the kernel image for page-table pages
 * allocated before the buddy allocator is ready.  Must be large enough for the
 * initial identity/high-VA maps and for the temporary mappings created during
 * paging_init().
 */
#define EARLY_MMU_POOL_SIZE	(ULL(1) << 16)	/* 64k */

#ifndef __LINKER__
/*
 * Physical/virtual offset computed at boot time by start.S.
 * It equals (physical_load_address - VIRT_LOAD_OFFSET) and is written to
 * memory before any C code runs, so the conversion macros below work at
 * runtime without a platform-specific PHYS_LOAD_OFFSET build flag.
 */
extern unsigned long g_load_offset;

#define __VA_PA__(x)		((unsigned long)(x) + g_load_offset)
#define __PA_VA__(x)		((unsigned long)(x) - g_load_offset)
#endif /* __LINKER__ */

#endif /* __ARCH_MEMORY_H__ */
