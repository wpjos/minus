#include "virtio.h"
#include "virtio_mmio.h"
#include "virtio_gpu.h"
#include "fb.h"
#include "mm.h"
#include "buddy.h"
#include "page.h"
#include "mmu.h"
#include "string.h"
#include "memory.h"
#include "module.h"
#include "printk.h"
#include "errno.h"

struct virtio_gpu {
	struct virtio_device *vdev;
	struct fb_info info;
	struct fb_ops ops;
	uint32_t resource_id;
	uint32_t width;
	uint32_t height;
	uint32_t stride;
	void *fb_base;
	uint64_t fb_phys;
	uint64_t fb_size;
	struct page *fb_pages;
};

static int virtio_gpu_send_cmd(struct virtio_device *vdev,
			       void *req, size_t req_len,
			       void *resp, size_t resp_len,
			       uint32_t expected_resp)
{
	struct vring_desc desc[2];
	uint32_t len;
	int cookie_val = 1;
	struct virtio_gpu_ctrl_hdr *hdr;
	int ret;

	desc[0].addr = __VA_PA__((uintptr_t)req);
	desc[0].len = (uint32_t)req_len;
	desc[0].flags = 0;
	desc[0].next = 1;

	desc[1].addr = __VA_PA__((uintptr_t)resp);
	desc[1].len = (uint32_t)resp_len;
	desc[1].flags = VRING_DESC_F_WRITE;
	desc[1].next = 0;

	ret = virtio_add_buf(vdev, desc, 2, &cookie_val);
	if (ret < 0)
		return ret;

	virtio_mmio_notify(vdev, 0);

	while (1) {
		void *cookie = virtio_get_buf(vdev, &len);
		if (cookie == &cookie_val)
			break;
	}

	hdr = (struct virtio_gpu_ctrl_hdr *)resp;
	return (hdr->type == expected_resp) ? 0 : -EIO;
}

static int virtio_gpu_get_display_info(struct virtio_device *vdev,
				       uint32_t *width, uint32_t *height)
{
	struct virtio_gpu_ctrl_hdr req;
	struct virtio_gpu_resp_display_info resp;
	int ret;

	memset(&req, 0, sizeof(req));
	req.type = VIRTIO_GPU_CMD_GET_DISPLAY_INFO;

	memset(&resp, 0, sizeof(resp));

	ret = virtio_gpu_send_cmd(vdev, &req, sizeof(req), &resp, sizeof(resp),
				  VIRTIO_GPU_RESP_OK_DISPLAY_INFO);
	if (ret < 0) {
		printk("virtio-gpu: GET_DISPLAY_INFO failed, resp type=%x\n",
		       resp.hdr.type);
		return ret;
	}

	if (resp.hdr.type != VIRTIO_GPU_RESP_OK_DISPLAY_INFO) {
		printk("virtio-gpu: unexpected display info resp type=%x\n",
		       resp.hdr.type);
		return -EIO;
	}

	if (resp.pmodes[0].enabled == 0) {
		printk("virtio-gpu: scanout 0 not enabled\n");
		return -ENODEV;
	}

	*width = resp.pmodes[0].r.width;
	*height = resp.pmodes[0].r.height;
	return 0;
}

static int virtio_gpu_resource_create_2d(struct virtio_device *vdev,
					 uint32_t resource_id,
					 uint32_t width, uint32_t height)
{
	struct virtio_gpu_resource_create_2d req;
	struct virtio_gpu_ctrl_hdr resp;

	memset(&req, 0, sizeof(req));
	req.hdr.type = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D;
	req.resource_id = resource_id;
	req.format = VIRTIO_GPU_FORMAT_X8R8G8B8_UNORM;
	req.width = width;
	req.height = height;

	memset(&resp, 0, sizeof(resp));

	return virtio_gpu_send_cmd(vdev, &req, sizeof(req), &resp, sizeof(resp),
				   VIRTIO_GPU_RESP_OK_NODATA);
}

static int virtio_gpu_resource_attach_backing(struct virtio_device *vdev,
					      uint32_t resource_id,
					      uint64_t phys, uint64_t size)
{
	struct {
		struct virtio_gpu_resource_attach_backing hdr;
		struct virtio_gpu_mem_entry entry;
	} req;
	struct virtio_gpu_ctrl_hdr resp;

	memset(&req, 0, sizeof(req));
	req.hdr.hdr.type = VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING;
	req.hdr.resource_id = resource_id;
	req.hdr.nr_entries = 1;
	req.hdr.entries[0].addr = phys;
	req.hdr.entries[0].length = (uint32_t)size;

	memset(&resp, 0, sizeof(resp));

	return virtio_gpu_send_cmd(vdev, &req, sizeof(req), &resp, sizeof(resp),
				   VIRTIO_GPU_RESP_OK_NODATA);
}

static int virtio_gpu_set_scanout(struct virtio_device *vdev,
				  uint32_t scanout_id, uint32_t resource_id,
				  uint32_t width, uint32_t height)
{
	struct virtio_gpu_set_scanout req;
	struct virtio_gpu_ctrl_hdr resp;

	memset(&req, 0, sizeof(req));
	req.hdr.type = VIRTIO_GPU_CMD_SET_SCANOUT;
	req.r.width = width;
	req.r.height = height;
	req.scanout_id = scanout_id;
	req.resource_id = resource_id;

	memset(&resp, 0, sizeof(resp));

	return virtio_gpu_send_cmd(vdev, &req, sizeof(req), &resp, sizeof(resp),
				   VIRTIO_GPU_RESP_OK_NODATA);
}

static int virtio_gpu_transfer_to_host_2d(struct virtio_device *vdev,
					  uint32_t resource_id,
					  uint32_t x, uint32_t y,
					  uint32_t w, uint32_t h,
					  uint32_t stride)
{
	struct virtio_gpu_transfer_to_host_2d req;
	struct virtio_gpu_ctrl_hdr resp;

	memset(&req, 0, sizeof(req));
	req.hdr.type = VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D;
	req.r.x = x;
	req.r.y = y;
	req.r.width = w;
	req.r.height = h;
	req.offset = (uint64_t)y * stride + (uint64_t)x * 4;
	req.resource_id = resource_id;

	memset(&resp, 0, sizeof(resp));

	return virtio_gpu_send_cmd(vdev, &req, sizeof(req), &resp, sizeof(resp),
				   VIRTIO_GPU_RESP_OK_NODATA);
}

static int virtio_gpu_resource_flush(struct virtio_device *vdev,
				     uint32_t resource_id,
				     uint32_t x, uint32_t y,
				     uint32_t w, uint32_t h)
{
	struct virtio_gpu_resource_flush req;
	struct virtio_gpu_ctrl_hdr resp;

	memset(&req, 0, sizeof(req));
	req.hdr.type = VIRTIO_GPU_CMD_RESOURCE_FLUSH;
	req.r.x = x;
	req.r.y = y;
	req.r.width = w;
	req.r.height = h;
	req.resource_id = resource_id;

	memset(&resp, 0, sizeof(resp));

	return virtio_gpu_send_cmd(vdev, &req, sizeof(req), &resp, sizeof(resp),
				   VIRTIO_GPU_RESP_OK_NODATA);
}

static int virtio_gpu_fb_flush(struct fb_info *info, int x, int y, int w, int h)
{
	struct virtio_gpu *gpu = container_of(info, struct virtio_gpu, info);
	uint32_t x0, y0, x1, y1;
	int ret;

	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;

	x0 = (uint32_t)x;
	y0 = (uint32_t)y;
	x1 = (uint32_t)(x + w);
	y1 = (uint32_t)(y + h);

	if (x1 > gpu->width)
		x1 = gpu->width;
	if (y1 > gpu->height)
		y1 = gpu->height;

	if (x1 <= x0 || y1 <= y0)
		return 0;

	ret = virtio_gpu_transfer_to_host_2d(gpu->vdev, gpu->resource_id,
					     x0, y0, x1 - x0, y1 - y0,
					     gpu->stride);
	if (ret < 0)
		return ret;

	return virtio_gpu_resource_flush(gpu->vdev, gpu->resource_id,
					 x0, y0, x1 - x0, y1 - y0);
}

static int virtio_gpu_probe(struct virtio_device *vdev)
{
	struct virtio_gpu *gpu;
	uint32_t width, height;
	uint64_t size;
	int ret;

	ret = virtio_mmio_setup_queue(vdev, 0, VIRTIO_GPU_QUEUE_SIZE);
	if (ret < 0)
		return ret;

	virtio_mmio_set_status(vdev, VIRTIO_CONFIG_S_DRIVER_OK);
	virtio_mmio_set_status(vdev, VIRTIO_CONFIG_S_STARTED);

	ret = virtio_gpu_get_display_info(vdev, &width, &height);
	if (ret < 0) {
		printk("virtio-gpu: failed to get display info\n");
		return ret;
	}

	printk("virtio-gpu: display %ux%u\n", width, height);

	gpu = (struct virtio_gpu *)kmalloc(sizeof(*gpu));
	if (!gpu)
		return -ENOMEM;
	memset(gpu, 0, sizeof(*gpu));

	gpu->vdev = vdev;
	gpu->resource_id = 1;
	gpu->width = width;
	gpu->height = height;
	gpu->stride = width * 4;
	size = (uint64_t)gpu->stride * height;
	gpu->fb_size = size;

	gpu->fb_pages = buddy_alloc_pages(size);
	if (!gpu->fb_pages) {
		printk("virtio-gpu: failed to allocate framebuffer memory\n");
		kfree(gpu);
		return -ENOMEM;
	}

	gpu->fb_phys = page_to_phy(gpu->fb_pages);
	gpu->fb_base = mmu_memremap(gpu->fb_phys, size);
	if (!gpu->fb_base) {
		printk("virtio-gpu: failed to map framebuffer\n");
		buddy_free_pages(gpu->fb_pages);
		kfree(gpu);
		return -ENOMEM;
	}

	memset(gpu->fb_base, 0, size);

	if (virtio_gpu_resource_create_2d(vdev, gpu->resource_id,
					  width, height) < 0) {
		printk("virtio-gpu: failed to create resource\n");
		goto fail_fb;
	}

	if (virtio_gpu_resource_attach_backing(vdev, gpu->resource_id,
					       gpu->fb_phys, size) < 0) {
		printk("virtio-gpu: failed to attach backing\n");
		goto fail_fb;
	}

	if (virtio_gpu_set_scanout(vdev, 0, gpu->resource_id,
				   width, height) < 0) {
		printk("virtio-gpu: failed to set scanout\n");
		goto fail_fb;
	}

	gpu->ops.fb_flush = virtio_gpu_fb_flush;

	gpu->info.dev = &vdev->dev;
	gpu->info.width = width;
	gpu->info.height = height;
	gpu->info.stride = gpu->stride;
	gpu->info.bpp = 32;
	gpu->info.format = FB_FORMAT_XRGB8888;
	gpu->info.screen_base = gpu->fb_base;
	gpu->info.screen_size = size;
	gpu->info.phys_base = gpu->fb_phys;
	gpu->info.fbops = &gpu->ops;

	if (fb_register(&gpu->info) < 0) {
		printk("virtio-gpu: failed to register framebuffer\n");
		goto fail_fb;
	}

	printk("virtio-gpu: framebuffer registered %ux%u@%p\n",
	       width, height, gpu->fb_base);
	return 0;

fail_fb:
	buddy_free_pages(gpu->fb_pages);
	kfree(gpu);
	return -EIO;
}

static const struct virtio_device_id virtio_gpu_id_table[] = {
	{ .device_id = VIRTIO_ID_GPU },
	{ /* sentinel */ }
};

static struct virtio_driver virtio_gpu_driver = {
	.id_table = virtio_gpu_id_table,
	.probe    = virtio_gpu_probe,
};

static void virtio_gpu_init(void)
{
	virtio_driver_register(&virtio_gpu_driver);
}
module_register(virtio_gpu, MODULE_LEVEL_LOW, virtio_gpu_init);
