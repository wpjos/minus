#include "gic_v3.h"
#include "platform.h"
#include "module.h"
#include "irq.h"
#include "mmu.h"

/* Cached MMIO bases */
static uint64_t g_gicd_base;
static uint64_t g_gicr_base;
static uint64_t g_gicr_size;

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

static inline void gicd_write64(uint32_t off, uint64_t val)
{
	gic_write64(g_gicd_base, off, val);
}

static void gicd_wait_for_rwp(void)
{
	while (gicd_read32(GICD_CTLR) & GICD_CTLR_RWP)
		;
}

/*
 * Redistributor helpers (base frame)
 */
static inline void gicr_write32(uint64_t base, uint32_t off, uint32_t val)
{
	gic_write32(base, off, val);
}

static inline uint32_t gicr_read32(uint64_t base, uint32_t off)
{
	return gic_read32(base, off);
}

static inline uint64_t gicr_read64(uint64_t base, uint32_t off)
{
	return gic_read64(base, off);
}

/*
 * SGI/PPI frame helpers
 */
static inline uint64_t gicr_sgi_base(void)
{
	return g_gicr_base + GICR_SGI_BASE_OFFSET;
}

static inline void gicr_sgi_write32(uint32_t off, uint32_t val)
{
	gic_write32(gicr_sgi_base(), off, val);
}

static inline uint32_t gicr_sgi_read32(uint32_t off)
{
	return gic_read32(gicr_sgi_base(), off);
}

/*
 * Bit manipulation for 32-bit grouped registers (one bit per IRQ)
 */
static inline void gic_irq_bit_op32(uint64_t base, uint32_t reg_off,
				    unsigned int irq, int set)
{
	uint32_t off = reg_off + (irq / 32) * 4;
	uint32_t mask = 1U << (irq % 32);
	uint32_t val = gic_read32(base, off);

	if (set)
		val |= mask;
	else
		val &= ~mask;
	gic_write32(base, off, val);
}

static inline uint32_t gic_irq_bit_read32(uint64_t base, uint32_t reg_off,
					  unsigned int irq)
{
	uint32_t off = reg_off + (irq / 32) * 4;

	return gic_read32(base, off) & (1U << (irq % 32));
}

/*
 * Priority register access (8-bit per IRQ)
 */
static inline void gic_irq_prio_write(unsigned int irq, uint8_t prio)
{
	uint64_t base;
	uint32_t off;

	if (irq <= GIC_MAX_PPI) {
		base = gicr_sgi_base();
		off = GICR_IPRIORITYR + irq;
	} else {
		base = g_gicd_base;
		off = GICD_IPRIORITYR + irq;
	}
	*(volatile uint8_t *)(uintptr_t)(base + off) = prio;
}

/*
 * Configuration register access (2-bit per IRQ)
 */
static inline void gic_irq_cfg_write(unsigned int irq, unsigned int type)
{
	uint64_t base;
	uint32_t off;
	uint32_t shift;
	uint32_t val;

	if (irq <= GIC_MAX_PPI) {
		base = gicr_sgi_base();
		off = GICR_ICFGR0 + ((irq - GIC_MIN_SGI) / 16) * 4;
	} else {
		base = g_gicd_base;
		off = GICD_ICFGR + ((irq - GIC_MIN_SPI) / 16) * 4;
	}

	shift = ((irq % 16) * 2);
	val = gic_read32(base, off);
	val &= ~(3U << shift);
	val |= ((uint32_t)type & 3U) << shift;
	gic_write32(base, off, val);
}

/*
 * Group register access (1-bit per IRQ in IGROUPR, plus IGRPMODR for GICv3)
 */
static inline void gic_irq_group_write(unsigned int irq, unsigned int group)
{
	uint64_t base;
	uint32_t off_group, off_mod;
	uint32_t bit;

	if (irq <= GIC_MAX_PPI) {
		base = gicr_sgi_base();
		off_group = GICR_IGROUPR0;
		off_mod = GICR_IGRPMODR0;
	} else {
		base = g_gicd_base;
		off_group = GICD_IGROUPR + (irq / 32) * 4;
		off_mod = GICD_IGRPMODR + (irq / 32) * 4;
	}

	bit = 1U << (irq % 32);

	/*
	 * Encoding:
	 *   IGROUPR=0, IGRPMODR=0  -> Group 0 (Secure)
	 *   IGROUPR=1, IGRPMODR=0  -> Group 1 Non-secure
	 *   IGROUPR=0, IGRPMODR=1  -> Group 1 Secure
	 */
	if (group == GIC_GROUP_G1NS) {
		gic_write32(base, off_group, gic_read32(base, off_group) | bit);
		gic_write32(base, off_mod, gic_read32(base, off_mod) & ~bit);
	} else if (group == GIC_GROUP_G1S) {
		gic_write32(base, off_group, gic_read32(base, off_group) & ~bit);
		gic_write32(base, off_mod, gic_read32(base, off_mod) | bit);
	} else {
		gic_write32(base, off_group, gic_read32(base, off_group) & ~bit);
		gic_write32(base, off_mod, gic_read32(base, off_mod) & ~bit);
	}
}

/*
 * Enable/disable helpers
 */
static inline void gicd_enable_irq(unsigned int irq)
{
	gic_irq_bit_op32(g_gicd_base, GICD_ISENABLER, irq, 1);
}

static inline void gicd_disable_irq(unsigned int irq)
{
	gic_irq_bit_op32(g_gicd_base, GICD_ICENABLER, irq, 1);
}

static inline void gicr_enable_irq(unsigned int irq)
{
	gic_irq_bit_op32(gicr_sgi_base(), GICR_ISENABLER0, irq, 1);
}

static inline void gicr_disable_irq(unsigned int irq)
{
	gic_irq_bit_op32(gicr_sgi_base(), GICR_ICENABLER0, irq, 1);
}

/*
 * Wake up the redistributor
 */
static void gic_redist_wake(void)
{
	uint32_t waker;

	waker = gicr_read32(g_gicr_base, GICR_WAKER);
	waker &= ~GICR_WAKER_ProcessorSleep;
	gicr_write32(g_gicr_base, GICR_WAKER, waker);

	/* Wait until ChildrenAsleep is cleared */
	while (gicr_read32(g_gicr_base, GICR_WAKER) & GICR_WAKER_ChildrenAsleep)
		;
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

	/* Configure SPIs: Group 1, default priority, level */
	for (i = GIC_MIN_SPI; i < g_nr_spis; i++) {
		gic_irq_group_write(i, GIC_GROUP_G1NS);
		gic_irq_prio_write(i, GIC_PRIO_DEFAULT);
		gic_irq_cfg_write(i, GIC_IRQ_TYPE_LEVEL);
		gicd_disable_irq(i);
	}

	/* Legacy mode: enable Group 0 + Group 1 */
	gicd_write32(GICD_CTLR, GICD_CTLR_ENABLE_G1_NS | GICD_CTLR_ENABLE_G0);
	gicd_wait_for_rwp();
}

/*
 * Initialize the redistributor (SGI/PPI frame)
 */
static void gic_redist_init(void)
{
	unsigned int i;

	/* Configure SGI/PPI: Group 1 NS, default priority, level */
	for (i = GIC_MIN_SGI; i <= GIC_MAX_PPI; i++) {
		gic_irq_group_write(i, GIC_GROUP_G1NS);
		gic_irq_prio_write(i, GIC_PRIO_DEFAULT);
	}

	/* PPI default type: level */
	for (i = GIC_MIN_PPI; i <= GIC_MAX_PPI; i++)
		gic_irq_cfg_write(i, GIC_IRQ_TYPE_LEVEL);

	/* Disable all PPIs initially */
	for (i = GIC_MIN_PPI; i <= GIC_MAX_PPI; i++)
		gicr_disable_irq(i);
}

/*
 * Initialize the CPU interface
 */
static void gic_cpu_init(void)
{
	uint64_t sre;

	/* Enable system register access at EL1 */
	sre = gic_get_icc_sre_el1();
	sre |= ICC_SRE_SRE;
	gic_set_icc_sre_el1(sre);

	/* Set priority mask and binary point */
	gic_set_icc_pmr_el1(GIC_PRIO_LOWEST);
	gic_set_icc_bpr1_el1(0);

	/* Enable Group 1 interrupts */
	gic_set_icc_igrpen1_el1(ICC_IGRPEN1_ENABLE);
}

/*
 * Platform probe
 */
static int gic_probe(struct platform_device *pdev)
{
	struct resource *res_d, *res_r;

	res_d = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	res_r = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res_d || !res_r) {
		return -1;
	}

	if (!early_mmu_ioremap(res_d->start, resource_size(res_d)) ||
	    !early_mmu_ioremap(res_r->start, resource_size(res_r))) {
		return -1;
	}

	g_gicd_base = res_d->start;
	g_gicr_base = res_r->start;
	g_gicr_size = resource_size(res_r);

	gic_redist_wake();
	gic_dist_init();
	gic_redist_init();
	gic_cpu_init();

	return 0;
}

static const struct of_device_id gic_of_match[] = {
	{ .compatible = "arm,gic-v3" },
	{ /* sentinel */ }
};

static struct platform_driver gic_driver = {
	.drv = { .name = "gic_v3" },
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
int gic_irq_enable(unsigned int irq)
{
	if (irq >= GIC_NR_IRQS)
		return -1;

	if (irq <= GIC_MAX_PPI)
		gicr_enable_irq(irq);
	else
		gicd_enable_irq(irq);
	return 0;
}

int gic_irq_disable(unsigned int irq)
{
	if (irq >= GIC_NR_IRQS)
		return -1;

	if (irq <= GIC_MAX_PPI)
		gicr_disable_irq(irq);
	else
		gicd_disable_irq(irq);
	return 0;
}

int gic_irq_set_priority(unsigned int irq, uint8_t prio)
{
	if (irq >= GIC_NR_IRQS)
		return -1;

	gic_irq_prio_write(irq, prio);
	return 0;
}

int gic_irq_set_type(unsigned int irq, unsigned int type)
{
	if (irq >= GIC_NR_IRQS)
		return -1;

	gic_irq_cfg_write(irq, type);
	return 0;
}

int gic_irq_set_group(unsigned int irq, unsigned int group)
{
	if (irq >= GIC_NR_IRQS)
		return -1;

	gic_irq_group_write(irq, group);
	return 0;
}

/*
 * IRQ handler registration and dispatch
 */
int gic_request_irq(unsigned int irq, irq_handler_t handler, void *dev_id)
{
	if (irq >= GIC_MAX_HANDLERS)
		return -1;

	g_handlers[irq] = handler;
	g_handler_data[irq] = dev_id;
	return 0;
}

void gic_handle_irq(void)
{
	unsigned int irq;
	irq_handler_t handler;

	irq = (unsigned int)gic_get_icc_iar1_el1();

	/* Spurious interrupt */
	if (irq >= 1020)
		return;

	handler = g_handlers[irq];
	if (handler)
		handler(irq, g_handler_data[irq]);

	gic_set_icc_eoir1_el1(irq);
}
