#include "wx-sdl2.h"

#include <SDL2/SDL.h>
#include <wx/defs.h>

#ifdef __WINDOWS__
#define BITMAP WINDOWS_BITMAP
#include <windows.h>
#include <windowsx.h>
#undef BITMAP
#endif


#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "ibm.h"
#include "cdrom-ioctl.h"
#include "cdrom-image.h"
#include "config.h"
#include "video.h"
#include "cpu.h"
#include "ide.h"
#include "model.h"
#include "mouse.h"
#include "nvr.h"
#include "sound.h"
#include "thread.h"
#include "disc.h"
#include "disc_img.h"
#include "mem.h"

#include "wx-sdl2-video.h"
#include "wx-utils.h"
#include "wx-common.h"
#include "wx-display.h"

#define ID_IS(s) wParam == wx_xrcid(s)
#define ID_RANGE(a, b) wParam >= wx_xrcid(a) && wParam <= wx_xrcid(b)

#define IDM_CDROM_REAL 1500

uint64_t timer_freq;


int gfx_present[GFX_MAX];

SDL_mutex* ghMutex;
SDL_mutex* mainMutex;
SDL_cond* mainCond;

SDL_Thread* mainthreadh = NULL;

SDL_TimerID onesectimer;

int running = 0;

int drawits = 0;

int romspresent[ROM_MAX];
int quited = 0;

SDL_Rect oldclip;

void* ghwnd = 0;

void* menu;

emulation_state_t emulation_state = EMULATION_STOPPED;
int pause = 0;

int window_doreset = 0;
int renderer_doreset = 0;
int window_dofullscreen = 0;
int window_dowindowed = 0;
int window_doremember = 0;
int window_doinputgrab = 0;
int window_doinputrelease = 0;
int window_dotogglefullscreen = 0;

int video_scale = 1;

int video_width = 640;
int video_height = 480;

char menuitem[60];

extern void add_config_callback(void(*loadconfig)(), void(*saveconfig)(), void(*onloaded)());

extern int config_selection_open(void* hwnd, int inited);

extern void sdl_set_window_title(const char* title);

void warning(const char *format, ...)
{
        char buf[1024];
        va_list ap;

        va_start(ap, format);
        vsprintf(buf, format, ap);
        va_end(ap);

        wx_messagebox(ghwnd, buf, "PCem", WX_MB_OK);
}

void updatewindowsize(int x, int y)
{
        video_width = x;
        video_height = y;

        display_resize(x, y);
}

void startblit()
{
        SDL_LockMutex(ghMutex);
}

void endblit()
{
        SDL_UnlockMutex(ghMutex);
}

void enter_fullscreen()
{
        window_dofullscreen = window_doinputgrab = 1;
}

void leave_fullscreen()
{
        window_dowindowed = window_doinputrelease = 1;
}

void toggle_fullscreen()
{
        window_dotogglefullscreen = 1;
}

uint64_t main_time;

int mainthread(void* param)
{
        int i;

        SDL_SetThreadPriority(SDL_THREAD_PRIORITY_HIGH);

        int t = 0;
        int frames = 0;
        uint32_t old_time, new_time;

        drawits = 0;
        old_time = SDL_GetTicks();
        running = 1;
        while (running)
        {
                new_time = SDL_GetTicks();
                drawits += new_time - old_time;
                old_time = new_time;

                if (drawits > 0 && !pause)
                {
                        uint64_t start_time = timer_read();
                        uint64_t end_time;
                        drawits -= 10;
                        if (drawits > 50)
                                drawits = 0;
                        runpc();
                        frames++;
                        if (frames >= 200 && nvr_dosave)
                        {
                                frames = 0;
                                nvr_dosave = 0;
                                savenvr();
                        }
                        end_time = timer_read();
                        main_time += end_time - start_time;
                }
                else
                        SDL_Delay(1);
        }

        SDL_LockMutex(mainMutex);
        SDL_CondSignal(mainCond);
        SDL_UnlockMutex(mainMutex);

        return TRUE;
}

void get_executable_name(char *s, int size)
{
        char* path = SDL_GetBasePath();
        strcpy(s, path);
}

void set_window_title(const char *s)
{
        sdl_set_window_title(s);
}

uint64_t timer_read()
{
        return SDL_GetPerformanceCounter();
}

Uint32 timer_onesec(Uint32 interval, void* param)
{
        onesec();
        return interval;
}

void sdl_loadconfig()
{
        video_fullscreen = config_get_int(CFG_GLOBAL, "SDL2", "fullscreen", video_fullscreen);
        video_fullscreen_mode = config_get_int(CFG_GLOBAL, "SDL2", "fullscreen_mode", video_fullscreen_mode);
        video_scale = config_get_int(CFG_GLOBAL, "SDL2", "scale", video_scale);
        video_scale_mode = config_get_int(CFG_GLOBAL, "SDL2", "scale_mode", video_scale_mode);
        video_vsync = config_get_int(CFG_GLOBAL, "SDL2", "vsync", video_vsync);
        video_focus_dim = config_get_int(CFG_GLOBAL, "SDL2", "focus_dim", video_focus_dim);
        requested_render_driver = sdl_get_render_driver_by_name(config_get_string(CFG_GLOBAL, "SDL2", "render_driver", ""), RENDERER_SOFTWARE);
}

void sdl_saveconfig()
{
        config_set_int(CFG_GLOBAL, "SDL2", "fullscreen", video_fullscreen);
        config_set_int(CFG_GLOBAL, "SDL2", "fullscreen_mode", video_fullscreen_mode);
        config_set_int(CFG_GLOBAL, "SDL2", "scale", video_scale);
        config_set_int(CFG_GLOBAL, "SDL2", "scale_mode", video_scale_mode);
        config_set_int(CFG_GLOBAL, "SDL2", "vsync", video_vsync);
        config_set_int(CFG_GLOBAL, "SDL2", "focus_dim", video_focus_dim);
        config_set_string(CFG_GLOBAL, "SDL2", "render_driver", (char*)requested_render_driver.sdl_id);
}

void update_cdrom_menu(void* hmenu)
{
        if (!cdrom_enabled)
                wx_checkmenuitem(menu, WX_ID("IDM_CDROM_DISABLED"), WX_MB_CHECKED);
        else
        {
                if (cdrom_drive == CDROM_IMAGE)
                        wx_checkmenuitem(menu, WX_ID("IDM_CDROM_IMAGE"), WX_MB_CHECKED);
                else if (cdrom_drive > 0)
                        wx_checkmenuitem(menu, IDM_CDROM_REAL+cdrom_drive, WX_MB_CHECKED);
                else
                        wx_checkmenuitem(menu, WX_ID("IDM_CDROM_EMPTY"), WX_MB_CHECKED);
        }
}

void wx_initmenu()
{
        char s[32];
        menu = wx_getmenu(ghwnd);

        void* cdrom_submenu = wx_getsubmenu(menu, WX_ID("IDM_CDROM"));

#ifdef __WINDOWS__
        int c;
        /* Loop through each Windows drive letter and test to see if
           it's a CDROM */
        for (c='A';c<='Z';c++)
        {
                sprintf(s,"%c:\\",c);
                if (GetDriveType(s)==DRIVE_CDROM)
                {
                        sprintf(s, "Host CD/DVD Drive (%c:)", c);
                        wx_appendmenu(cdrom_submenu, IDM_CDROM_REAL+(c-'A'), s, wxITEM_RADIO);
                }
        }
#elif __linux__
        wx_appendmenu(cdrom_submenu, IDM_CDROM_REAL, "Host CD/DVD Drive (/dev/cdrom)", wxITEM_RADIO);
#endif

}

void wx_setupmenu()
{
        int c;
        update_cdrom_menu(menu);
        if (vid_resize)
                wx_checkmenuitem(menu, WX_ID("IDM_VID_RESIZE"), WX_MB_CHECKED);
        sprintf(menuitem, "IDM_VID_FS[%d]", video_fullscreen_scale);
        wx_checkmenuitem(menu, WX_ID(menuitem), WX_MB_CHECKED);
        wx_checkmenuitem(menu, WX_ID("IDM_VID_FULLSCREEN"), video_fullscreen);
        wx_checkmenuitem(menu, WX_ID("IDM_VID_REMEMBER"),
                        window_remember ? WX_MB_CHECKED : WX_MB_UNCHECKED);
        wx_checkmenuitem(menu, WX_ID("IDM_BPB_DISABLE"), bpb_disable ? WX_MB_CHECKED : WX_MB_UNCHECKED);

        sprintf(menuitem, "IDM_VID_SCALE_MODE[%d]", video_scale_mode);
        wx_checkmenuitem(menu, WX_ID(menuitem), WX_MB_CHECKED);
        sprintf(menuitem, "IDM_VID_SCALE[%d]", video_scale);
        wx_checkmenuitem(menu, WX_ID(menuitem), WX_MB_CHECKED);
        sprintf(menuitem, "IDM_VID_FS_MODE[%d]", video_fullscreen_mode);
        wx_checkmenuitem(menu, WX_ID(menuitem), WX_MB_CHECKED);
        wx_checkmenuitem(menu, WX_ID("IDM_VID_VSYNC"), video_vsync);
        wx_checkmenuitem(menu, WX_ID("IDM_VID_LOST_FOCUS_DIM"), video_focus_dim);

        int num_renderers;
        sdl_render_driver* drivers = sdl_get_render_drivers(&num_renderers);
        for (c = 1; c < num_renderers; ++c)
        {
                sprintf(menuitem, "IDM_VID_RENDER_DRIVER[%d]", drivers[c].id);
                wx_enablemenuitem(menu, WX_ID(menuitem), 0);
        }
        SDL_RendererInfo renderInfo;
        for (c = 0; c < SDL_GetNumRenderDrivers(); ++c)
        {
                SDL_GetRenderDriverInfo(c, &renderInfo);
                sdl_render_driver* driver = sdl_get_render_driver_by_name_ptr(renderInfo.name);

                if (driver)
                {
                        pclog("Renderer: %s (%d)\n", renderInfo.name, driver->id);
                        sprintf(menuitem, "IDM_VID_RENDER_DRIVER[%d]", driver->id);
                        wx_enablemenuitem(menu, WX_ID(menuitem), 1);
                }
        }
        sprintf(menuitem, "IDM_VID_RENDER_DRIVER[%d]", requested_render_driver.id);
        wx_checkmenuitem(menu, WX_ID(menuitem), WX_MB_CHECKED);
}

void sdl_onconfigloaded()
{
        if (ghwnd)
                wx_callback(ghwnd, wx_setupmenu);
}

extern void wx_loadconfig();
extern void wx_saveconfig();

int pc_main(int argc, char** argv)
{
#ifndef __APPLE__
        display_init();
#endif
        sdl_video_init();

        add_config_callback(sdl_loadconfig, sdl_saveconfig, sdl_onconfigloaded);
        add_config_callback(wx_loadconfig, wx_saveconfig, 0);

        getpath();

        sound_init();

        initpc(argc, argv);
        resetpchard();

        return TRUE;
}

int wx_load_config(void* hwnd)
{
        if (!config_override)
        {
                if (!config_selection_open(NULL, 0))
                        return FALSE;
        }

        return TRUE;
}

int wx_start(void* hwnd)
{
        int c, d;
        ghwnd = hwnd;

#ifdef __APPLE__
        /* OSX requires SDL to be initialized after wxWidgets. */
        display_init();
#endif

        readflash = 0;

        wx_initmenu();
        wx_setupmenu();

        d = romset;
        for (c = 0; c < ROM_MAX; c++)
        {
                romset = c;
                romspresent[c] = loadbios();
                pclog("romset %i - %i\n", c, romspresent[c]);
        }

        for (c = 0; c < ROM_MAX; c++)
        {
                if (romspresent[c])
                        break;
        }
        if (c == ROM_MAX)
        {
                wx_messagebox(hwnd,
                                "No ROMs present!\nYou must have at least one romset to use PCem.",
                                "PCem fatal error", WX_MB_OK);
                return 0;
        }

        romset = d;
        c = loadbios();

        if (!c)
        {
                if (romset != -1)
                        wx_messagebox(hwnd,
                                        "Configured romset not available.\nDefaulting to available romset.",
                                        "PCem error", WX_MB_OK);
                for (c = 0; c < ROM_MAX; c++)
                {
                        if (romspresent[c])
                        {
                                romset = c;
                                model = model_getmodel(romset);
                                saveconfig(NULL);
                                resetpchard();
                                break;
                        }
                }
        }

        for (c = 0; c < GFX_MAX; c++)
                gfx_present[c] = video_card_available(video_old_to_new(c));

        if (!video_card_available(video_old_to_new(gfxcard)))
        {
                if (romset != -1)
                        wx_messagebox(hwnd,
                                        "Configured video BIOS not available.\nDefaulting to available romset.",
                                        "PCem error", WX_MB_OK);
                for (c = GFX_MAX - 1; c >= 0; c--)
                {
                        if (gfx_present[c])
                        {
                                gfxcard = c;
                                saveconfig(NULL);
                                resetpchard();
                                break;
                        }
                }
        }

        return TRUE;
}

int resume_emulation()
{
        if (emulation_state == EMULATION_PAUSED)
        {
                emulation_state = EMULATION_RUNNING;
                pause = 0;
                return TRUE;
        }
        return FALSE;
}

int start_emulation(void* params)
{
        if (resume_emulation())
                return TRUE;
        pclog("Starting emulation...\n");
        loadconfig(NULL);

        emulation_state = EMULATION_RUNNING;
        pause = 0;

        ghMutex = SDL_CreateMutex();
        mainMutex = SDL_CreateMutex();
        mainCond = SDL_CreateCond();

        loadbios();
        resetpchard();

        display_start(params);
        mainthreadh = SDL_CreateThread(mainthread, "Main Thread", NULL);

        onesectimer = SDL_AddTimer(1000, timer_onesec, NULL);

        updatewindowsize(640, 480);

        timer_freq = timer_read();

        if (show_machine_on_start)
                wx_show_status(ghwnd);

        return TRUE;
}

int pause_emulation()
{
        pclog("Emulation paused.\n");
        emulation_state = EMULATION_PAUSED;
        pause = 1;
        return TRUE;
}

int stop_emulation()
{
        emulation_state = EMULATION_STOPPED;
        pclog("Stopping emulation...\n");
        SDL_LockMutex(mainMutex);
        running = 0;
        SDL_CondWaitTimeout(mainCond, mainMutex, 10 * 1000);
        SDL_UnlockMutex(mainMutex);

        SDL_DestroyCond(mainCond);
        SDL_DestroyMutex(mainMutex);

        startblit();
        display_stop();

#if SDL_VERSION_ATLEAST(2, 0, 2)
        SDL_DetachThread(mainthreadh);
#endif
        mainthreadh = NULL;
        SDL_RemoveTimer(onesectimer);
        savenvr();
        saveconfig(NULL);

        endblit();
        SDL_DestroyMutex(ghMutex);

        pclog("Emulation stopped.\n");

        wx_close_status(ghwnd);

        return TRUE;
}

void reset_emulation()
{
        pause_emulation();
        SDL_Delay(100);
        resetpchard();
        resume_emulation();
}

int wx_stop()
{
        pclog("Shutting down...\n");
        closepc();
        display_close();
        sdl_video_close();

        printf("Shut down successfully!\n");
        return TRUE;
}

char openfilestring[260];
int getfile(void* hwnd, char *f, char *fn)
{
        int ret = wx_filedialog(hwnd, "Open", fn, f, 0, 1, openfilestring);
#ifdef __APPLE__
        /* wxWidgets on OSX may mess up the SDL-window somehow, so just in case we reset it here */
        window_doreset = 1;
#endif
        return ret;
}

int getsfile(void* hwnd, char *f, char *fn, char *dir, char *ext)
{
        int ret = wx_filedialog(hwnd, "Save", dir, f, ext, 0, openfilestring);
#ifdef __APPLE__
        window_doreset = 1;
#endif
        return ret;
}

void atapi_close(void)
{
        switch (cdrom_drive)
        {
        case CDROM_IMAGE:
                image_close();
                break;
        default:
                ioctl_close();
                break;
        }
}

int wx_handle_command(void* hwnd, int wParam, int checked)
{
        SDL_Rect rect;
        void* hmenu;
        char temp_image_path[1024];
        int new_cdrom_drive;
        hmenu = wx_getmenu(hwnd);

        if (ID_IS("IDM_STATUS"))
        {
                wx_show_status(hwnd);
        }
        else if (ID_IS("IDM_FILE_RESET"))
        {
                pause = 1;
                SDL_Delay(100);
                savenvr();
                resetpc();
                pause = 0;
        }
        else if (ID_IS("IDM_FILE_HRESET"))
        {
                pause = 1;
                SDL_Delay(100);
                savenvr();
                resetpchard();
                pause = 0;
        }
        else if (ID_IS("IDM_FILE_RESET_CAD"))
        {
                pause = 1;
                SDL_Delay(100);
                savenvr();
                resetpc_cad();
                pause = 0;
        }
        else if (ID_IS("IDM_FILE_EXIT"))
        {
//                wx_exit(hwnd, 0);
                wx_stop_emulation(hwnd);
        }
        else if (ID_IS("IDM_DISC_A"))
        {
                if (!getfile(hwnd,
                                "Disc image (*.img;*.ima;*.fdi)|*.img;*.ima;*.fdi|All files (*.*)|*.*",
                                discfns[0]))
                {
                        disc_close(0);
                        disc_load(0, openfilestring);
                        saveconfig(NULL);
                }
        }
        else if (ID_IS("IDM_DISC_B"))
        {
                if (!getfile(hwnd,
                                "Disc image (*.img;*.ima;*.fdi)|*.img;*.ima;*.fdi|All files (*.*)|*.*",
                                discfns[1]))
                {
                        disc_close(1);
                        disc_load(1, openfilestring);
                        saveconfig(NULL);
                }
        }
        else if (ID_IS("IDM_EJECT_A"))
        {
                disc_close(0);
                saveconfig(NULL);
        }
        else if (ID_IS("IDM_EJECT_B"))
        {
                disc_close(1);
                saveconfig(NULL);
        }
        else if (ID_IS("IDM_BPB_DISABLE"))
        {
                bpb_disable = checked;
                wx_checkmenuitem(hmenu, WX_ID("IDM_BPB_DISABLE"), bpb_disable ? WX_MB_CHECKED : WX_MB_UNCHECKED);
                saveconfig(NULL);
        }
        else if (ID_IS("IDM_MACHINE_TOGGLE"))
        {
                if (emulation_state != EMULATION_STOPPED)
                        wx_togglewindow(hwnd);
        }
        else if (ID_IS("IDM_VID_RESIZE"))
        {
                vid_resize = checked;
                window_doreset = 1;
                saveconfig(NULL);
        }
        else if (ID_IS("IDM_VID_REMEMBER"))
        {
                window_remember = checked;
                wx_checkmenuitem(hmenu, WX_ID("IDM_VID_REMEMBER"),
                                window_remember ? WX_MB_CHECKED : WX_MB_UNCHECKED);
                window_doremember = 1;
                saveconfig(NULL);
        }
        else if (ID_IS("IDM_VID_FULLSCREEN"))
        {
                if (video_fullscreen_first)
                {
                        video_fullscreen_first = 0;
                        wx_messagebox(hwnd,
                                        "Use CTRL + ALT + PAGE DOWN to return to windowed mode",
                                        "PCem", WX_MB_OK);
                }
                video_fullscreen = checked;
                saveconfig(NULL);
        }
        else if (ID_IS("IDM_VID_FULLSCREEN_TOGGLE"))
        {
                toggle_fullscreen();
        }
        else if (ID_RANGE("IDM_VID_FS[start]", "IDM_VID_FS[end]"))
        {
                video_fullscreen_scale = wParam - wx_xrcid("IDM_VID_FS[start]");
                saveconfig(NULL);
        }
        else if (ID_RANGE("IDM_VID_SCALE_MODE[start]", "IDM_VID_SCALE_MODE[end]"))
        {
                video_scale_mode = wParam - wx_xrcid("IDM_VID_SCALE_MODE[start]");
                renderer_doreset = 1;
                saveconfig(NULL);
        }
        else if (ID_RANGE("IDM_VID_SCALE[start]", "IDM_VID_SCALE[end]"))
        {
                video_scale = wParam - wx_xrcid("IDM_VID_SCALE[start]");
                display_resize(video_width, video_height);
                saveconfig(NULL);
        }
        else if (ID_RANGE("IDM_VID_FS_MODE[start]", "IDM_VID_FS_MODE[end]"))
        {
                video_fullscreen_mode = wParam - wx_xrcid("IDM_VID_FS_MODE[start]");
                saveconfig(NULL);
        }
        else if (ID_RANGE("IDM_VID_RENDER_DRIVER[start]", "IDM_VID_RENDER_DRIVER[end]"))
        {
                requested_render_driver = sdl_get_render_driver_by_id(wParam - wx_xrcid("IDM_VID_RENDER_DRIVER[start]"), RENDERER_AUTO);
                window_doreset = 1;
                saveconfig(NULL);
        }
        else if (ID_IS("IDM_VID_VSYNC"))
        {
                video_vsync = checked;
                renderer_doreset = 1;
                saveconfig(NULL);
        }
        else if (ID_IS("IDM_VID_LOST_FOCUS_DIM"))
        {
                video_focus_dim = checked;
                saveconfig(NULL);
        }
        else if (ID_IS("IDM_CDROM_DISABLED"))
        {
                if (cdrom_enabled)
                {
                        if (!confirm())
                        {
                                update_cdrom_menu(hmenu);
                                return 0;
                        }
                }
                if (!cdrom_enabled)
                {
                        /* Switching from disabled to disabled. Do nothing. */
                        update_cdrom_menu(hmenu);
                        return 0;
                }
                atapi->exit();
                atapi_close();
                ioctl_set_drive(0);
                old_cdrom_drive = cdrom_drive;
                cdrom_drive = 0;
                if (cdrom_enabled)
                {
                        pause = 1;
                        SDL_Delay(100);
                        cdrom_enabled = 0;
                        saveconfig(NULL);
                        resetpchard();
                        pause = 0;
                }
                update_cdrom_menu(hmenu);
        }
        else if (ID_IS("IDM_CDROM_EMPTY"))
        {
                if (!cdrom_enabled)
                {
                        if (!confirm())
                        {
                                update_cdrom_menu(hmenu);
                                return 0;
                        }
                }
                if ((cdrom_drive == 0) && cdrom_enabled)
                {
                        update_cdrom_menu(hmenu);
                        /* Switch from empty to empty. Do nothing. */
                        return 0;
                }
                atapi->exit();
                atapi_close();
                ioctl_set_drive(0);
                if (cdrom_enabled)
                {
                        /* Signal disc change to the emulated machine. */
                        atapi_insert_cdrom();
                }
                old_cdrom_drive = cdrom_drive;
                cdrom_drive = 0;
                saveconfig(NULL);
                if (!cdrom_enabled)
                {
                        pause = 1;
                        SDL_Delay(100);
                        cdrom_enabled = 1;
                        saveconfig(NULL);
                        resetpchard();
                        pause = 0;
                }
                update_cdrom_menu(hmenu);
        }
        else if (ID_IS("IDM_CDROM_IMAGE") || ID_IS("IDM_CDROM_IMAGE_LOAD"))
        {
                if (!getfile(hwnd,
                                "CD-ROM image (*.iso;*.cue)|*.iso;*.cue|All files (*.*)|*.*",
                                image_path))
                {
                        if (!cdrom_enabled)
                        {
                                if (!confirm())
                                {
                                        update_cdrom_menu(hmenu);
                                        return 0;
                                }
                        }
                        old_cdrom_drive = cdrom_drive;
                        strcpy(temp_image_path, openfilestring);
                        if ((strcmp(image_path, temp_image_path) == 0)
                                        && (cdrom_drive == CDROM_IMAGE)
                                        && cdrom_enabled)
                        {
                                /* Switching from ISO to the same ISO. Do nothing. */
                                update_cdrom_menu(hmenu);
                                return 0;
                        }
                        atapi->exit();
                        atapi_close();
                        image_open(temp_image_path);
                        if (cdrom_enabled)
                        {
                                /* Signal disc change to the emulated machine. */
                                atapi_insert_cdrom();
                        }
                        cdrom_drive = CDROM_IMAGE;
                        saveconfig(NULL);
                        if (!cdrom_enabled)
                        {
                                pause = 1;
                                SDL_Delay(100);
                                cdrom_enabled = 1;
                                saveconfig(NULL);
                                resetpchard();
                                pause = 0;
                        }
                        update_cdrom_menu(hmenu);
                } else
                        update_cdrom_menu(hmenu);
        }
        else if (wParam >= IDM_CDROM_REAL && wParam < IDM_CDROM_REAL+100)
        {
                if (!cdrom_enabled)
                {
                        if (!confirm())
                        {
                                update_cdrom_menu(hmenu);
                                return 0;
                        }
                }
                new_cdrom_drive = wParam-IDM_CDROM_REAL+1;
                if ((cdrom_drive == new_cdrom_drive) && cdrom_enabled)
                {
                        /* Switching to the same drive. Do nothing. */
                        update_cdrom_menu(hmenu);
                        return 0;
                }
                old_cdrom_drive = cdrom_drive;
                atapi->exit();
                atapi_close();
                ioctl_set_drive(new_cdrom_drive);
                if (cdrom_enabled)
                {
                        /* Signal disc change to the emulated machine. */
                        atapi_insert_cdrom();
                }
                cdrom_drive = new_cdrom_drive;
                saveconfig(NULL);
                if (!cdrom_enabled)
                {
                        pause = 1;
                        SDL_Delay(100);
                        cdrom_enabled = 1;
                        saveconfig(NULL);
                        resetpchard();
                        pause = 0;
                }
                update_cdrom_menu(hmenu);
        }
        return 0;
}
