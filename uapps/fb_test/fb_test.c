#include "libgfx.h"

static inline long syscall3(long n, long a, long b, long c)
{
	register long x8 __asm__("x8") = n;
	register long x0 __asm__("x0") = a;
	register long x1 __asm__("x1") = b;
	register long x2 __asm__("x2") = c;
	__asm__ volatile("svc #0" : "=r"(x0) : "r"(x0), "r"(x1), "r"(x2), "r"(x8)
			 : "memory", "x3", "x4", "x5",
			   "x6", "x7", "x9", "x10", "x11", "x12",
			   "x13", "x14", "x15", "x16", "x17", "x18");
	return x0;
}

static size_t strlen(const char *s)
{
	size_t n = 0;
	while (s[n])
		n++;
	return n;
}

static void print(const char *s)
{
	syscall3(64, 1, (long)s, (long)strlen(s));
}

static uint32_t lerp_channel(uint32_t c1, uint32_t c2, int num, int den)
{
	if (den <= 0)
		return c1;
	return (c1 * (den - num) + c2 * num) / den;
}

static uint32_t lerp_color(uint32_t c1, uint32_t c2, int num, int den)
{
	uint32_t r = lerp_channel((c1 >> 16) & 0xff, (c2 >> 16) & 0xff, num, den);
	uint32_t g = lerp_channel((c1 >> 8) & 0xff, (c2 >> 8) & 0xff, num, den);
	uint32_t b = lerp_channel(c1 & 0xff, c2 & 0xff, num, den);
	return GFX_RGB(r, g, b);
}

static void draw_gradient_bg(struct gfx_canvas *cv, uint32_t top,
			     uint32_t bottom)
{
	int h = (int)cv->height;
	for (int y = 0; y < h; y++)
		gfx_fill_rect(cv, 0, y, cv->width, 1,
			      lerp_color(top, bottom, y, h - 1));
}

static void draw_filled_circle(struct gfx_canvas *cv, int cx, int cy, int r,
			       uint32_t color)
{
	int r2 = r * r;
	for (int y = cy - r; y <= cy + r; y++) {
		for (int x = cx - r; x <= cx + r; x++) {
			int dx = x - cx;
			int dy = y - cy;
			if (dx * dx + dy * dy <= r2)
				gfx_set_pixel(cv, x, y, color);
		}
	}
}

static void draw_palette(struct gfx_canvas *cv, int x, int y, int n,
			 int size, int gap)
{
	static const uint32_t colors[] = {
		GFX_RGB(0xe9, 0x45, 0x60),
		GFX_RGB(0xff, 0xc3, 0x00),
		GFX_RGB(0x00, 0xd2, 0xff),
		GFX_RGB(0x2e, 0xcc, 0x71),
		GFX_RGB(0x9b, 0x59, 0xb6),
		GFX_RGB(0xf3, 0x9c, 0x12),
		GFX_RGB(0x1a, 0xbc, 0x9c),
		GFX_RGB(0x34, 0x49, 0x5e),
	};
	for (int i = 0; i < n; i++) {
		gfx_fill_rect(cv, x + i * (size + gap), y, size, size,
			      colors[i % (sizeof(colors) / sizeof(colors[0]))]);
		gfx_draw_rect(cv, x + i * (size + gap), y, size, size,
			      GFX_RGB(0x22, 0x22, 0x33));
	}
}

static void draw_title_bar(struct gfx_canvas *cv, const char *title)
{
	int h = 40;
	int tw = (int)strlen(title) * 8;
	int tx = ((int)cv->width - tw) / 2;
	int ty = (h - 8) / 2;

	gfx_fill_rect(cv, 0, 0, cv->width, h, GFX_RGB(0x16, 0x16, 0x28));
	gfx_draw_line(cv, 0, h - 1, cv->width - 1, h - 1,
		      GFX_RGB(0xe9, 0x45, 0x60));
	font_render_string(cv, &font_8x8, tx, ty, title, GFX_COLOR_WHITE,
			   GFX_RGB(0x16, 0x16, 0x28));
}

void _start(void)
{
	int fd;
	struct fb_info_req info;
	void *pixels;
	struct gfx_canvas cv;
	int cx, cy;

	print("fb_test: start\n");

	fd = fb_open();
	if (fd < 0) {
		print("fb_test: open failed\n");
		return;
	}

	if (fb_info(fd, &info) < 0) {
		print("fb_test: info failed\n");
		return;
	}

	print("fb_test: mmap\n");
	pixels = fb_mmap(fd, NULL);
	if (pixels == (void *)-1) {
		print("fb_test: mmap failed\n");
		return;
	}

	print("fb_test: draw\n");
	gfx_canvas_init(&cv, &info, pixels);

	draw_gradient_bg(&cv, GFX_RGB(0x1a, 0x1a, 0x2e),
			 GFX_RGB(0x0f, 0x0f, 0x23));

	draw_title_bar(&cv, "Minus OS / virtio-gpu");

	/* Large decorative circle. */
	cx = (int)cv.width * 3 / 4;
	cy = (int)cv.height * 5 / 12;
	draw_filled_circle(&cv, cx, cy, 120, GFX_RGB(0xe9, 0x45, 0x60));
	gfx_draw_circle(&cv, cx, cy, 120, GFX_RGB(0xff, 0xff, 0xff));
	draw_filled_circle(&cv, cx - 40, cy - 40, 30,
			   GFX_RGB(0xff, 0x80, 0x90));

	/* Framed info box. */
	gfx_fill_rect(&cv, 60, 80, 340, 120, GFX_RGB(0x16, 0x16, 0x28));
	gfx_draw_rect(&cv, 60, 80, 340, 120, GFX_RGB(0xff, 0xc3, 0x00));
	font_render_string(&cv, &font_8x8, 80, 100,
			   "Resolution:", GFX_RGB(0xff, 0xc3, 0x00),
			   GFX_RGB(0x16, 0x16, 0x28));
	font_render_string(&cv, &font_8x8, 80, 120,
			   "1280 x 800", GFX_COLOR_WHITE,
			   GFX_RGB(0x16, 0x16, 0x28));
	font_render_string(&cv, &font_8x8, 80, 150,
			   "Format: XRGB8888", GFX_COLOR_WHITE,
			   GFX_RGB(0x16, 0x16, 0x28));

	/* Color palette strip. */
	draw_palette(&cv, ((int)cv.width - (8 * 64 + 7 * 8)) / 2,
		     (int)cv.height - 120, 8, 64, 8);

	/* Screen border. */
	gfx_draw_rect(&cv, 8, 8, cv.width - 16, cv.height - 16,
		      GFX_RGB(0x33, 0x33, 0x44));

	print("fb_test: flush\n");
	fb_flush(fd);
	print("fb_test: done\n");

	while (1)
		;
}
