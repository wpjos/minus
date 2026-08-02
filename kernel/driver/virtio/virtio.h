#ifndef __VIRTIO_H__
#define __VIRTIO_H__

#include "types.h"
#include "dlist.h"
#include "platform.h"

/* Legacy virtio-mmio register offsets */
#define VIRTIO_MMIO_MAGIC_VALUE		0x000
#define VIRTIO_MMIO_VERSION		0x004
#define VIRTIO_MMIO_DEVICE_ID		0x008
#define VIRTIO_MMIO_VENDOR_ID		0x00c
#define VIRTIO_MMIO_HOST_FEATURES	0x010
#define VIRTIO_MMIO_HOST_FEATURES_SEL	0x014
#define VIRTIO_MMIO_GUEST_FEATURES	0x020
#define VIRTIO_MMIO_GUEST_FEATURES_SEL	0x024
#define VIRTIO_MMIO_GUEST_PAGE_SIZE	0x028
#define VIRTIO_MMIO_QUEUE_SEL		0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX	0x034
#define VIRTIO_MMIO_QUEUE_NUM		0x038
#define VIRTIO_MMIO_QUEUE_ALIGN		0x03c
#define VIRTIO_MMIO_QUEUE_PFN		0x040
#define VIRTIO_MMIO_QUEUE_NOTIFY	0x050
#define VIRTIO_MMIO_INTERRUPT_STATUS	0x060
#define VIRTIO_MMIO_INTERRUPT_ACK	0x064
#define VIRTIO_MMIO_STATUS		0x070

#define VIRTIO_MMIO_CONFIG_BASE		0x100

#define VIRTIO_MAGIC			0x74726976 /* 'virt' */
#define VIRTIO_VERSION_LEGACY		1

/* Status bits */
#define VIRTIO_CONFIG_S_ACKNOWLEDGE	1
#define VIRTIO_CONFIG_S_DRIVER		2
#define VIRTIO_CONFIG_S_DRIVER_OK	4
#define VIRTIO_CONFIG_S_FEATURES_OK	8
#define VIRTIO_CONFIG_S_FAILED		128

/* Modern virtio extension, but QEMU's legacy-mmio blk device needs it */
#define VIRTIO_CONFIG_S_STARTED		64

/* Interrupt status bits */
#define VIRTIO_MMIO_INT_VRING		(1 << 0)
#define VIRTIO_MMIO_INT_CONFIG		(1 << 1)

/* Descriptor flags */
#define VRING_DESC_F_NEXT		1
#define VRING_DESC_F_WRITE		2
#define VRING_DESC_F_INDIRECT		4

/* Virtqueue sizes */
#define VIRTIO_BLK_QUEUE_SIZE		16
#define VRING_DESC_SIZE			16

/* Virtio-blk device ID and features */
#define VIRTIO_ID_BLOCK			2

#define VIRTIO_BLK_F_SIZE_MAX		1
#define VIRTIO_BLK_F_SEG_MAX		2
#define VIRTIO_BLK_F_GEOMETRY		4
#define VIRTIO_BLK_F_RO			5
#define VIRTIO_BLK_F_BLK_SIZE		6
#define VIRTIO_BLK_F_FLUSH		9
#define VIRTIO_BLK_F_TOPOLOGY		10
#define VIRTIO_BLK_F_CONFIG_WCE		11

#define VIRTIO_F_ACCESS_PLATFORM	(1U << 28)

#define VIRTIO_BLK_T_IN			0
#define VIRTIO_BLK_T_OUT		1
#define VIRTIO_BLK_T_FLUSH		4

#define VIRTIO_BLK_S_OK			0
#define VIRTIO_BLK_S_IOERR		1
#define VIRTIO_BLK_S_UNSUPP		2

struct vring_desc {
	uint64_t addr;
	uint32_t len;
	uint16_t flags;
	uint16_t next;
};

struct vring_avail {
	uint16_t flags;
	uint16_t idx;
	uint16_t ring[];
};

struct vring_used_elem {
	uint32_t id;
	uint32_t len;
};

struct vring_used {
	uint16_t flags;
	uint16_t idx;
	struct vring_used_elem ring[];
};

struct virtqueue {
	uint32_t num;
	uint32_t free_desc;
	uint16_t last_used_idx;
	struct vring_desc *desc;
	struct vring_avail *avail;
	struct vring_used *used;
	void *data[VIRTIO_BLK_QUEUE_SIZE];
};

struct virtio_device {
	uintptr_t base;
	uint32_t device_id;
	uint32_t irq;
	struct virtqueue vq;
	struct platform_device *pdev;
};

struct virtio_blk_outhdr {
	uint32_t type;
	uint32_t ioprio;
	uint64_t sector;
};

struct virtio_blk_config {
	uint64_t capacity;
	uint32_t size_max;
	uint32_t seg_max;
	struct {
		uint16_t cylinders;
		uint8_t heads;
		uint8_t sectors;
	} geometry;
	uint32_t blk_size;
};

/* Legacy MMIO helpers */
static inline uint32_t virtio_readl(uintptr_t base, uint32_t off)
{
	return *(volatile uint32_t *)(base + off);
}

static inline void virtio_writel(uintptr_t base, uint32_t off, uint32_t val)
{
	*(volatile uint32_t *)(base + off) = val;
}

/* Transport API */
int virtio_mmio_probe(struct platform_device *pdev);
int virtio_mmio_setup_queue(struct virtio_device *vdev, uint32_t qid,
			    uint32_t num);
void virtio_mmio_notify(struct virtio_device *vdev, uint32_t qid);
int virtio_add_buf(struct virtio_device *vdev, struct vring_desc *descs,
		   uint32_t ndescs, void *cookie);
void *virtio_get_buf(struct virtio_device *vdev, uint32_t *len);

#if 0
/* Cache maintenance helpers for DMA buffers */
void dcache_clean_range(uintptr_t start, size_t size);
void dcache_invalidate_range(uintptr_t start, size_t size);
void dcache_clean_invalidate_range(uintptr_t start, size_t size);
#endif

#endif /* __VIRTIO_H__ */
