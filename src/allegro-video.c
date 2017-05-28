#include "allegro-main.h"
#include "ibm.h"
#include "video.h"

#include "allegro-video.h"

static void allegro_blit_memtoscreen(int x, int y, int y1, int y2, int w, int h);
static BITMAP *buffer32_vscale;
void allegro_video_init()
{
        set_color_depth(32);
        set_gfx_mode(GFX_AUTODETECT_WINDOWED, 640, 480, 0, 0);
        video_blit_memtoscreen_func = allegro_blit_memtoscreen;

	buffer32_vscale = create_bitmap(2048, 2048);
}

void allegro_video_close()
{
	destroy_bitmap(buffer32_vscale);
}

void allegro_video_update_size(int x, int y)
{
        if (set_gfx_mode(GFX_AUTODETECT_WINDOWED, x, y, 0, 0))
                fatal("Failed to set gfx mode %i,%i : %s\n", x, y, allegro_error);
}

static void allegro_blit_memtoscreen(int x, int y, int y1, int y2, int w, int h)
{
	if (h < winsizey)
	{
		int yy;

		for (yy = y+y1; yy < y+y2; yy++)
		{
			if (yy >= 0)
			{
				memcpy(&((uint32_t *)buffer32_vscale->line[yy*2])[x], &((uint32_t *)buffer32->line[yy])[x], w*4);
				memcpy(&((uint32_t *)buffer32_vscale->line[(yy*2)+1])[x], &((uint32_t *)buffer32->line[yy])[x], w*4);
			}
		}

		video_blit_complete();
	        blit(buffer32_vscale, screen, x, (y+y1)*2, 0, y1, w, (y2-y1)*2);
	}
	else
	{
	        blit(buffer32, screen, x, y+y1, 0, y1, w, y2-y1);
		video_blit_complete();
	}
}

