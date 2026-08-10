#include "virtio.h"
#include "mm.h"
#include "string.h"
#include "mmu.h"
#include "irq.h"
#include "platform.h"
#include "module.h"
#include "memory.h"
#include "printk.h"

static uint32_t vring_size(uint32_t num, uint32_t align)
{
	uint32_t desc_size = num * sizeof(struct vring_desc);
	uint32_t avail_size = sizeof(struct vring_avail) + num * sizeof(uint16_t);
	uint32_t used_size = sizeof(struct vring_used) +
			     num * sizeof(struct vring_used_elem);
	uint32_t used_offset;

	used_offset = (desc_size + avail_size + align - 1) & ~(align - 1);
	return used_offset + used_size;
}

int virtio_mmio_setup_queue(struct virtio_device *vdev, uint32_t qid,
			    uint32_t num)
{
	struct virtqueue *vq = &vdev->vq;
	uint32_t align = 4096;
	uint32_t size = vring_size(num, align);
	void *mem;
	struct vring_desc *desc;
	struct vring_avail *avail;
	struct vring_used *used;
	uintptr_t phys;
	uint32_t num_max;

	mem = kzalloc_pages(size);
	if (!mem)
		return -1;
	memset(mem, 0, size);

	desc = mem;
	/*
	 * Legacy virtio vring layout: descriptor table first, then the available
	 * ring immediately after it; the used ring is aligned to QueueAlign.
	 */
	avail = (struct vring_avail *)((uintptr_t)desc +
				sizeof(struct vring_desc) * num);
	used = (struct vring_used *)(ALIGN_UP((uintptr_t)avail +
			      sizeof(struct vring_avail) +
			      num * sizeof(uint16_t), align));

	vq->num = num;
	vq->free_desc = 0;
	vq->last_used_idx = 0;
	vq->desc = desc;
	vq->avail = avail;
	vq->used = used;

	for (uint32_t i = 0; i < num; i++) {
		vq->desc[i].next = (i + 1) < num ? i + 1 : 0;
		vq->data[i] = NULL;
	}

	virtio_writel(vdev->base, VIRTIO_MMIO_QUEUE_SEL, qid);
	num_max = virtio_readl(vdev->base, VIRTIO_MMIO_QUEUE_NUM_MAX);
	if (num_max && num_max < num) {
		kfree_pages(mem);
		return -1;
	}
	virtio_writel(vdev->base, VIRTIO_MMIO_QUEUE_NUM, num);
	virtio_writel(vdev->base, VIRTIO_MMIO_QUEUE_ALIGN, align);

	phys = __VA_PA__((uintptr_t)mem);
	virtio_writel(vdev->base, VIRTIO_MMIO_QUEUE_PFN, (uint32_t)(phys >> 12));

	return 0;
}

void virtio_mmio_notify(struct virtio_device *vdev, uint32_t qid)
{
	virtio_writel(vdev->base, VIRTIO_MMIO_QUEUE_NOTIFY, qid);
}

int virtio_add_buf(struct virtio_device *vdev, struct vring_desc *descs,
		   uint32_t ndescs, void *cookie)
{
	struct virtqueue *vq = &vdev->vq;
	uint16_t head;
	uint16_t prev = 0;
	uint16_t idx;

	if (ndescs == 0)
		return -1;

	head = (uint16_t)vq->free_desc;
	if (vq->data[head])
		return -1;

	for (uint32_t i = 0; i < ndescs; i++) {
		idx = (uint16_t)vq->free_desc;
		vq->desc[idx] = descs[i];
		vq->desc[idx].flags = descs[i].flags;
		if (i > 0) {
			vq->desc[prev].flags |= VRING_DESC_F_NEXT;
			vq->desc[prev].next = idx;
		}
		prev = idx;
		vq->free_desc = vq->desc[idx].next;
	}
	vq->desc[prev].flags &= ~VRING_DESC_F_NEXT;
	vq->data[head] = cookie;

	uint16_t avail_idx = vq->avail->idx % vq->num;
	vq->avail->ring[avail_idx] = head;
	__asm__ volatile("dmb sy");
	vq->avail->idx++;

	return 0;
}

void *virtio_get_buf(struct virtio_device *vdev, uint32_t *len)
{
	struct virtqueue *vq = &vdev->vq;
	struct vring_used_elem *elem;
	uint16_t used_idx;
	uint16_t desc_id;
	void *cookie;

	if (vq->last_used_idx == vq->used->idx)
		return NULL;

	__asm__ volatile("dmb sy");
	used_idx = vq->last_used_idx % vq->num;
	elem = &vq->used->ring[used_idx];
	desc_id = (uint16_t)elem->id;
	cookie = vq->data[desc_id];
	vq->data[desc_id] = NULL;

	/* Return free descriptors to the free list (simple append). */
	vq->desc[desc_id].next = (uint16_t)vq->free_desc;
	vq->free_desc = desc_id;

	if (len)
		*len = elem->len;

	vq->last_used_idx++;
	return cookie;
}

static int virtio_mmio_negotiate_features(struct virtio_device *vdev)
{
	uint32_t features;

	virtio_writel(vdev->base, VIRTIO_MMIO_HOST_FEATURES_SEL, 0);
	features = virtio_readl(vdev->base, VIRTIO_MMIO_HOST_FEATURES);

	/* Accept every feature the device offers. */
	virtio_writel(vdev->base, VIRTIO_MMIO_GUEST_FEATURES_SEL, 0);
	virtio_writel(vdev->base, VIRTIO_MMIO_GUEST_FEATURES, features);
	return 0;
}

void virtio_mmio_set_status(struct virtio_device *vdev, uint32_t status)
{
	uint32_t old;

	old = virtio_readl(vdev->base, VIRTIO_MMIO_STATUS);
	virtio_writel(vdev->base, VIRTIO_MMIO_STATUS, old | status);
}

int virtio_mmio_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct virtio_device *vdev;
	uint32_t magic, version, device_id;
	uintptr_t base;
	int ret;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -1;

	base = (uintptr_t)mmu_ioremap(res->start, resource_size(res));
	if (!base)
		return -1;

	magic = virtio_readl(base, VIRTIO_MMIO_MAGIC_VALUE);
	version = virtio_readl(base, VIRTIO_MMIO_VERSION);
	device_id = virtio_readl(base, VIRTIO_MMIO_DEVICE_ID);

	if (magic != VIRTIO_MAGIC || version != VIRTIO_VERSION_LEGACY)
		return -1;

	if (device_id == 0)
		return 0; /* no device */

	printk("virtio-mmio: probe device id=%u\n", device_id);

	vdev = (struct virtio_device *)kmalloc(sizeof(*vdev));
	if (!vdev)
		return -1;
	memset(vdev, 0, sizeof(*vdev));

	vdev->base = base;
	vdev->device_id = device_id;
	vdev->irq = (uint32_t)platform_get_irq(pdev, 0);
	vdev->pdev = pdev;

	virtio_mmio_set_status(vdev, VIRTIO_CONFIG_S_ACKNOWLEDGE);
	virtio_mmio_set_status(vdev, VIRTIO_CONFIG_S_DRIVER);

	ret = virtio_mmio_negotiate_features(vdev);
	if (ret < 0) {
		virtio_mmio_set_status(vdev, VIRTIO_CONFIG_S_FAILED);
		kfree(vdev);
		return -1;
	}

	virtio_mmio_set_status(vdev, VIRTIO_CONFIG_S_FEATURES_OK);

	virtio_writel(vdev->base, VIRTIO_MMIO_GUEST_PAGE_SIZE, 4096);

	virtio_device_register(vdev);

	return 0;
}

static int virtio_mmio_driver_probe(struct platform_device *pdev)
{
	return virtio_mmio_probe(pdev);
}

static const struct of_device_id virtio_mmio_of_match[] = {
	{ .compatible = "virtio,mmio" },
	{ /* sentinel */ }
};

static struct platform_driver virtio_mmio_driver = {
	.drv = { .name = "virtio-mmio" },
	.probe = virtio_mmio_driver_probe,
	.of_match_table = virtio_mmio_of_match,
};

static void virtio_mmio_init(void)
{
	platform_driver_register(&virtio_mmio_driver);
}
module_register(virtio_mmio, MODULE_LEVEL_LOW, virtio_mmio_init);
