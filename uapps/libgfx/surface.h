#ifndef __LIBGFX_SURFACE_H__
#define __LIBGFX_SURFACE_H__

#include "gfx.h"

struct gfx_surface {
	struct gfx_canvas cv;
};

/* Initialize a surface using a caller-provided pixel buffer. */
void gfx_surface_init(struct gfx_surface *surf, void *pixels,
		      uint32_t width, uint32_t height);

/* Blit a surface onto a canvas at (x, y). */
void gfx_surface_blit(struct gfx_canvas *dst, int x, int y,
		      const struct gfx_surface *surf);

#endif /* __LIBGFX_SURFACE_H__ */
