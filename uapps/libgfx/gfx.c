#include "gfx.h"
#include "string.h"

static inline int clip(int v, int min, int max)
{
	if (v < min)
		return min;
	if (v > max)
		return max;
	return v;
}

void gfx_canvas_init(struct gfx_canvas *cv, const struct fb_info_req *info,
		     void *pixels)
{
	cv->width = info->width;
	cv->height = info->height;
	cv->stride = info->stride;
	cv->format = info->format;
	cv->pixels = pixels;
}

void gfx_set_pixel(struct gfx_canvas *cv, int x, int y, uint32_t color)
{
	uint32_t *p;

	if (x < 0 || x >= (int)cv->width || y < 0 || y >= (int)cv->height)
		return;

	p = (uint32_t *)((char *)cv->pixels + y * cv->stride + x * 4);
	*p = color;
}

void gfx_clear(struct gfx_canvas *cv, uint32_t color)
{
	gfx_fill_rect(cv, 0, 0, cv->width, cv->height, color);
}

void gfx_fill_rect(struct gfx_canvas *cv, int x, int y, int w, int h,
		   uint32_t color)
{
	int x0, y0, x1, y1;
	int row, col;

	x0 = clip(x, 0, (int)cv->width);
	y0 = clip(y, 0, (int)cv->height);
	x1 = clip(x + w, 0, (int)cv->width);
	y1 = clip(y + h, 0, (int)cv->height);

	if (x1 <= x0 || y1 <= y0)
		return;

	for (row = y0; row < y1; row++) {
		uint32_t *p = (uint32_t *)((char *)cv->pixels +
					   row * cv->stride + x0 * 4);
		for (col = x0; col < x1; col++)
			*p++ = color;
	}
}

void gfx_draw_rect(struct gfx_canvas *cv, int x, int y, int w, int h,
		   uint32_t color)
{
	gfx_draw_line(cv, x, y, x + w - 1, y, color);
	gfx_draw_line(cv, x + w - 1, y, x + w - 1, y + h - 1, color);
	gfx_draw_line(cv, x + w - 1, y + h - 1, x, y + h - 1, color);
	gfx_draw_line(cv, x, y + h - 1, x, y, color);
}

void gfx_draw_line(struct gfx_canvas *cv, int x0, int y0, int x1, int y1,
		   uint32_t color)
{
	int dx, dy, sx, sy, err, e2;

	dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
	dy = (y1 > y0) ? (y0 - y1) : (y1 - y0);
	sx = (x0 < x1) ? 1 : -1;
	sy = (y0 < y1) ? 1 : -1;
	err = dx + dy;

	while (1) {
		gfx_set_pixel(cv, x0, y0, color);
		if (x0 == x1 && y0 == y1)
			break;
		e2 = 2 * err;
		if (e2 >= dy) {
			err += dy;
			x0 += sx;
		}
		if (e2 <= dx) {
			err += dx;
			y0 += sy;
		}
	}
}

void gfx_draw_circle(struct gfx_canvas *cv, int cx, int cy, int r,
		     uint32_t color)
{
	int x, y, d;

	x = 0;
	y = r;
	d = 3 - 2 * r;

	while (x <= y) {
		gfx_set_pixel(cv, cx + x, cy + y, color);
		gfx_set_pixel(cv, cx - x, cy + y, color);
		gfx_set_pixel(cv, cx + x, cy - y, color);
		gfx_set_pixel(cv, cx - x, cy - y, color);
		gfx_set_pixel(cv, cx + y, cy + x, color);
		gfx_set_pixel(cv, cx - y, cy + x, color);
		gfx_set_pixel(cv, cx + y, cy - x, color);
		gfx_set_pixel(cv, cx - y, cy - x, color);

		if (d < 0)
			d = d + 4 * x + 6;
		else {
			d = d + 4 * (x - y) + 10;
			y--;
		}
		x++;
	}
}

void gfx_blit(struct gfx_canvas *dst, int x, int y,
	      const struct gfx_canvas *src)
{
	int src_x, src_y;
	int dst_x, dst_y;
	int w, h;

	w = (int)src->width;
	h = (int)src->height;

	if (x < 0) {
		src_x = -x;
		dst_x = 0;
		w += x;
	} else {
		src_x = 0;
		dst_x = x;
	}

	if (y < 0) {
		src_y = -y;
		dst_y = 0;
		h += y;
	} else {
		src_y = 0;
		dst_y = y;
	}

	if (dst_x + w > (int)dst->width)
		w = (int)dst->width - dst_x;
	if (dst_y + h > (int)dst->height)
		h = (int)dst->height - dst_y;

	if (w <= 0 || h <= 0)
		return;

	for (int row = 0; row < h; row++) {
		uint32_t *s = (uint32_t *)((char *)src->pixels +
					   (src_y + row) * src->stride +
					   src_x * 4);
		uint32_t *d = (uint32_t *)((char *)dst->pixels +
					   (dst_y + row) * dst->stride +
					   dst_x * 4);
		for (int col = 0; col < w; col++)
			*d++ = *s++;
	}
}
