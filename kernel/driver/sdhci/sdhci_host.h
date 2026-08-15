#ifndef __SDHCI_DRV_H__
#define __SDHCI_DRV_H__

#include "types.h"
#include "sdhci.h"
#include "blkdev.h"
#include "platform.h"

struct sdhci_host {
	void *ioaddr;
	void *cfg_regs;
	uint32_t irq;
	uint32_t base_clock;
	uint32_t clock;
	uint8_t bus_width;
	uint32_t caps;

	/* card state */
	uint32_t rca;
	uint64_t capacity;	/* number of 512-byte sectors */
	uint32_t ocr;
	uint8_t	 high_capacity;	/* CCS bit from OCR: 1=SDHC/SDXC block addressing */

	/* pending transfer mode for the next data command */
	uint32_t transfer_mode;

	struct platform_device *pdev;
	struct block_device bdev;
};

/* core SDHCI helpers */
/*
 * BCM2712 SDHCI sits on a bus that only supports 32-bit transactions.
 * All byte/halfword accesses are done as 32-bit read-modify-write cycles
 * against the naturally-aligned register word.
 */
static inline uint32_t sdhci_readl_unaligned(struct sdhci_host *host, int reg)
{
	return *(volatile uint32_t *)((uintptr_t)host->ioaddr + (reg & ~3));
}

static inline void sdhci_writel_unaligned(struct sdhci_host *host, uint32_t val,
					  int reg)
{
	*(volatile uint32_t *)((uintptr_t)host->ioaddr + (reg & ~3)) = val;
}

static inline uint8_t sdhci_readb(struct sdhci_host *host, int reg)
{
	uint32_t shift = (reg & 3) * 8;
	return (uint8_t)(sdhci_readl_unaligned(host, reg) >> shift);
}

static inline uint16_t sdhci_readw(struct sdhci_host *host, int reg)
{
	uint32_t shift = (reg & 2) * 8;
	return (uint16_t)(sdhci_readl_unaligned(host, reg) >> shift);
}

static inline uint32_t sdhci_readl(struct sdhci_host *host, int reg)
{
	return *(volatile uint32_t *)((uintptr_t)host->ioaddr + reg);
}

static inline void sdhci_writeb(struct sdhci_host *host, uint8_t val, int reg)
{
	uint32_t shift = (reg & 3) * 8;
	uint32_t mask = 0xffU << shift;
	uint32_t tmp;

	tmp = sdhci_readl_unaligned(host, reg);
	tmp = (tmp & ~mask) | (((uint32_t)val << shift) & mask);
	sdhci_writel_unaligned(host, tmp, reg);
}

static inline void sdhci_writew(struct sdhci_host *host, uint16_t val, int reg)
{
	uint32_t shift = (reg & 2) * 8;
	uint32_t mask = 0xffffU << shift;
	uint32_t tmp;

	tmp = sdhci_readl_unaligned(host, reg);
	tmp = (tmp & ~mask) | (((uint32_t)val << shift) & mask);
	sdhci_writel_unaligned(host, tmp, reg);
}

static inline void sdhci_writel(struct sdhci_host *host, uint32_t val, int reg)
{
	*(volatile uint32_t *)((uintptr_t)host->ioaddr + reg) = val;
}

int sdhci_reset(struct sdhci_host *host, uint8_t mask);
int sdhci_set_clock(struct sdhci_host *host, uint32_t hz);
void sdhci_set_power(struct sdhci_host *host);
int sdhci_wait_irq(struct sdhci_host *host, uint32_t mask, uint32_t timeout_us);
int sdhci_wait_inhibit(struct sdhci_host *host, uint32_t mask,
		       uint32_t timeout_us);
int sdhci_send_command(struct sdhci_host *host, uint8_t cmd, uint32_t arg,
		       uint32_t flags, uint32_t *resp);
int sdhci_transfer_pio(struct sdhci_host *host, void *buf, size_t len,
		       int write);
void sdhci_udelay(uint32_t us);

/* host init exposed by core */
int sdhci_host_init(struct sdhci_host *host);

/* SD protocol layer */
int sd_card_init(struct sdhci_host *host);
int sd_read_blocks(struct sdhci_host *host, uint64_t lba, size_t nr_blocks,
		   void *buf);
int sd_write_blocks(struct sdhci_host *host, uint64_t lba, size_t nr_blocks,
		    const void *buf);

/* block device registration */
void mmcblk_probe(struct sdhci_host *host);

#endif /* __SDHCI_DRV_H__ */
