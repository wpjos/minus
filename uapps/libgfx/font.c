#include "font.h"

void font_render_char(struct gfx_canvas *cv, const struct font *f,
		      int x, int y, char c, uint32_t fg, uint32_t bg)
{
	const uint8_t *glyph;
	int row, col;

	if (c < f->first || c > f->last)
		return;

	glyph = f->data + ((c - f->first) * f->height);

	for (row = 0; row < f->height; row++) {
		uint8_t bits = glyph[row];
		for (col = 0; col < f->width; col++) {
			if (bits & (0x80 >> col))
				gfx_set_pixel(cv, x + col, y + row, fg);
			else if (bg != fg)
				gfx_set_pixel(cv, x + col, y + row, bg);
		}
	}
}

void font_render_string(struct gfx_canvas *cv, const struct font *f,
			int x, int y, const char *s, uint32_t fg, uint32_t bg)
{
	while (*s) {
		font_render_char(cv, f, x, y, *s, fg, bg);
		x += f->width;
		s++;
	}
}
