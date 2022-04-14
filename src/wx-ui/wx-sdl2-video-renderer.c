#include <SDL2/SDL.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "video.h"
#include "wx-utils.h"
#include "wx-sdl2-video.h"
#include "wx-sdl2-video-renderer.h"

static SDL_Texture* texture = NULL;
static SDL_Renderer* renderer = NULL;

extern int video_scale_mode;
extern int video_vsync;
extern int video_focus_dim;
extern int take_screenshot;
extern void screenshot_taken(unsigned char* rgb, int width, int height);

int sdl_video_renderer_init(SDL_Window* window, sdl_render_driver requested_render_driver, SDL_Rect screen)
{
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, video_scale_mode ? "1" : "0");
        const char* driver = requested_render_driver.sdl_id;
        if (!driver) driver = "0";
        SDL_SetHint(SDL_HINT_RENDER_DRIVER, driver);

        int flags = SDL_RENDERER_ACCELERATED;
        if (video_vsync) {
                flags |= SDL_RENDERER_PRESENTVSYNC;
        }
        renderer = SDL_CreateRenderer(window, -1, flags);

        if (!renderer)
        {
                char message[200];
                sprintf(message,
                                "SDL window could not be created! Error: %s\n",
                                SDL_GetError());
                wx_messagebox(0, message, "SDL Error", WX_MB_OK);
                return SDL_FALSE;
        }

        SDL_RendererInfo rendererInfo;
        SDL_GetRendererInfo(renderer, &rendererInfo);
        sdl_render_driver* d = sdl_get_render_driver_by_name_ptr(rendererInfo.name);
        if (!d)
                strcpy(current_render_driver_name, "Unknown");
        else
                strcpy(current_render_driver_name, d->name);

        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                        SDL_TEXTUREACCESS_STREAMING,
                        screen.w, screen.h);

        return SDL_TRUE;

}

void sdl_video_renderer_close()
{
        if (texture)
        {
                SDL_DestroyTexture(texture);
                texture = NULL;
        }
        if (renderer)
        {
                SDL_DestroyRenderer(renderer);
                renderer = NULL;
        }
}

void sdl_video_renderer_update(SDL_Window* window, SDL_Rect updated_rect, BITMAP* screen)
{
        SDL_UpdateTexture(texture, &updated_rect, &((uint32_t*) screen->dat)[updated_rect.y * screen->w + updated_rect.x], screen->w * 4);
}

void sdl_video_renderer_present(SDL_Window* window, SDL_Rect texture_rect, SDL_Rect window_rect, SDL_Rect screen)
{
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, &texture_rect, &window_rect);
        int sshot = take_screenshot;
        if (!sshot)
        {
                if (video_focus_dim && !(SDL_GetWindowFlags(window)&SDL_WINDOW_INPUT_FOCUS)) {
                        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0x80);
                        SDL_RenderFillRect(renderer, NULL);
                        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xff);
                        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
                }
                if (flash.enabled)
                {
                        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                        SDL_SetRenderDrawColor(renderer, flash.color[0], flash.color[1], flash.color[2], flash.color[3]);
                        SDL_RenderFillRect(renderer, NULL);
                        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xff);
                        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
                }
        }
        SDL_RenderPresent(renderer);

        if (sshot)
        {
                take_screenshot = 0;

                int width = window_rect.w;
                int height = window_rect.h;

                SDL_GetWindowSize(window, &width, &height);

                /* seems to work without rendering to texture first */
//                SDL_Texture* tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_TARGET, width, height);
//
//                SDL_SetRenderTarget(renderer, tex);
//                SDL_RenderClear(renderer);
//                SDL_RenderCopy(renderer, texture, &texture_rect, &window_rect);
//                SDL_RenderPresent(renderer);

                unsigned char* rgba = (unsigned char*)malloc(width*height*4);
                int res = SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ABGR8888, rgba, width*4);

//                SDL_SetRenderTarget(renderer, NULL);
//                SDL_DestroyTexture(tex);

                if (!res)
                {
                        int x, y;
                        unsigned char* rgb = (unsigned char*)malloc(width*height*3);

                        for (x = 0; x < width; ++x)
                        {
                                for (y = 0; y < height; ++y)
                                {
                                        rgb[(y*width+x)*3+0] = rgba[(y*width+x)*4+0];
                                        rgb[(y*width+x)*3+1] = rgba[(y*width+x)*4+1];
                                        rgb[(y*width+x)*3+2] = rgba[(y*width+x)*4+2];
                                }
                        }

                        screenshot_taken(rgb, width, height);

                        free(rgb);
                }
                else
                        screenshot_taken(0, 0, 0);

                free(rgba);
        }

}

sdl_renderer_t* sdl2_renderer_create()
{
        sdl_renderer_t* renderer = malloc(sizeof(sdl_renderer_t));
        renderer->init = sdl_video_renderer_init;
        renderer->close = sdl_video_renderer_close;
        renderer->update = sdl_video_renderer_update;
        renderer->present = sdl_video_renderer_present;
        renderer->always_update = 0;
        return renderer;
}

void sdl2_renderer_close(sdl_renderer_t* renderer)
{
        free(renderer);
}

int sdl2_renderer_available(struct sdl_render_driver* driver)
{
        int i;
        SDL_RendererInfo renderInfo;
        for (i = 0; i < SDL_GetNumRenderDrivers(); ++i)
        {
                SDL_GetRenderDriverInfo(i, &renderInfo);
                if (!strcmp(driver->sdl_id, renderInfo.name))
                        return 1;
        }
        return 0;

}
