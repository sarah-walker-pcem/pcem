#define  _WIN32_WINNT 0x0501
#include <SDL2/SDL.h>
#include "video.h"
#include "wx-sdl2-video.h"
#include "wx-utils.h"
#include "ibm.h"
#include "mouse.h"
#include "wx-display.h"
#include "plat-keyboard.h"

#ifdef _WIN32
#define BITMAP WINDOWS_BITMAP
#undef UNICODE
#include <windows.h>
#include <windowsx.h>
#undef BITMAP
#endif

HHOOK hKeyboardHook;

static HINSTANCE hinstance = 0;
static HWND hwnd = 0;
static HMENU menu = 0;

#define szClassName "PCemMainWnd"
#define szSubClassName "PCemSubWnd"

static RAWINPUTDEVICE device;
static uint16_t scancode_map[65536];

SDL_mutex* rendererMutex;
SDL_cond* rendererCond;
SDL_Thread* renderthread = NULL;

SDL_Window* window = NULL;
SDL_Window* dummy_window = NULL;

int rendering = 0;

int mousecapture = 0;

extern int pause;
extern int video_scale;
extern int take_screenshot;

void* window_ptr;
void* menu_ptr;

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

/*Minimum window width to prevent menu wrapping.
  This is a horrible hack to work around issues with PCem, SDL2, and the
  AdjustWindowRectEx() function. Allowing the menu to wrap causes some quite odd
  window behaviour, mainly shooting to the bottom of the screen when you try to
  move it.
  This hard coded limit is highly fragile and will be replaced by a proper fix as
  soon as I work out what that should be!*/
#define MIN_WIDTH 360

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

        if (winsizex < MIN_WIDTH)
                winsizex = MIN_WIDTH;
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

int is_fullscreen()
{
        if (window)
        {
                int flags = SDL_GetWindowFlags(window);
                return (flags&SDL_WINDOW_FULLSCREEN) || (flags&SDL_WINDOW_FULLSCREEN_DESKTOP);
        }
        return 0;
}

/* This is so we can disambiguate scan codes that would otherwise conflict and get
   passed on incorrectly. */
UINT16 convert_scan_code(UINT16 scan_code)
{
        switch (scan_code)
        {
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

void get_registry_key_map()
{
        char *keyName = "SYSTEM\\CurrentControlSet\\Control\\Keyboard Layout";
        char *valueName = "Scancode Map";
        char buf[32768];
        DWORD bufSize;
        HKEY hKey;
        int j;

        /* First, prepare the default scan code map list which is 1:1.
           Remappings will be inserted directly into it.
           65536 bytes so scan codes fit in easily and it's easy to find what each maps too,
           since each array element is a scan code and provides for E0, etc. ones too. */
        for (j = 0; j < 65536; j++)
                scancode_map[j] = convert_scan_code(j);

        bufSize = 32768;
        pclog("Preparing scan code map list...\n");
        /* Get the scan code remappings from:
           HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Keyboard Layout */
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, keyName, 0, 1, &hKey) == ERROR_SUCCESS)
        {
                if(RegQueryValueEx(hKey, valueName, NULL, NULL, (LPBYTE)buf, &bufSize) == ERROR_SUCCESS)
                {
                        UINT32 *bufEx2 = (UINT32 *) buf;
                        int scMapCount = bufEx2[2];
                        pclog("%lu scan code mappings found!\n", scMapCount);
                        if ((bufSize != 0) && (scMapCount != 0))
                        {
                                UINT16 *bufEx = (UINT16 *) (buf + 12);
                                pclog("More than zero scan code mappings found, processing...\n");
                                for (j = 0; j < scMapCount*2; j += 2)
                                {
                                        /* Each scan code is 32-bit: 16 bits of remapped scan code,
                                           and 16 bits of original scan code. */
                                        int scancode_unmapped = bufEx[j + 1];
                                        int scancode_mapped = bufEx[j];

                                        scancode_mapped = convert_scan_code(scancode_mapped);

                                        scancode_map[scancode_unmapped] = scancode_mapped;
                                        pclog("Scan code mapping %u detected: %X -> %X\n", scancode_unmapped, scancode_mapped, scancode_map[scancode_unmapped]);
                                }
                                pclog("Done processing!\n");
                        }
                }
                RegCloseKey(hKey);
        }
        pclog("Done preparing!\n");
}

LRESULT CALLBACK LowLevelKeyboardProc( int nCode, WPARAM wParam, LPARAM lParam )
{
        if (nCode < 0 || nCode != HC_ACTION || (!mousecapture && !video_fullscreen))
                return CallNextHookEx( hKeyboardHook, nCode, wParam, lParam);

        KBDLLHOOKSTRUCT* p = (KBDLLHOOKSTRUCT*)lParam;

        if (p->vkCode == VK_TAB && p->flags & LLKHF_ALTDOWN) return 1; //disable alt-tab
        if (p->vkCode == VK_SPACE && p->flags & LLKHF_ALTDOWN) return 1; //disable alt-tab
        if((p->vkCode == VK_LWIN) || (p->vkCode == VK_RWIN)) return 1;//disable windows keys
        if (p->vkCode == VK_ESCAPE && p->flags & LLKHF_ALTDOWN) return 1;//disable alt-escape
        BOOL bControlKeyDown = GetAsyncKeyState (VK_CONTROL) >> ((sizeof(SHORT) * 8) - 1);//checks ctrl key pressed
        if (p->vkCode == VK_ESCAPE && bControlKeyDown) return 1; //disable ctrl-escape

        return CallNextHookEx( hKeyboardHook, nCode, wParam, lParam );
}

LRESULT CALLBACK WindowProcedure (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
        switch (message)
        {
        case WM_COMMAND:
                /* pass through commands to wx window */
                wx_winsendmessage(window_ptr, message, wParam, lParam);
                return 0;
                break;
        case WM_INPUT:
        {
                UINT size;
                RAWINPUT *raw;

                if (!infocus)
                        break;

                GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &size, sizeof(RAWINPUTHEADER));

                raw = malloc(size);

                /* Here we read the raw input data for the keyboard */
                GetRawInputData((HRAWINPUT)(lParam), RID_INPUT, raw, &size, sizeof(RAWINPUTHEADER));

                /* If the input is keyboard, we process it */
                if (raw->header.dwType == RIM_TYPEKEYBOARD)
                {
                        const RAWKEYBOARD rawKB = raw->data.keyboard;
                        USHORT scancode = rawKB.MakeCode;

                        // pclog("Keyboard input received: S:%X VK:%X F:%X\n", c, d, e);

                        /* If it's not a scan code that starts with 0xE1 */
                        if (!(rawKB.Flags & RI_KEY_E1))
                        {
                                if (rawKB.Flags & RI_KEY_E0)
                                        scancode |= (0xE0 << 8);

                                /* Remap it according to the list from the Registry */
                                scancode = scancode_map[scancode];

                                if ((scancode >> 8) == 0xF0)
                                        scancode |= 0x100; /* Extended key code in disambiguated format */
                                else if ((scancode >> 8) == 0xE0)
                                        scancode |= 0x80; /* Normal extended key code */

                                /* If it's not 0 (therefore not 0xE1, 0xE2, etc),
                                   then pass it on to the rawinputkey array */
                                if (!(scancode & 0xf00))
                                        rawinputkey[scancode & 0x1ff] = !(rawKB.Flags & RI_KEY_BREAK);
                        }
                }
                free(raw);

        }
        break;
        case WM_SETFOCUS:
                infocus=1;
        break;
        case WM_KILLFOCUS:
                infocus=0;
                if (is_fullscreen())
                        window_dowindowed = 1;
                window_doinputrelease = 1;
                memset(rawinputkey, 0, sizeof(rawinputkey));
        break;
        case WM_CLOSE:
        case WM_DESTROY:
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP:
                return 0;

        case WM_SYSCOMMAND:
                if (wParam == SC_KEYMENU && HIWORD(lParam) <= 0 && (video_fullscreen || mousecapture))
                        return 0; /*disable ALT key for menu*/
                break;
        default:
                break;
        }
        return DefWindowProc (hwnd, message, wParam, lParam);
}

LRESULT CALLBACK subWindowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
        switch (message)
        {
                default:
                return DefWindowProc(hwnd, message, wParam, lParam);
        }
        return 0;
}

int display_init()
{
        SDL_SetHint(SDL_HINT_WINDOWS_DISABLE_THREAD_NAMING, "1");
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

void display_start(void* wnd_ptr)
{
        window_ptr = wnd_ptr;
        menu_ptr = wx_getmenu(wnd_ptr);

        get_registry_key_map();

        infocus = 1;

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
        SDL_DetachThread(renderthread);
        releasemouse();

}

void sdl_set_window_title(const char* title) {
        if (hwnd && !is_fullscreen())
                SetWindowText(hwnd, title);
}

int get_border_size(int* top, int* left, int* bottom, int* right)
{
        int res = SDL_GetWindowBordersSize(window, top, left, bottom, right);
        if (top) *top -= GetSystemMetrics(SM_CYMENUSIZE);

        return res;
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
                { SDL_SCANCODE_KP_4, 0x48 },
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
        SDL_SetHint(SDL_HINT_GRAB_KEYBOARD, "0");
        SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
        SDL_SetHint(SDL_HINT_WINDOWS_NO_CLOSE_ON_ALT_F4, "1");

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

int window_create()
{
        int i;
        WNDCLASSEX wincl;        /* Data structure for the windowclass */

        hinstance = (HINSTANCE)GetModuleHandle(NULL);
        /* The Window structure */
        wincl.hInstance = hinstance;
        wincl.lpszClassName = szClassName;
        wincl.lpfnWndProc = WindowProcedure;      /* This function is called by windows */
        wincl.style = CS_DBLCLKS;                 /* Catch double-clicks */
        wincl.cbSize = sizeof (WNDCLASSEX);

        /* Use default icon and mouse-pointer */
        wincl.hIcon = LoadIcon (NULL, IDI_APPLICATION);
        wincl.hIconSm = LoadIcon (NULL, IDI_APPLICATION);
        wincl.hCursor = NULL;//LoadCursor (NULL, IDC_ARROW);
        wincl.lpszMenuName = NULL;                 /* No menu */
        wincl.cbClsExtra = 0;                      /* No extra bytes after the window class */
        wincl.cbWndExtra = 0;                      /* structure or the window instance */
        /* Use Windows's default color as the background of the window */
        wincl.hbrBackground = (HBRUSH) COLOR_BACKGROUND;

        /* Register the window class, and if it fails quit the program */
        if (!RegisterClassEx(&wincl))
                return 0;

        wincl.lpszClassName = szSubClassName;
        wincl.lpfnWndProc = subWindowProcedure;      /* This function is called by windows */

        if (!RegisterClassEx(&wincl))
                return 0;

        menu = CreateMenu();
        HMENU native_menu = (HMENU)wx_getnativemenu(menu_ptr);
        int count = GetMenuItemCount(native_menu);
        MENUITEMINFO info;
        for (i = 0; i < count; ++i)
        {
                char label[256];
                memset(&info, 0, sizeof(MENUITEMINFO));
                info.cbSize = sizeof(MENUITEMINFO);
                info.fMask = MIIM_TYPE | MIIM_ID;
                info.fType = MFT_STRING;
                info.cch = 256;
                info.dwTypeData = label;
                if (GetMenuItemInfo(native_menu, i, 1, &info))
                        AppendMenu(menu, MF_STRING | MF_POPUP, (UINT)GetSubMenu(native_menu, i), info.dwTypeData);
        }

        /* The class is registered, let's create the program*/
        hwnd = CreateWindowEx (
                0,                   /* Extended possibilites for variation */
                szClassName,         /* Classname */
                "PCem v16",          /* Title Text */
                WS_OVERLAPPEDWINDOW&~WS_SIZEBOX, /* default window */
                CW_USEDEFAULT,       /* Windows decides the position */
                CW_USEDEFAULT,       /* where the window ends up on the screen */
                640+(GetSystemMetrics(SM_CXFIXEDFRAME)*2),                 /* The programs width */
                480+(GetSystemMetrics(SM_CYFIXEDFRAME)*2)+GetSystemMetrics(SM_CYMENUSIZE)+GetSystemMetrics(SM_CYCAPTION)+1,                 /* and height in pixels */
                HWND_DESKTOP,        /* The window is a child-window to desktop */
                menu,                /* Menu */
                hinstance,           /* Program Instance handler */
                NULL                 /* No Window Creation data */
        );

        /* Make the window visible on the screen */
        ShowWindow(hwnd, 1);

        memset(rawinputkey, 0, sizeof(rawinputkey));
        device.usUsagePage = 0x01;
        device.usUsage = 0x06;
        device.dwFlags = RIDEV_NOHOTKEYS;
        device.hwndTarget = hwnd;

        if (RegisterRawInputDevices(&device, 1, sizeof(device)))
                pclog("Raw input registered!\n");
        else
                pclog("Raw input registration failed!\n");

        hKeyboardHook = SetWindowsHookEx( WH_KEYBOARD_LL,  LowLevelKeyboardProc, GetModuleHandle(NULL), 0 );

        if (requested_render_driver.sdl_window_params & SDL_WINDOW_OPENGL)
        {
                /* create dummy window for opengl */
                dummy_window = SDL_CreateWindow("GL3 test", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1, 1, SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL);
                if (dummy_window)
                {
                        char ptr[32];
                        snprintf(ptr, 31, "%p", dummy_window);
                        ptr[31] = 0;
                        SDL_SetHint(SDL_HINT_VIDEO_WINDOW_SHARE_PIXEL_FORMAT, ptr);
                }
        }
        window = SDL_CreateWindowFrom(hwnd);
        if (!window)
        {
                char message[200];
                sprintf(message,
                                "SDL window could not be created! Error: %s\n",
                                SDL_GetError());
                wx_messagebox(window_ptr, message, "SDL Error", WX_MB_OK);
                return 0;
        }

        SDL_SetWindowPosition(window, rect.x, rect.y);
        SDL_SetWindowSize(window, rect.w, rect.h);

        if (vid_resize)
                window_dosetresize = 1;

        render_time = 0;
        render_fps = 0;
        render_frame_time = SDL_GetTicks();
        render_frames = 0;
        return 1;
}

void window_close()
{
        int i;
        sdl_renderer_close();

        int count = GetMenuItemCount(menu);
        for (i = 0; i < count; ++i)
                RemoveMenu(menu, 0, MF_BYPOSITION);
        DestroyMenu(menu);
        menu = 0;
        SetMenu(hwnd, 0);

        if (window) {
                SDL_GetWindowPosition(window, &rect.x, &rect.y);
                SDL_GetWindowSize(window, &rect.w, &rect.h);
                get_border_size(&border_y, &border_x, 0, 0);
                rect.x -= border_x;
                rect.y -= border_y;

                SDL_DestroyWindow(window);
        }
        window = NULL;

        if (dummy_window)
        {
                SDL_SetHint(SDL_HINT_VIDEO_WINDOW_SHARE_PIXEL_FORMAT, "");
                SDL_DestroyWindow(dummy_window);
        }
        dummy_window = NULL;

        UnhookWindowsHookEx( hKeyboardHook );
        DestroyWindow(hwnd);
        hwnd = NULL;

        UnregisterClass(szSubClassName, hinstance);
        UnregisterClass(szClassName, hinstance);

}

int render()
{
        uint64_t start_time = timer_read();
        uint64_t end_time;

        if (window_doreset)
        {
                pclog("window_doreset\n");
                window_doreset = 0;
                renderer_doreset = 0;
                return 0;
        }
        if (window_dosetresize)
        {
                window_dosetresize = 0;
                SDL_GetWindowSize(window, &rect.w, &rect.h);
                SDL_SetWindowResizable(window, vid_resize == 1);
                SDL_SetWindowSize(window, rect.w, rect.h);

                if (vid_resize == 2)
                        SDL_SetWindowSize(window, custom_resolution_width, custom_resolution_height);

                device_force_redraw();
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
                        }
                        else if (event.button.button == SDL_BUTTON_MIDDLE && !is_fullscreen() && !(mouse_get_type(mouse_type) & MOUSE_TYPE_3BUTTON))
                                window_doinputrelease = 1;
                        break;
                case SDL_MOUSEWHEEL:
                        if (mousecapture) mouse_wheel_update(event.wheel.y);
                        break;
                case SDL_WINDOWEVENT:
                        if (event.window.event == SDL_WINDOWEVENT_CLOSE)
                                wx_stop_emulation(window_ptr);
                        if (event.window.event == SDL_WINDOWEVENT_RESIZED)
                                device_force_redraw();

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
                SetMenu(hwnd, 0);
                video_wait_for_blit();
                SDL_RaiseWindow(window);
                SDL_GetGlobalMouseState(&remembered_mouse_x, &remembered_mouse_y);
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
        if (mousecapture)
        {
                SDL_Rect rect;
                SDL_GetWindowSize(window, &rect.w, &rect.h);
                SDL_WarpMouseInWindow(window, rect.w/2, rect.h/2);
        }

        if (window_doinputrelease) {
                window_doinputrelease = 0;
                releasemouse();
        }
        if (window_dowindowed)
        {
                window_dowindowed = 0;
                SetMenu(hwnd, menu);
                SDL_SetWindowFullscreen(window, 0);
                SDL_SetWindowSize(window, remembered_rect.w, remembered_rect.h);
                SDL_SetWindowPosition(window, remembered_rect.x, remembered_rect.y);
                SDL_WarpMouseGlobal(remembered_mouse_x, remembered_mouse_y);
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
