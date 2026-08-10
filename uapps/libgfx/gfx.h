#ifndef __LIBGFX_GFX_H__
#define __LIBGFX_GFX_H__

#include "types.h"
#include "minus_fb.h"

#define GFX_FORMAT_XRGB8888	0

#define GFX_COLOR_BLACK		0xFF000000
#define GFX_COLOR_WHITE		0xFFFFFFFF
#define GFX_COLOR_RED		0xFFFF0000
#define GFX_COLOR_GREEN		0xFF00FF00
#define GFX_COLOR_BLUE		0xFF0000FF

#define GFX_RGB(r, g, b)	(0xFF000000U | ((uint32_t)(r) << 16) | \
				 ((uint32_t)(g) << 8) | (uint32_t)(b))

struct gfx_canvas {
	uint32_t width;
	uint32_t height;
	uint32_t stride;	/* bytes per scanline */
	uint32_t format;
	void *pixels;
};

void gfx_canvas_init(struct gfx_canvas *cv, const struct fb_info_req *info,
		     void *pixels);
void gfx_set_pixel(struct gfx_canvas *cv, int x, int y, uint32_t color);
void gfx_clear(struct gfx_canvas *cv, uint32_t color);
void gfx_fill_rect(struct gfx_canvas *cv, int x, int y, int w, int h,
		   uint32_t color);
void gfx_draw_rect(struct gfx_canvas *cv, int x, int y, int w, int h,
		   uint32_t color);
void gfx_draw_line(struct gfx_canvas *cv, int x0, int y0, int x1, int y1,
		   uint32_t color);
void gfx_draw_circle(struct gfx_canvas *cv, int cx, int cy, int r,
		     uint32_t color);
void gfx_blit(struct gfx_canvas *dst, int x, int y,
	      const struct gfx_canvas *src);

#endif /* __LIBGFX_GFX_H__ */
