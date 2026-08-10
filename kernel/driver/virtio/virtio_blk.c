#include "virtio.h"
#include "blkdev.h"
#include "mm.h"
#include "string.h"
#include "bitops.h"
#include "memory.h"
#include "module.h"

struct virtio_blk {
	struct virtio_device *vdev;
	struct block_device bdev;
	uint64_t capacity;
	uint32_t block_size;
};

struct virtio_blk_req {
	struct virtio_blk_outhdr hdr;
	uint8_t status;
	struct virtio_blk *vblk;
	int done;
};

static int virtio_blk_request(struct block_device *bdev, uint64_t lba,
			      size_t nr_blocks, void *buf, int dir)
{
	struct virtio_blk *vblk = (struct virtio_blk *)bdev->bd_private;
	struct virtio_device *vdev = vblk->vdev;
	struct virtio_blk_req *req;
	struct vring_desc desc[3];
	uint32_t len;
	int ret;
	uint64_t sector;

	if (!vdev)
		return -1;

	req = (struct virtio_blk_req *)kmalloc(sizeof(*req));
	if (!req)
		return -1;

	sector = lba * (bdev->bd_block_size / 512);

	req->hdr.type = (dir == REQ_OP_WRITE) ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
	req->hdr.ioprio = 0;
	req->hdr.sector = sector;
	req->status = 0xff;
	req->vblk = vblk;
	req->done = 0;

	desc[0].addr = __VA_PA__((uintptr_t)&req->hdr);
	desc[0].len = sizeof(req->hdr);
	desc[0].flags = 0;
	desc[0].next = 1;

	desc[1].addr = __VA_PA__((uintptr_t)buf);
	desc[1].len = (uint32_t)(nr_blocks * bdev->bd_block_size);
	desc[1].flags = (dir == REQ_OP_READ) ? VRING_DESC_F_WRITE : 0;
	desc[1].next = 2;

	desc[2].addr = __VA_PA__((uintptr_t)&req->status);
	desc[2].len = 1;
	desc[2].flags = VRING_DESC_F_WRITE;
	desc[2].next = 0;

	ret = virtio_add_buf(vdev, desc, 3, req);
	if (ret < 0) {
		kfree(req);
		return -1;
	}

	virtio_mmio_notify(vdev, 0);

	/* Poll for completion. */
	while (!req->done) {
		void *cookie = virtio_get_buf(vdev, &len);
		if (cookie == req)
			req->done = 1;
	}

	ret = (req->status == VIRTIO_BLK_S_OK) ? 0 : -1;
	kfree(req);
	return ret;
}

static struct block_device_operations virtio_blk_ops = {
	.request = virtio_blk_request,
};

static const struct virtio_device_id virtio_blk_id_table[] = {
	{ .device_id = VIRTIO_ID_BLOCK },
	{ /* sentinel */ }
};

static int virtio_blk_probe(struct virtio_device *vdev)
{
	struct virtio_blk *vblk;
	struct virtio_blk_config *cfg;
	int ret;

	ret = virtio_mmio_setup_queue(vdev, 0, VIRTIO_BLK_QUEUE_SIZE);
	if (ret < 0)
		return -1;

	virtio_mmio_set_status(vdev, VIRTIO_CONFIG_S_DRIVER_OK);
	virtio_mmio_set_status(vdev, VIRTIO_CONFIG_S_STARTED);

	cfg = (struct virtio_blk_config *)(vdev->base + VIRTIO_MMIO_CONFIG_BASE);

	vblk = (struct virtio_blk *)kmalloc(sizeof(*vblk));
	if (!vblk)
		return -1;
	memset(vblk, 0, sizeof(*vblk));

	vblk->vdev = vdev;
	vblk->capacity = (uint64_t)cfg->capacity;
	vblk->block_size = 512;

	vblk->bdev.bd_dev = MKDEV(VIRTBLK_MAJOR, 0);
	vblk->bdev.bd_block_size = 512;
	vblk->bdev.bd_block_size_bits = 9;
	vblk->bdev.bd_private = vblk;

	if (register_blkdev(VIRTBLK_MAJOR, "virtblk", &virtio_blk_ops) != 0) {
		kfree(vblk);
		return -1;
	}

	if (add_block_device(&vblk->bdev) != 0) {
		unregister_blkdev(VIRTBLK_MAJOR);
		kfree(vblk);
		return -1;
	}

	return 0;
}

static struct virtio_driver virtio_blk_driver = {
	.id_table = virtio_blk_id_table,
	.probe    = virtio_blk_probe,
};

static void virtio_blk_init(void)
{
	virtio_driver_register(&virtio_blk_driver);
}

module_register(virtio_blk, MODULE_LEVEL_LOW, virtio_blk_init);
