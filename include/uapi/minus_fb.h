#ifndef __MINUS_FB_H__
#define __MINUS_FB_H__

#include "types.h"

/* Framebuffer pixel formats. */
#define FB_FORMAT_XRGB8888	0
#define FB_FORMAT_RGB565	1

/* Framebuffer info returned to userspace. */
struct fb_info_req {
	uint32_t width;
	uint32_t height;
	uint32_t stride;
	uint32_t format;
	uint64_t size;
};

/* Framebuffer ioctl commands (POSIX-style). */
#define FBIOGET_INFO		0x4600
#define FBIO_FLUSH		0x4601

/* mmap protection and flags. */
#define PROT_READ	0x1
#define PROT_WRITE	0x2

#define MAP_SHARED	0x01
#define MAP_PRIVATE	0x02

#endif /* __MINUS_FB_H__ */
