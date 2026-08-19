#include "sdhci_host.h"
#include "platform.h"
#include "of.h"
#include "fdt.h"
#include "module.h"
#include "mmu.h"
#include "irq.h"
#include "mm.h"
#include "printk.h"
#include "string.h"
#include "errno.h"

/* Broadcom SDIO CFG registers (second reg region) */
#define SDIO_CFG_CTRL			0x00
#define SDIO_CFG_CTRL_SDCD_N_TEST_EN	(1U << 31)
#define SDIO_CFG_CTRL_SDCD_N_TEST_LEV	(1U << 30)

#define SDIO_CFG_CQ_CAPABILITY		0x4c
#define SDIO_CFG_CQ_CAPABILITY_FMUL	(3U << 12)

#define SDIO_CFG_MAX_50MHZ_MODE		0x1ac
#define SDIO_CFG_MAX_50MHZ_MODE_ENABLE		(1U << 0)
#define SDIO_CFG_MAX_50MHZ_MODE_STRAP_OVERRIDE	(1U << 31)

/*
 * BCM2712-specific SDIO CFG initialization, based on Linux sdhci-brcmstb.
 * Programs the delay-line PHY clock source and command-queue timer frequency.
 */
static void brcm_sdhci_cfginit_2712(struct sdhci_host *host)
{
	uint32_t reg;
	uint32_t base_clk_mhz;
	volatile uint32_t *cfg;

	if (!host->cfg_regs)
		return;

	cfg = (volatile uint32_t *)host->cfg_regs;

	/* Select delay-line PHY as clock source (needed for UHS/tuning modes). */
	reg = cfg[SDIO_CFG_MAX_50MHZ_MODE / sizeof(uint32_t)];
	reg &= ~SDIO_CFG_MAX_50MHZ_MODE_ENABLE;
	reg |= SDIO_CFG_MAX_50MHZ_MODE_STRAP_OVERRIDE;
	cfg[SDIO_CFG_MAX_50MHZ_MODE / sizeof(uint32_t)] = reg;

	/*
	 * Set command-queue timer frequency. The 13:12 field selects a
	 * multiplier and the lower bits hold the base clock in MHz.
	 */
	base_clk_mhz = host->base_clock / 1000000;
	if (base_clk_mhz == 0)
		base_clk_mhz = 100;
	reg = SDIO_CFG_CQ_CAPABILITY_FMUL | (base_clk_mhz & 0xfff);
	cfg[SDIO_CFG_CQ_CAPABILITY / sizeof(uint32_t)] = reg;
}

static int brcm_sdhci_irq_handler(unsigned int irq, void *dev_id)
{
	struct sdhci_host *host = (struct sdhci_host *)dev_id;
	uint32_t intmask;

	(void)irq;

	if (!host || !host->ioaddr)
		return 0;

	intmask = sdhci_readl(host, SDHCI_INT_STATUS);
	if (intmask)
		sdhci_writel(host, intmask, SDHCI_INT_STATUS);

	return 0;
}

static void brcm_sdhci_force_card_present(struct sdhci_host *host)
{
	uint32_t cfg_ctrl;

	if (!host->cfg_regs)
		return;

	cfg_ctrl = *(volatile uint32_t *)host->cfg_regs;
	cfg_ctrl &= ~SDIO_CFG_CTRL_SDCD_N_TEST_LEV;
	cfg_ctrl |= SDIO_CFG_CTRL_SDCD_N_TEST_EN;
	*(volatile uint32_t *)host->cfg_regs = cfg_ctrl;
}

/*
 * A non-removable SDHCI host with subnodes is an SDIO bus (e.g. the onboard
 * WiFi/BT controller).  We do not support SDIO yet, so skip SD card
 * initialization instead of printing scary CMD8 failure messages.
 */
static int brcm_sdhci_is_sdio_controller(int nodeoffset)
{
	const void *fdt = fdt_base();

	if (!fdt_getprop(fdt, nodeoffset, "non-removable", NULL))
		return 0;

	if (fdt_first_subnode(fdt, nodeoffset) < 0)
		return 0;

	return 1;
}

static int brcm_sdhci_probe(struct platform_device *pdev)
{
	struct resource *mem, *cfg, *irq_res;
	struct sdhci_host *host;
	uint32_t base_clock;
	int irq;
	int ret;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	cfg = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	irq_res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!mem || !irq_res) {
		printk("brcm_sdhci: missing reg/irq resource\n");
		return -EINVAL;
	}

	host = (struct sdhci_host *)kzalloc(sizeof(*host));
	if (!host)
		return -ENOMEM;

	host->pdev = pdev;
	pdev->dev.driver_data = host;
	host->ioaddr = mmu_ioremap(mem->start, resource_size(mem));
	if (!host->ioaddr) {
		printk("brcm_sdhci: ioremap failed\n");
		kfree(host);
		return -ENOMEM;
	}

	if (cfg) {
		host->cfg_regs = mmu_ioremap(cfg->start, resource_size(cfg));
		if (!host->cfg_regs) {
			printk("brcm_sdhci: cfg ioremap failed\n");
			kfree(host);
			return -ENOMEM;
		}
	}

	irq = (int)irq_res->start;
	host->irq = (uint32_t)irq;

	base_clock = of_get_clock_frequency(fdt_base(), pdev->dev.of_node);
	if (base_clock == 0) {
		printk("brcm_sdhci: no clock, fallback to 100 MHz\n");
		base_clock = 100000000;
	}
	host->base_clock = base_clock;

	if (request_irq((unsigned int)irq, brcm_sdhci_irq_handler, host) != 0) {
		printk("brcm_sdhci: failed to request irq %d\n", irq);
		kfree(host);
		return -EINVAL;
	}

	if (enable_irq((unsigned int)irq) != 0) {
		printk("brcm_sdhci: failed to enable irq %d\n", irq);
		kfree(host);
		return -EINVAL;
	}

	printk("brcm_sdhci: probe io=%p irq=%d base_clock=%d node=%p\n",
	       host->ioaddr, host->irq, host->base_clock,
	       (uint64_t)pdev->dev.of_node);

	brcm_sdhci_cfginit_2712(host);

	ret = sdhci_host_init(host);
	if (ret < 0) {
		printk("brcm_sdhci: host init failed\n");
		kfree(host);
		return ret;
	}

	{
		uint32_t state = sdhci_readl(host, SDHCI_PRESENT_STATE);
		printk("brcm_sdhci: present_state=%p card_present=%d\n",
		       (void *)(unsigned long long)state,
		       (int)(state & SDHCI_CARD_PRESENT) ? 1 : 0);
	}

	if (brcm_sdhci_is_sdio_controller(pdev->dev.of_node)) {
		printk("brcm_sdhci: SDIO controller, skip SD card init\n");
		return 0;
	}

	/*
	 * If the controller does not see a card, try the Broadcom CFG override
	 * used for slots without a working card-detect line.
	 */
	if (!(sdhci_readl(host, SDHCI_PRESENT_STATE) & SDHCI_CARD_PRESENT)) {
		printk("brcm_sdhci: forcing card present via cfg\n");
		brcm_sdhci_force_card_present(host);
	}

	ret = sd_card_init(host);
	if (ret < 0) {
		printk("brcm_sdhci: no card or card init failed\n");
		kfree(host);
		return ret;
	}

	mmcblk_probe(host);

	printk("brcm_sdhci: registered mmcblk%d\n",
	       (int)MINOR(host->bdev.bd_dev));
	return 0;
}

static int brcm_sdhci_remove(struct platform_device *pdev)
{
	struct sdhci_host *host = (struct sdhci_host *)pdev->dev.driver_data;

	if (host) {
		sdhci_writel(host, 0, SDHCI_INT_ENABLE);
		sdhci_writel(host, 0, SDHCI_SIGNAL_ENABLE);
		kfree(host);
	}
	return 0;
}

static const struct of_device_id brcm_sdhci_match[] = {
	{ .compatible = "brcm,bcm2712-sdhci" },
	{ /* sentinel */ }
};

static struct platform_driver brcm_sdhci_driver = {
	.drv = { .name = "brcm,bcm2712-sdhci" },
	.probe = brcm_sdhci_probe,
	.remove = brcm_sdhci_remove,
	.of_match_table = brcm_sdhci_match,
};

static void brcm_sdhci_init(void)
{
	platform_driver_register(&brcm_sdhci_driver);
}
module_register(brcm_sdhci, MODULE_LEVEL_LOW, brcm_sdhci_init);
