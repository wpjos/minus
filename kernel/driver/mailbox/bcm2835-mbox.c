#include "platform.h"
#include "of.h"
#include "fdt.h"
#include "mmu.h"
#include "mm.h"
#include "module.h"
#include "printk.h"
#include "string.h"
#include "irqflags.h"
#include "memory.h"

/* BCM2835-style mailbox register offsets */
#define MAILBOX_READ		0x00
#define MAILBOX_POLL		0x10
#define MAILBOX_SENDER		0x14
#define MAILBOX_STATUS		0x18
#define MAILBOX_CONFIG		0x1c
#define MAILBOX_WRITE		0x20

#define MAILBOX_STATUS_EMPTY	(1U << 30)
#define MAILBOX_STATUS_FULL	(1U << 31)

/* Mailbox channels */
#define MAILBOX_CHANNEL_POWER	0
#define MAILBOX_CHANNEL_FB		1
#define MAILBOX_CHANNEL_VUART	2
#define MAILBOX_CHANNEL_VCHIQ	3
#define MAILBOX_CHANNEL_LED		4
#define MAILBOX_CHANNEL_PROP	8 /* VideoCore property channel */

#define MBOX_POLL_LOOPS		1000000

static void *g_mbox_base;

static inline void dmb(void)
{
	__asm__ volatile("dmb sy" ::: "memory");
}

static inline uint32_t mbox_readl(uint32_t reg)
{
	return *(volatile uint32_t *)((uintptr_t)g_mbox_base + reg);
}

static inline void mbox_writel(uint32_t reg, uint32_t val)
{
	*(volatile uint32_t *)((uintptr_t)g_mbox_base + reg) = val;
}

static int mbox_write(uint32_t data)
{
	int i;

	for (i = 0; i < MBOX_POLL_LOOPS; i++) {
		if (!(mbox_readl(MAILBOX_STATUS) & MAILBOX_STATUS_FULL))
			break;
	}
	if (i == MBOX_POLL_LOOPS) {
		printk("bcm2835-mbox: write timeout\n");
		return -1;
	}

	mbox_writel(MAILBOX_WRITE, data);
	return 0;
}

static int mbox_read(uint32_t *data)
{
	int i;
	uint32_t val;

	for (i = 0; i < MBOX_POLL_LOOPS; i++) {
		if (!(mbox_readl(MAILBOX_STATUS) & MAILBOX_STATUS_EMPTY))
			break;
	}
	if (i == MBOX_POLL_LOOPS) {
		printk("bcm2835-mbox: read timeout\n");
		return -1;
	}

	val = mbox_readl(MAILBOX_READ);
	if ((val & 0xf) != MAILBOX_CHANNEL_PROP) {
		printk("bcm2835-mbox: unexpected channel %u\n", val & 0xf);
		return -1;
	}

	*data = val & ~0xf;
	return 0;
}

/*
 * Send a property message to the VideoCore firmware and wait for the response.
 * @buf must be 16-byte aligned and must live in memory visible to the GPU.
 * On BCM2712 the GPU bus address is the same as the ARM physical address, so
 * we pass __VA_PA__(buf) directly.
 */
int rpi_firmware_property(void *buf, size_t size)
{
	uint64_t bus_addr;
	unsigned long flags;
	uint32_t resp_addr;
	int ret;

	if (!g_mbox_base || !buf)
		return -1;

	/*
	 * Ensure the property buffer is written back to memory before the GPU
	 * reads it.  A full system dmb is used because the system currently has
	 * no cache-maintenance helpers for normal allocations.
	 */
	dmb();

	bus_addr = __VA_PA__((uintptr_t)buf);
	if (bus_addr & 0xf) {
		printk("bcm2835-mbox: buffer not 16-byte aligned\n");
		return -1;
	}

	local_irq_save(flags);

	ret = mbox_write((uint32_t)(bus_addr | MAILBOX_CHANNEL_PROP));
	if (ret < 0)
		goto out;

	/*
	 * The firmware writes the same bus address back through the mailbox once
	 * it has filled in the response.
	 */
	ret = mbox_read(&resp_addr);
	if (ret < 0)
		goto out;

	if (resp_addr != (uint32_t)bus_addr) {
		printk("bcm2835-mbox: response address mismatch %p vs %p\n",
		       (void *)(unsigned long long)resp_addr,
		       (void *)(unsigned long long)bus_addr);
		ret = -1;
		goto out;
	}

	ret = 0;

out:
	local_irq_restore(flags);
	dmb();
	return ret;
}

static int bcm2835_mbox_probe(struct platform_device *pdev)
{
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		printk("bcm2835-mbox: missing memory resource\n");
		return -1;
	}

	g_mbox_base = mmu_ioremap(res->start, resource_size(res));
	if (!g_mbox_base) {
		printk("bcm2835-mbox: ioremap failed\n");
		return -1;
	}

	printk("bcm2835-mbox: probed at %p\n", g_mbox_base);
	return 0;
}

static int bcm2835_mbox_remove(struct platform_device *pdev)
{
	(void)pdev;
	g_mbox_base = NULL;
	return 0;
}

static const struct of_device_id bcm2835_mbox_match[] = {
	{ .compatible = "brcm,bcm2835-mbox" },
	{ /* sentinel */ }
};

static struct platform_driver bcm2835_mbox_driver = {
	.drv = { .name = "bcm2835-mbox" },
	.probe = bcm2835_mbox_probe,
	.remove = bcm2835_mbox_remove,
	.of_match_table = bcm2835_mbox_match,
};

static void bcm2835_mbox_init(void)
{
	platform_driver_register(&bcm2835_mbox_driver);
}
module_register(bcm2835_mbox, MODULE_LEVEL_LOW, bcm2835_mbox_init);
