#ifndef __GIC_V2_H__
#define __GIC_V2_H__

#include "types.h"
#include "irq.h"

/*
 * GIC-400 (ARM Generic Interrupt Controller v2) register definitions.
 *
 * GIC-400 has four memory regions:
 *   reg[0]: Distributor (GICD)
 *   reg[1]: CPU interface (GICC)
 *   reg[2]: Virtual interface control (GICH)  -- not used here
 *   reg[3]: Virtual CPU interface (GICV)      -- not used here
 *
 * This driver only uses GICD and GICC.
 */

/*
 * Distributor register offsets
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
#define GICD_SGIR		0x0f00

/* GICD_CTLR bits */
#define GICD_CTLR_ENABLE	(1U << 0)

/* GICD_TYPER bits */
#define GICD_TYPER_IT_LINES_NUMBER_SHIFT	0
#define GICD_TYPER_IT_LINES_NUMBER_MASK		0x1f
#define GICD_TYPER_CPU_NUMBER_SHIFT		5
#define GICD_TYPER_CPU_NUMBER_MASK		(0x7U << 5)

/*
 * CPU interface register offsets
 */
#define GICC_CTLR		0x0000
#define GICC_PMR		0x0004
#define GICC_BPR		0x0008
#define GICC_IAR		0x000c
#define GICC_EOIR		0x0010
#define GICC_RPR		0x0014
#define GICC_HPIR		0x0018
#define GICC_ABPR		0x001c

/* GICC_CTLR bits (non-secure view) */
#define GICC_CTLR_ENABLE	(1U << 0)

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
#define GIC_SPURIOUS_IRQ	1023

/*
 * Priority helpers. Lower value = higher priority.
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
 *   IGROUPR = 0  -> Group 0 (Secure)
 *   IGROUPR = 1  -> Group 1 (Non-secure)
 */
#define GIC_GROUP_G1NS		1
#define GIC_GROUP_G0		0

/*
 * MMIO helper macros.
 */
#define gic_read32(base, off)		(*(volatile uint32_t *)((uintptr_t)(base) + (off)))
#define gic_write32(base, off, val)	(*(volatile uint32_t *)((uintptr_t)(base) + (off)) = (val))
#define gic_read64(base, off)		(*(volatile uint64_t *)((uintptr_t)(base) + (off)))
#define gic_write64(base, off, val)	(*(volatile uint64_t *)((uintptr_t)(base) + (off)) = (val))

/*
 * Public GIC-400 API exposed to the rest of the kernel.
 */
int gic_irq_enable(unsigned int irq);
int gic_irq_disable(unsigned int irq);
int gic_irq_set_priority(unsigned int irq, uint8_t prio);
int gic_irq_set_type(unsigned int irq, unsigned int type);
int gic_irq_set_group(unsigned int irq, unsigned int group);
int gic_request_irq(unsigned int irq, irq_handler_t handler, void *dev_id);
void gic_handle_irq(void);

#endif /* __GIC_V2_H__ */
