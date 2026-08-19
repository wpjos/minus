#include "sdhci_host.h"
#include "string.h"
#include "printk.h"
#include "generic_timer.h"
#include "irqflags.h"
#include "errno.h"

#define SDHCI_TIMEOUT_US	1000000

#define cpu_relax() __asm__ volatile("yield" ::: "memory")

void sdhci_udelay(uint32_t us)
{
	uint64_t freq = generic_timer_get_cntfrq();
	uint64_t start = generic_timer_get_cntpct();
	uint64_t ticks = ((uint64_t)us * freq) / 1000000;

	if (freq == 0)
		return;

	while ((generic_timer_get_cntpct() - start) < ticks)
		cpu_relax();
}

int sdhci_reset(struct sdhci_host *host, uint8_t mask)
{
	uint8_t val;
	int timeout = 100;

	if (host->ioaddr == NULL)
		return -EINVAL;

	sdhci_writeb(host, mask, SDHCI_SOFTWARE_RESET);

	do {
		sdhci_udelay(10);
		val = sdhci_readb(host, SDHCI_SOFTWARE_RESET);
		if ((val & mask) == 0)
			return 0;
	} while (--timeout > 0);

	printk("sdhci: reset timeout mask=%p val=%p\n",
	       (void *)(unsigned long long)mask,
	       (void *)(unsigned long long)val);
	return -ETIMEDOUT;
}

static uint16_t sdhci_calc_clock(struct sdhci_host *host, uint32_t clock,
				 uint32_t *clock_out)
{
	uint32_t base = host->base_clock;
	uint16_t div;
	uint32_t freq;

	if (clock == 0 || base == 0)
		return 0;

	if (clock >= base) {
		freq = base;
		div = 0;
	} else {
		/* SDHCI divider: base / (2 * div), div=0 means /1 */
		for (div = 1; div < 256; div++) {
			freq = base / (2 * div);
			if (freq <= clock)
				break;
		}
	}

	if (clock_out)
		*clock_out = freq;

	return div << SDHCI_DIVIDER_SHIFT;
}

int sdhci_set_clock(struct sdhci_host *host, uint32_t hz)
{
	uint16_t clk;
	uint32_t actual;
	int timeout;

	if (host->ioaddr == NULL)
		return -EINVAL;

	/* disable SD clock first */
	clk = sdhci_readw(host, SDHCI_CLOCK_CONTROL);
	clk &= ~SDHCI_CLOCK_CARD_EN;
	sdhci_writew(host, clk, SDHCI_CLOCK_CONTROL);

	if (hz == 0) {
		host->clock = 0;
		return 0;
	}

	clk = sdhci_calc_clock(host, hz, &actual);
	if (clk == 0 && hz != 0)
		return -EINVAL;

	clk |= SDHCI_CLOCK_INT_EN;
	sdhci_writew(host, clk, SDHCI_CLOCK_CONTROL);

	timeout = 10000;
	while (!(sdhci_readw(host, SDHCI_CLOCK_CONTROL) & SDHCI_CLOCK_INT_STABLE)) {
		if (--timeout <= 0) {
			printk("sdhci: clock stable timeout\n");
			return -ETIMEDOUT;
		}
		sdhci_udelay(1);
	}

	clk |= SDHCI_CLOCK_CARD_EN;
	sdhci_writew(host, clk, SDHCI_CLOCK_CONTROL);

	host->clock = actual;
	return 0;
}

void sdhci_set_power(struct sdhci_host *host)
{
	uint8_t pwr = SDHCI_POWER_330 | SDHCI_POWER_ON;

	sdhci_writeb(host, 0, SDHCI_POWER_CONTROL);
	sdhci_udelay(10);
	sdhci_writeb(host, pwr, SDHCI_POWER_CONTROL);
	sdhci_udelay(10);
}

int sdhci_wait_irq(struct sdhci_host *host, uint32_t mask, uint32_t timeout_us)
{
	uint32_t intmask;
	uint64_t freq = generic_timer_get_cntfrq();
	uint64_t start = generic_timer_get_cntpct();
	uint64_t ticks = ((uint64_t)timeout_us * freq) / 1000000;

	if (freq == 0)
		return -EINVAL;

	while ((generic_timer_get_cntpct() - start) < ticks) {
		intmask = sdhci_readl(host, SDHCI_INT_STATUS);
		if (intmask & SDHCI_INT_ERROR) {
			printk("sdhci: irq error status=%p\n",
			       (void *)(unsigned long long)intmask);
			sdhci_writel(host, intmask & SDHCI_INT_ERROR_MASK,
				     SDHCI_INT_STATUS);
			return -EIO;
		}
		if (intmask & mask) {
			sdhci_writel(host, intmask & mask, SDHCI_INT_STATUS);
			return 0;
		}
		cpu_relax();
	}

	printk("sdhci: wait_irq timeout mask=%p status=%p\n",
	       (void *)(unsigned long long)mask,
	       (void *)(unsigned long long)sdhci_readl(host, SDHCI_INT_STATUS));
	return -ETIMEDOUT;
}

int sdhci_wait_inhibit(struct sdhci_host *host, uint32_t mask,
		      uint32_t timeout_us)
{
	uint64_t freq = generic_timer_get_cntfrq();
	uint64_t start = generic_timer_get_cntpct();
	uint64_t ticks = ((uint64_t)timeout_us * freq) / 1000000;

	if (freq == 0)
		return -EINVAL;

	while ((generic_timer_get_cntpct() - start) < ticks) {
		if (!(sdhci_readl(host, SDHCI_PRESENT_STATE) & mask))
			return 0;
		cpu_relax();
	}

	return -ETIMEDOUT;
}

int sdhci_send_command(struct sdhci_host *host, uint8_t cmd, uint32_t arg,
		       uint32_t flags, uint32_t *resp)
{
	uint32_t resp_type = flags & SDHCI_CMD_RESP_MASK;
	uint16_t cmd_flags = (uint16_t)(flags & 0xff);
	uint32_t cmd_word;
	int ret;
	int i;

	if (host->ioaddr == NULL)
		return -EINVAL;

	/* wait for command line free */
	if (sdhci_wait_inhibit(host, SDHCI_CMD_INHIBIT, 100000) < 0) {
		printk("sdhci: cmd inhibit timeout cmd=%d\n", (int)cmd);
		return -ETIMEDOUT;
	}

	/* clear command related interrupt status */
	sdhci_writel(host, SDHCI_INT_CMD_MASK, SDHCI_INT_STATUS);

	sdhci_writel(host, arg, SDHCI_ARGUMENT);

	/*
	 * SDHCI command register layout:
	 *   bits [13:8] command index, bits [5:0] response / control flags.
	 * Add CRC and index checks for responses that carry them.
	 */
	if (resp_type == SDHCI_CMD_RESP_SHORT ||
	    resp_type == SDHCI_CMD_RESP_SHORT_BUSY) {
		/* ACMD41 (R3) has no CRC and no index check */
		if (cmd != 41)
			cmd_flags |= SDHCI_CMD_CRC | SDHCI_CMD_INDEX;
	} else if (resp_type == SDHCI_CMD_RESP_LONG) {
		cmd_flags |= SDHCI_CMD_CRC;
	}

	cmd_word = ((uint32_t)(((cmd & 0x3f) << 8) | cmd_flags) << 16) |
		   (host->transfer_mode & 0xffff);
	sdhci_writel(host, cmd_word, SDHCI_TRANSFER_MODE);

	ret = sdhci_wait_irq(host, SDHCI_INT_RESPONSE, SDHCI_TIMEOUT_US);
	if (ret < 0) {
		printk("sdhci: cmd=%d wait response failed ret=%d\n",
		       (int)cmd, ret);
		return ret;
	}

	if (resp) {
		if (resp_type == SDHCI_CMD_RESP_LONG) {
			for (i = 0; i < 4; i++)
				resp[i] = sdhci_readl(host,
					      SDHCI_RESPONSE + (3 - i) * 4);
		} else if (resp_type == SDHCI_CMD_RESP_SHORT ||
			   resp_type == SDHCI_CMD_RESP_SHORT_BUSY) {
			resp[0] = sdhci_readl(host, SDHCI_RESPONSE);
		}
	}

	return 0;
}

int sdhci_transfer_pio(struct sdhci_host *host, void *buf, size_t len,
		       int write)
{
	uint32_t *ptr = (uint32_t *)buf;
	size_t words = len / sizeof(uint32_t);
	size_t i;
	uint32_t intmask;
	uint32_t present_mask;
	uint64_t freq;
	uint64_t ticks;
	uint64_t start;
	uint32_t state;

	if (host->ioaddr == NULL)
		return -EINVAL;

	present_mask = write ? SDHCI_SPACE_AVAILABLE : SDHCI_DATA_AVAILABLE;
	freq = generic_timer_get_cntfrq();
	ticks = ((uint64_t)SDHCI_TIMEOUT_US * freq) / 1000000;

	for (i = 0; i < words; i++) {
		start = generic_timer_get_cntpct();
		while (1) {
			state = sdhci_readl(host, SDHCI_PRESENT_STATE);
			if (state & present_mask)
				break;
			if (freq &&
			    (generic_timer_get_cntpct() - start) >= ticks) {
				printk("sdhci: pio word %d buffer timeout "
				       "present=%p\n",
				       (int)i,
				       (void *)(unsigned long long)state);
				return -ETIMEDOUT;
			}
		}

		if (write)
			sdhci_writel(host, ptr[i], SDHCI_BUFFER);
		else
			ptr[i] = sdhci_readl(host, SDHCI_BUFFER);
	}

	/* wait for data lines to go idle (transfer complete) */
	start = generic_timer_get_cntpct();
	while (1) {
		state = sdhci_readl(host, SDHCI_PRESENT_STATE);
		if (!(state & SDHCI_DAT_INHIBIT))
			break;
		if (freq &&
		    (generic_timer_get_cntpct() - start) >= ticks) {
			printk("sdhci: pio data_end timeout present=%p\n",
			       (void *)(unsigned long long)state);
			return -ETIMEDOUT;
		}
	}

	/* clear any remaining data status */
	intmask = sdhci_readl(host, SDHCI_INT_STATUS);
	sdhci_writel(host, intmask & SDHCI_INT_DATA_MASK, SDHCI_INT_STATUS);

	return 0;
}

int sdhci_host_init(struct sdhci_host *host)
{
	uint32_t caps;
	int ret;

	if (host->ioaddr == NULL)
		return -EINVAL;

	sdhci_writel(host, SDHCI_INT_ALL_MASK, SDHCI_INT_STATUS);
	sdhci_writel(host, 0, SDHCI_INT_ENABLE);
	sdhci_writel(host, 0, SDHCI_SIGNAL_ENABLE);

	ret = sdhci_reset(host, SDHCI_RESET_ALL);
	if (ret < 0)
		return ret;

	sdhci_set_power(host);

	caps = sdhci_readl(host, SDHCI_CAPS);
	host->caps = caps;

	/* default to 1-bit bus, 400 kHz identification clock */
	sdhci_writeb(host, 0, SDHCI_HOST_CONTROL);

	ret = sdhci_set_clock(host, 400000);
	if (ret < 0)
		return ret;

	/*
	 * Enable status bits for everything we poll, but do not signal any
	 * interrupts.  The brcm_sdhci_irq_handler currently clears the whole
	 * status register on entry, which races with our polling of INT_STATUS
	 * and causes CMD/DATA done bits to be missed.
	 */
	sdhci_writel(host, SDHCI_INT_CMD_MASK | SDHCI_INT_DATA_MASK |
		     SDHCI_INT_CARD_INSERT | SDHCI_INT_CARD_REMOVE,
		     SDHCI_INT_ENABLE);
	sdhci_writel(host, 0, SDHCI_SIGNAL_ENABLE);

	return 0;
}
