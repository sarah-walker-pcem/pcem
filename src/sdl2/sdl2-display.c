#ifdef _WIN32
#define _WIN32_WINNT 0x0501
#endif
#include <SDL2/SDL.h>
#include "video.h"
#include "sdl2-video.h"
#include "sdl2-host.h"
#include "sdl2-emu.h"
#include "ibm.h"
#include "mouse.h"
#include "sdl2-display.h"
#include "plat-keyboard.h"
#include "config.h"
#include "model.h"

#ifdef _WIN32
#define BITMAP WINDOWS_BITMAP
#undef UNICODE
#include <windows.h>
#include <windowsx.h>
#undef BITMAP
#endif

#include <pcem/api.h>

#ifdef __APPLE__
#include <objc/message.h>
#include <objc/runtime.h>
#endif

#if (defined(__linux__) || defined(__FreeBSD__)) && defined(PCEM_SDL_X11)
#define PCEM_DISPLAY_X11
#include <SDL2/SDL_syswm.h>
#include <X11/Xlib.h>
#endif

SDL_mutex *rendererMutex;
SDL_cond *rendererCond;
SDL_Thread *renderthread = NULL;

SDL_Window *window = NULL;

static volatile int rendering = 0;

int mousecapture = 0;

extern int fps;

static void *window_ptr = NULL;
static void *display_frame = NULL;
static void *native_canvas = NULL;
static char display_title[600];
static char display_status_title[800];
static volatile int display_fullscreen = 0;
static volatile int canvas_width = 0;
static volatile int canvas_height = 0;

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

PCEM_API extern void device_force_redraw();
extern void mouse_wheel_update(int);

#define host sdl_host_get()

static int is_embedded() { return host->panel_create != NULL; }

#ifdef _WIN32
#define szMsgClassName "PCemMsgWnd"
#define szSubClassName "PCemSubWnd"
#define WM_PCEM_CREATE_CANVAS (WM_APP + 1)
#define WM_PCEM_DESTROY_CANVAS (WM_APP + 2)
#define WM_PCEM_FOCUS_CANVAS (WM_APP + 3)

static HINSTANCE hinstance = 0;
static HWND msg_hwnd = 0;
static HWND sub_hwnd = 0;
static HHOOK hKeyboardHook = 0;
static RAWINPUTDEVICE device;
static uint16_t scancode_map[65536];
SDL_Window *dummy_window = NULL;
#endif

#ifdef __APPLE__
typedef struct {
        double x, y, width, height;
} cocoa_rect_t;

static void *cocoa_content_view = NULL;
static void *cocoa_canvas_super = NULL;

static void *cocoa_get(void *object, const char *selector) {
        return ((void *(*)(void *, SEL))objc_msgSend)(object, sel_registerName(selector));
}

static void cocoa_set(void *object, const char *selector, void *value) {
        ((void (*)(void *, SEL, void *))objc_msgSend)(object, sel_registerName(selector), value);
}

static void cocoa_save_views() {
        void *nswindow = cocoa_get(native_canvas, "window");
        cocoa_content_view = nswindow ? cocoa_get(nswindow, "contentView") : NULL;
        cocoa_canvas_super = cocoa_get(native_canvas, "superview");
        if (cocoa_content_view)
                cocoa_get(cocoa_content_view, "retain");
}

static void cocoa_restore_views() {
        void *nswindow = cocoa_get(native_canvas, "window");
        if (nswindow && cocoa_content_view && cocoa_get(nswindow, "contentView") != cocoa_content_view) {
                cocoa_set(nswindow, "setContentView:", cocoa_content_view);
                if (cocoa_canvas_super)
                        cocoa_set(cocoa_canvas_super, "addSubview:", native_canvas);
        }
        if (cocoa_content_view)
                cocoa_get(cocoa_content_view, "release");
        cocoa_content_view = NULL;
}

static void cocoa_fit_subviews() {
        void *subviews, *superview;
        long count, i;
        cocoa_rect_t rect = {0, 0, 0, 0};

        if (!native_canvas || canvas_width <= 0 || canvas_height <= 0)
                return;
        rect.width = canvas_width;
        rect.height = canvas_height;
        superview = cocoa_get(native_canvas, "superview");
        if (superview) {
                void *siblings = cocoa_get(cocoa_get(superview, "subviews"), "copy");
                count = ((long (*)(void *, SEL))objc_msgSend)(siblings, sel_registerName("count"));
                for (i = 0; i < count; i++) {
                        void *view = ((void *(*)(void *, SEL, long))objc_msgSend)(siblings, sel_registerName("objectAtIndex:"), i);
                        if (view != native_canvas && !strncmp(object_getClassName(view), "SDL", 3))
                                cocoa_set(native_canvas, "addSubview:", view);
                }
                cocoa_get(siblings, "release");
        }
        subviews = cocoa_get(native_canvas, "subviews");
        count = ((long (*)(void *, SEL))objc_msgSend)(subviews, sel_registerName("count"));
        for (i = 0; i < count; i++) {
                void *view = ((void *(*)(void *, SEL, long))objc_msgSend)(subviews, sel_registerName("objectAtIndex:"), i);
                ((void (*)(void *, SEL, cocoa_rect_t))objc_msgSend)(view, sel_registerName("setFrame:"), rect);
                ((void (*)(void *, SEL, unsigned long))objc_msgSend)(view, sel_registerName("setAutoresizingMask:"), 18);
        }
}
#endif

#ifdef PCEM_DISPLAY_X11
static Display *x11_display = NULL;
static Window x11_canvas = 0;
#endif

SDL_Event event;

uint64_t render_time = 0;
int render_fps = 0;
uint32_t render_frame_time = 0;
uint32_t render_frames = 0;
static uint32_t status_update_time = 0;

int is_fullscreen() { return display_fullscreen; }

void display_get_size(SDL_Window *window, int *width, int *height) {
#ifdef __APPLE__
        if (canvas_width > 0 && canvas_height > 0) {
                *width = canvas_width;
                *height = canvas_height;
                return;
        }
#endif
        SDL_GetWindowSize(window, width, height);
}

void display_resize(int width, int height) {
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

void releasemouse() {
        if (mousecapture) {
                SDL_SetWindowGrab(window, SDL_FALSE);
                SDL_SetRelativeMouseMode(SDL_FALSE);
                mousecapture = 0;
        }
}

static void ui_call(SDL_HOST_CALLBACK callback, void *data) { sdl_host_ui_call(callback, data); }

#ifdef _WIN32
UINT16 convert_scan_code(UINT16 scan_code) {
        switch (scan_code) {
        case 0xE001:
                return 0xF001;
        case 0xE002:
                return 0xF002;
        case 0xE005:
                return 0xF005;
        case 0xE006:
                return 0xF006;
        case 0xE007:
                return 0xF007;
        case 0xE071:
                return 0xF008;
        case 0xE072:
                return 0xF009;
        case 0xE07F:
                return 0xF00A;
        case 0xE0E1:
                return 0xF00B;
        case 0xE0EE:
                return 0xF00C;
        case 0xE0F1:
                return 0xF00D;
        case 0xE0FE:
                return 0xF00E;
        case 0xE0EF:
                return 0xF00F;

        default:
                return scan_code;
        }
}

void get_registry_key_map() {
        char *keyName = "SYSTEM\\CurrentControlSet\\Control\\Keyboard Layout";
        char *valueName = "Scancode Map";
        char buf[32768];
        DWORD bufSize;
        HKEY hKey;
        int j;

        for (j = 0; j < 65536; j++)
                scancode_map[j] = convert_scan_code(j);

        bufSize = 32768;
        pclog("Preparing scan code map list...\n");
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, keyName, 0, 1, &hKey) == ERROR_SUCCESS) {
                if (RegQueryValueEx(hKey, valueName, NULL, NULL, (LPBYTE)buf, &bufSize) == ERROR_SUCCESS) {
                        UINT32 *bufEx2 = (UINT32 *)buf;
                        int scMapCount = bufEx2[2];
                        pclog("%lu scan code mappings found!\n", scMapCount);
                        if ((bufSize != 0) && (scMapCount != 0)) {
                                UINT16 *bufEx = (UINT16 *)(buf + 12);
                                pclog("More than zero scan code mappings found, processing...\n");
                                for (j = 0; j < scMapCount * 2; j += 2) {
                                        int scancode_unmapped = bufEx[j + 1];
                                        int scancode_mapped = bufEx[j];

                                        scancode_mapped = convert_scan_code(scancode_mapped);

                                        scancode_map[scancode_unmapped] = scancode_mapped;
                                        pclog("Scan code mapping %u detected: %X -> %X\n", scancode_unmapped, scancode_mapped,
                                              scancode_map[scancode_unmapped]);
                                }
                                pclog("Done processing!\n");
                        }
                }
                RegCloseKey(hKey);
        }
        pclog("Done preparing!\n");
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
        if (nCode < 0 || nCode != HC_ACTION || (!mousecapture && !video_fullscreen))
                return CallNextHookEx(hKeyboardHook, nCode, wParam, lParam);

        KBDLLHOOKSTRUCT *p = (KBDLLHOOKSTRUCT *)lParam;

        if (p->vkCode == VK_TAB && p->flags & LLKHF_ALTDOWN)
                return 1;
        if (p->vkCode == VK_SPACE && p->flags & LLKHF_ALTDOWN)
                return 1;
        if ((p->vkCode == VK_LWIN) || (p->vkCode == VK_RWIN))
                return 1;
        if (p->vkCode == VK_ESCAPE && p->flags & LLKHF_ALTDOWN)
                return 1;
        BOOL bControlKeyDown = GetAsyncKeyState(VK_CONTROL) >> ((sizeof(SHORT) * 8) - 1);
        if (p->vkCode == VK_ESCAPE && bControlKeyDown)
                return 1;

        return CallNextHookEx(hKeyboardHook, nCode, wParam, lParam);
}

static void process_raw_input(LPARAM lParam) {
        UINT size;
        RAWINPUT *raw;

        GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &size, sizeof(RAWINPUTHEADER));

        raw = malloc(size);

        GetRawInputData((HRAWINPUT)(lParam), RID_INPUT, raw, &size, sizeof(RAWINPUTHEADER));

        if (raw->header.dwType == RIM_TYPEKEYBOARD) {
                const RAWKEYBOARD rawKB = raw->data.keyboard;
                USHORT scancode = rawKB.MakeCode;

                if (!(rawKB.Flags & RI_KEY_E1)) {
                        if (rawKB.Flags & RI_KEY_E0)
                                scancode |= (0xE0 << 8);

                        scancode = scancode_map[scancode];

                        if ((scancode >> 8) == 0xF0)
                                scancode |= 0x100;
                        else if ((scancode >> 8) == 0xE0)
                                scancode |= 0x80;

                        if (!(scancode & 0xf00))
                                rawinputkey[scancode & 0x1ff] = !(rawKB.Flags & RI_KEY_BREAK);
                }
        }
        free(raw);
}

LRESULT CALLBACK MsgWindowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
        switch (message) {
        case WM_INPUT:
                if (infocus)
                        process_raw_input(lParam);
                break;
        case WM_PCEM_CREATE_CANVAS: {
                RECT r;
                HWND parent = (HWND)native_canvas;
                GetClientRect(parent, &r);
                HWND child = CreateWindowEx(WS_EX_NOPARENTNOTIFY, szSubClassName, "", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 0,
                                            0, r.right - r.left, r.bottom - r.top, parent, NULL, hinstance, NULL);
                sub_hwnd = child;
                return (LRESULT)child;
        }
        case WM_PCEM_DESTROY_CANVAS:
                if ((HWND)wParam == sub_hwnd)
                        sub_hwnd = 0;
                DestroyWindow((HWND)wParam);
                return 0;
        case WM_PCEM_FOCUS_CANVAS:
                if (sub_hwnd)
                        SetFocus(sub_hwnd);
                return 0;
        default:
                break;
        }
        return DefWindowProc(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK subWindowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
        switch (message) {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP:
        case WM_CHAR:
        case WM_SYSCHAR:
                return 0;
        case WM_ERASEBKGND:
                return 1;
        default:
                break;
        }
        return DefWindowProc(hwnd, message, wParam, lParam);
}

static int windows_setup() {
        WNDCLASSEX wincl;

        if (msg_hwnd)
                return 1;

        hinstance = (HINSTANCE)GetModuleHandle(NULL);
        memset(&wincl, 0, sizeof(wincl));
        wincl.cbSize = sizeof(WNDCLASSEX);
        wincl.hInstance = hinstance;
        wincl.lpszClassName = szMsgClassName;
        wincl.lpfnWndProc = MsgWindowProcedure;
        if (!RegisterClassEx(&wincl))
                return 0;

        wincl.lpszClassName = szSubClassName;
        wincl.lpfnWndProc = subWindowProcedure;
        wincl.style = CS_OWNDC;
        wincl.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        if (!RegisterClassEx(&wincl))
                return 0;

        msg_hwnd = CreateWindowEx(0, szMsgClassName, "", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hinstance, NULL);
        if (!msg_hwnd)
                return 0;

        get_registry_key_map();

        device.usUsagePage = 0x01;
        device.usUsage = 0x06;
        device.dwFlags = RIDEV_NOHOTKEYS | RIDEV_INPUTSINK;
        device.hwndTarget = msg_hwnd;

        if (RegisterRawInputDevices(&device, 1, sizeof(device)))
                pclog("Raw input registered!\n");
        else
                pclog("Raw input registration failed!\n");

        return 1;
}
#endif

#ifdef PCEM_DISPLAY_X11
static int x11_setup() {
        Window parent = (Window)(uintptr_t)native_canvas;

        if (x11_canvas)
                return 1;
        if (!parent)
                return 0;

        if (!x11_display)
                x11_display = XOpenDisplay(NULL);
        if (!x11_display)
                return 0;
        x11_canvas = XCreateSimpleWindow(x11_display, parent, 0, 0, canvas_width > 0 ? canvas_width : 1,
                                         canvas_height > 0 ? canvas_height : 1, 0, 0,
                                         BlackPixel(x11_display, DefaultScreen(x11_display)));
        if (!x11_canvas)
                return 0;
        XMapWindow(x11_display, x11_canvas);
        XSync(x11_display, False);
        return 1;
}
#endif

static SDL_SpinLock title_lock = 0;
static int own_window_valid = 0;
static int own_x, own_y, own_w, own_h;

static void ui_call_panel(SDL_HOST_CALLBACK callback, void *data) {
        if (is_embedded())
                ui_call(callback, data);
        else
                callback(data);
}

static void panel_get_rect(int *x, int *y, int *w, int *h) {
        if (is_embedded()) {
                if (display_frame && host->panel_get_rect)
                        host->panel_get_rect(display_frame, x, y, w, h);
        } else if (window) {
                SDL_GetWindowPosition(window, x, y);
                SDL_GetWindowSize(window, w, h);
        }
}

static void panel_canvas_size(int *w, int *h) {
        *w = *h = 0;
        if (is_embedded()) {
                if (display_frame && host->panel_canvas_size)
                        host->panel_canvas_size(display_frame, w, h);
        } else if (window)
                SDL_GetWindowSize(window, w, h);
}

static void panel_set_size(int w, int h, int resizable) {
        if (is_embedded()) {
                if (display_frame && host->panel_set_size)
                        host->panel_set_size(display_frame, w, h, resizable);
        } else if (window) {
#if SDL_VERSION_ATLEAST(2, 0, 5)
                SDL_SetWindowResizable(window, resizable ? SDL_TRUE : SDL_FALSE);
#endif
                SDL_SetWindowSize(window, w, h);
        }
}

static void panel_set_fullscreen(int fullscreen) {
        if (is_embedded()) {
                if (display_frame && host->panel_set_fullscreen)
                        host->panel_set_fullscreen(display_frame, fullscreen);
        } else if (window)
                SDL_SetWindowFullscreen(window, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

static int display_status_update(void *data) {
        char s[256];
        char title[sizeof(display_title)];

        if (is_embedded() ? !display_frame : !window)
                return 0;

        if (emulation_state == EMULATION_PAUSED)
                strcpy(s, "Paused");
        else if (!mousecapture)
                strcpy(s, "Click to capture mouse");
        else if (mouse_get_type(mouse_type) & MOUSE_TYPE_3BUTTON)
                strcpy(s, "Press CTRL-END to release mouse");
        else
                strcpy(s, "Press CTRL-END or middle button to release mouse");

        if (is_embedded()) {
                if (host->panel_set_status)
                        host->panel_set_status(display_frame, s, fps, render_fps);
        } else {
                SDL_AtomicLock(&title_lock);
                strcpy(title, display_title);
                SDL_AtomicUnlock(&title_lock);
                snprintf(display_status_title, sizeof(display_status_title), "%s - Speed: %d%% - FPS: %d - %s", title, fps,
                         render_fps, s);
                if (strcmp(SDL_GetWindowTitle(window), display_status_title))
                        SDL_SetWindowTitle(window, display_status_title);
        }

        if (window_remember && !display_fullscreen) {
                int x = window_x, y = window_y, w = window_w, h = window_h;
                panel_get_rect(&x, &y, &w, &h);
                window_x = x;
                window_y = y;
                window_w = w;
                window_h = h;
        }
        return 0;
}

void sdl_display_event(int event, int param1, int param2) {
        switch (event) {
        case SDL_DISPLAY_CLOSE:
                if (host->request_stop)
                        host->request_stop();
                else
                        display_end();
                break;
        case SDL_DISPLAY_ACTIVATE:
                infocus = 1;
                break;
        case SDL_DISPLAY_DEACTIVATE:
                infocus = 0;
                if (display_fullscreen)
                        window_dowindowed = 1;
                window_doinputrelease = 1;
#ifdef _WIN32
                memset(rawinputkey, 0, sizeof(rawinputkey));
#endif
                break;
        case SDL_DISPLAY_SIZE:
                canvas_width = param1;
                canvas_height = param2;
#ifdef __APPLE__
                cocoa_fit_subviews();
#endif
#ifdef _WIN32
                if (sub_hwnd)
                        MoveWindow(sub_hwnd, 0, 0, param1, param2, TRUE);
#endif
#ifdef PCEM_DISPLAY_X11
                if (x11_canvas) {
                        XResizeWindow(x11_display, x11_canvas, param1 > 0 ? param1 : 1, param2 > 0 ? param2 : 1);
                        XFlush(x11_display);
                }
#endif
                device_force_redraw();
                break;
        case SDL_DISPLAY_MOUSE_UP:
                if (!mousecapture) {
                        if (param1 == 1 && !pauseEmulation) {
                                window_doinputgrab = 1;
                                if (video_fullscreen)
                                        window_dofullscreen = 1;
                        }
                } else if (param1 == 2 && !display_fullscreen && !(mouse_get_type(mouse_type) & MOUSE_TYPE_3BUTTON))
                        window_doinputrelease = 1;
                break;
        case SDL_DISPLAY_DESTROY:
                display_frame = NULL;
                native_canvas = NULL;
                break;
        }
}

static int ui_setsize(void *data) {
        if ((is_embedded() && !display_frame) || display_fullscreen)
                return 0;

        if (vid_resize == 2)
                panel_set_size(custom_resolution_width, custom_resolution_height, 0);
        else if (vid_resize == 1) {
                int w, h;
                panel_canvas_size(&w, &h);
                panel_set_size(w, h, 1);
        } else
                panel_set_size(winsizex, winsizey, 0);
        return 0;
}

static int ui_fullscreen(void *data) {
        panel_set_fullscreen(data != NULL);
        if (!data)
                ui_setsize(NULL);
        return 0;
}

static int ui_title(void *data) {
        if (data) {
                if (is_embedded()) {
                        if (display_frame && host->panel_set_title)
                                host->panel_set_title(display_frame, (char *)data);
                } else {
                        SDL_AtomicLock(&title_lock);
                        snprintf(display_title, sizeof(display_title), "%s", (char *)data);
                        SDL_AtomicUnlock(&title_lock);
                }
        }
        free(data);
        return 0;
}

static int ui_remember(void *data) {
        if (is_embedded() ? display_frame != NULL : window != NULL) {
                panel_get_rect(&window_x, &window_y, &window_w, &window_h);
                saveconfig(NULL);
        }
        return 0;
}

int display_init() {
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

void display_close() { SDL_Quit(); }

void display_start(void *wnd_ptr) {
        window_ptr = wnd_ptr;
        snprintf(display_title, sizeof(display_title), "PCem " PCEM_VERSION_STRING " - %s - %s", config_name, model_getname());

#ifdef __APPLE__
        ((void (*)(void *, SEL, signed char))objc_msgSend)(objc_getClass("NSWindow"), sel_registerName("setAllowsAutomaticWindowTabbing:"),
                                                           0);
#endif

        if (is_embedded()) {
                if (!display_frame) {
                        display_frame = host->panel_create(wnd_ptr, "PCem " PCEM_VERSION_STRING, sdl_display_event);
                        if (display_frame) {
                                if (host->panel_set_title)
                                        host->panel_set_title(display_frame, display_title);
                                if (window_remember && window_w > 0 && window_h > 0) {
                                        panel_set_size(window_w, window_h, vid_resize == 1);
                                        if (host->panel_set_position)
                                                host->panel_set_position(display_frame, window_x, window_y);
                                } else if (host->panel_center)
                                        host->panel_center(display_frame);
                        }
                }
                if (vid_resize == 2)
                        panel_set_size(custom_resolution_width, custom_resolution_height, 0);
                if (display_frame && host->panel_show)
                        host->panel_show(display_frame, 1);
                native_canvas = (display_frame && host->panel_native) ? host->panel_native(display_frame) : NULL;
                {
                        int w, h;
                        panel_canvas_size(&w, &h);
                        canvas_width = w;
                        canvas_height = h;
                }
        } else {
                native_canvas = NULL;
                canvas_width = 0;
                canvas_height = 0;
                own_window_valid = 0;
        }

#ifdef _WIN32
        if (!windows_setup())
                pclog("Could not set up display windows!\n");
        hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(NULL), 0);
        memset(rawinputkey, 0, sizeof(rawinputkey));
#endif
#ifdef PCEM_DISPLAY_X11
        if (is_embedded() && !x11_setup())
                sdl_host_message("PCem error", "Could not create the display. PCem requires X11.");
#endif

        infocus = 1;
        display_fullscreen = 0;

        atexit(releasemouse);
        rendererMutex = SDL_CreateMutex();
        rendererCond = SDL_CreateCond();
        renderer_start();
}

void display_stop() {
        renderer_stop(10 * 1000);

        SDL_DestroyMutex(rendererMutex);
        SDL_DestroyCond(rendererCond);
#if SDL_VERSION_ATLEAST(2, 0, 2)
        SDL_DetachThread(renderthread);
#endif
        releasemouse();

#ifdef _WIN32
        if (hKeyboardHook)
                UnhookWindowsHookEx(hKeyboardHook);
        hKeyboardHook = 0;
        if (sub_hwnd) {
                DestroyWindow(sub_hwnd);
                sub_hwnd = 0;
        }
#endif

        display_fullscreen = 0;
        if (display_frame && host->panel_show)
                host->panel_show(display_frame, 0);
}

void sdl_set_window_title(const char *title) {
        if (title && (!is_embedded() || display_frame))
                ui_call_panel(ui_title, strdup(title));
}

static const struct {
        SDL_Scancode sdl;
        int system;
} SDLScancodeToSystemScancode[] = {{SDL_SCANCODE_A, 0x1e},
                                   {SDL_SCANCODE_B, 0x30},
                                   {SDL_SCANCODE_C, 0x2e},
                                   {SDL_SCANCODE_D, 0x20},
                                   {SDL_SCANCODE_E, 0x12},
                                   {SDL_SCANCODE_F, 0x21},
                                   {SDL_SCANCODE_G, 0x22},
                                   {SDL_SCANCODE_H, 0x23},
                                   {SDL_SCANCODE_I, 0x17},
                                   {SDL_SCANCODE_J, 0x24},
                                   {SDL_SCANCODE_K, 0x25},
                                   {SDL_SCANCODE_L, 0x26},
                                   {SDL_SCANCODE_M, 0x32},
                                   {SDL_SCANCODE_N, 0x31},
                                   {SDL_SCANCODE_O, 0x18},
                                   {SDL_SCANCODE_P, 0x19},
                                   {SDL_SCANCODE_Q, 0x10},
                                   {SDL_SCANCODE_R, 0x13},
                                   {SDL_SCANCODE_S, 0x1f},
                                   {SDL_SCANCODE_T, 0x14},
                                   {SDL_SCANCODE_U, 0x16},
                                   {SDL_SCANCODE_V, 0x2f},
                                   {SDL_SCANCODE_W, 0x11},
                                   {SDL_SCANCODE_X, 0x2d},
                                   {SDL_SCANCODE_Y, 0x15},
                                   {SDL_SCANCODE_Z, 0x2c},
                                   {SDL_SCANCODE_0, 0x0B},
                                   {SDL_SCANCODE_1, 0x02},
                                   {SDL_SCANCODE_2, 0x03},
                                   {SDL_SCANCODE_3, 0x04},
                                   {SDL_SCANCODE_4, 0x05},
                                   {SDL_SCANCODE_5, 0x06},
                                   {SDL_SCANCODE_6, 0x07},
                                   {SDL_SCANCODE_7, 0x08},
                                   {SDL_SCANCODE_8, 0x09},
                                   {SDL_SCANCODE_9, 0x0A},
                                   {SDL_SCANCODE_GRAVE, 0x29},
                                   {SDL_SCANCODE_MINUS, 0x0c},
                                   {SDL_SCANCODE_EQUALS, 0x0d},
                                   {SDL_SCANCODE_NONUSBACKSLASH, 0x56},
                                   {SDL_SCANCODE_BACKSLASH, 0x2b},
                                   {SDL_SCANCODE_BACKSPACE, 0x0e},
                                   {SDL_SCANCODE_SPACE, 0x39},
                                   {SDL_SCANCODE_TAB, 0x0f},
                                   {SDL_SCANCODE_CAPSLOCK, 0x3a},
                                   {SDL_SCANCODE_LSHIFT, 0x2a},
                                   {SDL_SCANCODE_LCTRL, 0x1d},
                                   {SDL_SCANCODE_LGUI, 0xdb},
                                   {SDL_SCANCODE_LALT, 0x38},
                                   {SDL_SCANCODE_RSHIFT, 0x36},
                                   {SDL_SCANCODE_RCTRL, 0x9d},
                                   {SDL_SCANCODE_RGUI, 0xdc},
                                   {SDL_SCANCODE_RALT, 0xb8},
                                   {SDL_SCANCODE_SYSREQ, 0x54},
                                   {SDL_SCANCODE_APPLICATION, 0xdd},
                                   {SDL_SCANCODE_RETURN, 0x1c},
                                   {SDL_SCANCODE_ESCAPE, 0x01},
                                   {SDL_SCANCODE_F1, 0x3B},
                                   {SDL_SCANCODE_F2, 0x3C},
                                   {SDL_SCANCODE_F3, 0x3D},
                                   {SDL_SCANCODE_F4, 0x3e},
                                   {SDL_SCANCODE_F5, 0x3f},
                                   {SDL_SCANCODE_F6, 0x40},
                                   {SDL_SCANCODE_F7, 0x41},
                                   {SDL_SCANCODE_F8, 0x42},
                                   {SDL_SCANCODE_F9, 0x43},
                                   {SDL_SCANCODE_F10, 0x44},
                                   {SDL_SCANCODE_F11, 0x57},
                                   {SDL_SCANCODE_F12, 0x58},
                                   {SDL_SCANCODE_SCROLLLOCK, 0x46},
                                   {SDL_SCANCODE_LEFTBRACKET, 0x1a},
                                   {SDL_SCANCODE_RIGHTBRACKET, 0x1b},
                                   {SDL_SCANCODE_INSERT, 0xd2},
                                   {SDL_SCANCODE_HOME, 0xc7},
                                   {SDL_SCANCODE_PAGEUP, 0xc9},
                                   {SDL_SCANCODE_DELETE, 0xd3},
                                   {SDL_SCANCODE_END, 0xcf},
                                   {SDL_SCANCODE_PAGEDOWN, 0xd1},
                                   {SDL_SCANCODE_UP, 0xc8},
                                   {SDL_SCANCODE_LEFT, 0xcb},
                                   {SDL_SCANCODE_DOWN, 0xd0},
                                   {SDL_SCANCODE_RIGHT, 0xcd},
                                   {SDL_SCANCODE_NUMLOCKCLEAR, 0x45},
                                   {SDL_SCANCODE_KP_DIVIDE, 0xb5},
                                   {SDL_SCANCODE_KP_MULTIPLY, 0x37},
                                   {SDL_SCANCODE_KP_MINUS, 0x4a},
                                   {SDL_SCANCODE_KP_PLUS, 0x4e},
                                   {SDL_SCANCODE_KP_ENTER, 0x9c},
                                   {SDL_SCANCODE_KP_PERIOD, 0x53},
                                   {SDL_SCANCODE_KP_0, 0x52},
                                   {SDL_SCANCODE_KP_1, 0x4f},
                                   {SDL_SCANCODE_KP_2, 0x50},
                                   {SDL_SCANCODE_KP_3, 0x51},
                                   {SDL_SCANCODE_KP_4, 0x4b},
                                   {SDL_SCANCODE_KP_5, 0x4c},
                                   {SDL_SCANCODE_KP_6, 0x4d},
                                   {SDL_SCANCODE_KP_7, 0x47},
                                   {SDL_SCANCODE_KP_8, 0x48},
                                   {SDL_SCANCODE_KP_9, 0x49},
                                   {SDL_SCANCODE_SEMICOLON, 0x27},
                                   {SDL_SCANCODE_APOSTROPHE, 0x28},
                                   {SDL_SCANCODE_COMMA, 0x33},
                                   {SDL_SCANCODE_PERIOD, 0x34},
                                   {SDL_SCANCODE_SLASH, 0x35},
                                   {SDL_SCANCODE_PRINTSCREEN, 0xb7}};

int sdl_scancode(SDL_Scancode scancode) {
        int i;
        for (i = 0; i < SDL_arraysize(SDLScancodeToSystemScancode); ++i) {
                if (SDLScancodeToSystemScancode[i].sdl == scancode) {
                        return SDLScancodeToSystemScancode[i].system;
                }
        }
        return -1;
}


void window_setup() {
#ifdef _WIN32
        SDL_SetHint(SDL_HINT_GRAB_KEYBOARD, "0");
        SDL_SetHint(SDL_HINT_WINDOWS_NO_CLOSE_ON_ALT_F4, "1");
#else
        SDL_SetHint(SDL_HINT_GRAB_KEYBOARD, "1");
#endif
#if SDL_VERSION_ATLEAST(2, 0, 5)
        SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
#endif

        if (start_in_fullscreen) {
                start_in_fullscreen = 0;
                window_dofullscreen = 1;
                window_doinputgrab = 1;
        }
}

static SDL_Window *own_window_create(int opengl) {
        Uint32 flags = opengl ? SDL_WINDOW_OPENGL : 0;
        int x = SDL_WINDOWPOS_CENTERED, y = SDL_WINDOWPOS_CENTERED;
        int w = winsizex, h = winsizey;
        char title[sizeof(display_title)];

        if (vid_resize == 1)
                flags |= SDL_WINDOW_RESIZABLE;
        if (vid_resize == 2) {
                w = custom_resolution_width;
                h = custom_resolution_height;
        }
        if (own_window_valid) {
                x = own_x;
                y = own_y;
                w = own_w;
                h = own_h;
        } else if (window_remember && window_w > 0 && window_h > 0) {
                x = window_x;
                y = window_y;
                w = window_w;
                h = window_h;
        }
        if (display_fullscreen)
                flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;

        SDL_AtomicLock(&title_lock);
        strcpy(title, display_title);
        SDL_AtomicUnlock(&title_lock);
        return SDL_CreateWindow(title, x, y, w > 0 ? w : 640, h > 0 ? h : 480, flags);
}

int window_create() {
        void *native = NULL;
        int opengl = requested_render_driver.sdl_window_params & SDL_WINDOW_OPENGL;

#ifdef _WIN32
        if (msg_hwnd && native_canvas)
                native = (void *)SendMessage(msg_hwnd, WM_PCEM_CREATE_CANVAS, 0, 0);
        if (native && opengl) {
                dummy_window = SDL_CreateWindow("GL3 test", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1, 1,
                                                SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL);
                if (dummy_window) {
                        char ptr[32];
                        snprintf(ptr, 31, "%p", dummy_window);
                        ptr[31] = 0;
                        SDL_SetHint(SDL_HINT_VIDEO_WINDOW_SHARE_PIXEL_FORMAT, ptr);
                }
        }
#elif defined(PCEM_DISPLAY_X11)
        native = (void *)x11_canvas;
#else
        native = native_canvas;
#endif

#ifdef SDL_HINT_VIDEO_FOREIGN_WINDOW_OPENGL
        SDL_SetHint(SDL_HINT_VIDEO_FOREIGN_WINDOW_OPENGL, opengl ? "1" : "0");
#endif

#ifdef __APPLE__
        if (native)
                cocoa_save_views();
#endif
        if (native)
                window = SDL_CreateWindowFrom(native);
        else if (!is_embedded())
                window = own_window_create(opengl);
        else
                window = NULL;
#ifdef __APPLE__
        if (native) {
                cocoa_restore_views();
                ui_setsize(NULL);
        }
#endif
        if (!window) {
                sdl_host_message("SDL Error", "SDL window could not be created! Error: %s\n", SDL_GetError());
                return 0;
        }

#ifdef PCEM_DISPLAY_X11
        if (native) {
                SDL_SysWMinfo info;
                SDL_VERSION(&info.version);
                if (SDL_GetWindowWMInfo(window, &info) && info.subsystem == SDL_SYSWM_X11) {
                        XSelectInput(info.info.x11.display, info.info.x11.window,
                                     KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                                             EnterWindowMask | LeaveWindowMask | FocusChangeMask | StructureNotifyMask |
                                             ExposureMask | KeymapStateMask);
                        XFlush(info.info.x11.display);
                }
        }
#endif

        window_dosetresize = 1;

        render_time = 0;
        render_fps = 0;
        render_frame_time = SDL_GetTicks();
        render_frames = 0;
        return 1;
}

void window_close() {
        sdl_renderer_close();

        if (window && !is_embedded() && !display_fullscreen) {
                SDL_GetWindowPosition(window, &own_x, &own_y);
                SDL_GetWindowSize(window, &own_w, &own_h);
                own_window_valid = 1;
        }
        if (window)
                SDL_DestroyWindow(window);
        window = NULL;

#ifdef _WIN32
        if (dummy_window) {
                SDL_SetHint(SDL_HINT_VIDEO_WINDOW_SHARE_PIXEL_FORMAT, "");
                SDL_DestroyWindow(dummy_window);
        }
        dummy_window = NULL;

        if (sub_hwnd)
                PostMessage(msg_hwnd, WM_PCEM_DESTROY_CANVAS, (WPARAM)sub_hwnd, 0);
#endif
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
                ui_call_panel(ui_setsize, NULL);
                device_force_redraw();
        }
        if (renderer_doreset) {
                pclog("renderer_doreset\n");
                renderer_doreset = 0;
                sdl_renderer_close();
                sdl_renderer_init(window);
#ifdef __APPLE__
                cocoa_fit_subviews();
#endif

                device_force_redraw();
                video_wait_for_blit();
        }
        while (SDL_PollEvent(&event)) {
                switch (event.type) {
                case SDL_MOUSEBUTTONUP:
                        if (event.button.button == SDL_BUTTON_LEFT)
                                sdl_display_event(SDL_DISPLAY_MOUSE_UP, 1, 0);
                        else if (event.button.button == SDL_BUTTON_MIDDLE)
                                sdl_display_event(SDL_DISPLAY_MOUSE_UP, 2, 0);
                        break;
                case SDL_MOUSEWHEEL:
                        if (mousecapture)
                                mouse_wheel_update(event.wheel.y);
                        break;
                case SDL_WINDOWEVENT:
                        if (event.window.event == SDL_WINDOWEVENT_RESIZED || event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                                device_force_redraw();
                        if (!is_embedded()) {
                                if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
                                        sdl_display_event(SDL_DISPLAY_ACTIVATE, 0, 0);
                                else if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
                                        sdl_display_event(SDL_DISPLAY_DEACTIVATE, 0, 0);
                                else if (event.window.event == SDL_WINDOWEVENT_CLOSE)
                                        sdl_display_event(SDL_DISPLAY_CLOSE, 0, 0);
                        }
                        break;
                case SDL_QUIT:
                        if (!is_embedded())
                                sdl_display_event(SDL_DISPLAY_CLOSE, 0, 0);
                        break;
#ifndef _WIN32
                case SDL_KEYDOWN: {
                        int key_idx = sdl_scancode(event.key.keysym.scancode);
                        if (key_idx != -1)
                                rawinputkey[key_idx] = event.key.timestamp ? event.key.timestamp : 1;
                        break;
                }
                case SDL_KEYUP: {
                        int key_idx = sdl_scancode(event.key.keysym.scancode);
                        if (key_idx != -1)
                                rawinputkey[key_idx] = 0;
                        break;
                }
#endif
                }
        }
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
                if (!display_fullscreen)
                        window_doinputrelease = 1;
        }
        if (window_doremember) {
                window_doremember = 0;
                ui_call_panel(ui_remember, NULL);
        }

        if (window_dotogglefullscreen) {
                window_dotogglefullscreen = 0;
                if (mousecapture || display_fullscreen) {
                        window_doinputrelease = 1;
                        if (display_fullscreen)
                                window_dowindowed = 1;
                } else {
                        window_doinputgrab = 1;
                        window_dofullscreen = 1;
                }
        }

        if (window_dofullscreen) {
                window_dofullscreen = 0;
                if (!display_fullscreen) {
                        video_wait_for_blit();
#if SDL_VERSION_ATLEAST(2, 0, 4)
                        SDL_GetGlobalMouseState(&remembered_mouse_x, &remembered_mouse_y);
#endif
                        display_fullscreen = 1;
                        ui_call_panel(ui_fullscreen, (void *)1);
                        device_force_redraw();
                }
        }
        if (window_doinputgrab) {
                window_doinputgrab = 0;
                mousecapture = 1;
                SDL_Rect rect;
                display_get_size(window, &rect.w, &rect.h);
                SDL_WarpMouseInWindow(window, rect.w / 2, rect.h / 2);
                SDL_SetWindowGrab(window, SDL_TRUE);
                SDL_SetRelativeMouseMode(SDL_TRUE);
                SDL_GetRelativeMouseState(0, 0);
#ifdef _WIN32
                PostMessage(msg_hwnd, WM_PCEM_FOCUS_CANVAS, 0, 0);
#endif
        }

        if (window_doinputrelease) {
                window_doinputrelease = 0;
                releasemouse();
        }
        if (window_dowindowed) {
                window_dowindowed = 0;
                if (display_fullscreen) {
                        display_fullscreen = 0;
                        ui_call_panel(ui_fullscreen, NULL);
#if SDL_VERSION_ATLEAST(2, 0, 4)
                        SDL_WarpMouseGlobal(remembered_mouse_x, remembered_mouse_y);
#endif
                        device_force_redraw();
                }
        }

        if (win_doresize) {
                win_doresize = 0;
                if (!vid_resize && !display_fullscreen) {
                        ui_call_panel(ui_setsize, NULL);
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

        if (ticks - status_update_time >= 100) {
                status_update_time = ticks;
                ui_call_panel(display_status_update, NULL);
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
                pclog("Start rendering...\n");
                while (rendering && internal_rendering) {
                        if (!render())
                                internal_rendering = 0;

                        SDL_Delay(1);
                }
                pclog("Stop rendering...\n");
                window_close();
        }

        SDL_LockMutex(rendererMutex);
        SDL_CondSignal(rendererCond);
        SDL_UnlockMutex(rendererMutex);

        return SDL_TRUE;
}

void display_run(void) { renderer_thread(NULL); }

void display_end(void) { rendering = 0; }

void renderer_start() {
        if (!rendering) {
                if (host->start_render_loop) {
                        host->start_render_loop(display_run);
                } else {
                        SDL_LockMutex(rendererMutex);
                        renderthread = SDL_CreateThread(renderer_thread, "SDL2 Thread", NULL);
                        SDL_CondWait(rendererCond, rendererMutex);
                        SDL_UnlockMutex(rendererMutex);
                }
        }
}

void renderer_stop(int timeout) {
        if (host->start_render_loop) {
                rendering = 0;
                window_close();
        } else if (rendering) {
                SDL_LockMutex(rendererMutex);
                rendering = 0;
                if (timeout)
                        SDL_CondWaitTimeout(rendererCond, rendererMutex, timeout);
                else
                        SDL_CondWait(rendererCond, rendererMutex);
                SDL_UnlockMutex(rendererMutex);
                renderthread = NULL;
        }
}
