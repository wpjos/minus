#ifndef __BLKDEV_H__
#define __BLKDEV_H__

#include "types.h"
#include "dlist.h"

#define REQ_OP_READ	0
#define REQ_OP_WRITE	1

struct block_device;

struct block_device_operations {
	int  (*open)(struct block_device *bdev);
	void (*release)(struct block_device *bdev);
	int  (*request)(struct block_device *bdev, uint64_t lba,
			size_t nr_blocks, void *buf, int dir);
};

struct block_device {
	dev_t			bd_dev;
	unsigned int		bd_block_size;
	unsigned char		bd_block_size_bits;
	struct block_device_operations *bd_ops;
	void			*bd_private;
	struct dlist_node	bd_list;
	int			bd_ref_count;
	uint64_t		bd_start_lba;	/* partition start LBA on parent device */
	uint64_t		bd_nr_blocks;	/* partition size in blocks (0 = whole device) */
};

static inline unsigned int block_size(struct block_device *bdev)
{
	return bdev ? bdev->bd_block_size : 0;
}

/* Device number helpers */
#define MAJOR(dev)	((unsigned int)((dev) >> 20))
#define MINOR(dev)	((unsigned int)((dev) & 0xfffff))
#define MKDEV(maj, min)	(((dev_t)(maj) << 20) | (dev_t)(min))

/* Legacy major numbers used by Minus */
#define VIRTBLK_MAJOR	8
#define MMCBLK_MAJOR	179

/* mmcblk device/partition minor layout: mmcblkN @ N*8, mmcblkNpP @ N*8+P */
#define MMC_PARTS_PER_DEV	8
#define MMC_DEV_MINOR(devno)	((devno) * MMC_PARTS_PER_DEV)
#define MMC_PART_MINOR(devno, partno)	(MMC_DEV_MINOR(devno) + (partno))

void blkdev_init(void);

int register_blkdev(unsigned int major, const char *name,
		    struct block_device_operations *ops);
int unregister_blkdev(unsigned int major);
int add_block_device(struct block_device *bdev);

struct block_device *bdev_get_by_dev(dev_t dev);
struct block_device *bdev_get_by_name(const char *dev_name);
void bdev_put(struct block_device *bdev);

int bdev_read_blocks(struct block_device *bdev, uint64_t lba,
		     size_t nr_blocks, void *buf);
int bdev_write_blocks(struct block_device *bdev, uint64_t lba,
		      size_t nr_blocks, void *buf);

#endif /* __BLKDEV_H__ */
