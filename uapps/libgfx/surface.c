#include "surface.h"

void gfx_surface_init(struct gfx_surface *surf, void *pixels,
		      uint32_t width, uint32_t height)
{
	surf->cv.width = width;
	surf->cv.height = height;
	surf->cv.stride = width * 4;
	surf->cv.format = GFX_FORMAT_XRGB8888;
	surf->cv.pixels = pixels;
}

void gfx_surface_blit(struct gfx_canvas *dst, int x, int y,
		      const struct gfx_surface *surf)
{
	gfx_blit(dst, x, y, &surf->cv);
}
