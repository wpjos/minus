#ifndef __ARCH_MMU_H__
#define __ARCH_MMU_H__

#include "types.h"

/*
 * AArch64 stage-1 translation table helpers (4 KB granule).
 * These are used by the early identity-map page tables built before
 * start_kernel() is entered.
 */

/* Descriptor types (bits [1:0]) */
#define PTE_TYPE_INVALID    0x0ULL
#define PTE_TYPE_BLOCK      0x1ULL   /* valid block descriptor */
#define PTE_TYPE_TABLE      0x3ULL   /* valid table descriptor */
#define PTE_TYPE_PAGE       0x3ULL   /* valid page descriptor at L3 */

/* Memory attribute index (bits [4:2]) into MAIR_EL1 */
#define PTE_ATTR_NORMAL     (0x0ULL << 2)  /* MAIR index 0: Normal WBWA */
#define PTE_ATTR_DEVICE     (0x1ULL << 2)  /* MAIR index 1: Device-nGnRnE */

/* Access permissions (bits [7:6]) */
#define PTE_AP_RW_EL1       (0x0ULL << 6)  /* RW at EL1, none at EL0 */
#define PTE_AP_RW_ANY       (0x1ULL << 6)  /* RW at any EL */
#define PTE_AP_RO_EL1       (0x2ULL << 6)  /* RO at EL1, none at EL0 */
#define PTE_AP_RO_ANY       (0x3ULL << 6)  /* RO at any EL */

/* Shareability (bits [9:8]) */
#define PTE_SH_NON          (0x0ULL << 8)
#define PTE_SH_OUTER        (0x2ULL << 8)
#define PTE_SH_INNER        (0x3ULL << 8)

/* Other bits */
#define PTE_AF              (1ULL << 10)   /* Access Flag */
#define PTE_NG              (1ULL << 11)   /* Non-global */
#define PTE_PXN             (1ULL << 53)   /* Privileged Execute Never */
#define PTE_UXN             (1ULL << 54)   /* Unprivileged Execute Never */

/* Descriptor field masks */
#define PTE_TYPE_MASK       0x3ULL
#define PTE_PHYS_MASK       0xFFFFFFFFF000ULL

/*
 * Stage-1 descriptor construction.
 * The physical address must already be aligned to the appropriate block/page
 * size (table: 4 KB, block at L2: 2 MB, page at L3: 4 KB).
 */
#define PTE_TABLE(pa) \
    (((uint64_t)(pa) & PTE_PHYS_MASK) | PTE_TYPE_TABLE)

#define PTE_BLOCK(pa, attr) \
    (((uint64_t)(pa) & 0xFFFFE00000ULL) | PTE_TYPE_BLOCK | (attr) | PTE_AF)

/* Number of entries per 4 KB translation table */
#define PTE_ENTRIES         512

/* Page table level shifts for 4 KB granule, 48-bit VA */
#define PGD_SHIFT           39      /* Level 0: 512 GB */
#define PUD_SHIFT           30      /* Level 1: 1 GB */
#define PMD_SHIFT           21      /* Level 2: 2 MB */
#define PTE_SHIFT           12      /* Level 3: 4 KB */

/*
 * Early 48-bit page-table allocation.
 *   1 PGD (level 0, 512 x 512 GB)
 *   EARLY_PUD_PAGES PUDs (level 1, 512 x 1 GB)
 *   EARLY_PMD_PAGES PMDs (level 2, 512 x 2 MB)
 *
 * The default sizes are enough for the kernel image mapped both at its
 * physical load address and at its linked high virtual address, plus the
 * UART and a few runtime MMIO regions.
 */
#ifndef EARLY_PUD_PAGES
#define EARLY_PUD_PAGES     4
#endif
#ifndef EARLY_PMD_PAGES
#define EARLY_PMD_PAGES     8
#endif
#define EARLY_PGD_PAGES     1
#define EARLY_PGTABLE_PAGES (EARLY_PGD_PAGES + EARLY_PUD_PAGES + EARLY_PMD_PAGES)
#define EARLY_PGTABLE_SIZE  (EARLY_PGTABLE_PAGES * 4096)

/* Region attributes passed to early_mmu_map_blocks() */
#define MMU_REGION_NORMAL   (PTE_ATTR_NORMAL | PTE_SH_INNER | PTE_AP_RW_EL1)
#define MMU_REGION_DEVICE   (PTE_ATTR_DEVICE | PTE_AP_RW_EL1 | PTE_PXN | PTE_UXN)
#define MMU_REGION_NORMAL_RO    (PTE_ATTR_NORMAL | PTE_SH_INNER | PTE_AP_RO_EL1)
#define MMU_REGION_NORMAL_XN    (PTE_ATTR_NORMAL | PTE_SH_INNER | PTE_AP_RW_EL1 | PTE_PXN)

/*
 * TCR_EL1 configuration: 4 KB granule, 48-bit VA for both halves,
 * Normal Inner/Outer WBWA, Inner Shareable, 48-bit physical address space.
 */
#define TCR_T0SZ_SHIFT      0
#define TCR_T0SZ_48BIT      (16ULL << TCR_T0SZ_SHIFT)
#define TCR_EPD0            (0ULL << 7)    /* bit 7: TTBR0 walk enabled */
#define TCR_IRGN0_SHIFT     8
#define TCR_IRGN0_WBWA      (1ULL << TCR_IRGN0_SHIFT)    /* 01 = Inner WBWA */
#define TCR_ORGN0_SHIFT     10
#define TCR_ORGN0_WBWA      (1ULL << TCR_ORGN0_SHIFT)    /* 01 = Outer WBWA */
#define TCR_SH0_SHIFT       12
#define TCR_SH0_INNER       (3ULL << TCR_SH0_SHIFT)      /* 11 = Inner Shareable */
#define TCR_TG0_SHIFT       14
#define TCR_TG0_4KB         (0ULL << TCR_TG0_SHIFT)      /* 00 = 4 KB */
#define TCR_T1SZ_SHIFT      16
#define TCR_T1SZ_48BIT      (16ULL << TCR_T1SZ_SHIFT)
#define TCR_A1              (0ULL << 22)
#define TCR_IRGN1_SHIFT     24
#define TCR_IRGN1_WBWA      (1ULL << TCR_IRGN1_SHIFT)
#define TCR_ORGN1_SHIFT     26
#define TCR_ORGN1_WBWA      (1ULL << TCR_ORGN1_SHIFT)
#define TCR_SH1_SHIFT       28
#define TCR_SH1_INNER       (3ULL << TCR_SH1_SHIFT)
#define TCR_TG1_SHIFT       30
#define TCR_TG1_4KB         (2ULL << TCR_TG1_SHIFT)      /* 10 = 4 KB */
#define TCR_IPS_SHIFT       32
#define TCR_IPS_48BIT       (5ULL << TCR_IPS_SHIFT)      /* 101 = 48-bit PA */

#define MMU_TCR_VAL         (TCR_T0SZ_48BIT | TCR_EPD0 | \
                             TCR_IRGN0_WBWA | TCR_ORGN0_WBWA | TCR_SH0_INNER | \
                             TCR_TG0_4KB | \
                             TCR_T1SZ_48BIT | TCR_A1 | \
                             TCR_IRGN1_WBWA | TCR_ORGN1_WBWA | TCR_SH1_INNER | \
                             TCR_TG1_4KB | TCR_IPS_48BIT)

/* MAIR_EL1: Attr0 = Normal WBWA, Attr1 = Device-nGnRnE */
#define MAIR_ATTR0_NORMAL   0xFFULL
#define MAIR_ATTR1_DEVICE   0x04ULL
#define MMU_MAIR_VAL        ((MAIR_ATTR1_DEVICE << 8) | MAIR_ATTR0_NORMAL)

/* SCTLR_EL1 enable bits */
#define SCTLR_M             (1ULL << 0)
#define SCTLR_C             (1ULL << 2)
#define SCTLR_I             (1ULL << 12)
#define MMU_SCTLR_ENABLE    (SCTLR_M | SCTLR_C | SCTLR_I)

void early_mmu_init();

/* Runtime MMIO mapping (available only after paging_init()). */
void *mmu_ioremap(uint64_t phys, uint64_t size);

void mmu_sync(void);
void mmu_map_blocks(uint64_t va, uint64_t pa, uint64_t size, uint64_t attr);
void mmu_disable_ttbr0(void);

/* Called by paging_init() once the buddy allocator is ready. */
void mmu_set_paging_ready(void);

#endif /* __ARCH_MMU_H__ */
