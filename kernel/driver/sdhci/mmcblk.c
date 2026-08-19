#include "sdhci_host.h"
#include "module.h"
#include "printk.h"
#include "mm.h"
#include "string.h"
#include "errno.h"

struct mbr_partition {
	uint8_t	 status;
	uint8_t	 chs_start[3];
	uint8_t	 type;
	uint8_t	 chs_end[3];
	uint32_t start_lba;
	uint32_t nr_sectors;
} __attribute__((packed));

struct mbr {
	uint8_t		  bootstrap[446];
	struct mbr_partition  parts[4];
	uint16_t	  signature;
} __attribute__((packed));

static inline uint16_t le16(const uint8_t *p)
{
	return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static inline uint32_t le32(const uint8_t *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
	       ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int mmcblk_request(struct block_device *bdev, uint64_t lba,
			  size_t nr_blocks, void *buf, int dir)
{
	struct sdhci_host *host = (struct sdhci_host *)bdev->bd_private;
	uint64_t sector_lba, abs_lba, nr_sectors;
	int ret;

	if (!host)
		return -EINVAL;

	/*
	 * The VFS block layer passes lba/nr_blocks in units of bd_block_size.
	 * The SDHCI layer and partition bounds are in 512-byte sectors, so
	 * convert before adding the partition start offset.
	 */
	sector_lba = lba * (bdev->bd_block_size / 512);
	nr_sectors = nr_blocks * (bdev->bd_block_size / 512);

	/* Bound-check partition-relative accesses. */
	if (bdev->bd_nr_blocks) {
		if (sector_lba + nr_sectors < sector_lba)
			return -EINVAL;
		if (sector_lba + nr_sectors > bdev->bd_nr_blocks)
			return -EINVAL;
	}

	abs_lba = bdev->bd_start_lba + sector_lba;

	if (dir == REQ_OP_READ)
		ret = sd_read_blocks(host, abs_lba, nr_sectors, buf);
	else
		ret = sd_write_blocks(host, abs_lba, nr_sectors, buf);

	return ret;
}

static int mmcblk_open(struct block_device *bdev)
{
	struct sdhci_host *host = (struct sdhci_host *)bdev->bd_private;

	if (!host || host->ioaddr == NULL)
		return -EINVAL;
	return 0;
}

static void mmcblk_release(struct block_device *bdev)
{
	(void)bdev;
}

static struct block_device_operations mmcblk_ops = {
	.open = mmcblk_open,
	.release = mmcblk_release,
	.request = mmcblk_request,
};

static int mmcblk_registered;
static int mmcblk_next_devno;

static void mmcblk_scan_partitions(struct sdhci_host *host, int host_idx)
{
	struct mbr mbr;
	int i, ret;

	ret = bdev_read_blocks(&host->bdev, 0, 1, &mbr);
	if (ret != 0) {
		printk("mmcblk: failed to read MBR\n");
		return;
	}

	if (le16((const uint8_t *)&mbr.signature) != 0xAA55) {
		printk("mmcblk: no MBR signature\n");
		return;
	}

	for (i = 0; i < 4; i++) {
		struct mbr_partition *p = &mbr.parts[i];
		uint8_t type = p->type;
		uint32_t start = le32((const uint8_t *)&p->start_lba);
		uint32_t size = le32((const uint8_t *)&p->nr_sectors);
		struct block_device *part;

		if (type == 0 || size == 0)
			continue;

		part = (struct block_device *)kmalloc(sizeof(*part));
		if (!part) {
			printk("mmcblk: failed to alloc partition %d\n", i + 1);
			continue;
		}

		memset(part, 0, sizeof(*part));
		part->bd_dev = MKDEV(MMCBLK_MAJOR,
				     MMC_PART_MINOR(host_idx, i + 1));
		part->bd_block_size = 512;
		part->bd_block_size_bits = 9;
		part->bd_private = host;
		part->bd_start_lba = start;
		part->bd_nr_blocks = size;

		if (add_block_device(part) != 0) {
			printk("mmcblk: failed to add partition %d\n", i + 1);
			kfree(part);
			continue;
		}

		printk("mmcblk: registered mmcblk%dp%d start=%p size=%p type=%p\n",
		       host_idx, i + 1,
		       (void *)(unsigned long long)start,
		       (void *)(unsigned long long)size,
		       (void *)(unsigned long long)type);
	}
}

void mmcblk_probe(struct sdhci_host *host)
{
	int host_idx;

	if (!mmcblk_registered) {
		if (register_blkdev(MMCBLK_MAJOR, "mmcblk", &mmcblk_ops) != 0)
			return;
		mmcblk_registered = 1;
	}

	host_idx = mmcblk_next_devno++;
	host->bdev.bd_dev = MKDEV(MMCBLK_MAJOR, MMC_DEV_MINOR(host_idx));
	host->bdev.bd_block_size = 512;
	host->bdev.bd_block_size_bits = 9;
	host->bdev.bd_private = host;
	host->bdev.bd_start_lba = 0;
	host->bdev.bd_nr_blocks = host->capacity;

	if (add_block_device(&host->bdev) != 0) {
		printk("mmcblk: failed to add block device\n");
		return;
	}

	printk("mmcblk: registered mmcblk%d\n", host_idx);

	mmcblk_scan_partitions(host, host_idx);
}
