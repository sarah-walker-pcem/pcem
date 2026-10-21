#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif
#include <wx/thread.h>

#include <SDL2/SDL.h>

#include "wx-app.h"
#include "wx-utils.h"
#include "wx-common.h"
#include "wx-display.h"

extern "C" {
#include "video.h"
#include "wx-sdl2-video.h"
#include "ibm.h"
#include "plat-keyboard.h"
#include "mouse.h"
}

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef __WXGTK__
#include <dlfcn.h>
#include <stdint.h>
#endif

SDL_mutex *rendererMutex;
SDL_cond *rendererCond;
SDL_Thread *renderthread = NULL;

SDL_Window *window = NULL;
static SDL_Window *dummy_window = NULL;

int rendering = 0;
static int render_thread_done = 0;

int mousecapture = 0;

extern "C" {
extern int video_scale;
extern int take_screenshot;

PCEM_API void device_force_redraw();
void mouse_wheel_update(int);
void toggle_fullscreen();
}

SDL_Rect remembered_rect;
int remembered_mouse_x = 0;
int remembered_mouse_y = 0;

int custom_resolution_width = 640;
int custom_resolution_height = 480;

int win_doresize = 0;
int winsizex = 640, winsizey = 480;

int trigger_fullscreen = 0;
int trigger_screenshot = 0;
int trigger_togglewindow = 0;
int trigger_inputrelease = 0;

#define MIN_WIDTH 360

static Frame *frame = NULL;
static wxWindow *canvas = NULL;
static void *native_handle = NULL;
static int embedded = 0;
static int input_hooked = 0;
static volatile int fullscreen_active = 0;

static SDL_SpinLock host_mouse_lock = 0;
static int host_mouse_dx = 0, host_mouse_dy = 0, host_mouse_buttons = 0;

extern "C" void renderer_start();
extern "C" void renderer_stop(int timeout);

template <typename F> static void main_thread_call(F fn) {
        if (frame)
                frame->CallAfter(fn);
}

static void host_mouse_move(int dx, int dy) {
        SDL_AtomicLock(&host_mouse_lock);
        host_mouse_dx += dx;
        host_mouse_dy += dy;
        SDL_AtomicUnlock(&host_mouse_lock);
}

static void host_mouse_button(int button, int down) {
        SDL_AtomicLock(&host_mouse_lock);
        if (down)
                host_mouse_buttons |= button;
        else
                host_mouse_buttons &= ~button;
        SDL_AtomicUnlock(&host_mouse_lock);
}

static void host_mouse_reset() {
        SDL_AtomicLock(&host_mouse_lock);
        host_mouse_dx = host_mouse_dy = host_mouse_buttons = 0;
        SDL_AtomicUnlock(&host_mouse_lock);
}

extern "C" int is_fullscreen() {
        if (embedded)
                return fullscreen_active;
        if (window) {
                int flags = SDL_GetWindowFlags(window);
                return (flags & SDL_WINDOW_FULLSCREEN) || (flags & SDL_WINDOW_FULLSCREEN_DESKTOP);
        }
        return 0;
}

static void release_on_middle_button() {
        if (!is_fullscreen() && !(mouse_get_type(mouse_type) & MOUSE_TYPE_3BUTTON))
                window_doinputrelease = 1;
}

extern "C" int display_get_mouse(int *dx, int *dy, int *buttons) {
        if (!embedded)
                return 0;

        SDL_AtomicLock(&host_mouse_lock);
        *dx = host_mouse_dx;
        *dy = host_mouse_dy;
        *buttons = host_mouse_buttons;
        host_mouse_dx = host_mouse_dy = 0;
        SDL_AtomicUnlock(&host_mouse_lock);
        return 1;
}

#ifdef _WIN32
static HHOOK hKeyboardHook = NULL;
static uint16_t scancode_map[65536];

static UINT16 convert_scan_code(UINT16 scan_code) {
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

static void get_registry_key_map() {
        const char *keyName = "SYSTEM\\CurrentControlSet\\Control\\Keyboard Layout";
        const char *valueName = "Scancode Map";
        char buf[32768];
        DWORD bufSize;
        HKEY hKey;
        int j;

        for (j = 0; j < 65536; j++)
                scancode_map[j] = convert_scan_code(j);

        bufSize = 32768;
        pclog("Preparing scan code map list...\n");
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, keyName, 0, 1, &hKey) == ERROR_SUCCESS) {
                if (RegQueryValueExA(hKey, valueName, NULL, NULL, (LPBYTE)buf, &bufSize) == ERROR_SUCCESS) {
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

static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
        if (nCode != HC_ACTION || !infocus || (!mousecapture && !fullscreen_active))
                return CallNextHookEx(hKeyboardHook, nCode, wParam, lParam);

        KBDLLHOOKSTRUCT *p = (KBDLLHOOKSTRUCT *)lParam;

        if (p->vkCode == VK_TAB && p->flags & LLKHF_ALTDOWN)
                return 1; // disable alt-tab
        if (p->vkCode == VK_SPACE && p->flags & LLKHF_ALTDOWN)
                return 1; // disable alt-space
        if ((p->vkCode == VK_LWIN) || (p->vkCode == VK_RWIN))
                return 1; // disable windows keys
        if (p->vkCode == VK_ESCAPE && p->flags & LLKHF_ALTDOWN)
                return 1;                                                                 // disable alt-escape
        BOOL bControlKeyDown = GetAsyncKeyState(VK_CONTROL) >> ((sizeof(SHORT) * 8) - 1); // checks ctrl key pressed
        if (p->vkCode == VK_ESCAPE && bControlKeyDown)
                return 1; // disable ctrl-escape

        return CallNextHookEx(hKeyboardHook, nCode, wParam, lParam);
}

static void rawinput_register(HWND hwnd, int enable) {
        RAWINPUTDEVICE devices[2];

        /* Keyboard */
        devices[0].usUsagePage = 0x01;
        devices[0].usUsage = 0x06;
        devices[0].dwFlags = enable ? RIDEV_NOHOTKEYS : RIDEV_REMOVE;
        devices[0].hwndTarget = enable ? hwnd : NULL;
        /* Mouse */
        devices[1].usUsagePage = 0x01;
        devices[1].usUsage = 0x02;
        devices[1].dwFlags = enable ? 0 : RIDEV_REMOVE;
        devices[1].hwndTarget = enable ? hwnd : NULL;

        if (RegisterRawInputDevices(devices, 2, sizeof(RAWINPUTDEVICE)))
                pclog("Raw input %s!\n", enable ? "registered" : "unregistered");
        else
                pclog("Raw input registration failed!\n");
}

static void handle_raw_input(HRAWINPUT input) {
        static int wheel_remainder = 0;
        RAWINPUT raw;
        UINT size = sizeof(raw);

        if (GetRawInputData(input, RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER)) == (UINT)-1)
                return;

        if (raw.header.dwType == RIM_TYPEKEYBOARD) {
                const RAWKEYBOARD &rawKB = raw.data.keyboard;
                USHORT scancode = rawKB.MakeCode;

                if (!infocus)
                        return;

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
        } else if (raw.header.dwType == RIM_TYPEMOUSE && mousecapture) {
                const RAWMOUSE &m = raw.data.mouse;
                USHORT flags = m.usButtonFlags;

                if (!(m.usFlags & MOUSE_MOVE_ABSOLUTE))
                        host_mouse_move(m.lLastX, m.lLastY);

                if (flags & RI_MOUSE_LEFT_BUTTON_DOWN)
                        host_mouse_button(1, 1);
                if (flags & RI_MOUSE_LEFT_BUTTON_UP)
                        host_mouse_button(1, 0);
                if (flags & RI_MOUSE_RIGHT_BUTTON_DOWN)
                        host_mouse_button(2, 1);
                if (flags & RI_MOUSE_RIGHT_BUTTON_UP)
                        host_mouse_button(2, 0);
                if (flags & RI_MOUSE_MIDDLE_BUTTON_DOWN)
                        host_mouse_button(4, 1);
                if (flags & RI_MOUSE_MIDDLE_BUTTON_UP) {
                        host_mouse_button(4, 0);
                        release_on_middle_button();
                }
                if (flags & RI_MOUSE_WHEEL) {
                        wheel_remainder += (SHORT)m.usButtonData;
                        if (wheel_remainder / WHEEL_DELTA) {
                                mouse_wheel_update(wheel_remainder / WHEEL_DELTA);
                                wheel_remainder %= WHEEL_DELTA;
                        }
                }
        }
}

static void clip_cursor_to_canvas() {
        RECT rect;
        GetWindowRect((HWND)native_handle, &rect);
        int cx = (rect.left + rect.right) / 2;
        int cy = (rect.top + rect.bottom) / 2;
        RECT clip = {cx, cy, cx + 1, cy + 1};
        SetCursorPos(cx, cy);
        ClipCursor(&clip);
}
#endif /* _WIN32 */

#ifdef __WXGTK__
static int evdev_to_scancode(int code) {
        if (code >= 1 && code <= 83)
                return code;

        switch (code) {
        case 86:
                return 0x56;
        case 87:
                return 0x57;
        case 88:
                return 0x58;
        case 96:
                return 0x9c;
        case 97:
                return 0x9d;
        case 98:
                return 0xb5;
        case 99:
                return 0xb7;
        case 100:
                return 0xb8;
        case 102:
                return 0xc7;
        case 103:
                return 0xc8;
        case 104:
                return 0xc9;
        case 105:
                return 0xcb;
        case 106:
                return 0xcd;
        case 107:
                return 0xcf;
        case 108:
                return 0xd0;
        case 109:
                return 0xd1;
        case 110:
                return 0xd2;
        case 111:
                return 0xd3;
        case 125:
                return 0xdb;
        case 126:
                return 0xdc;
        case 127:
                return 0xdd;
        }
        return -1;
}

static void *gtk_get_x11_window(wxWindow *win) {
        typedef void (*widget_realize_t)(void *);
        typedef void *(*widget_get_window_t)(void *);
        typedef int (*window_ensure_native_t)(void *);
        typedef unsigned long (*window_get_xid_t)(void *);
        typedef size_t (*window_get_type_t)(void);
        typedef int (*type_check_instance_is_a_t)(void *, size_t);

        widget_realize_t widget_realize = (widget_realize_t)dlsym(RTLD_DEFAULT, "gtk_widget_realize");
        widget_get_window_t widget_get_window = (widget_get_window_t)dlsym(RTLD_DEFAULT, "gtk_widget_get_window");
        window_ensure_native_t window_ensure_native = (window_ensure_native_t)dlsym(RTLD_DEFAULT, "gdk_window_ensure_native");
        window_get_xid_t window_get_xid = (window_get_xid_t)dlsym(RTLD_DEFAULT, "gdk_x11_window_get_xid");
        window_get_type_t x11_window_get_type = (window_get_type_t)dlsym(RTLD_DEFAULT, "gdk_x11_window_get_type");
        type_check_instance_is_a_t type_check_instance_is_a =
                (type_check_instance_is_a_t)dlsym(RTLD_DEFAULT, "g_type_check_instance_is_a");

        if (!widget_realize || !widget_get_window || !window_ensure_native || !window_get_xid || !x11_window_get_type ||
            !type_check_instance_is_a)
                return NULL;

        const char *driver = SDL_GetCurrentVideoDriver();
        if (!driver || strcmp(driver, "x11"))
                return NULL;

        void *widget = win->GetHandle();
        widget_realize(widget);
        void *gdk_window = widget_get_window(widget);
        if (!gdk_window || !type_check_instance_is_a(gdk_window, x11_window_get_type()))
                return NULL;
        if (!window_ensure_native(gdk_window))
                return NULL;

        return (void *)(uintptr_t)window_get_xid(gdk_window);
}
#endif /* __WXGTK__ */

static void *get_native_handle(wxWindow *win) {
#if defined(__WXMSW__)
        return win->GetHWND();
#elif defined(__WXGTK__)
        return gtk_get_x11_window(win);
#else
        return NULL;
#endif
}

static void canvas_capture_mouse(bool capture) {
#ifdef __WXGTK__
        if (!canvas)
                return;
        if (capture) {
                if (!canvas->HasCapture())
                        canvas->CaptureMouse();
                canvas->SetCursor(wxCursor(wxCURSOR_BLANK));
                wxSize size = canvas->GetClientSize();
                canvas->WarpPointer(size.x / 2, size.y / 2);
        } else {
                if (canvas->HasCapture())
                        canvas->ReleaseMouse();
                canvas->SetCursor(wxNullCursor);
        }
#endif
}

class SDLCanvas : public wxWindow {
    public:
        SDLCanvas(wxWindow *parent)
                : wxWindow(parent, wxID_ANY, wxDefaultPosition, wxSize(640, 480), wxWANTS_CHARS | wxBORDER_NONE) {
                SetBackgroundStyle(wxBG_STYLE_PAINT);
                SetBackgroundColour(*wxBLACK);

                Bind(wxEVT_PAINT, &SDLCanvas::OnPaint, this);
                Bind(wxEVT_SIZE, &SDLCanvas::OnSize, this);
                Bind(wxEVT_LEFT_DOWN, &SDLCanvas::OnMouseButton, this);
                Bind(wxEVT_LEFT_UP, &SDLCanvas::OnMouseButton, this);
                Bind(wxEVT_RIGHT_DOWN, &SDLCanvas::OnMouseButton, this);
                Bind(wxEVT_RIGHT_UP, &SDLCanvas::OnMouseButton, this);
                Bind(wxEVT_MIDDLE_DOWN, &SDLCanvas::OnMouseButton, this);
                Bind(wxEVT_MIDDLE_UP, &SDLCanvas::OnMouseButton, this);
                Bind(wxEVT_MOTION, &SDLCanvas::OnMouseMove, this);
                Bind(wxEVT_MOUSEWHEEL, &SDLCanvas::OnMouseWheel, this);
                Bind(wxEVT_MOUSE_CAPTURE_LOST, &SDLCanvas::OnCaptureLost, this);
                Bind(wxEVT_KEY_DOWN, &SDLCanvas::OnKey, this);
                Bind(wxEVT_KEY_UP, &SDLCanvas::OnKey, this);
        }

#ifdef __WXMSW__
        WXLRESULT MSWWindowProc(WXUINT message, WXWPARAM wParam, WXLPARAM lParam) {
                switch (message) {
                case WM_INPUT:
                        handle_raw_input((HRAWINPUT)lParam);
                        break;
                case WM_KEYDOWN:
                case WM_KEYUP:
                case WM_SYSKEYDOWN:
                case WM_SYSKEYUP:
                case WM_CHAR:
                case WM_SYSCHAR:
                        return 0;
                }
                return wxWindow::MSWWindowProc(message, wParam, lParam);
        }
#endif

    private:
        void OnPaint(wxPaintEvent &event) { wxPaintDC dc(this); }

        void OnSize(wxSizeEvent &event) {
                if (window_remember && vid_resize == 1 && !fullscreen_active) {
                        wxSize size = GetClientSize();
                        window_w = size.x;
                        window_h = size.y;
                }
                event.Skip();
        }

        void OnMouseButton(wxMouseEvent &event) {
                if (event.ButtonDown())
                        SetFocus();

                if (!mousecapture) {
                        if (event.LeftUp() && !IS_PAUSED) {
                                window_doinputgrab = 1;
                                if (video_fullscreen)
                                        window_dofullscreen = 1;
                        }
                        return;
                }
#ifdef __WXGTK__
                int button = 0;
                if (event.GetButton() == wxMOUSE_BTN_LEFT)
                        button = 1;
                else if (event.GetButton() == wxMOUSE_BTN_RIGHT)
                        button = 2;
                else if (event.GetButton() == wxMOUSE_BTN_MIDDLE)
                        button = 4;
                host_mouse_button(button, event.ButtonDown());
                if (button == 4 && event.ButtonUp())
                        release_on_middle_button();
#endif
        }

        void OnMouseMove(wxMouseEvent &event) {
#ifdef __WXGTK__
                if (mousecapture) {
                        wxSize size = GetClientSize();
                        int dx = event.GetX() - size.x / 2;
                        int dy = event.GetY() - size.y / 2;
                        if (dx || dy) {
                                host_mouse_move(dx, dy);
                                WarpPointer(size.x / 2, size.y / 2);
                        }
                }
#endif
        }

        void OnMouseWheel(wxMouseEvent &event) {
#ifdef __WXGTK__
                if (mousecapture && event.GetWheelAxis() == wxMOUSE_WHEEL_VERTICAL)
                        mouse_wheel_update(event.GetWheelRotation() / event.GetWheelDelta());
#endif
        }

        void OnCaptureLost(wxMouseCaptureLostEvent &event) { window_doinputrelease = 1; }

        void OnKey(wxKeyEvent &event) {
#ifdef __WXGTK__
                int scancode = evdev_to_scancode((int)event.GetRawKeyFlags() - 8);
                if (scancode >= 0 && infocus)
                        rawinputkey[scancode] = event.GetEventType() == wxEVT_KEY_DOWN;
#endif
        }
};

wxWindow *display_create_canvas(wxWindow *parent) { return new SDLCanvas(parent); }

static void on_frame_activate(wxActivateEvent &event) {
        if (event.GetActive())
                infocus = 1;
        else {
                infocus = 0;
                if (fullscreen_active)
                        window_dowindowed = 1;
                window_doinputrelease = 1;
                memset(rawinputkey, 0, sizeof(rawinputkey));
        }
        event.Skip();
}

static void on_frame_move(wxMoveEvent &event) {
        if (window_remember && !fullscreen_active && frame && !frame->IsIconized() && !frame->IsMaximized()) {
                wxPoint pos = frame->GetPosition();
                window_x = pos.x;
                window_y = pos.y;
        }
        event.Skip();
}

static void frame_set_display_size(int width, int height) {
        if (!frame || !embedded || fullscreen_active || frame->IsMaximized())
                return;
        frame->SetClientSize(width, height);
}

static void frame_set_resizable(int resizable) {
        if (!frame)
                return;
        long style = frame->GetWindowStyleFlag();
        long new_style = resizable ? (style | wxRESIZE_BORDER | wxMAXIMIZE_BOX) : (style & ~(wxRESIZE_BORDER | wxMAXIMIZE_BOX));
        if (style != new_style) {
                if (!resizable && frame->IsMaximized())
                        frame->Maximize(false);
                frame->SetWindowStyleFlag(new_style);
        }
}

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

        if (winsizex < MIN_WIDTH)
                winsizex = MIN_WIDTH;
        win_doresize = 1;
}

#ifdef _WIN32
static void sync_cursor_visibility() {
        static int cursor_hidden = 0;
        int hide = mousecapture && embedded;

        if (hide != cursor_hidden) {
                cursor_hidden = hide;
                ShowCursor(hide ? FALSE : TRUE);
        }
}
#endif

static void grab_input() {
        mousecapture = 1;
        if (embedded) {
                host_mouse_reset();
#ifdef _WIN32
                clip_cursor_to_canvas();
                main_thread_call([]() { sync_cursor_visibility(); });
#else
                main_thread_call([]() { canvas_capture_mouse(true); });
#endif
        } else {
                SDL_GetRelativeMouseState(0, 0);
                SDL_SetWindowGrab(window, SDL_TRUE);
                SDL_SetRelativeMouseMode(SDL_TRUE);
        }
}

extern "C" void releasemouse() {
        if (!mousecapture)
                return;
        mousecapture = 0;
        if (embedded) {
                host_mouse_reset();
#ifdef _WIN32
                ClipCursor(NULL);
                if (wxThread::IsMain())
                        sync_cursor_visibility();
                else
                        main_thread_call([]() { sync_cursor_visibility(); });
#else
                if (wxThread::IsMain())
                        canvas_capture_mouse(false);
                else
                        main_thread_call([]() { canvas_capture_mouse(false); });
#endif
        } else if (window) {
                SDL_SetWindowGrab(window, SDL_FALSE);
                SDL_SetRelativeMouseMode(SDL_FALSE);
        }
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
        static int atexit_registered = 0;

        frame = (Frame *)wnd_ptr;
        canvas = frame->GetCanvas();
        fullscreen_active = 0;
        infocus = 1;

        int width = 640, height = 480;
        if (vid_resize == 2) {
                width = custom_resolution_width;
                height = custom_resolution_height;
        } else if (vid_resize == 1 && window_remember && window_w > 0 && window_h > 0) {
                width = window_w;
                height = window_h;
        }
        frame_set_resizable(vid_resize == 1);
        canvas->Show();
        frame->SetClientSize(width, height);
        if (window_remember)
                frame->Move(window_x, window_y);
        else
                frame->Centre();
        frame->Show();
        frame->Raise();

        native_handle = get_native_handle(canvas);
        embedded = native_handle != NULL;
        input_hooked = embedded;
        if (embedded) {
                canvas->SetFocus();
                frame->Bind(wxEVT_ACTIVATE, on_frame_activate);
                frame->Bind(wxEVT_MOVE, on_frame_move);
#ifdef _WIN32
                get_registry_key_map();
                memset(rawinputkey, 0, sizeof(rawinputkey));
                rawinput_register((HWND)native_handle, 1);
                hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(NULL), 0);
#endif
        } else {
                pclog("display_start: can't render into the main window, using a separate SDL window\n");
                canvas->Hide();
                frame->SetClientSize(width, 0);
        }

        if (!atexit_registered) {
                atexit(releasemouse);
                atexit_registered = 1;
        }
        rendererMutex = SDL_CreateMutex();
        rendererCond = SDL_CreateCond();
        renderer_start();
}

extern "C" void display_stop() {
        renderer_stop(10 * 1000);

        SDL_DestroyMutex(rendererMutex);
        SDL_DestroyCond(rendererCond);
#if SDL_VERSION_ATLEAST(2, 0, 2)
        SDL_DetachThread(renderthread);
#endif
        releasemouse();

        if (input_hooked) {
                input_hooked = 0;
#ifdef _WIN32
                UnhookWindowsHookEx(hKeyboardHook);
                hKeyboardHook = NULL;
                rawinput_register(NULL, 0);
#endif
                frame->Unbind(wxEVT_ACTIVATE, on_frame_activate);
                frame->Unbind(wxEVT_MOVE, on_frame_move);
        }
        if (frame->IsFullScreen())
                frame->ShowFullScreen(false);
        fullscreen_active = 0;
        frame->Hide();
}

extern "C" void sdl_set_window_title(const char *title) {
        wxString s(title);
        main_thread_call([s]() {
                if (frame && !fullscreen_active)
                        frame->SetTitle(s);
        });
        if (!embedded && window && !is_fullscreen())
                SDL_SetWindowTitle(window, title);
}

static int get_border_size(int *top, int *left, int *bottom, int *right) {
#if SDL_VERSION_ATLEAST(2, 0, 5)
        return SDL_GetWindowBordersSize(window, top, left, bottom, right);
#else
        if (top)
                *top = 0;
        if (left)
                *left = 0;
        if (bottom)
                *bottom = 0;
        if (right)
                *right = 0;
        return 0;
#endif
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

static int sdl_scancode(SDL_Scancode scancode) {
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

static void window_setup() {
        SDL_SetHint(SDL_HINT_GRAB_KEYBOARD, embedded ? "0" : "1");
#if SDL_VERSION_ATLEAST(2, 0, 5)
        SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
#endif
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

static int window_create_embedded() {
        if (requested_render_driver.sdl_window_params & SDL_WINDOW_OPENGL) {
#ifdef SDL_HINT_VIDEO_FOREIGN_WINDOW_OPENGL
                SDL_SetHint(SDL_HINT_VIDEO_FOREIGN_WINDOW_OPENGL, "1");
#endif
                dummy_window = SDL_CreateWindow("GL3 test", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1, 1,
                                                SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL);
                if (dummy_window) {
                        char ptr[32];
                        snprintf(ptr, 31, "%p", dummy_window);
                        ptr[31] = 0;
                        SDL_SetHint(SDL_HINT_VIDEO_WINDOW_SHARE_PIXEL_FORMAT, ptr);
                }
        }

        window = SDL_CreateWindowFrom(native_handle);
        if (!window) {
                pclog("SDL_CreateWindowFrom failed: %s\n", SDL_GetError());
                return 0;
        }

        if (vid_resize)
                window_dosetresize = 1;
        return 1;
}

static int window_create() {
        if (embedded && !window_create_embedded()) {
                /* fall back to a separate SDL window */
                embedded = 0;
                main_thread_call([]() {
                        if (canvas)
                                canvas->Hide();
                        if (frame)
                                frame->SetClientSize(frame->GetClientSize().x, 0);
                });
        }

        if (!embedded) {
                window = SDL_CreateWindow("PCem Display", rect.x, rect.y, rect.w, rect.h,
                                          requested_render_driver.sdl_window_params |
                                                  (vid_resize == 1 ? SDL_WINDOW_RESIZABLE : 0));
                if (!window) {
                        char message[200];
                        sprintf(message, "SDL window could not be created! Error: %s\n", SDL_GetError());
                        wx_messagebox(frame, message, "SDL Error", WX_MB_OK);
                        return 0;
                }
        }

        render_time = 0;
        render_fps = 0;
        render_frame_time = SDL_GetTicks();
        render_frames = 0;
        return 1;
}

static void window_close() {
        sdl_renderer_close();

        if (window) {
                if (!embedded) {
                        SDL_GetWindowPosition(window, &rect.x, &rect.y);
                        SDL_GetWindowSize(window, &rect.w, &rect.h);
                        get_border_size(&border_y, &border_x, 0, 0);
                        rect.x -= border_x;
                        rect.y -= border_y;
                }

                SDL_DestroyWindow(window);
        }
        window = NULL;

        if (dummy_window) {
                SDL_SetHint(SDL_HINT_VIDEO_WINDOW_SHARE_PIXEL_FORMAT, "");
                SDL_DestroyWindow(dummy_window);
        }
        dummy_window = NULL;
}

static void handle_sdl_events() {
        while (SDL_PollEvent(&event)) {
                switch (event.type) {
                case SDL_MOUSEBUTTONUP:
                        if (embedded)
                                break;
                        if (!mousecapture) {
                                if (event.button.button == SDL_BUTTON_LEFT && !IS_PAUSED) {
                                        window_doinputgrab = 1;
                                        if (video_fullscreen)
                                                window_dofullscreen = 1;
                                }
                        } else if (event.button.button == SDL_BUTTON_MIDDLE)
                                release_on_middle_button();
                        break;
                case SDL_MOUSEWHEEL:
                        if (mousecapture && !embedded)
                                mouse_wheel_update(event.wheel.y);
                        break;
                case SDL_WINDOWEVENT:
                        if (event.window.event == SDL_WINDOWEVENT_RESIZED || event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                                device_force_redraw();
                        if (embedded)
                                break;

                        if (event.window.event == SDL_WINDOWEVENT_CLOSE)
                                wx_stop_emulation(frame);
                        if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                                if (is_fullscreen())
                                        window_dowindowed = 1;
                                window_doinputrelease = 1;
                        }

                        if (window_remember) {
                                int flags = SDL_GetWindowFlags(window);
                                if (!(flags & SDL_WINDOW_FULLSCREEN) && !(flags & SDL_WINDOW_FULLSCREEN_DESKTOP)) {
                                        if (event.window.event == SDL_WINDOWEVENT_MOVED) {
                                                get_border_size(&border_y, &border_x, 0, 0);
                                                window_x = event.window.data1 - border_x;
                                                window_y = event.window.data2 - border_y;
                                        } else if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                                                window_w = event.window.data1;
                                                window_h = event.window.data2;
                                        }
                                }
                        }
                        break;
                case SDL_KEYDOWN: {
                        if (embedded)
                                break;
                        int key_idx = sdl_scancode(event.key.keysym.scancode);
                        if (key_idx != -1)
                                rawinputkey[key_idx] = event.key.timestamp;
                        break;
                }
                case SDL_KEYUP: {
                        if (embedded)
                                break;
                        int key_idx = sdl_scancode(event.key.keysym.scancode);
                        if (key_idx != -1)
                                rawinputkey[key_idx] = 0;
                        break;
                }
                }
        }
}

static void handle_hotkeys() {
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
}

static int render() {
        uint64_t start_time = timer_read();
        uint64_t end_time;

        if (window_dosetresize) {
                window_dosetresize = 0;
                if (embedded) {
                        int resizable = vid_resize == 1;
                        int width = vid_resize == 2 ? custom_resolution_width : winsizex;
                        int height = vid_resize == 2 ? custom_resolution_height : winsizey;
                        main_thread_call([resizable, width, height]() {
                                frame_set_resizable(resizable);
                                if (!resizable)
                                        frame_set_display_size(width, height);
                        });
                } else {
#if SDL_VERSION_ATLEAST(2, 0, 5)
                        SDL_GetWindowSize(window, &rect.w, &rect.h);
                        SDL_SetWindowResizable(window, (SDL_bool)(vid_resize == 1));
                        SDL_SetWindowSize(window, rect.w, rect.h);
#else
                        window_doreset = 1;
#endif
                        if (vid_resize == 2)
                                SDL_SetWindowSize(window, custom_resolution_width, custom_resolution_height);
                }

                device_force_redraw();
        }
        if (window_doreset) {
                pclog("window_doreset\n");
                window_doreset = 0;
                renderer_doreset = 0;
                return 0;
        }
        if (renderer_doreset) {
                pclog("renderer_doreset\n");
                renderer_doreset = 0;
                sdl_renderer_close();
                sdl_renderer_init(window);

                device_force_redraw();
                video_wait_for_blit();
        }

        handle_sdl_events();
        handle_hotkeys();

        if (window_doremember) {
                window_doremember = 0;
                if (embedded) {
                        main_thread_call([]() {
                                if (!frame || !canvas)
                                        return;
                                if (!fullscreen_active) {
                                        wxPoint pos = frame->GetPosition();
                                        wxSize size = canvas->GetClientSize();
                                        window_x = pos.x;
                                        window_y = pos.y;
                                        window_w = size.x;
                                        window_h = size.y;
                                }
                                saveconfig(NULL);
                        });
                } else {
                        SDL_GetWindowPosition(window, &window_x, &window_y);
                        SDL_GetWindowSize(window, &window_w, &window_h);
                        get_border_size(&border_y, &border_x, 0, 0);
                        window_x -= border_x;
                        window_y -= border_y;
                        saveconfig(NULL);
                }
        }

        if (window_dotogglefullscreen) {
                window_dotogglefullscreen = 0;
                if (mousecapture || is_fullscreen()) {
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
                if (embedded) {
                        /* Borderless fullscreen of the whole frame, without menu and status bar */
                        if (!fullscreen_active) {
                                fullscreen_active = 1;
                                main_thread_call([]() {
                                        if (frame)
                                                frame->ShowFullScreen(true, wxFULLSCREEN_ALL);
                                        if (canvas)
                                                canvas->SetFocus();
#ifdef _WIN32
                                        /* the canvas moved, so move the cursor clip with it */
                                        if (mousecapture)
                                                clip_cursor_to_canvas();
#endif
                                });
                        }
                } else {
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
                        SDL_SetWindowFullscreen(window,
                                                video_fullscreen_mode == 0 ? SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_FULLSCREEN);
                }
                device_force_redraw();
        }
        if (window_doinputgrab) {
                window_doinputgrab = 0;
                grab_input();
        }

        if (window_doinputrelease) {
                window_doinputrelease = 0;
                releasemouse();
        }
        if (window_dowindowed) {
                window_dowindowed = 0;
                if (embedded) {
                        if (fullscreen_active) {
                                fullscreen_active = 0;
                                main_thread_call([]() {
                                        if (frame)
                                                frame->ShowFullScreen(false);
                                });
                        }
                } else {
                        SDL_SetWindowFullscreen(window, 0);
                        SDL_SetWindowSize(window, remembered_rect.w, remembered_rect.h);
                        SDL_SetWindowPosition(window, remembered_rect.x, remembered_rect.y);
#if SDL_VERSION_ATLEAST(2, 0, 4)
                        SDL_WarpMouseGlobal(remembered_mouse_x, remembered_mouse_y);
#endif
                }
                device_force_redraw();
        }

        if (win_doresize) {
                win_doresize = 0;
                if (embedded) {
                        if (!vid_resize) {
                                int width = winsizex, height = winsizey;
                                main_thread_call([width, height]() { frame_set_display_size(width, height); });
                        }
                } else {
                        int flags = SDL_GetWindowFlags(window);
                        if (!vid_resize || (flags & SDL_WINDOW_FULLSCREEN)) {
                                SDL_GetWindowSize(window, &rect.w, &rect.h);
                                if (rect.w != winsizex || rect.h != winsizey) {
                                        SDL_GetWindowPosition(window, &rect.x, &rect.y);
                                        SDL_SetWindowSize(window, winsizex, winsizey);
                                        SDL_SetWindowPosition(window, rect.x, rect.y);
                                        device_force_redraw();
                                }
                        }
                }
        }

#ifndef _WIN32
        if (embedded) {
                int w, h;
                wxSize size = canvas->GetClientSize();
                SDL_GetWindowSize(window, &w, &h);
                if (size.x > 0 && size.y > 0 && (w != size.x || h != size.y)) {
                        SDL_SetWindowSize(window, size.x, size.y);
                        device_force_redraw();
                }
        }
#endif

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

static int renderer_thread(void *params) {
        int internal_rendering;

        SDL_LockMutex(rendererMutex);
        render_thread_done = 0;
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
        render_thread_done = 1;
        SDL_CondSignal(rendererCond);
        SDL_UnlockMutex(rendererMutex);

        return SDL_TRUE;
}

void *timer = 0;

static void render_timer() {
#ifdef PCEM_RENDER_TIMER_LOOP
        /* For some reason this while-loop works on OSX, which also fixes missing events. No idea why though. */
        renderer_thread(0);
#else
        if (rendering && !render()) {
                window_close();
                window_create();
                renderer_doreset = 1;
        }
#endif
}

static void render_start_timer() {
#ifdef PCEM_RENDER_TIMER_LOOP
        timer = wx_createtimer(render_timer);
        wx_starttimer(timer, 500, 1);
#else
        timer = wx_createtimer(render_timer);
        window_setup();
        if (window_create()) {
                rendering = 1;
                renderer_doreset = 1;
                wx_starttimer(timer, 1, 0);
        }
#endif
}

extern "C" void renderer_start() {
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

extern "C" void renderer_stop(int timeout) {
#if defined(PCEM_RENDER_WITH_TIMER) && defined(PCEM_RENDER_TIMER_LOOP)
        rendering = 0;
        window_close();
        wx_destroytimer(timer);
#else
        if (rendering) {
                uint32_t start = SDL_GetTicks();

                SDL_LockMutex(rendererMutex);
                rendering = 0;
                while (!render_thread_done && (!timeout || SDL_GetTicks() - start < (uint32_t)timeout)) {
                        SDL_CondWaitTimeout(rendererCond, rendererMutex, 10);
#ifdef _WIN32
                        MSG msg;
                        SDL_UnlockMutex(rendererMutex);
                        PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE | PM_QS_SENDMESSAGE);
                        SDL_LockMutex(rendererMutex);
#endif
                }
                SDL_UnlockMutex(rendererMutex);
                renderthread = NULL;
        }
        if (timer)
                wx_destroytimer(timer);
        timer = 0;
#endif
}
