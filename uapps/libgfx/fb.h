#ifndef __LIBGFX_FB_H__
#define __LIBGFX_FB_H__

#include "types.h"
#include "minus_fb.h"

int fb_open(void);
int fb_info(int fd, struct fb_info_req *info);
void *fb_mmap(int fd, size_t *size);
int fb_flush(int fd);
int fb_close(int fd);

#endif /* __LIBGFX_FB_H__ */
