#ifndef __VIRTIO_MMIO_H__
#define __VIRTIO_MMIO_H__

#include "virtio.h"

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

/* Interrupt status bits */
#define VIRTIO_MMIO_INT_VRING		(1 << 0)
#define VIRTIO_MMIO_INT_CONFIG		(1 << 1)

#define VRING_DESC_SIZE			16

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
void virtio_mmio_set_status(struct virtio_device *vdev, uint32_t status);
void virtio_mmio_notify(struct virtio_device *vdev, uint32_t qid);
int virtio_add_buf(struct virtio_device *vdev, struct vring_desc *descs,
		   uint32_t ndescs, void *cookie);
void *virtio_get_buf(struct virtio_device *vdev, uint32_t *len);

#endif /* __VIRTIO_MMIO_H__ */
