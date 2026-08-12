#ifndef __VIRTIO_H__
#define __VIRTIO_H__

#include "types.h"
#include "dlist.h"
#include "platform.h"

/* Status bits */
#define VIRTIO_CONFIG_S_ACKNOWLEDGE	1
#define VIRTIO_CONFIG_S_DRIVER		2
#define VIRTIO_CONFIG_S_DRIVER_OK	4
#define VIRTIO_CONFIG_S_FEATURES_OK	8
#define VIRTIO_CONFIG_S_FAILED		128

/* Modern virtio extension, but QEMU's legacy-mmio blk device needs it */
#define VIRTIO_CONFIG_S_STARTED		64

/* Descriptor flags */
#define VRING_DESC_F_NEXT		1
#define VRING_DESC_F_WRITE		2
#define VRING_DESC_F_INDIRECT		4

#define VIRTQUEUE_MAX_SIZE	64

/* Virtio device IDs */
#define VIRTIO_ID_BLOCK		2
#define VIRTIO_ID_GPU		16

/* General feature bits */
#define VIRTIO_F_ACCESS_PLATFORM	(1U << 28)

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
	void *data[VIRTQUEUE_MAX_SIZE];
};

struct virtio_device {
	struct device dev;
	uintptr_t base;
	uint32_t device_id;
	uint32_t irq;
	struct virtqueue vq;
	struct platform_device *pdev;
};

/*
 * Virtio device ID match table entry.
 */
struct virtio_device_id {
	uint32_t device_id;
	const void *data;
};

/*
 * Virtio driver - binds to devices by device_id.
 */
struct virtio_driver {
	struct driver drv;
	const struct virtio_device_id *id_table;
	int (*probe)(struct virtio_device *vdev);
	int (*remove)(struct virtio_device *vdev);
};

#define virtio_driver_of(d) container_of(d, struct virtio_driver, drv)
#define virtio_device_of(d) container_of(d, struct virtio_device, dev)

/* Bus API */
int virtio_driver_register(struct virtio_driver *drv);
int virtio_device_register(struct virtio_device *vdev);

#endif /* __VIRTIO_H__ */
