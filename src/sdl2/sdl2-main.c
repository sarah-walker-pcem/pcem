#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#define BITMAP WINDOWS_BITMAP
#undef UNICODE
#include <windows.h>
#undef BITMAP
#endif

#include "ibm.h"
#include "config.h"
#include "sdl2-emu.h"
#include "sdl2-host.h"

static void (*render_loop)(void) = NULL;

static void standalone_start_render_loop(void (*loop)(void)) { render_loop = loop; }

static const sdl_host_t standalone_host = {.start_render_loop = standalone_start_render_loop};

static int run(int argc, char **argv) {
        SDL_SetMainReady();
        sdl_host_set(&standalone_host);

        if (!pc_main(argc, argv))
                return -1;

        start_emulation(NULL);
        if (render_loop)
                render_loop();
        stop_emulation();
        emulator_shutdown();
        return 0;
}

#ifdef _WIN32
int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, char *cmdline, int nShowCmd) { return run(__argc, __argv); }
#else
int main(int argc, char **argv) { return run(argc, argv); }
#endif
