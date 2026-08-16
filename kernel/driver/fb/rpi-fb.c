#include "platform.h"
#include "fb.h"
#include "of.h"
#include "fdt.h"
#include "mmu.h"
#include "mm.h"
#include "buddy.h"
#include "page.h"
#include "module.h"
#include "printk.h"
#include "string.h"

/*
 * rpi-fb - Raspberry Pi framebuffer driver.
 *
 * Modern Raspberry Pi firmware (including Pi 5 with BCM2712) exposes the
 * pre-allocated scanout buffer through a "simple-framebuffer" node in the
 * DTB.  We support that directly, plus the legacy "brcm,bcm2708-fb" node and
 * a mailbox property-channel fallback for older firmware configurations.
 */

/* VideoCore property tags for framebuffer allocation (legacy path) */
#define RPI_TAG_SET_PHYSICAL_WH	0x48003
#define RPI_TAG_SET_VIRTUAL_WH	0x48004
#define RPI_TAG_SET_DEPTH	0x48005
#define RPI_TAG_SET_PIXEL_ORDER	0x48006
#define RPI_TAG_ALLOCATE_BUFFER	0x40001
#define RPI_TAG_GET_PITCH	0x40008

#define RPI_FB_WIDTH	1280
#define RPI_FB_HEIGHT	800
#define RPI_FB_BPP	32
#define RPI_FB_ALIGN	16

#define RPI_TAG_REQUEST		0x00000000
#define RPI_TAG_RESPONSE_MASK	0x80000000

struct rpi_fb {
	struct fb_info info;
	struct fb_ops ops;
	struct page *prop_page;
	void *prop_buf;
	void *fb_base;
	uint64_t fb_phys;
	uint64_t fb_size;
	struct platform_device *pdev;
};

static void rpi_fb_clear(void *base, size_t size)
{
	volatile uint64_t *p = base;
	size_t i, n;

	n = size / sizeof(uint64_t);
	for (i = 0; i < n; i++)
		p[i] = 0;

	/* Handle any trailing bytes with 32-bit writes. */
	if (size % sizeof(uint64_t)) {
		volatile uint32_t *q = (volatile uint32_t *)((char *)base + n * sizeof(uint64_t));
		q[0] = 0;
	}
}

/* Provided by kernel/driver/mailbox/bcm2835-mbox.c */
extern int rpi_firmware_property(void *buf, size_t size);

static inline void put_u32(void **p, uint32_t v)
{
	uint32_t *d = (uint32_t *)*p;
	*d = v;
	*p = (void *)(d + 1);
}

static inline uint32_t get_u32(void **p)
{
	uint32_t *s = (uint32_t *)*p;
	uint32_t v = *s;
	*p = (void *)(s + 1);
	return v;
}

/* Append a tag with a fixed-size payload; payload must be a multiple of 4 bytes. */
static void *rpi_fb_tag(void *p, uint32_t tag, const uint32_t *payload,
			uint32_t payload_bytes)
{
	uint32_t i;

	put_u32(&p, tag);
	put_u32(&p, payload_bytes);
	put_u32(&p, RPI_TAG_REQUEST);
	for (i = 0; i < payload_bytes / sizeof(uint32_t); i++)
		put_u32(&p, payload[i]);
	return p;
}

static int rpi_fb_parse_format(const char *fmt)
{
	if (!fmt)
		return FB_FORMAT_XRGB8888;

	if (strcmp(fmt, "x8r8g8b8") == 0 || strcmp(fmt, "a8r8g8b8") == 0)
		return FB_FORMAT_XRGB8888;

	if (strcmp(fmt, "r5g6b5") == 0)
		return FB_FORMAT_RGB565;

	return FB_FORMAT_XRGB8888;
}

static int rpi_fb_setup_from_simplefb(struct rpi_fb *rpi,
				      struct platform_device *pdev)
{
	struct resource *res;
	const void *fdt = fdt_base();
	int node = pdev->dev.of_node;
	const fdt32_t *prop;
	int len;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t stride = 0;
	const char *fmt = NULL;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		printk("rpi-fb: simple-framebuffer missing reg resource\n");
		return -1;
	}

	prop = of_get_property(fdt, node, "width", &len);
	if (prop && len >= (int)sizeof(*prop))
		width = fdt32_to_cpu(prop[0]);

	prop = of_get_property(fdt, node, "height", &len);
	if (prop && len >= (int)sizeof(*prop))
		height = fdt32_to_cpu(prop[0]);

	prop = of_get_property(fdt, node, "stride", &len);
	if (prop && len >= (int)sizeof(*prop))
		stride = fdt32_to_cpu(prop[0]);

	prop = of_get_property(fdt, node, "format", &len);
	if (prop && len > 0)
		fmt = (const char *)prop;

	if (width == 0 || height == 0 || stride == 0) {
		printk("rpi-fb: simple-framebuffer missing w/h/stride\n");
		return -1;
	}

	rpi->fb_phys = res->start;
	rpi->fb_size = resource_size(res);

	printk("rpi-fb: simple-framebuffer res start=%p size=%p\n",
	       (void *)(unsigned long long)rpi->fb_phys,
	       (void *)(unsigned long long)rpi->fb_size);

	rpi->fb_base = mmu_ioremap(rpi->fb_phys, rpi->fb_size);
	if (!rpi->fb_base) {
		printk("rpi-fb: failed to map simple-framebuffer\n");
		return -1;
	}

	rpi_fb_clear(rpi->fb_base, rpi->fb_size);

	rpi->ops.fb_flush = NULL;

	rpi->info.dev = &pdev->dev;
	rpi->info.width = width;
	rpi->info.height = height;
	rpi->info.stride = stride;
	rpi->info.format = rpi_fb_parse_format(fmt);
	rpi->info.bpp = (rpi->info.format == FB_FORMAT_RGB565) ? 16 : 32;
	rpi->info.screen_base = rpi->fb_base;
	rpi->info.screen_size = rpi->fb_size;
	rpi->info.phys_base = rpi->fb_phys;
	rpi->info.fbops = &rpi->ops;

	if (fb_register(&rpi->info) < 0) {
		printk("rpi-fb: failed to register framebuffer\n");
		return -1;
	}

	printk("rpi-fb: simple-framebuffer %ux%u stride=%u fmt=%s phys=%p size=%p\n",
	       width, height, stride, fmt ? fmt : "?",
	       (void *)(unsigned long long)rpi->fb_phys,
	       (void *)(unsigned long long)rpi->fb_size);
	return 0;
}

static int rpi_fb_alloc_framebuffer(struct rpi_fb *rpi,
				    struct platform_device *pdev)
{
	void *p;
	uint32_t payload[2];
	uint32_t alloc_resp[2];
	uint32_t pitch;
	uint32_t size;
	uint32_t code;
	int ret;

	p = rpi->prop_buf;
	put_u32(&p, 0); /* buffer size placeholder */
	put_u32(&p, RPI_TAG_REQUEST);

	payload[0] = RPI_FB_WIDTH;
	payload[1] = RPI_FB_HEIGHT;
	p = rpi_fb_tag(p, RPI_TAG_SET_PHYSICAL_WH, payload, sizeof(payload));
	p = rpi_fb_tag(p, RPI_TAG_SET_VIRTUAL_WH, payload, sizeof(payload));

	payload[0] = RPI_FB_BPP;
	p = rpi_fb_tag(p, RPI_TAG_SET_DEPTH, payload, sizeof(uint32_t));

	/* Pixel order: 0 = BGR, 1 = RGB.  Try BGR first. */
	payload[0] = 0;
	p = rpi_fb_tag(p, RPI_TAG_SET_PIXEL_ORDER, payload, sizeof(uint32_t));

	/* Allocate buffer request: alignment only. */
	payload[0] = RPI_FB_ALIGN;
	payload[1] = 0;
	p = rpi_fb_tag(p, RPI_TAG_ALLOCATE_BUFFER, payload, sizeof(payload));

	/* Get pitch. */
	payload[0] = 0;
	p = rpi_fb_tag(p, RPI_TAG_GET_PITCH, payload, sizeof(uint32_t));

	/* End tag. */
	put_u32(&p, 0);

	/* Fill in buffer size. */
	size = (uint32_t)((uintptr_t)p - (uintptr_t)rpi->prop_buf);
	*(uint32_t *)rpi->prop_buf = size;

	ret = rpi_firmware_property(rpi->prop_buf, size);
	if (ret < 0) {
		printk("rpi-fb: firmware property call failed\n");
		return -1;
	}

	/*
	 * Walk the response buffer and extract the allocate-buffer address/size
	 * and pitch.  We skip each tag's header and payload.
	 */
	p = (uint8_t *)rpi->prop_buf + sizeof(uint32_t); /* size */
	code = get_u32(&p);
	if ((code & RPI_TAG_RESPONSE_MASK) == 0) {
		printk("rpi-fb: firmware did not set response bit (%p)\n",
		       (void *)(unsigned long long)code);
		return -1;
	}

	alloc_resp[0] = 0;
	alloc_resp[1] = 0;
	pitch = 0;

	while (1) {
		uint32_t tag;
		uint32_t val_size;
		uint32_t req_resp;

		tag = get_u32(&p);
		if (tag == 0)
			break;

		val_size = get_u32(&p);
		req_resp = get_u32(&p);
		(void)req_resp;

		switch (tag) {
		case RPI_TAG_ALLOCATE_BUFFER:
			alloc_resp[0] = get_u32(&p);
			alloc_resp[1] = get_u32(&p);
			break;
		case RPI_TAG_GET_PITCH:
			pitch = get_u32(&p);
			break;
		default:
			/* Skip payload. */
			p = (uint8_t *)p + val_size;
			break;
		}

		/* Tags are padded to 4-byte boundaries. */
		if (val_size % sizeof(uint32_t))
			p = (uint8_t *)p + (sizeof(uint32_t) - (val_size % sizeof(uint32_t)));
	}

	if (alloc_resp[0] == 0 || alloc_resp[1] == 0 || pitch == 0) {
		printk("rpi-fb: invalid firmware response alloc=%p/%p pitch=%p\n",
		       (void *)(unsigned long long)alloc_resp[0],
		       (void *)(unsigned long long)alloc_resp[1],
		       (void *)(unsigned long long)pitch);
		return -1;
	}

	/*
	 * On BCM2712 the address returned by the firmware is already a CPU
	 * physical address; no legacy 0x3FFFFFFF bus-to-phys translation is
	 * required.
	 */
	rpi->fb_phys = alloc_resp[0];
	rpi->fb_size = alloc_resp[1];

	rpi->fb_base = mmu_ioremap(rpi->fb_phys, rpi->fb_size);
	if (!rpi->fb_base) {
		printk("rpi-fb: failed to map framebuffer\n");
		return -1;
	}

	rpi_fb_clear(rpi->fb_base, rpi->fb_size);

	rpi->ops.fb_flush = NULL;

	rpi->info.dev = pdev ? &pdev->dev : NULL;
	rpi->info.width = RPI_FB_WIDTH;
	rpi->info.height = RPI_FB_HEIGHT;
	rpi->info.stride = pitch;
	rpi->info.bpp = RPI_FB_BPP;
	rpi->info.format = FB_FORMAT_XRGB8888;
	rpi->info.screen_base = rpi->fb_base;
	rpi->info.screen_size = rpi->fb_size;
	rpi->info.phys_base = rpi->fb_phys;
	rpi->info.fbops = &rpi->ops;

	if (fb_register(&rpi->info) < 0) {
		printk("rpi-fb: failed to register framebuffer\n");
		return -1;
	}

	printk("rpi-fb: %ux%u@%u bpp, pitch=%u phys=%p size=%p\n",
	       RPI_FB_WIDTH, RPI_FB_HEIGHT, RPI_FB_BPP, pitch,
	       (void *)(unsigned long long)rpi->fb_phys,
	       (void *)(unsigned long long)rpi->fb_size);
	return 0;
}

static struct rpi_fb *rpi_fb_create_instance(struct platform_device *pdev)
{
	struct rpi_fb *rpi;

	rpi = (struct rpi_fb *)kzalloc(sizeof(*rpi));
	if (!rpi)
		return NULL;

	rpi->prop_page = buddy_alloc_pages(PAGE_SIZE);
	if (!rpi->prop_page) {
		printk("rpi-fb: failed to allocate property buffer\n");
		kfree(rpi);
		return NULL;
	}
	rpi->prop_buf = page_to_virt(rpi->prop_page);
	memset(rpi->prop_buf, 0, PAGE_SIZE);

	rpi->pdev = pdev;

	if (rpi_fb_alloc_framebuffer(rpi, pdev) < 0) {
		buddy_free_pages(rpi->prop_page);
		kfree(rpi);
		return NULL;
	}

	if (pdev)
		pdev->dev.driver_data = rpi;
	return rpi;
}

static int rpi_fb_probe(struct platform_device *pdev)
{
	struct rpi_fb *rpi;
	const char *compat;
	int len;

	rpi = (struct rpi_fb *)kzalloc(sizeof(*rpi));
	if (!rpi)
		return -1;

	compat = of_get_property(fdt_base(), pdev->dev.of_node, "compatible", &len);
	if (compat && strcmp(compat, "simple-framebuffer") == 0) {
		if (rpi_fb_setup_from_simplefb(rpi, pdev) < 0) {
			kfree(rpi);
			return -1;
		}
	} else {
		/* Legacy brcm,bcm2708-fb or fallback: use mailbox allocation. */
		rpi->prop_page = buddy_alloc_pages(PAGE_SIZE);
		if (!rpi->prop_page) {
			printk("rpi-fb: failed to allocate property buffer\n");
			kfree(rpi);
			return -1;
		}
		rpi->prop_buf = page_to_virt(rpi->prop_page);
		memset(rpi->prop_buf, 0, PAGE_SIZE);
		rpi->pdev = pdev;

		if (rpi_fb_alloc_framebuffer(rpi, pdev) < 0) {
			buddy_free_pages(rpi->prop_page);
			kfree(rpi);
			return -1;
		}
	}

	pdev->dev.driver_data = rpi;
	return 0;
}

static int rpi_fb_remove(struct platform_device *pdev)
{
	struct rpi_fb *rpi = (struct rpi_fb *)pdev->dev.driver_data;

	if (rpi) {
		fb_unregister(&rpi->info);
		if (rpi->prop_page)
			buddy_free_pages(rpi->prop_page);
		kfree(rpi);
	}
	return 0;
}

static const struct of_device_id rpi_fb_match[] = {
	{ .compatible = "simple-framebuffer" },
	{ .compatible = "brcm,bcm2708-fb" },
	{ /* sentinel */ }
};

static struct platform_driver rpi_fb_driver = {
	.drv = { .name = "rpi-fb" },
	.probe = rpi_fb_probe,
	.remove = rpi_fb_remove,
	.of_match_table = rpi_fb_match,
};

static void rpi_fb_init(void)
{
	platform_driver_register(&rpi_fb_driver);

	/*
	 * If the firmware DTB does not contain a framebuffer node, try to
	 * allocate one ourselves through the mailbox property channel.
	 */
	if (!fb_get_info()) {
		printk("rpi-fb: no DT fb node, trying firmware allocation\n");
		rpi_fb_create_instance(NULL);
	}
}
module_register(rpi_fb, MODULE_LEVEL_LOW, rpi_fb_init);
