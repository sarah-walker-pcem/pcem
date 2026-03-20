/*
 * Unified Qt SDL2 display implementation.
 *
 * Instead of creating a separate native window (HWND on Windows) or an independent
 * SDL window (Linux/Mac), this embeds the SDL rendering surface inside a QWidget
 * that is the central widget of the QMainWindow.
 *
 * The QMainWindow provides the single menu bar, toolbar, and status bar.
 * SDL_CreateWindowFrom() is used with the QWidget's native window handle.
 */

#include <SDL2/SDL.h>

extern "C" {
#include "video.h"
#include "qt-sdl2-video.h"
#include "qt-utils.h"
#include "ibm.h"
#include "qt-display.h"
#include "plat-keyboard.h"
#include "mouse.h"
}

#include "qt-app.h"

#include <QApplication>
#include <QWidget>
#include <QSize>
#include <QCursor>
#include <QPoint>

#ifdef _WIN32
#define BITMAP WINDOWS_BITMAP
#undef UNICODE
#include <windows.h>
#include <windowsx.h>
#undef BITMAP
#endif

SDL_mutex *rendererMutex = NULL;
SDL_cond *rendererCond = NULL;
SDL_Thread *renderthread = NULL;

SDL_Window *window = NULL;
SDL_Window *dummy_window = NULL;

int rendering = 0;

int mousecapture = 0;

extern "C" {
extern volatile int pause;
extern int video_scale;
extern int take_screenshot;
}

static MainWindow *mainWindowPtr = NULL;

SDL_Rect remembered_rect = {0};
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

extern "C" {
void device_force_redraw();
void mouse_wheel_update(int);
void toggle_fullscreen();
void qt_mouse_motion(int dx, int dy);
void qt_mouse_set_buttons(int buttons);
}

#ifdef _WIN32
static HHOOK ll_keyboard_hook = NULL;

static int ll_vkey_to_scancode(DWORD vkCode, DWORD scanCode, DWORD flags) {
        int sc = scanCode;
        int e0 = (flags & LLKHF_EXTENDED) ? 1 : 0;

        if (e0) {
                switch (sc) {
                case 0x1c: return 0x9c;
                case 0x1d: return 0x9d;
                case 0x35: return 0xb5;
                case 0x38: return 0xb8;
                case 0x47: return 0xc7;
                case 0x48: return 0xc8;
                case 0x49: return 0xc9;
                case 0x4b: return 0xcb;
                case 0x4d: return 0xcd;
                case 0x4f: return 0xcf;
                case 0x50: return 0xd0;
                case 0x51: return 0xd1;
                case 0x52: return 0xd2;
                case 0x53: return 0xd3;
                case 0x5b: return 0xdb;
                case 0x5c: return 0xdc;
                case 0x5d: return 0xdd;
                case 0x37: return 0xb7;
                default: return sc | 0x80;
                }
        }
        return sc;
}

static LRESULT CALLBACK ll_keyboard_proc(int nCode, WPARAM wParam, LPARAM lParam) {
        if (nCode == HC_ACTION && mousecapture) {
                KBDLLHOOKSTRUCT *kb = (KBDLLHOOKSTRUCT *)lParam;
                int sc = ll_vkey_to_scancode(kb->vkCode, kb->scanCode, kb->flags);
                int pressed = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);

                if (sc >= 0 && sc < 272) {
                        rawinputkey[sc] = pressed;

                        /* Ctrl+Alt+End releases capture */
                        if (pressed && sc == 0xcf &&
                            rawinputkey[0x1d] && rawinputkey[0x38]) {
                                extern int window_doinputrelease;
                                window_doinputrelease = 1;
                        }
                }
                /* Block all system keys from reaching the OS */
                return 1;
        }
        return CallNextHookEx(ll_keyboard_hook, nCode, wParam, lParam);
}

static void rawinput_register(int enable) {
        if (enable && !ll_keyboard_hook) {
                ll_keyboard_hook = SetWindowsHookExW(WH_KEYBOARD_LL, ll_keyboard_proc,
                        GetModuleHandleW(NULL), 0);

                /* Register for raw mouse input — gives unaccelerated deltas
                   without cursor repositioning jitter */
                RAWINPUTDEVICE rid;
                rid.usUsagePage = 0x01;
                rid.usUsage = 0x02; /* Mouse */
                rid.dwFlags = 0;
                rid.hwndTarget = NULL;
                RegisterRawInputDevices(&rid, 1, sizeof(rid));
        } else if (!enable && ll_keyboard_hook) {
                UnhookWindowsHookEx(ll_keyboard_hook);
                ll_keyboard_hook = NULL;

                RAWINPUTDEVICE rid;
                rid.usUsagePage = 0x01;
                rid.usUsage = 0x02;
                rid.dwFlags = RIDEV_REMOVE;
                rid.hwndTarget = NULL;
                RegisterRawInputDevices(&rid, 1, sizeof(rid));
        }
}
#endif /* _WIN32 */

extern "C" void display_resize(int width, int height) {
        winsizex = width * (video_scale + 1) >> 1;
        winsizey = height * (video_scale + 1) >> 1;

        SDL_Rect rect;
        rect.x = rect.y = 0;
        rect.w = winsizex;
        rect.h = winsizey;
        sdl_scale(video_fullscreen_scale, rect, &rect, winsizex, winsizey);
        winsizex = rect.w;
        winsizey = rect.h;

        win_doresize = 1;
}

extern "C" void releasemouse() {
        if (mousecapture) {
                mousecapture = 0;

#ifdef _WIN32
                rawinput_register(0);
                ClipCursor(NULL);
                SetCursor(LoadCursor(NULL, IDC_ARROW));
#else
                QMetaObject::invokeMethod(mainWindowPtr, []() {
                        if (mainWindowPtr && mainWindowPtr->sdlCanvas()) {
                                mainWindowPtr->sdlCanvas()->releaseMouse();
                                mainWindowPtr->sdlCanvas()->setCursor(Qt::ArrowCursor);
                        }
                }, Qt::QueuedConnection);
#endif

                memset(rawinputkey, 0, sizeof(rawinputkey));
        }
}

extern "C" int is_fullscreen() {
        if (window) {
                int flags = SDL_GetWindowFlags(window);
                return (flags & SDL_WINDOW_FULLSCREEN) || (flags & SDL_WINDOW_FULLSCREEN_DESKTOP);
        }
        return 0;
}

extern "C" int display_init() {
#ifdef _WIN32
        SDL_SetHint(SDL_HINT_WINDOWS_DISABLE_THREAD_NAMING, "1");
#endif
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
                printf("SDL could not initialize! Error: %s\n", SDL_GetError());
                return 0;
        }

        SDL_version ver;
        SDL_GetVersion(&ver);
        printf("SDL %i.%i.%i initialized.\n", ver.major, ver.minor, ver.patch);

        return 1;
}

extern "C" void display_close() { SDL_Quit(); }

extern "C" void display_start(void *wnd_ptr) {
        mainWindowPtr = static_cast<MainWindow *>(wnd_ptr);
        pclog("display_start: mainWindowPtr=%p, sdlCanvas=%p\n",
                (void *)mainWindowPtr, mainWindowPtr ? (void *)mainWindowPtr->sdlCanvas() : nullptr);

        infocus = 1;

        atexit(releasemouse);
        rendererMutex = SDL_CreateMutex();
        rendererCond = SDL_CreateCond();
        pclog("display_start: calling renderer_start\n");
        renderer_start();
        pclog("display_start: renderer_start returned\n");
}

extern "C" void display_stop() {
        renderer_stop(10 * 1000);

        SDL_DestroyMutex(rendererMutex);
        SDL_DestroyCond(rendererCond);
        SDL_DetachThread(renderthread);
        releasemouse();
}

extern "C" void sdl_set_window_title(const char *title) {
        if (window && !is_fullscreen())
                SDL_SetWindowTitle(window, title);
}

extern "C" int get_border_size(int *top, int *left, int *bottom, int *right) {
        if (top) *top = 0;
        if (left) *left = 0;
        if (bottom) *bottom = 0;
        if (right) *right = 0;
        return 0;
}

static const struct {
        SDL_Scancode sdl;
        int system;
} SDLScancodeToSystemScancode[] = {
        {SDL_SCANCODE_A, 0x1e}, {SDL_SCANCODE_B, 0x30}, {SDL_SCANCODE_C, 0x2e},
        {SDL_SCANCODE_D, 0x20}, {SDL_SCANCODE_E, 0x12}, {SDL_SCANCODE_F, 0x21},
        {SDL_SCANCODE_G, 0x22}, {SDL_SCANCODE_H, 0x23}, {SDL_SCANCODE_I, 0x17},
        {SDL_SCANCODE_J, 0x24}, {SDL_SCANCODE_K, 0x25}, {SDL_SCANCODE_L, 0x26},
        {SDL_SCANCODE_M, 0x32}, {SDL_SCANCODE_N, 0x31}, {SDL_SCANCODE_O, 0x18},
        {SDL_SCANCODE_P, 0x19}, {SDL_SCANCODE_Q, 0x10}, {SDL_SCANCODE_R, 0x13},
        {SDL_SCANCODE_S, 0x1f}, {SDL_SCANCODE_T, 0x14}, {SDL_SCANCODE_U, 0x16},
        {SDL_SCANCODE_V, 0x2f}, {SDL_SCANCODE_W, 0x11}, {SDL_SCANCODE_X, 0x2d},
        {SDL_SCANCODE_Y, 0x15}, {SDL_SCANCODE_Z, 0x2c},
        {SDL_SCANCODE_0, 0x0B}, {SDL_SCANCODE_1, 0x02}, {SDL_SCANCODE_2, 0x03},
        {SDL_SCANCODE_3, 0x04}, {SDL_SCANCODE_4, 0x05}, {SDL_SCANCODE_5, 0x06},
        {SDL_SCANCODE_6, 0x07}, {SDL_SCANCODE_7, 0x08}, {SDL_SCANCODE_8, 0x09},
        {SDL_SCANCODE_9, 0x0A},
        {SDL_SCANCODE_GRAVE, 0x29}, {SDL_SCANCODE_MINUS, 0x0c}, {SDL_SCANCODE_EQUALS, 0x0d},
        {SDL_SCANCODE_NONUSBACKSLASH, 0x56}, {SDL_SCANCODE_BACKSLASH, 0x2b},
        {SDL_SCANCODE_BACKSPACE, 0x0e}, {SDL_SCANCODE_SPACE, 0x39}, {SDL_SCANCODE_TAB, 0x0f},
        {SDL_SCANCODE_CAPSLOCK, 0x3a}, {SDL_SCANCODE_LSHIFT, 0x2a}, {SDL_SCANCODE_LCTRL, 0x1d},
        {SDL_SCANCODE_LGUI, 0xdb}, {SDL_SCANCODE_LALT, 0x38}, {SDL_SCANCODE_RSHIFT, 0x36},
        {SDL_SCANCODE_RCTRL, 0x9d}, {SDL_SCANCODE_RGUI, 0xdc}, {SDL_SCANCODE_RALT, 0xb8},
        {SDL_SCANCODE_SYSREQ, 0x54}, {SDL_SCANCODE_APPLICATION, 0xdd},
        {SDL_SCANCODE_RETURN, 0x1c}, {SDL_SCANCODE_ESCAPE, 0x01},
        {SDL_SCANCODE_F1, 0x3B}, {SDL_SCANCODE_F2, 0x3C}, {SDL_SCANCODE_F3, 0x3D},
        {SDL_SCANCODE_F4, 0x3e}, {SDL_SCANCODE_F5, 0x3f}, {SDL_SCANCODE_F6, 0x40},
        {SDL_SCANCODE_F7, 0x41}, {SDL_SCANCODE_F8, 0x42}, {SDL_SCANCODE_F9, 0x43},
        {SDL_SCANCODE_F10, 0x44}, {SDL_SCANCODE_F11, 0x57}, {SDL_SCANCODE_F12, 0x58},
        {SDL_SCANCODE_SCROLLLOCK, 0x46},
        {SDL_SCANCODE_LEFTBRACKET, 0x1a}, {SDL_SCANCODE_RIGHTBRACKET, 0x1b},
        {SDL_SCANCODE_INSERT, 0xd2}, {SDL_SCANCODE_HOME, 0xc7}, {SDL_SCANCODE_PAGEUP, 0xc9},
        {SDL_SCANCODE_DELETE, 0xd3}, {SDL_SCANCODE_END, 0xcf}, {SDL_SCANCODE_PAGEDOWN, 0xd1},
        {SDL_SCANCODE_UP, 0xc8}, {SDL_SCANCODE_LEFT, 0xcb}, {SDL_SCANCODE_DOWN, 0xd0},
        {SDL_SCANCODE_RIGHT, 0xcd}, {SDL_SCANCODE_NUMLOCKCLEAR, 0x45},
        {SDL_SCANCODE_KP_DIVIDE, 0xb5}, {SDL_SCANCODE_KP_MULTIPLY, 0x37},
        {SDL_SCANCODE_KP_MINUS, 0x4a}, {SDL_SCANCODE_KP_PLUS, 0x4e},
        {SDL_SCANCODE_KP_ENTER, 0x9c}, {SDL_SCANCODE_KP_PERIOD, 0x53},
        {SDL_SCANCODE_KP_0, 0x52}, {SDL_SCANCODE_KP_1, 0x4f}, {SDL_SCANCODE_KP_2, 0x50},
        {SDL_SCANCODE_KP_3, 0x51}, {SDL_SCANCODE_KP_4, 0x4b}, {SDL_SCANCODE_KP_5, 0x4c},
        {SDL_SCANCODE_KP_6, 0x4d}, {SDL_SCANCODE_KP_7, 0x47}, {SDL_SCANCODE_KP_8, 0x48},
        {SDL_SCANCODE_KP_9, 0x49},
        {SDL_SCANCODE_SEMICOLON, 0x27}, {SDL_SCANCODE_APOSTROPHE, 0x28},
        {SDL_SCANCODE_COMMA, 0x33}, {SDL_SCANCODE_PERIOD, 0x34}, {SDL_SCANCODE_SLASH, 0x35},
        {SDL_SCANCODE_PRINTSCREEN, 0xb7}
};

extern "C" int sdl_scancode(SDL_Scancode scancode) {
        int i;
        for (i = 0; i < SDL_arraysize(SDLScancodeToSystemScancode); ++i) {
                if (SDLScancodeToSystemScancode[i].sdl == scancode)
                        return SDLScancodeToSystemScancode[i].system;
        }
        return -1;
}

SDL_Event event = {0};
SDL_Rect rect = {0};

uint64_t render_time = 0;
int render_fps = 0;
uint32_t render_frame_time = 0;
uint32_t render_frames = 0;

void window_setup() {
        SDL_SetHint(SDL_HINT_GRAB_KEYBOARD, "0");
        SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
#ifdef _WIN32
        SDL_SetHint(SDL_HINT_WINDOWS_NO_CLOSE_ON_ALT_F4, "1");
#endif

        if (start_in_fullscreen) {
                start_in_fullscreen = 0;
                window_dofullscreen = 1;
                window_doinputgrab = 1;
        }

        if (window_remember) {
                rect.x = window_x;
                rect.y = window_y;
                rect.w = window_w;
                rect.h = window_h;
        } else {
                rect.x = SDL_WINDOWPOS_CENTERED;
                rect.y = SDL_WINDOWPOS_CENTERED;
                rect.w = 640;
                rect.h = 480;
        }

        if (vid_resize == 2) {
                rect.w = custom_resolution_width;
                rect.h = custom_resolution_height;
        }
}

int window_create() {
        pclog("window_create: mainWindowPtr=%p\n", (void *)mainWindowPtr);
        if (!mainWindowPtr || !mainWindowPtr->sdlCanvas()) {
                pclog("window_create: FAILED - no mainWindow or canvas\n");
                return 0;
        }

        QWidget *canvas = mainWindowPtr->sdlCanvas();
        pclog("window_create: canvas=%p, getting winId...\n", (void *)canvas);

        /* Ensure the widget has a native window handle */
        WId cachedWinId = canvas->winId();
        pclog("window_create: winId=%p\n", (void *)cachedWinId);

        if (requested_render_driver.sdl_window_params & SDL_WINDOW_OPENGL) {
                dummy_window = SDL_CreateWindow("GL3 test", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1, 1,
                                                SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL);
                if (dummy_window) {
                        char ptr[32];
                        snprintf(ptr, 31, "%p", dummy_window);
                        ptr[31] = 0;
                        SDL_SetHint(SDL_HINT_VIDEO_WINDOW_SHARE_PIXEL_FORMAT, ptr);
                }
        }

        /* Create SDL window from the Qt widget's native handle */
        pclog("window_create: calling SDL_CreateWindowFrom with winId=%p\n", (void *)cachedWinId);
        window = SDL_CreateWindowFrom((void *)cachedWinId);
        if (!window) {
                pclog("window_create: SDL_CreateWindowFrom FAILED: %s\n", SDL_GetError());
                return 0;
        }
        pclog("window_create: SDL window created OK\n");

        SDL_SetWindowSize(window, rect.w, rect.h);

        /* Keyboard input is handled by SDLCanvas::keyPressEvent/keyReleaseEvent in qt-app.cc */

        if (vid_resize)
                window_dosetresize = 1;

        render_time = 0;
        render_fps = 0;
        render_frame_time = SDL_GetTicks();
        render_frames = 0;
        return 1;
}

void window_close() {
        sdl_renderer_close();

        if (window) {
                SDL_GetWindowPosition(window, &rect.x, &rect.y);
                SDL_GetWindowSize(window, &rect.w, &rect.h);
                if (window_remember) {
                        window_x = rect.x;
                        window_y = rect.y;
                }
                SDL_DestroyWindow(window);
        }
        window = NULL;

        if (dummy_window) {
                SDL_SetHint(SDL_HINT_VIDEO_WINDOW_SHARE_PIXEL_FORMAT, "");
                SDL_DestroyWindow(dummy_window);
        }
        dummy_window = NULL;

        /* Keyboard hooks removed - using Qt key events instead */
}

int render() {
        uint64_t start_time = timer_read();
        uint64_t end_time;

        if (window_doreset) {
                pclog("window_doreset\n");
                window_doreset = 0;
                renderer_doreset = 0;
                return 0;
        }
        if (window_dosetresize) {
                window_dosetresize = 0;

                int neww = rect.w, newh = rect.h;
                if (vid_resize == 2) {
                        neww = custom_resolution_width;
                        newh = custom_resolution_height;
                }

                /* Resize Qt widget from the main thread */
                QMetaObject::invokeMethod(mainWindowPtr, [neww, newh]() {
                        if (mainWindowPtr && mainWindowPtr->sdlCanvas()) {
                                mainWindowPtr->sdlCanvas()->setMinimumSize(neww, newh);
                                mainWindowPtr->sdlCanvas()->setMaximumSize(
                                        vid_resize == 1 ? QSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX) : QSize(neww, newh));
                                mainWindowPtr->sdlCanvas()->resize(neww, newh);
                                mainWindowPtr->adjustSize();
                        }
                }, Qt::QueuedConnection);

                device_force_redraw();
        }
        if (renderer_doreset) {
                pclog("renderer_doreset: closing old renderer\n");
                renderer_doreset = 0;
                sdl_renderer_close();
                pclog("renderer_doreset: calling sdl_renderer_init\n");
                int rinit = sdl_renderer_init(window);
                pclog("renderer_doreset: sdl_renderer_init returned %d\n", rinit);

                device_force_redraw();
                video_wait_for_blit();
        }
        while (SDL_PollEvent(&event)) {
                switch (event.type) {
                case SDL_MOUSEBUTTONUP:
                        if (!mousecapture) {
                                if (event.button.button == SDL_BUTTON_LEFT && !pause) {
                                        window_doinputgrab = 1;
                                        if (video_fullscreen)
                                                window_dofullscreen = 1;
                                } else if (event.button.button == SDL_BUTTON_RIGHT) {
                                        /* Right-click popup menu via Qt */
                                        QMenu *menu = mainWindowPtr->getMenu();
                                        if (menu)
                                                wx_popupmenu(mainWindowPtr, menu, 0, 0);
                                }
                        }
                        break;
                case SDL_MOUSEWHEEL:
                        if (mousecapture)
                                mouse_wheel_update(event.wheel.y);
                        break;
                case SDL_WINDOWEVENT:
                        if (event.window.event == SDL_WINDOWEVENT_CLOSE)
                                wx_stop_emulation(mainWindowPtr);
                        if (event.window.event == SDL_WINDOWEVENT_RESIZED)
                                device_force_redraw();

                        if (window_remember) {
                                int flags = SDL_GetWindowFlags(window);
                                if (!(flags & SDL_WINDOW_FULLSCREEN) && !(flags & SDL_WINDOW_FULLSCREEN_DESKTOP)) {
                                        if (event.window.event == SDL_WINDOWEVENT_MOVED) {
                                                window_x = event.window.data1;
                                                window_y = event.window.data2;
                                        } else if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                                                window_w = event.window.data1;
                                                window_h = event.window.data2;
                                        }
                                }
                        }
                        break;
                case SDL_KEYDOWN: {
                        int key_idx = sdl_scancode(event.key.keysym.scancode);
                        if (key_idx != -1)
                                rawinputkey[key_idx] = event.key.timestamp;
                        break;
                }
                case SDL_KEYUP: {
                        int key_idx = sdl_scancode(event.key.keysym.scancode);
                        if (key_idx != -1)
                                rawinputkey[key_idx] = 0;
                        break;
                }
                }
        }

        /* Hotkey handling */
        if ((rawinputkey[sdl_scancode(SDL_SCANCODE_PAGEDOWN)] || rawinputkey[sdl_scancode(SDL_SCANCODE_KP_3)]) &&
            (rawinputkey[sdl_scancode(SDL_SCANCODE_LCTRL)] || rawinputkey[sdl_scancode(SDL_SCANCODE_RCTRL)]) &&
            (rawinputkey[sdl_scancode(SDL_SCANCODE_LALT)] || rawinputkey[sdl_scancode(SDL_SCANCODE_RALT)]))
                trigger_fullscreen = 1;
        else if (trigger_fullscreen) {
                trigger_fullscreen = 0;
                toggle_fullscreen();
        } else if ((rawinputkey[sdl_scancode(SDL_SCANCODE_PAGEUP)] || rawinputkey[sdl_scancode(SDL_SCANCODE_KP_9)]) &&
                   (rawinputkey[sdl_scancode(SDL_SCANCODE_LCTRL)] || rawinputkey[sdl_scancode(SDL_SCANCODE_RCTRL)]) &&
                   (rawinputkey[sdl_scancode(SDL_SCANCODE_LALT)] || rawinputkey[sdl_scancode(SDL_SCANCODE_RALT)]))
                trigger_screenshot = 1;
        else if (trigger_screenshot) {
                trigger_screenshot = 0;
                take_screenshot = 1;
        } else if ((rawinputkey[sdl_scancode(SDL_SCANCODE_END)] || rawinputkey[sdl_scancode(SDL_SCANCODE_KP_1)]) &&
                   (rawinputkey[sdl_scancode(SDL_SCANCODE_LCTRL)] || rawinputkey[sdl_scancode(SDL_SCANCODE_RCTRL)]))
                trigger_inputrelease = 1;
        else if (trigger_inputrelease) {
                trigger_inputrelease = 0;
                if (!is_fullscreen())
                        window_doinputrelease = 1;
        }

        if (window_doremember) {
                window_doremember = 0;
                SDL_GetWindowPosition(window, &window_x, &window_y);
                SDL_GetWindowSize(window, &window_w, &window_h);
                saveconfig(NULL);
        }

        if (window_dotogglefullscreen) {
                window_dotogglefullscreen = 0;
                if (SDL_GetWindowGrab(window) || is_fullscreen()) {
                        window_doinputrelease = 1;
                        if (is_fullscreen())
                                window_dowindowed = 1;
                } else {
                        window_doinputgrab = 1;
                        window_dofullscreen = 1;
                }
        }

        if (window_dofullscreen) {
                window_dofullscreen = 0;
                SDL_GetWindowPosition(window, &remembered_rect.x, &remembered_rect.y);
                SDL_GetWindowSize(window, &remembered_rect.w, &remembered_rect.h);
                video_wait_for_blit();
                SDL_RaiseWindow(window);
                SDL_GetGlobalMouseState(&remembered_mouse_x, &remembered_mouse_y);
                SDL_SetWindowFullscreen(window,
                                        video_fullscreen_mode == 0 ? SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_FULLSCREEN);
                device_force_redraw();
        }
        if (window_doinputgrab) {
                window_doinputgrab = 0;
                mousecapture = 1;

#ifdef _WIN32
                rawinput_register(1);
                /* Lock cursor to single pixel at canvas center */
                {
                        HWND hwnd = (HWND)mainWindowPtr->sdlCanvas()->winId();
                        RECT wr;
                        GetWindowRect(hwnd, &wr);
                        int cx = (wr.left + wr.right) / 2;
                        int cy = (wr.top + wr.bottom) / 2;
                        RECT clip;
                        clip.left = cx;
                        clip.top = cy;
                        clip.right = cx + 1;
                        clip.bottom = cy + 1;
                        SetCursorPos(cx, cy);
                        ClipCursor(&clip);
                        SetCursor(NULL);
                }
#else
                /* Non-Windows: fall back to Qt grab */
                QMetaObject::invokeMethod(mainWindowPtr, []() {
                        if (mainWindowPtr && mainWindowPtr->sdlCanvas()) {
                                QWidget *canvas = mainWindowPtr->sdlCanvas();
                                canvas->setCursor(Qt::BlankCursor);
                                canvas->grabMouse();
                        }
                }, Qt::QueuedConnection);
#endif
        }

        if (window_doinputrelease) {
                window_doinputrelease = 0;
                mousecapture = 0;

#ifdef _WIN32
                rawinput_register(0);
                ClipCursor(NULL);
                SetCursor(LoadCursor(NULL, IDC_ARROW));
#else
                QMetaObject::invokeMethod(mainWindowPtr, []() {
                        if (mainWindowPtr && mainWindowPtr->sdlCanvas()) {
                                mainWindowPtr->sdlCanvas()->releaseMouse();
                                mainWindowPtr->sdlCanvas()->setCursor(Qt::ArrowCursor);
                        }
                }, Qt::QueuedConnection);
#endif

                memset(rawinputkey, 0, sizeof(rawinputkey));
        }
        if (window_dowindowed) {
                window_dowindowed = 0;
                SDL_SetWindowFullscreen(window, 0);
                SDL_SetWindowSize(window, remembered_rect.w, remembered_rect.h);
                SDL_SetWindowPosition(window, remembered_rect.x, remembered_rect.y);
                SDL_WarpMouseGlobal(remembered_mouse_x, remembered_mouse_y);
                device_force_redraw();
        }

        if (win_doresize) {
                win_doresize = 0;
                if (!vid_resize) {
                        int neww = winsizex, newh = winsizey;
                        /* Resize Qt widget from the main thread */
                        QMetaObject::invokeMethod(mainWindowPtr, [neww, newh]() {
                                if (mainWindowPtr && mainWindowPtr->sdlCanvas()) {
                                        mainWindowPtr->sdlCanvas()->setMinimumSize(neww, newh);
                                        mainWindowPtr->sdlCanvas()->setMaximumSize(neww, newh);
                                        mainWindowPtr->sdlCanvas()->resize(neww, newh);
                                        mainWindowPtr->adjustSize();
                                }
                        }, Qt::QueuedConnection);
                        device_force_redraw();
                }
        }

        if (sdl_renderer_update(window))
                sdl_renderer_present(window);

        end_time = timer_read();
        render_time += end_time - start_time;

        ++render_frames;
        uint32_t ticks = SDL_GetTicks();
        if (ticks - render_frame_time >= 1000) {
                render_fps = render_frames / ((ticks - render_frame_time) / 1000.0);
                render_frames = 0;
                render_frame_time = ticks;
        }

        return 1;
}

int renderer_thread(void *params) {
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
                while (rendering && internal_rendering) {
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

void *timer = 0;

void render_timer() {
#ifdef PCEM_RENDER_TIMER_LOOP
        renderer_thread(0);
#else
        if (rendering && !render()) {
                window_close();
                window_create();
                renderer_doreset = 1;
        }
#endif
}

void render_start_timer() {
#ifdef PCEM_RENDER_TIMER_LOOP
        timer = wx_createtimer(render_timer);
        wx_starttimer(timer, 500, 1);
#else
        window_setup();
        if (window_create()) {
                rendering = 1;
                renderer_doreset = 1;
                wx_starttimer(timer, 1, 0);
        }
#endif
}

void renderer_start() {
        if (!rendering) {
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

void renderer_stop(int timeout) {
#if defined(PCEM_RENDER_WITH_TIMER) && !defined(PCEM_RENDER_TIMER_LOOP)
        rendering = 0;
        window_close();
        wx_destroytimer(timer);
#else
        if (rendering) {
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
