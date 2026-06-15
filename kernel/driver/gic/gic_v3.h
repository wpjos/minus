#ifndef __GIC_V3_H__
#define __GIC_V3_H__

#include "types.h"
#include "sysreg.h"

/*
 * GICv3 Distributor register offsets
 */
#define GICD_CTLR		0x0000
#define GICD_TYPER		0x0004
#define GICD_IIDR		0x0008
#define GICD_STATUSR		0x0010
#define GICD_SETSPI_NSR		0x0040
#define GICD_CLRSPI_NSR		0x0048
#define GICD_SETSPI_SR		0x0050
#define GICD_CLRSPI_SR		0x0058
#define GICD_IGROUPR		0x0080
#define GICD_ISENABLER		0x0100
#define GICD_ICENABLER		0x0180
#define GICD_ISPENDR		0x0200
#define GICD_ICPENDR		0x0280
#define GICD_ISACTIVER		0x0300
#define GICD_ICACTIVER		0x0380
#define GICD_IPRIORITYR		0x0400
#define GICD_ITARGETSR		0x0800
#define GICD_ICFGR		0x0c00
#define GICD_IGRPMODR		0x0d00
#define GICD_NSACR		0x0e00
#define GICD_SGIR		0x0f00
#define GICD_IROUTER		0x6000
#define GICD_PIDR2		0xffe8

/* GICD_CTLR bits */
#define GICD_CTLR_ENABLE_G1_NS	(1U << 0)
#define GICD_CTLR_ENABLE_G0		(1U << 1)
#define GICD_CTLR_ENABLE_G1_S	(1U << 2)
#define GICD_CTLR_ARE_S			(1U << 4)
#define GICD_CTLR_ARE_NS		(1U << 5)
#define GICD_CTLR_DS			(1U << 6)
#define GICD_CTLR_RWP			(1U << 31)

/* GICD_TYPER bits */
#define GICD_TYPER_IT_LINES_NUMBER_SHIFT	0
#define GICD_TYPER_IT_LINES_NUMBER_MASK		0x1f

/*
 * GICv3 Redistributor register offsets
 */
#define GICR_CTLR		0x0000
#define GICR_IIDR		0x0004
#define GICR_TYPER		0x0008
#define GICR_STATUSR		0x0010
#define GICR_WAKER		0x0014

#define GICR_WAKER_ProcessorSleep	(1U << 1)
#define GICR_WAKER_ChildrenAsleep	(1U << 2)

/* SGI/PPI frame is at +0x10000 from redistributor base */
#define GICR_SGI_BASE_OFFSET	0x10000

#define GICR_IGROUPR0		0x0080
#define GICR_ISENABLER0		0x0100
#define GICR_ICENABLER0		0x0180
#define GICR_ISPENDR0		0x0200
#define GICR_ICPENDR0		0x0280
#define GICR_ISACTIVER0		0x0300
#define GICR_ICACTIVER0		0x0380
#define GICR_IPRIORITYR		0x0400
#define GICR_ICFGR0		0x0c00
#define GICR_ICFGR1		0x0c04
#define GICR_IGRPMODR0		0x0d00
#define GICR_NSACR		0x0e00

/*
 * GICv3 CPU interface system registers (EL1)
 */
DEFINE_SYS_REG_RW_OPS(gic, icc_sre_el1, ICC_SRE_EL1)
DEFINE_SYS_REG_RW_OPS(gic, icc_ctlr_el1, ICC_CTLR_EL1)
DEFINE_SYS_REG_RW_OPS(gic, icc_pmr_el1, ICC_PMR_EL1)
DEFINE_SYS_REG_RW_OPS(gic, icc_bpr0_el1, ICC_BPR0_EL1)
DEFINE_SYS_REG_RW_OPS(gic, icc_bpr1_el1, ICC_BPR1_EL1)
DEFINE_SYS_REG_RW_OPS(gic, icc_igrpen0_el1, ICC_IGRPEN0_EL1)
DEFINE_SYS_REG_RW_OPS(gic, icc_igrpen1_el1, ICC_IGRPEN1_EL1)
DEFINE_SYS_REG_RO_OPS(gic, icc_iar0_el1, ICC_IAR0_EL1)
DEFINE_SYS_REG_RW_OPS(gic, icc_eoir0_el1, ICC_EOIR0_EL1)
DEFINE_SYS_REG_RO_OPS(gic, icc_iar1_el1, ICC_IAR1_EL1)
DEFINE_SYS_REG_RW_OPS(gic, icc_eoir1_el1, ICC_EOIR1_EL1)
DEFINE_SYS_REG_RO_OPS(gic, icc_hppir1_el1, ICC_HPPIR1_EL1)

/* ICC_SRE_EL1 bits */
#define ICC_SRE_SRE		(1U << 0)
#define ICC_SRE_DFB		(1U << 1)
#define ICC_SRE_DIB		(1U << 2)

/* ICC_IGRPEN1_EL1 bits */
#define ICC_IGRPEN1_ENABLE	(1U << 0)

/* ICC_CTLR_EL1 bits */
#define ICC_CTLR_EOIMODE	(1U << 1)

/*
 * Interrupt ID ranges
 */
#define GIC_MIN_SGI		0
#define GIC_MAX_SGI		15
#define GIC_MIN_PPI		16
#define GIC_MAX_PPI		31
#define GIC_MIN_SPI		32
#define GIC_MAX_SPI		1019

#define GIC_NR_IRQS		1020

/*
 * Priority helpers. Lower value = higher priority.
 * GICv3 uses 8-bit priorities, but only the upper bits may be implemented.
 */
#define GIC_PRIO_DEFAULT	0xa0
#define GIC_PRIO_PMR		0xf0
#define GIC_PRIO_LOWEST		0xff

/*
 * Configuration: level vs edge
 */
#define GIC_IRQ_TYPE_LEVEL	0
#define GIC_IRQ_TYPE_EDGE	2

/*
 * Group configuration
 */
#define GIC_GROUP_G1S		0
#define GIC_GROUP_G1NS		1
#define GIC_GROUP_G0		2

/*
 * MMIO helper macros.  The driver stores distributor/redistributor bases
 * as uint64_t physical (and identity-mapped) addresses.
 */
#define gic_read32(base, off)		(*(volatile uint32_t *)((uintptr_t)(base) + (off)))
#define gic_write32(base, off, val)	(*(volatile uint32_t *)((uintptr_t)(base) + (off)) = (val))
#define gic_read64(base, off)		(*(volatile uint64_t *)((uintptr_t)(base) + (off)))
#define gic_write64(base, off, val)	(*(volatile uint64_t *)((uintptr_t)(base) + (off)) = (val))

/*
 * Public GICv3 API exposed to the rest of the kernel.
 */
int gic_irq_enable(unsigned int irq);
int gic_irq_disable(unsigned int irq);
int gic_irq_set_priority(unsigned int irq, uint8_t prio);
int gic_irq_set_type(unsigned int irq, unsigned int type);
int gic_irq_set_group(unsigned int irq, unsigned int group);
void gic_handle_irq(void);

#endif /* __GIC_V3_H__ */
