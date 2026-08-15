#include "sdhci_host.h"
#include "string.h"
#include "printk.h"
#include "bitops.h"

/* SD command indices */
#define CMD0	0
#define CMD2	2
#define CMD3	3
#define CMD6	6
#define CMD7	7
#define CMD8	8
#define CMD9	9
#define CMD12	12
#define CMD16	16
#define CMD17	17
#define CMD18	18
#define CMD24	24
#define CMD25	25
#define CMD55	55

#define ACMD6	6
#define ACMD41	41

/* CMD8 argument: VHS = 2.7-3.6 V, check pattern 0xaa */
#define CMD8_ARG	0x000001aa

/* ACMD41 argument: HCS + voltage window 2.7-3.6 V */
#define ACMD41_ARG	0x40ff8000

#define SD_OCR_CCS	(1U << 30)
#define SD_OCR_BUSY	(1U << 31)

static inline int sdhci_cmd(struct sdhci_host *host, uint8_t cmd,
			    uint32_t arg, uint32_t flags, uint32_t *resp)
{
	return sdhci_send_command(host, cmd, arg, flags, resp);
}

static int sd_app_cmd(struct sdhci_host *host, uint32_t rca)
{
	uint32_t resp;
	int ret;

	ret = sdhci_cmd(host, CMD55, rca << 16, SDHCI_CMD_RESP_SHORT, &resp);
	if (ret < 0)
		return ret;

	/* app_cmd bit (bit 5) should be set */
	if ((resp & 0x20) == 0)
		return -1;

	return 0;
}

static int sd_cmd8(struct sdhci_host *host)
{
	uint32_t resp;
	int ret;

	ret = sdhci_cmd(host, CMD8, CMD8_ARG, SDHCI_CMD_RESP_SHORT, &resp);
	if (ret < 0)
		return ret;

	if ((resp & 0xff) != (CMD8_ARG & 0xff))
		return -1;

	return 0;
}

static int sd_acmd41(struct sdhci_host *host)
{
	uint32_t resp;
	int ret;
	int timeout = 1000;

	while (timeout-- > 0) {
		ret = sd_app_cmd(host, 0);
		if (ret < 0)
			return ret;

		ret = sdhci_cmd(host, ACMD41, ACMD41_ARG, SDHCI_CMD_RESP_SHORT,
				&resp);
		if (ret < 0)
			return ret;

		if (resp & SD_OCR_BUSY) {
			host->ocr = resp;
			return 0;
		}

		sdhci_udelay(1000);
	}

	return -1;
}

static int sd_cmd2(struct sdhci_host *host, uint32_t *cid)
{
	return sdhci_cmd(host, CMD2, 0, SDHCI_CMD_RESP_LONG, cid);
}

static int sd_cmd3(struct sdhci_host *host, uint32_t *rca)
{
	uint32_t resp;
	int ret;

	ret = sdhci_cmd(host, CMD3, 0, SDHCI_CMD_RESP_SHORT, &resp);
	if (ret < 0)
		return ret;

	*rca = (resp >> 16) & 0xffff;
	return 0;
}

static int sd_cmd9(struct sdhci_host *host, uint32_t rca, uint32_t *csd)
{
	return sdhci_cmd(host, CMD9, rca << 16, SDHCI_CMD_RESP_LONG, csd);
}

static int sd_select_card(struct sdhci_host *host, uint32_t rca)
{
	return sdhci_cmd(host, CMD7, rca << 16, SDHCI_CMD_RESP_SHORT_BUSY,
			 NULL);
}

static int sd_set_bus_width(struct sdhci_host *host, uint32_t rca, int width)
{
	uint32_t arg;
	int ret;

	ret = sd_app_cmd(host, rca);
	if (ret < 0)
		return ret;

	arg = (width == 4) ? 2 : 0;
	return sdhci_cmd(host, ACMD6, arg, SDHCI_CMD_RESP_SHORT, NULL);
}

static int sd_set_blocklen(struct sdhci_host *host, uint32_t len)
{
	return sdhci_cmd(host, CMD16, len, SDHCI_CMD_RESP_SHORT, NULL);
}

static uint64_t sd_parse_csd_v2_capacity(uint32_t *csd)
{
	uint32_t c_size;
	uint64_t sectors;

	/*
	 * CSD v2.0: C_SIZE is bits [69:48] of the CSD register, i.e. 22 bits
	 * starting at bit 48.  SDHCI stores the 128-bit CSD response as:
	 *   csd[0] = RESPONSE[3] = bits [127:104]
	 *   csd[1] = RESPONSE[2] = bits [103:72]
	 *   csd[2] = RESPONSE[1] = bits [71:40]
	 *   csd[3] = RESPONSE[0] = bits [39:8]
	 * Therefore C_SIZE is in csd[2], bits [29:8].
	 */
	c_size = (csd[2] >> 8) & 0x3fffff;
	sectors = ((uint64_t)(c_size + 1)) * 1024;

	return sectors;
}

static int sd_switch_hs(struct sdhci_host *host)
{
	uint32_t resp;
	uint32_t arg;
	uint32_t switch_status[16];
	int ret;
	int i;

	/* CMD6 transfers one 64-byte block */
	sdhci_writew(host, 64, SDHCI_BLOCK_SIZE);
	sdhci_writew(host, 1, SDHCI_BLOCK_COUNT);
	host->transfer_mode = SDHCI_TRNS_BLK_CNT_EN | SDHCI_TRNS_READ;

	/* CMD6: switch function, mode=check, function=high-speed */
	arg = 0x00fffff1;
	ret = sdhci_send_command(host, CMD6, arg,
				 SDHCI_CMD_RESP_SHORT | SDHCI_CMD_DATA,
				 &resp);
	if (ret < 0)
		goto out;

	if (sdhci_transfer_pio(host, switch_status, sizeof(switch_status), 0) < 0) {
		ret = -1;
		goto out;
	}

	/* Check if high-speed function is supported */
	if (((switch_status[4] >> 24) & 0xf) != 0x1) {
		ret = -1;
		goto out;
	}

	/* Mode=switch, function=high-speed */
	arg = 0x80fffff1;
	ret = sdhci_send_command(host, CMD6, arg,
				 SDHCI_CMD_RESP_SHORT | SDHCI_CMD_DATA,
				 &resp);
	if (ret < 0)
		goto out;

	ret = sdhci_transfer_pio(host, switch_status, sizeof(switch_status), 0);
	if (ret < 0)
		goto out;

	/* verify function switched */
	if (((switch_status[4] >> 24) & 0xf) != 0x1) {
		ret = -1;
		goto out;
	}

	for (i = 0; i < 4; i++) {
		if (((switch_status[3] >> (i * 4)) & 0xf) != 0x1) {
			ret = -1;
			goto out;
		}
	}

out:
	host->transfer_mode = 0;
	return ret;
}

int sd_card_init(struct sdhci_host *host)
{
	uint32_t cid[4];
	uint32_t csd[4];
	uint32_t rca;
	int ret;
	int ccs;
	uint32_t state;
	int i;

	/* check card present */
	state = sdhci_readl(host, SDHCI_PRESENT_STATE);
	if (!(state & SDHCI_CARD_PRESENT)) {
		printk("sd: no card present\n");
		return -1;
	}

	/* wait for card stable */
	for (i = 0; i < 100; i++) {
		if (sdhci_readl(host, SDHCI_PRESENT_STATE) & SDHCI_CARD_STATE_STABLE)
			break;
		sdhci_udelay(1000);
	}

	/* CMD0: go idle */
	ret = sdhci_cmd(host, CMD0, 0, SDHCI_CMD_RESP_NONE, NULL);
	if (ret < 0)
		return ret;

	sdhci_udelay(2000);

	/* CMD8: check voltage / SDHC support */
	ret = sd_cmd8(host);
	if (ret < 0) {
		printk("sd: CMD8 failed, not an SDHC/SDXC card\n");
		return ret;
	}

	/* ACMD41: get OCR, wait for card ready */
	ret = sd_acmd41(host);
	if (ret < 0) {
		printk("sd: ACMD41 failed\n");
		return ret;
	}

	ccs = (host->ocr & SD_OCR_CCS) ? 1 : 0;
	host->high_capacity = ccs;

	/* CMD2: get CID */
	ret = sd_cmd2(host, cid);
	if (ret < 0) {
		printk("sd: CMD2 failed\n");
		return ret;
	}

	/* CMD3: get RCA */
	ret = sd_cmd3(host, &rca);
	if (ret < 0) {
		printk("sd: CMD3 failed\n");
		return ret;
	}

	host->rca = rca;

	/* CMD9: get CSD */
	ret = sd_cmd9(host, rca, csd);
	if (ret < 0) {
		printk("sd: CMD9 failed\n");
		return ret;
	}

	host->capacity = sd_parse_csd_v2_capacity(csd);

	/* CMD7: select card */
	ret = sd_select_card(host, rca);
	if (ret < 0) {
		printk("sd: CMD7 failed\n");
		return ret;
	}

	/* set block length to 512 */
	ret = sd_set_blocklen(host, 512);
	if (ret < 0) {
		printk("sd: CMD16 failed\n");
		return ret;
	}

	/* try 4-bit bus */
	ret = sd_set_bus_width(host, rca, 4);
	if (ret == 0) {
		uint8_t ctrl;

		ctrl = sdhci_readb(host, SDHCI_HOST_CONTROL);
		ctrl |= SDHCI_CTRL_4BITBUS;
		sdhci_writeb(host, ctrl, SDHCI_HOST_CONTROL);
		host->bus_width = 4;
	} else {
		host->bus_width = 1;
	}

	/* try high speed */
	ret = sd_switch_hs(host);
	if (ret == 0) {
		uint8_t ctrl;

		if (sdhci_set_clock(host, 50000000) < 0)
			sdhci_set_clock(host, 25000000);

		ctrl = sdhci_readb(host, SDHCI_HOST_CONTROL);
		ctrl |= SDHCI_CTRL_HISPD;
		sdhci_writeb(host, ctrl, SDHCI_HOST_CONTROL);
	} else {
		sdhci_set_clock(host, 25000000);
	}

	printk("sd: RCA=%p capacity=%d MiB bus=%d-bit clock=%d Hz\n",
	       (void *)(unsigned long long)rca,
	       (int)(host->capacity / 2048),
	       (int)host->bus_width,
	       (int)host->clock);

	return 0;
}

static int sd_send_data_cmd(struct sdhci_host *host, uint8_t cmd,
			    uint32_t arg, void *buf, size_t nr_blocks,
			    int write)
{
	uint16_t mode;
	uint32_t resp;
	int ret;

	if (nr_blocks == 0)
		return -1;

	/* wait for data line free */
	if (sdhci_wait_inhibit(host, SDHCI_DAT_INHIBIT, 100000) < 0)
		return -1;

	mode = SDHCI_TRNS_BLK_CNT_EN;
	if (nr_blocks > 1)
		mode |= SDHCI_TRNS_MULTI;
	if (!write)
		mode |= SDHCI_TRNS_READ;

	sdhci_writew(host, 512, SDHCI_BLOCK_SIZE);
	sdhci_writew(host, (uint16_t)nr_blocks, SDHCI_BLOCK_COUNT);

	host->transfer_mode = mode;
	ret = sdhci_send_command(host, cmd, arg,
				 SDHCI_CMD_RESP_SHORT | SDHCI_CMD_DATA,
				 &resp);
	host->transfer_mode = 0;
	if (ret < 0) {
		printk("sd: data_cmd cmd=%d send failed ret=%d\n",
		       (int)cmd, ret);
		return ret;
	}

	ret = sdhci_transfer_pio(host, buf, nr_blocks * 512, write);
	if (ret < 0) {
		printk("sd: data_cmd cmd=%d pio failed ret=%d\n",
		       (int)cmd, ret);
		return ret;
	}

	if (nr_blocks > 1) {
		/* CMD12: stop transmission */
		ret = sdhci_cmd(host, CMD12, 0,
				SDHCI_CMD_RESP_SHORT_BUSY, NULL);
		if (ret < 0) {
			printk("sd: data_cmd cmd=%d stop failed ret=%d\n",
			       (int)cmd, ret);
			return ret;
		}
	}

	return 0;
}

int sd_read_blocks(struct sdhci_host *host, uint64_t lba, size_t nr_blocks,
		   void *buf)
{
	uint8_t cmd;
	uint32_t arg;

	if (host->ioaddr == NULL || buf == NULL)
		return -1;

	cmd = (nr_blocks > 1) ? CMD18 : CMD17;
	arg = host->high_capacity ? (uint32_t)lba : (uint32_t)(lba * 512);

	return sd_send_data_cmd(host, cmd, arg, buf, nr_blocks, 0);
}

int sd_write_blocks(struct sdhci_host *host, uint64_t lba, size_t nr_blocks,
		    const void *buf)
{
	uint8_t cmd;
	uint32_t arg;

	if (host->ioaddr == NULL || buf == NULL)
		return -1;

	cmd = (nr_blocks > 1) ? CMD25 : CMD24;
	arg = host->high_capacity ? (uint32_t)lba : (uint32_t)(lba * 512);

	return sd_send_data_cmd(host, cmd, arg, (void *)buf, nr_blocks, 1);
}
