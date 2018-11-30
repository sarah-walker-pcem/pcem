#include <SDL2/SDL.h>
#include "video.h"
#include "wx-sdl2-video.h"
#include "wx-utils.h"
#include "ibm.h"
#include "wx-display.h"
#include "plat-keyboard.h"

#ifdef __WINDOWS__
#define BITMAP WINDOWS_BITMAP
#undef UNICODE
#include <windows.h>
#include <windowsx.h>
#undef BITMAP

HHOOK hKeyboardHook;
int modkeystate[255];

#endif

SDL_mutex* rendererMutex;
SDL_cond* rendererCond;
SDL_Thread* renderthread = NULL;

SDL_Window* window = NULL;

int rendering = 0;

int mousecapture = 0;

extern int pause;
extern int video_scale;
extern int take_screenshot;

void* ghwnd;
void* menu;

SDL_Rect remembered_rect;
int remembered_mouse_x = 0;
int remembered_mouse_y = 0;

int custom_resolution_width = 640;
int custom_resolution_height = 480;

int win_doresize = 0;
int winsizex = 640, winsizey = 480;

void renderer_start();
void renderer_stop(int timeout);

int trigger_fullscreen = 0;
int trigger_screenshot = 0;
int trigger_togglewindow = 0;
int trigger_inputrelease = 0;

extern void device_force_redraw();
extern void mouse_wheel_update(int);
extern void toggle_fullscreen();

void display_resize(int width, int height)
{
        winsizex = width*(video_scale+1) >> 1;
        winsizey = height*(video_scale+1) >> 1;

        SDL_Rect rect;
        rect.x = rect.y = 0;
        rect.w = winsizex;
        rect.h = winsizey;
        sdl_scale(video_fullscreen_scale, rect, &rect, winsizex, winsizey);
        winsizex = rect.w;
        winsizey = rect.h;

        win_doresize = 1;
}

void releasemouse()
{
        if (mousecapture)
        {
                SDL_SetWindowGrab(window, SDL_FALSE);
                SDL_SetRelativeMouseMode(SDL_FALSE);
                mousecapture = 0;
        }
}

int display_init()
{
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0)
        {
                printf("SDL could not initialize! Error: %s\n", SDL_GetError());
                return 0;
        }

        SDL_version ver;
        SDL_GetVersion(&ver);
        printf("SDL %i.%i.%i initialized.\n", ver.major, ver.minor, ver.patch);
        return 1;
}

void display_close()
{
        SDL_Quit();
}

void display_start(void* hwnd)
{
        ghwnd = hwnd;
        menu = wx_getmenu(hwnd);
        atexit(releasemouse);
        rendererMutex = SDL_CreateMutex();
        rendererCond = SDL_CreateCond();
        renderer_start();
}

void display_stop()
{
        renderer_stop(10 * 1000);

        SDL_DestroyMutex(rendererMutex);
        SDL_DestroyCond(rendererCond);
#if SDL_VERSION_ATLEAST(2, 0, 2)
        SDL_DetachThread(renderthread);
#endif
        releasemouse();
}

int is_fullscreen()
{
        int flags = SDL_GetWindowFlags(window);
        return (flags&SDL_WINDOW_FULLSCREEN) || (flags&SDL_WINDOW_FULLSCREEN_DESKTOP);
}

void sdl_set_window_title(const char* title) {
        if (window && !is_fullscreen())
                SDL_SetWindowTitle(window, title);
}

int get_border_size(int* top, int* left, int* bottom, int* right)
{
#if SDL_VERSION_ATLEAST(2, 0, 5)
        return SDL_GetWindowBordersSize(window, top, left, bottom, right);
#else
        if (top) *top = 0;
        if (left) *left = 0;
        if (bottom) *bottom = 0;
        if (right) *right = 0;
        return 0;
#endif
}

static const struct {
        SDL_Scancode sdl;
        int system;
} SDLScancodeToSystemScancode[] = {
                { SDL_SCANCODE_A, 0x1e },
                { SDL_SCANCODE_B, 0x30 },
                { SDL_SCANCODE_C, 0x2e },
                { SDL_SCANCODE_D, 0x20 },
                { SDL_SCANCODE_E, 0x12 },
                { SDL_SCANCODE_F, 0x21 },
                { SDL_SCANCODE_G, 0x22 },
                { SDL_SCANCODE_H, 0x23 },
                { SDL_SCANCODE_I, 0x17 },
                { SDL_SCANCODE_J, 0x24 },
                { SDL_SCANCODE_K, 0x25 },
                { SDL_SCANCODE_L, 0x26 },
                { SDL_SCANCODE_M, 0x32 },
                { SDL_SCANCODE_N, 0x31 },
                { SDL_SCANCODE_O, 0x18 },
                { SDL_SCANCODE_P, 0x19 },
                { SDL_SCANCODE_Q, 0x10 },
                { SDL_SCANCODE_R, 0x13 },
                { SDL_SCANCODE_S, 0x1f },
                { SDL_SCANCODE_T, 0x14 },
                { SDL_SCANCODE_U, 0x16 },
                { SDL_SCANCODE_V, 0x2f },
                { SDL_SCANCODE_W, 0x11 },
                { SDL_SCANCODE_X, 0x2d },
                { SDL_SCANCODE_Y, 0x15 },
                { SDL_SCANCODE_Z, 0x2c },
                { SDL_SCANCODE_0, 0x0B },
                { SDL_SCANCODE_1, 0x02 },
                { SDL_SCANCODE_2, 0x03 },
                { SDL_SCANCODE_3, 0x04 },
                { SDL_SCANCODE_4, 0x05 },
                { SDL_SCANCODE_5, 0x06 },
                { SDL_SCANCODE_6, 0x07 },
                { SDL_SCANCODE_7, 0x08 },
                { SDL_SCANCODE_8, 0x09 },
                { SDL_SCANCODE_9, 0x0A },
                { SDL_SCANCODE_GRAVE, 0x29 },
                { SDL_SCANCODE_MINUS, 0x0c },
                { SDL_SCANCODE_EQUALS, 0x0d },
                { SDL_SCANCODE_NONUSBACKSLASH, 0x56 },
                { SDL_SCANCODE_BACKSLASH, 0x2b },
                { SDL_SCANCODE_BACKSPACE, 0x0e },
                { SDL_SCANCODE_SPACE, 0x39 },
                { SDL_SCANCODE_TAB, 0x0f },
                { SDL_SCANCODE_CAPSLOCK, 0x3a },
                { SDL_SCANCODE_LSHIFT, 0x2a },
                { SDL_SCANCODE_LCTRL, 0x1d },
                { SDL_SCANCODE_LGUI, 0xdb },
                { SDL_SCANCODE_LALT, 0x38 },
                { SDL_SCANCODE_RSHIFT, 0x36 },
                { SDL_SCANCODE_RCTRL, 0x9d },
                { SDL_SCANCODE_RGUI, 0xdc },
                { SDL_SCANCODE_RALT, 0xb8 },
                { SDL_SCANCODE_SYSREQ, 0x54 },
                { SDL_SCANCODE_APPLICATION, 0xdd },
                { SDL_SCANCODE_RETURN, 0x1c },
                { SDL_SCANCODE_ESCAPE, 0x01 },
                { SDL_SCANCODE_F1, 0x3B },
                { SDL_SCANCODE_F2, 0x3C },
                { SDL_SCANCODE_F3, 0x3D },
                { SDL_SCANCODE_F4, 0x3e },
                { SDL_SCANCODE_F5, 0x3f },
                { SDL_SCANCODE_F6, 0x40 },
                { SDL_SCANCODE_F7, 0x41 },
                { SDL_SCANCODE_F8, 0x42 },
                { SDL_SCANCODE_F9, 0x43 },
                { SDL_SCANCODE_F10, 0x44 },
                { SDL_SCANCODE_F11, 0x57 },
                { SDL_SCANCODE_F12, 0x58 },
                { SDL_SCANCODE_SCROLLLOCK, 0x46 },
                { SDL_SCANCODE_LEFTBRACKET, 0x1a },
                { SDL_SCANCODE_RIGHTBRACKET, 0x1b },
                { SDL_SCANCODE_INSERT, 0xd2 },
                { SDL_SCANCODE_HOME, 0xc7 },
                { SDL_SCANCODE_PAGEUP, 0xc9 },
                { SDL_SCANCODE_DELETE, 0xd3 },
                { SDL_SCANCODE_END, 0xcf },
                { SDL_SCANCODE_PAGEDOWN, 0xd1 },
                { SDL_SCANCODE_UP, 0xc8 },
                { SDL_SCANCODE_LEFT, 0xcb },
                { SDL_SCANCODE_DOWN, 0xd0 },
                { SDL_SCANCODE_RIGHT, 0xcd },
                { SDL_SCANCODE_NUMLOCKCLEAR, 0x45 },
                { SDL_SCANCODE_KP_DIVIDE, 0xb5 },
                { SDL_SCANCODE_KP_MULTIPLY, 0x37 },
                { SDL_SCANCODE_KP_MINUS, 0x4a },
                { SDL_SCANCODE_KP_PLUS, 0x4e },
                { SDL_SCANCODE_KP_ENTER, 0x9c },
                { SDL_SCANCODE_KP_PERIOD, 0x53 },
                { SDL_SCANCODE_KP_0, 0x52 },
                { SDL_SCANCODE_KP_1, 0x4f },
                { SDL_SCANCODE_KP_2, 0x50 },
                { SDL_SCANCODE_KP_3, 0x51 },
                { SDL_SCANCODE_KP_4, 0x4b },
                { SDL_SCANCODE_KP_5, 0x4c },
                { SDL_SCANCODE_KP_6, 0x4d },
                { SDL_SCANCODE_KP_7, 0x47 },
                { SDL_SCANCODE_KP_8, 0x48 },
                { SDL_SCANCODE_KP_9, 0x49 },
                { SDL_SCANCODE_SEMICOLON, 0x27 },
                { SDL_SCANCODE_APOSTROPHE, 0x28 },
                { SDL_SCANCODE_COMMA, 0x33 },
                { SDL_SCANCODE_PERIOD, 0x34 },
                { SDL_SCANCODE_SLASH, 0x35 },
                { SDL_SCANCODE_PRINTSCREEN, 0xb7 }
};

int sdl_scancode(SDL_Scancode scancode)
{
        int i;
        for (i = 0; i < SDL_arraysize(SDLScancodeToSystemScancode); ++i) {
                if (SDLScancodeToSystemScancode[i].sdl == scancode) {
                        return SDLScancodeToSystemScancode[i].system;
                }
        }
        return -1;
}

SDL_Event event;
SDL_Rect rect;
int border_x, border_y = 0;

uint64_t render_time = 0;
int render_fps = 0;
uint32_t render_frame_time = 0;
uint32_t render_frames = 0;

void window_setup()
{
        SDL_SetHint(SDL_HINT_GRAB_KEYBOARD, "1");
#if SDL_VERSION_ATLEAST(2, 0, 5)
        SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
#endif

        if (start_in_fullscreen)
        {
                start_in_fullscreen = 0;
                window_dofullscreen = 1;
                window_doinputgrab = 1;
        }

        if (window_remember)
        {
                rect.x = window_x;
                rect.y = window_y;
                rect.w = window_w;
                rect.h = window_h;
        }
        else
        {
                rect.x = SDL_WINDOWPOS_CENTERED;
                rect.y = SDL_WINDOWPOS_CENTERED;
                rect.w = 640;
                rect.h = 480;
        }

        if (vid_resize == 2)
        {
                rect.w = custom_resolution_width;
                rect.h = custom_resolution_height;
        }
}

#ifdef __WINDOWS__
int sdl_winhook(int code)
{
        switch(code)
        {
                case VK_LMENU:
                        return SDL_SCANCODE_LALT;
                case VK_LCONTROL:
                        return SDL_SCANCODE_LCTRL;
                case VK_LWIN:
                        return SDL_SCANCODE_LGUI;
                case VK_LSHIFT:
                        return SDL_SCANCODE_LSHIFT;
                case VK_RMENU:
                        return SDL_SCANCODE_RALT;
                case VK_RCONTROL:
                        return SDL_SCANCODE_RCTRL;
                case VK_RWIN:
                        return SDL_SCANCODE_RGUI;
                case VK_RSHIFT:
                        return SDL_SCANCODE_RSHIFT;
        }

        return -1;
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
        KBDLLHOOKSTRUCT* p = (KBDLLHOOKSTRUCT*)lParam;

        int c = p->vkCode;

        int steal = 0;
        int s = sdl_winhook(c);
        if (s != -1)
        {
                int key_state = !(p->flags & LLKHF_UP);
                if (abs(modkeystate[c]) != key_state)
                {
                        /* if mousecapture is 0, key_state should be negative */
                        if (!mousecapture)
                                key_state = -key_state;
                        /* if key_state is 0 and modkeystate[c] is negative,
                         an sdl_event should not be generated */
                        if (key_state > 0 || modkeystate[c] > 0)
                        {
                                steal = key_state != 0;
                                SDL_Event event;
                                event.key.keysym.scancode = s;
                                event.key.timestamp = p->time;

                                event.type = key_state ? SDL_KEYDOWN : SDL_KEYUP;
                                SDL_PushEvent(&event);
                        }
                        modkeystate[c] = key_state;
                }
                if (steal)
                        return 1;
        }

        return CallNextHookEx( hKeyboardHook, nCode, wParam, lParam );
}
#endif

int window_create()
{
        window = SDL_CreateWindow("PCem Display",
                        rect.x, rect.y, rect.w, rect.h,
                        requested_render_driver.sdl_window_params | (vid_resize == 1 ? SDL_WINDOW_RESIZABLE : 0));
        if (!window)
        {
                char message[200];
                sprintf(message,
                                "SDL window could not be created! Error: %s\n",
                                SDL_GetError());
                wx_messagebox(ghwnd, message, "SDL Error", WX_MB_OK);
                return 0;
        }

#ifdef __WINDOWS__
        memset(modkeystate, 0, sizeof(modkeystate));
        hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL,  LowLevelKeyboardProc, GetModuleHandle(NULL), 0);
#endif

        render_time = 0;
        render_fps = 0;
        render_frame_time = SDL_GetTicks();
        render_frames = 0;
        return 1;
}

void window_close()
{
#ifdef __WINDOWS__
        UnhookWindowsHookEx( hKeyboardHook );
#endif

        sdl_renderer_close();

        if (window) {
                SDL_GetWindowPosition(window, &rect.x, &rect.y);
                SDL_GetWindowSize(window, &rect.w, &rect.h);
                get_border_size(&border_y, &border_x, 0, 0);
                rect.x -= border_x;
                rect.y -= border_y;

                SDL_DestroyWindow(window);
        }
        window = NULL;
}

int render()
{
        uint64_t start_time = timer_read();
        uint64_t end_time;

        if (window_dosetresize)
        {
                window_dosetresize = 0;
#if SDL_VERSION_ATLEAST(2, 0, 5)
                SDL_GetWindowSize(window, &rect.w, &rect.h);
                SDL_SetWindowResizable(window, vid_resize == 1);
                SDL_SetWindowSize(window, rect.w, rect.h);
#else
                window_doreset = 1;
#endif
                if (vid_resize == 2)
                        SDL_SetWindowSize(window, custom_resolution_width, custom_resolution_height);

                device_force_redraw();
        }
        if (window_doreset)
        {
                pclog("window_doreset\n");
                window_doreset = 0;
                renderer_doreset = 0;
                return 0;
        }
        if (renderer_doreset)
        {
                pclog("renderer_doreset\n");
                renderer_doreset = 0;
                sdl_renderer_close();
                sdl_renderer_init(window);

                device_force_redraw();
                video_wait_for_blit();
        }
        while(SDL_PollEvent(&event))
        {
                switch (event.type) {
                case SDL_MOUSEBUTTONUP:
                        if (!mousecapture)
                        {
                                if (event.button.button == SDL_BUTTON_LEFT && !pause)
                                {
                                        window_doinputgrab = 1;
                                        if (video_fullscreen)
                                                window_dofullscreen = 1;
                                }
                                else if (event.button.button == SDL_BUTTON_RIGHT)
                                        wx_popupmenu(ghwnd, menu, 0, 0);

                        }
                        else if (event.button.button == SDL_BUTTON_MIDDLE && !is_fullscreen())
                                window_doinputrelease = 1;
                        break;
                case SDL_MOUSEWHEEL:
                        if (mousecapture) mouse_wheel_update(event.wheel.y);
                        break;
                case SDL_WINDOWEVENT:
                        if (event.window.event == SDL_WINDOWEVENT_CLOSE)
                                wx_stop_emulation(ghwnd);
                        if (event.window.event == SDL_WINDOWEVENT_RESIZED)
                                device_force_redraw();
                        if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
                        {
                                if (is_fullscreen())
                                        window_dowindowed = 1;
                                window_doinputrelease = 1;
                        }

                        if (window_remember)
                        {
                                int flags = SDL_GetWindowFlags(window);
                                if (!(flags&SDL_WINDOW_FULLSCREEN) && !(flags&SDL_WINDOW_FULLSCREEN_DESKTOP))
                                {
                                        if (event.window.event == SDL_WINDOWEVENT_MOVED)
                                        {
                                                get_border_size(&border_y, &border_x, 0, 0);
                                                window_x = event.window.data1-border_x;
                                                window_y = event.window.data2-border_y;
                                        }
                                        else if (event.window.event == SDL_WINDOWEVENT_RESIZED)
                                        {
                                                window_w = event.window.data1;
                                                window_h = event.window.data2;
                                        }
                                        //save_window_pos = 1;
                                }
                        }

                        break;
                case SDL_KEYDOWN:
                {
#ifdef __WINDOWS__
                        /* international keyboard workaround */
                        if (event.key.keysym.scancode == SDL_SCANCODE_RALT &&
                                        (event.key.keysym.mod&KMOD_LCTRL) &&
                                        event.key.timestamp == rawinputkey[sdl_scancode(SDL_SCANCODE_LCTRL)])
                                rawinputkey[sdl_scancode(SDL_SCANCODE_LCTRL)] = 0;
#endif
                        int key_idx = sdl_scancode(event.key.keysym.scancode);
                        if (key_idx != -1)
                                rawinputkey[key_idx] = event.key.timestamp;
                        break;
                }
                case SDL_KEYUP:
                {
                        int key_idx = sdl_scancode(event.key.keysym.scancode);
                        if (key_idx != -1)
                                rawinputkey[key_idx] = 0;
                        break;
                }
                }
        }
        if (rawinputkey[sdl_scancode(SDL_SCANCODE_PAGEDOWN)] &&
                        (rawinputkey[sdl_scancode(SDL_SCANCODE_LCTRL)] || rawinputkey[sdl_scancode(SDL_SCANCODE_RCTRL)]) &&
                        (rawinputkey[sdl_scancode(SDL_SCANCODE_LALT)] || rawinputkey[sdl_scancode(SDL_SCANCODE_RALT)]))
                trigger_fullscreen = 1;
        else if (trigger_fullscreen)
        {
                trigger_fullscreen = 0;
                toggle_fullscreen();
        }
        else if (rawinputkey[sdl_scancode(SDL_SCANCODE_PAGEUP)] &&
                        (rawinputkey[sdl_scancode(SDL_SCANCODE_LCTRL)] || rawinputkey[sdl_scancode(SDL_SCANCODE_RCTRL)]) &&
                        (rawinputkey[sdl_scancode(SDL_SCANCODE_LALT)] || rawinputkey[sdl_scancode(SDL_SCANCODE_RALT)]))
                trigger_screenshot = 1;
        else if (trigger_screenshot)
        {
                trigger_screenshot = 0;
                take_screenshot = 1;
        }
        else if (event.key.keysym.scancode == SDL_SCANCODE_END &&
                        (rawinputkey[sdl_scancode(SDL_SCANCODE_LCTRL)] || rawinputkey[sdl_scancode(SDL_SCANCODE_RCTRL)]))
                trigger_inputrelease = 1;
        else if (trigger_inputrelease)
        {
                trigger_inputrelease = 0;
                if (!is_fullscreen())
                        window_doinputrelease = 1;
        }
        if (window_doremember)
        {
                window_doremember = 0;
                SDL_GetWindowPosition(window, &window_x, &window_y);
                SDL_GetWindowSize(window, &window_w, &window_h);
                get_border_size(&border_y, &border_x, 0, 0);
                window_x -= border_x;
                window_y -= border_y;
                saveconfig(NULL);
        }

        if (window_dotogglefullscreen)
        {
                window_dotogglefullscreen = 0;
                if (SDL_GetWindowGrab(window) || is_fullscreen())
                {
                        window_doinputrelease = 1;
                        if (is_fullscreen())
                                window_dowindowed = 1;
                }
                else
                {
                        window_doinputgrab = 1;
                        window_dofullscreen = 1;
                }
        }

        if (window_dofullscreen)
        {
                window_dofullscreen = 0;
                video_wait_for_blit();
                SDL_RaiseWindow(window);
#if SDL_VERSION_ATLEAST(2, 0, 4)
                SDL_GetGlobalMouseState(&remembered_mouse_x, &remembered_mouse_y);
#endif
                SDL_GetWindowPosition(window, &remembered_rect.x, &remembered_rect.y);
                get_border_size(&border_y, &border_x, 0, 0);
                remembered_rect.x -= border_x;
                remembered_rect.y -= border_y;
                SDL_GetWindowSize(window, &remembered_rect.w, &remembered_rect.h);
                SDL_SetWindowFullscreen(window, video_fullscreen_mode == 0 ? SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_FULLSCREEN);
                device_force_redraw();
        }
        if (window_doinputgrab) {
                window_doinputgrab = 0;
                mousecapture = 1;
                SDL_GetRelativeMouseState(0, 0);
                SDL_SetWindowGrab(window, SDL_TRUE);
                SDL_SetRelativeMouseMode(SDL_TRUE);
        }

        if (window_doinputrelease) {
                window_doinputrelease = 0;
                mousecapture = 0;
                SDL_SetWindowGrab(window, SDL_FALSE);
                SDL_SetRelativeMouseMode(SDL_FALSE);
        }
        if (window_dowindowed)
        {
                window_dowindowed = 0;
                SDL_SetWindowFullscreen(window, 0);
                SDL_SetWindowSize(window, remembered_rect.w, remembered_rect.h);
                SDL_SetWindowPosition(window, remembered_rect.x, remembered_rect.y);
#if SDL_VERSION_ATLEAST(2, 0, 4)
                SDL_WarpMouseGlobal(remembered_mouse_x, remembered_mouse_y);
#endif
                device_force_redraw();
        }

        if (win_doresize)
        {
		int flags = SDL_GetWindowFlags(window);

                win_doresize = 0;
                if (!vid_resize || (flags&SDL_WINDOW_FULLSCREEN)) {
                        SDL_GetWindowSize(window, &rect.w, &rect.h);
                        if (rect.w != winsizex || rect.h != winsizey) {
                                SDL_GetWindowPosition(window, &rect.x, &rect.y);
                                SDL_SetWindowSize(window, winsizex, winsizey);
                                SDL_SetWindowPosition(window, rect.x, rect.y);
                                device_force_redraw();
                        }
                }
        }

        if (sdl_renderer_update(window)) sdl_renderer_present(window);

        end_time = timer_read();
        render_time += end_time - start_time;

        ++render_frames;
        uint32_t ticks = SDL_GetTicks();
        if (ticks-render_frame_time >= 1000) {
                render_fps = render_frames/((ticks-render_frame_time)/1000.0);
                render_frames = 0;
                render_frame_time = ticks;
        }

        return 1;
}

int renderer_thread(void* params)
{
        int internal_rendering;

        SDL_LockMutex(rendererMutex);
        SDL_CondSignal(rendererCond);
        SDL_UnlockMutex(rendererMutex);

        window_setup();

        rendering = 1;
        while (rendering) {

                if (!window_create())
                        rendering = 0;

                renderer_doreset = 1;
                internal_rendering = 1;
                while (rendering && internal_rendering)
                {
                        if (!render())
                                internal_rendering = 0;

                        SDL_Delay(1);
                }
                window_close();
        }

        SDL_LockMutex(rendererMutex);
        SDL_CondSignal(rendererCond);
        SDL_UnlockMutex(rendererMutex);

        return SDL_TRUE;
}

void* timer = 0;

void render_timer()
{
#ifdef PCEM_RENDER_TIMER_LOOP
        /* For some reason this while-loop works on OSX, which also fixes missing events. No idea why though. */
        renderer_thread(0);
#else
        if (rendering && !render())
        {
                window_close();
                window_create();
                renderer_doreset = 1;
        }
#endif
}

void render_start_timer()
{
#ifdef PCEM_RENDER_TIMER_LOOP
        timer = wx_createtimer(render_timer);
        wx_starttimer(timer, 500, 1);
#else
        window_setup();
        if (window_create())
        {
                rendering = 1;
                renderer_doreset = 1;
                wx_starttimer(timer, 1, 0);
        }
#endif
}

void renderer_start()
{
        if (!rendering)
        {
#ifdef PCEM_RENDER_WITH_TIMER
                render_start_timer();
#else
                SDL_LockMutex(rendererMutex);
                renderthread = SDL_CreateThread(renderer_thread, "SDL2 Thread", NULL);
                SDL_CondWait(rendererCond, rendererMutex);
                SDL_UnlockMutex(rendererMutex);
#endif
        }
}

void renderer_stop(int timeout)
{
#if defined(PCEM_RENDER_WITH_TIMER) && !defined(PCEM_RENDER_TIMER_LOOP)
        rendering = 0;
        window_close();
        wx_destroytimer(timer);
#else
        if (rendering)
        {
                SDL_LockMutex(rendererMutex);
                rendering = 0;
                if (timeout)
                        SDL_CondWaitTimeout(rendererCond, rendererMutex, timeout);
                else
                        SDL_CondWait(rendererCond, rendererMutex);
                SDL_UnlockMutex(rendererMutex);
                renderthread = NULL;
        }
        if (timer)
        	wx_destroytimer(timer);
#endif
}
