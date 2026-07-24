#include "gic_v2.h"
#include "gic.h"
#include "platform.h"
#include "module.h"
#include "irq.h"
#include "mmu.h"

/* Cached MMIO bases */
static uint64_t g_gicd_base;
static uint64_t g_gicc_base;

/* Number of SPIs supported by the distributor */
static unsigned int g_nr_spis;

/* Simple IRQ handler table */
#define GIC_MAX_HANDLERS	GIC_NR_IRQS

static irq_handler_t g_handlers[GIC_MAX_HANDLERS];
static void *g_handler_data[GIC_MAX_HANDLERS];

/*
 * Distributor helpers
 */
static inline void gicd_write32(uint32_t off, uint32_t val)
{
	gic_write32(g_gicd_base, off, val);
}

static inline uint32_t gicd_read32(uint32_t off)
{
	return gic_read32(g_gicd_base, off);
}

/*
 * CPU interface helpers
 */
static inline void gicc_write32(uint32_t off, uint32_t val)
{
	gic_write32(g_gicc_base, off, val);
}

static inline uint32_t gicc_read32(uint32_t off)
{
	return gic_read32(g_gicc_base, off);
}

static void gicd_wait_for_rwp(void)
{
	/* GICv2 distributor has no RWP bit; a small delay is enough. */
	for (volatile int i = 0; i < 100; i++)
		;
}

/*
 * Bit manipulation for 32-bit grouped registers (one bit per IRQ)
 */
static inline void gic_irq_bit_op32(uint32_t reg_off, unsigned int irq,
				    int set)
{
	uint32_t off = reg_off + (irq / 32) * 4;
	uint32_t mask = 1U << (irq % 32);
	uint32_t val = gicd_read32(off);

	if (set)
		val |= mask;
	else
		val &= ~mask;
	gicd_write32(off, val);
}

static inline uint32_t gic_irq_bit_read32(uint32_t reg_off, unsigned int irq)
{
	uint32_t off = reg_off + (irq / 32) * 4;

	return gicd_read32(off) & (1U << (irq % 32));
}

/*
 * Priority register access (8-bit per IRQ)
 */
static inline void gic_irq_prio_write(unsigned int irq, uint8_t prio)
{
	uint32_t off = GICD_IPRIORITYR + irq;
	*(volatile uint8_t *)(uintptr_t)(g_gicd_base + off) = prio;
}

/*
 * Configuration register access (2-bit per IRQ)
 */
static inline void gic_irq_cfg_write(unsigned int irq, unsigned int type)
{
	uint32_t off = GICD_ICFGR + (irq / 16) * 4;
	uint32_t shift = (irq % 16) * 2;
	uint32_t val = gicd_read32(off);

	val &= ~(3U << shift);
	val |= ((uint32_t)type & 3U) << shift;
	gicd_write32(off, val);
}

/*
 * Group register access (1-bit per IRQ in IGROUPR)
 *   0 -> Group 0 (Secure)
 *   1 -> Group 1 (Non-secure)
 */
static inline void gic_irq_group_write(unsigned int irq, unsigned int group)
{
	uint32_t off = GICD_IGROUPR + (irq / 32) * 4;
	uint32_t mask = 1U << (irq % 32);
	uint32_t val = gicd_read32(off);

	if (group == GIC_GROUP_G1NS)
		val |= mask;
	else
		val &= ~mask;
	gicd_write32(off, val);
}

/*
 * SPI CPU target: route SPIs to CPU 0
 */
static inline void gic_irq_target_write(unsigned int irq, uint8_t target)
{
	uint32_t off = GICD_ITARGETSR + (irq / 4) * 4;
	uint32_t shift = (irq % 4) * 8;
	uint32_t val = gicd_read32(off);

	val &= ~(0xffU << shift);
	val |= ((uint32_t)target & 0xffU) << shift;
	gicd_write32(off, val);
}

/*
 * Enable/disable helpers
 */
static inline void gicd_enable_irq(unsigned int irq)
{
	gic_irq_bit_op32(GICD_ISENABLER, irq, 1);
}

static inline void gicd_disable_irq(unsigned int irq)
{
	gic_irq_bit_op32(GICD_ICENABLER, irq, 1);
}

/*
 * Initialize the distributor
 */
static void gic_dist_init(void)
{
	uint32_t typer;
	unsigned int i;

	typer = gicd_read32(GICD_TYPER);
	g_nr_spis = ((typer & GICD_TYPER_IT_LINES_NUMBER_MASK) + 1) * 32;
	if (g_nr_spis > GIC_NR_IRQS)
		g_nr_spis = GIC_NR_IRQS;

	/* Disable distributor while configuring */
	gicd_write32(GICD_CTLR, 0);
	gicd_wait_for_rwp();

	/* Configure SPIs: Group 1, default priority, level, route to CPU 0 */
	for (i = GIC_MIN_SPI; i < g_nr_spis; i++) {
		gic_irq_group_write(i, GIC_GROUP_G1NS);
		gic_irq_prio_write(i, GIC_PRIO_DEFAULT);
		gic_irq_cfg_write(i, GIC_IRQ_TYPE_LEVEL);
		gic_irq_target_write(i, 0x01);
		gicd_disable_irq(i);
	}

	/* Configure SGI/PPI: Group 1, default priority */
	for (i = GIC_MIN_SGI; i <= GIC_MAX_PPI; i++) {
		gic_irq_group_write(i, GIC_GROUP_G1NS);
		gic_irq_prio_write(i, GIC_PRIO_DEFAULT);
	}

	/* PPI default type: level */
	for (i = GIC_MIN_PPI; i <= GIC_MAX_PPI; i++)
		gic_irq_cfg_write(i, GIC_IRQ_TYPE_LEVEL);

	/* Disable all PPIs initially */
	for (i = GIC_MIN_PPI; i <= GIC_MAX_PPI; i++)
		gicd_disable_irq(i);

	/* Enable distributor */
	gicd_write32(GICD_CTLR, GICD_CTLR_ENABLE);
	gicd_wait_for_rwp();
}

/*
 * Initialize the CPU interface
 */
static void gic_cpu_init(void)
{
	/* Priority mask: allow all priorities */
	gicc_write32(GICC_PMR, GIC_PRIO_LOWEST);

	/* Binary point */
	gicc_write32(GICC_BPR, 0);

	/* Enable CPU interface (Group 1 in non-secure state) */
	gicc_write32(GICC_CTLR, GICC_CTLR_ENABLE);
}

/*
 * Platform probe
 */
static const struct gic_ops gic_v2_ops;

static int gic_probe(struct platform_device *pdev)
{
	struct resource *res_d, *res_c;

	res_d = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	res_c = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res_d || !res_c) {
		return -1;
	}

	g_gicd_base = (uint64_t)mmu_ioremap(res_d->start, resource_size(res_d));
	g_gicc_base = (uint64_t)mmu_ioremap(res_c->start, resource_size(res_c));
	if (!g_gicd_base || !g_gicc_base)
		return -1;

	gic_dist_init();
	gic_cpu_init();

	gic_register_ops(&gic_v2_ops);

	return 0;
}

static const struct of_device_id gic_of_match[] = {
	{ .compatible = "arm,gic-400" },
	{ /* sentinel */ }
};

static struct platform_driver gic_driver = {
	.drv = { .name = "gic_v2" },
	.probe = gic_probe,
	.remove = NULL,
	.of_match_table = gic_of_match,
};

static void gic_init(void)
{
	platform_driver_register(&gic_driver);
}
module_register(gic, MODULE_LEVEL_HIGH, gic_init);

/*
 * Public API implementation
 */
static int gic_v2_irq_enable(unsigned int irq)
{
	if (irq >= GIC_NR_IRQS)
		return -1;

	gicd_enable_irq(irq);
	return 0;
}

static int gic_v2_irq_disable(unsigned int irq)
{
	if (irq >= GIC_NR_IRQS)
		return -1;

	gicd_disable_irq(irq);
	return 0;
}

static int gic_v2_irq_set_priority(unsigned int irq, uint8_t prio)
{
	if (irq >= GIC_NR_IRQS)
		return -1;

	gic_irq_prio_write(irq, prio);
	return 0;
}

static int gic_v2_irq_set_type(unsigned int irq, unsigned int type)
{
	if (irq >= GIC_NR_IRQS)
		return -1;

	gic_irq_cfg_write(irq, type);
	return 0;
}

static int gic_v2_irq_set_group(unsigned int irq, unsigned int group)
{
	if (irq >= GIC_NR_IRQS)
		return -1;

	gic_irq_group_write(irq, group);
	return 0;
}

/*
 * IRQ handler registration and dispatch
 */
static int gic_v2_request_irq(unsigned int irq, irq_handler_t handler, void *dev_id)
{
	if (irq >= GIC_MAX_HANDLERS)
		return -1;

	g_handlers[irq] = handler;
	g_handler_data[irq] = dev_id;
	return 0;
}

static void gic_v2_handle_irq(void)
{
	unsigned int irq;
	irq_handler_t handler;

	irq = (unsigned int)gicc_read32(GICC_IAR);

	/* Spurious interrupt */
	if (irq >= 1020)
		return;

	handler = g_handlers[irq];
	if (handler)
		handler(irq, g_handler_data[irq]);

	gicc_write32(GICC_EOIR, irq);
}

static const struct gic_ops gic_v2_ops = {
	.irq_enable = gic_v2_irq_enable,
	.irq_disable = gic_v2_irq_disable,
	.irq_set_priority = gic_v2_irq_set_priority,
	.irq_set_type = gic_v2_irq_set_type,
	.irq_set_group = gic_v2_irq_set_group,
	.request_irq = gic_v2_request_irq,
	.handle_irq = gic_v2_handle_irq,
};
