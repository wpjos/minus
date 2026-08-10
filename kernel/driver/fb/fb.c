#include "fb.h"
#include "string.h"

static struct fb_info *g_fb_info;

int fb_register(struct fb_info *info)
{
	if (!info)
		return -1;
	g_fb_info = info;
	return 0;
}

void fb_unregister(struct fb_info *info)
{
	if (g_fb_info == info)
		g_fb_info = NULL;
}

struct fb_info *fb_get_info(void)
{
	return g_fb_info;
}
