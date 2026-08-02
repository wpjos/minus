#include "blkdev.h"
#include "string.h"
#include "printk.h"
#include "mm.h"
#include "module.h"

#define MAX_BLKDEVS	16

static struct block_device *g_blkdevs[MAX_BLKDEVS];
static struct dlist_node g_bdev_list;

static int blkdev_major_to_index(unsigned int major)
{
	if (major == 0 || major >= MAX_BLKDEVS)
		return -1;
	return (int)major;
}

int register_blkdev(unsigned int major, const char *name,
		    struct block_device_operations *ops)
{
	int idx;

	(void)name;

	idx = blkdev_major_to_index(major);
	if (idx < 0)
		return -1;
	if (g_blkdevs[idx])
		return -1;

	g_blkdevs[idx] = (struct block_device *)ops;
	dlist_init(&g_bdev_list);
	return 0;
}

int unregister_blkdev(unsigned int major)
{
	int idx = blkdev_major_to_index(major);

	if (idx < 0)
		return -1;
	g_blkdevs[idx] = NULL;
	return 0;
}

/*
 * Register a concrete block device instance. The major number must already
 * have been registered via register_blkdev().
 */
int add_block_device(struct block_device *bdev)
{
	int idx;
	struct block_device_operations *ops;

	if (!bdev)
		return -1;

	idx = blkdev_major_to_index(MAJOR(bdev->bd_dev));
	if (idx < 0)
		return -1;

	ops = (struct block_device_operations *)g_blkdevs[idx];
	if (!ops)
		return -1;

	bdev->bd_ops = ops;
	bdev->bd_ref_count = 1;
	dlist_add(&g_bdev_list, &bdev->bd_list);

	if (ops->open) {
		if (ops->open(bdev) != 0)
			return -1;
	}

	return 0;
}

struct block_device *bdev_get_by_dev(dev_t dev)
{
	struct block_device *bdev;

	dlist_for_each_entry(bdev, &g_bdev_list, bd_list) {
		if (bdev->bd_dev == dev) {
			bdev->bd_ref_count++;
			return bdev;
		}
	}

	/* Not found in list: try to instantiate from a registered major. */
	int idx = blkdev_major_to_index(MAJOR(dev));
	struct block_device_operations *ops;

	if (idx < 0)
		return NULL;
	ops = (struct block_device_operations *)g_blkdevs[idx];
	if (!ops || !ops->open)
		return NULL;

	bdev = (struct block_device *)kmalloc(sizeof(*bdev));
	if (!bdev)
		return NULL;
	memset(bdev, 0, sizeof(*bdev));
	bdev->bd_dev = dev;
	bdev->bd_block_size = 512;
	bdev->bd_block_size_bits = 9;
	bdev->bd_ops = ops;
	bdev->bd_ref_count = 1;

	if (ops->open(bdev) != 0) {
		kfree(bdev);
		return NULL;
	}

	dlist_add(&g_bdev_list, &bdev->bd_list);
	return bdev;
}

void bdev_put(struct block_device *bdev)
{
	if (!bdev)
		return;

	bdev->bd_ref_count--;
	if (bdev->bd_ref_count <= 0) {
		if (bdev->bd_ops && bdev->bd_ops->release)
			bdev->bd_ops->release(bdev);
		dlist_del(&bdev->bd_list);
		kfree(bdev);
	}
}

int bdev_read_blocks(struct block_device *bdev, uint64_t lba,
		     size_t nr_blocks, void *buf)
{
	if (!bdev || !bdev->bd_ops || !bdev->bd_ops->request)
		return -1;
	return bdev->bd_ops->request(bdev, lba, nr_blocks, buf, REQ_OP_READ);
}

int bdev_write_blocks(struct block_device *bdev, uint64_t lba,
		      size_t nr_blocks, void *buf)
{
	if (!bdev || !bdev->bd_ops || !bdev->bd_ops->request)
		return -1;
	return bdev->bd_ops->request(bdev, lba, nr_blocks, buf, REQ_OP_WRITE);
}

void blkdev_init(void)
{
	dlist_init(&g_bdev_list);
}
module_register(blkdev, MODULE_LEVEL_CORE, blkdev_init);
