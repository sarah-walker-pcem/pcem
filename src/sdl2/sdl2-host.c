#include <SDL2/SDL.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "sdl2-host.h"

static const sdl_host_t default_host = {0};
static const sdl_host_t *current_host = &default_host;

void sdl_host_set(const sdl_host_t *host) { current_host = host ? host : &default_host; }

const sdl_host_t *sdl_host_get(void) { return current_host; }

void sdl_host_message(const char *title, const char *format, ...) {
        char message[2048];
        va_list ap;

        va_start(ap, format);
        vsnprintf(message, sizeof(message), format, ap);
        va_end(ap);

        if (current_host->message) {
                current_host->message(title, message);
                return;
        }
        fprintf(stderr, "%s: %s\n", title, message);
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, message, NULL);
}

void sdl_host_ui_call(SDL_HOST_CALLBACK callback, void *data) {
        if (current_host->ui_call)
                current_host->ui_call(callback, data);
        else
                callback(data);
}

int sdl_host_image_save(const char *path, const char *name, const char *format, unsigned char *rgb, int width, int height) {
        char fn[1024];
        SDL_Surface *surface;
        size_t len;
        int res;

        if (current_host->image_save)
                return current_host->image_save(path, name, format, rgb, width, height);

        len = strlen(path);
        snprintf(fn, sizeof(fn), "%s%s%s.bmp", path, (len && (path[len - 1] == '/' || path[len - 1] == '\\')) ? "" : "/", name);
        surface = SDL_CreateRGBSurfaceWithFormatFrom(rgb, width, height, 24, width * 3, SDL_PIXELFORMAT_RGB24);
        if (!surface)
                return 0;
        res = SDL_SaveBMP(surface, fn) == 0;
        SDL_FreeSurface(surface);
        return res;
}

unsigned char *sdl_host_image_load(const char *path, int *width, int *height) {
        SDL_Surface *surface, *converted;
        unsigned char *data;
        int y;

        if (current_host->image_load)
                return current_host->image_load(path, width, height);

        surface = SDL_LoadBMP(path);
        if (!surface)
                return NULL;
        converted = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
        SDL_FreeSurface(surface);
        if (!converted)
                return NULL;

        data = malloc(converted->w * converted->h * 4);
        if (data) {
                SDL_LockSurface(converted);
                for (y = 0; y < converted->h; y++)
                        memcpy(data + y * converted->w * 4, (unsigned char *)converted->pixels + y * converted->pitch,
                               converted->w * 4);
                SDL_UnlockSurface(converted);
                *width = converted->w;
                *height = converted->h;
        }
        SDL_FreeSurface(converted);
        return data;
}

void sdl_host_date_format(char *s, int size, const char *format) {
        time_t now = time(NULL);
        struct tm *tm = localtime(&now);

        if (!tm || !strftime(s, size, format, tm))
                s[0] = 0;
}
