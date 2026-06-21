#include "types.h"
#include "string.h"
#include "sysreg.h"
#include "mmu.h"
#include "memory.h"

/*
 * Early stage-1 page tables for 48-bit VA, 4 KB granule.
 *
 * The kernel is linked at high virtual addresses but loaded at a low physical
 * address, and this code runs with the MMU off.  Because the CPU executes
 * from its physical load address, PC-relative symbol references (adrp/ldr=)
 * resolve to *physical* addresses.  Therefore all static/global variables in
 * this file are accessed through their physical addresses automatically.
 */

#define LEVEL_0		0
#define LEVEL_1		1
#define LEVEL_2		2
#define LEVEL_N		3

/*
 * Two fixed three-level tables: one for the identity map used at boot and for
 * runtime MMIO, and one for the kernel's high virtual mapping.  The linker
 * reserves three contiguous 4 KB pages per table (PGD, PUD, PMD).
 */
extern uint64_t __init_pgd[LEVEL_N * PTE_ENTRIES];
extern uint64_t __idmap_pgd[LEVEL_N * PTE_ENTRIES];

static const uint32_t shift_table[LEVEL_N] = {PGD_SHIFT, PUD_SHIFT, PMD_SHIFT};

#define BLOCK_SIZE	0x200000ULL	/* 2 MB */

/*
 * Return a pointer to the descriptor at @level that covers @va.
 * The table is a fixed three-level array: tbl[0] is the PGD, tbl[1] the PUD
 * and tbl[2] the PMD.
 */
static uint64_t *early_mmu_get_page_desc(uint64_t *tbl, uint64_t va, uint32_t level)
{
    uint64_t idx = (va >> shift_table[level]) & (PTE_ENTRIES - 1);
    return &tbl[level * PTE_ENTRIES + idx];
}

/*
 * Return the physical address of the fixed page-table page at @level.
 * MMU-off callers pass tbl as a physical address; MMU-on callers reference the
 * identity-map table through its linked high VA, so translate back to PA.
 */
static uint64_t early_mmu_table_phys(uint64_t *tbl, uint32_t level)
{
    uint64_t addr = (uint64_t)&tbl[level * PTE_ENTRIES];

    if (addr >= VIRT_LOAD_BASE)
        return __VA_PA__(addr);

    return addr;
}

/*
 * Map a single translation entry for @va.
 *
 * For non-leaf levels (LEVEL_0/LEVEL_1) we install a table descriptor pointing
 * at the next fixed-level page.  At LEVEL_2 we install a 2 MB block descriptor
 * mapping @va to @pa with @attr.
 */
static void early_mmu_map_block(uint64_t *tbl, uint64_t va, uint64_t pa,
                                uint32_t level, uint32_t level_n, uint64_t attr)
{
    uint64_t *desc = early_mmu_get_page_desc(tbl, va, level);

    if (level == level_n) {
        *desc = PTE_BLOCK(pa, attr);
        return;
    }
    *desc = PTE_TABLE(early_mmu_table_phys(tbl, level + 1));
    early_mmu_map_block(tbl, va, pa, level + 1, level_n, attr);
}

/*
 * Map [va, va+size) -> [pa + (va - aligned_va), ...) using 2 MB blocks.
 * Both @va and @pa are expanded to whole 2 MB boundaries (down for the start,
 * up for the limit) because the mapping uses L2 block descriptors.
 *
 * A 2 MB block descriptor preserves the low 21 bits of the VA inside the block,
 * so @va and @pa must have the same 2 MB offset.  The caller is responsible
 * for ensuring this; if it is violated we hang early so the mistake is obvious.
 */
#define EARLY_MMU_BUG(cond)     do { if (cond) { while (1); } } while (0)

static void early_mmu_map_range(uint64_t *tbl,
                                uint64_t va, uint64_t pa,
                                uint64_t size, uint64_t attr)
{
    uint64_t start_va = va & ~(BLOCK_SIZE - 1);
    uint64_t start_pa = pa & ~(BLOCK_SIZE - 1);
    uint64_t limit = ALIGN_UP(va + size, BLOCK_SIZE);
    uint64_t blk_va, blk_pa;

    EARLY_MMU_BUG((va & (BLOCK_SIZE - 1)) != (pa & (BLOCK_SIZE - 1)));

    for (blk_va = start_va, blk_pa = start_pa; blk_va < limit;
         blk_va += BLOCK_SIZE, blk_pa += BLOCK_SIZE) {
        early_mmu_map_block(tbl, blk_va, blk_pa, LEVEL_0, LEVEL_2, attr);
    }
}

static void mmu_sync(void)
{
    __asm__ volatile("dsb sy" ::: "memory");
    __asm__ volatile("tlbi vmalle1" ::: "memory");
    __asm__ volatile("dsb sy" ::: "memory");
    __asm__ volatile("isb" ::: "memory");
}

void early_mmu_init(void)
{
    extern char __image_start[], __image_end[];
    uint64_t image_start_pa = (uint64_t)__image_start;
    uint64_t image_start_va = __PA_VA__(image_start_pa);
    uint64_t image_size = (uint64_t)__image_end - (uint64_t)__image_start;
    uint64_t sctlr;

    memset(__init_pgd, 0, LEVEL_N * PAGE_SIZE);
    memset(__idmap_pgd, 0, LEVEL_N * PAGE_SIZE);

    /*
     * Identity-map the kernel physical load region so execution survives
     * the MMU-enable moment while the CPU is still at a physical PC.
     */
    early_mmu_map_range(__idmap_pgd, image_start_pa, image_start_pa,
                        image_size, MMU_REGION_NORMAL);

    /*
     * Map the kernel at its linked high virtual address.
     * This is the mapping the kernel uses once it jumps to VA space.
     */
    early_mmu_map_range(__init_pgd, image_start_va, image_start_pa,
                        image_size, MMU_REGION_NORMAL);

    /*
     * Ensure all page table writes are visible and any stale TLB entries are
     * flushed before configuring the MMU.
     */
    mmu_sync();

    /* Configure translation regime for 48-bit VA */
    sys_reg_write(MAIR_EL1, MMU_MAIR_VAL);
    sys_reg_write(TCR_EL1, MMU_TCR_VAL);
    sys_reg_write(TTBR0_EL1, (uint64_t)__idmap_pgd);
    sys_reg_write(TTBR1_EL1, (uint64_t)__init_pgd);
    __asm__ volatile("isb" ::: "memory");

    /* Enable MMU, D-cache and I-cache */
    sctlr = sys_reg_read(SCTLR_EL1);
    sctlr |= MMU_SCTLR_ENABLE;
    sys_reg_write(SCTLR_EL1, sctlr);
    __asm__ volatile("isb" ::: "memory");

    /* Invalidate the entire instruction cache for the new regime */
    __asm__ volatile("ic iallu" ::: "memory");
    __asm__ volatile("dsb sy" ::: "memory");
    __asm__ volatile("isb" ::: "memory");
}

/*
 * Runtime region mapping (called by drivers after early_mmu_init).
 * Maps [phys, phys+size) as Device-nGnRnE and returns the virtual address.
 * Devices are kept identity-mapped, so the returned VA equals @phys.
 */
void *early_mmu_ioremap(uint64_t pa, uint64_t size)
{
    early_mmu_map_range(__idmap_pgd, pa, pa, size, MMU_REGION_DEVICE);
    mmu_sync();
    return (void *)(uintptr_t)pa;
}
