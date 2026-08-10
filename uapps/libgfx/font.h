#ifndef __LIBGFX_FONT_H__
#define __LIBGFX_FONT_H__

#include "types.h"
#include "gfx.h"

struct font {
	int width;
	int height;
	int first;
	int last;
	const uint8_t *data;
};

extern const struct font font_8x8;

void font_render_char(struct gfx_canvas *cv, const struct font *f,
		      int x, int y, char c, uint32_t fg, uint32_t bg);
void font_render_string(struct gfx_canvas *cv, const struct font *f,
			int x, int y, const char *s, uint32_t fg, uint32_t bg);

#endif /* __LIBGFX_FONT_H__ */
