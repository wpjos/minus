#ifndef __DRIVER_FB_H__
#define __DRIVER_FB_H__

#include "types.h"
#include "dlist.h"

#define FB_FORMAT_XRGB8888	0
#define FB_FORMAT_RGB565	1

struct device;

struct fb_info {
	struct device *dev;
	uint32_t width;
	uint32_t height;
	uint32_t stride;	/* bytes per scanline */
	uint32_t bpp;
	uint32_t format;	/* FB_FORMAT_* */
	void *screen_base;	/* kernel virtual address of framebuffer */
	uint64_t screen_size;
	uint64_t phys_base;	/* physical address for mmap */
	struct fb_ops *fbops;
	struct dlist_node node;
};

struct fb_ops {
	int (*fb_flush)(struct fb_info *info, int x, int y, int w, int h);
};

int fb_register(struct fb_info *info);
void fb_unregister(struct fb_info *info);
struct fb_info *fb_get_info(void);

#endif /* __DRIVER_FB_H__ */
