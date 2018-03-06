#include "wx-sdl2.h"

#include <SDL2/SDL.h>
#include <wx/defs.h>

#ifdef __WINDOWS__
#define BITMAP WINDOWS_BITMAP
#undef UNICODE
#include <windows.h>
#include <windowsx.h>
#undef BITMAP
#endif


#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <math.h>
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
#include "plat-joystick.h"
#include "plat-midi.h"
#include "scsi_zip.h"
#include "sound.h"
#include "thread.h"
#include "disc.h"
#include "disc_img.h"
#include "mem.h"
#include "paths.h"

#include "wx-sdl2-video.h"
#include "wx-utils.h"
#include "wx-common.h"
#include "wx-display.h"

#if __APPLE__
#define pause __pause
#include <util.h>
#include <fcntl.h>
#include <unistd.h>
#undef pause
#endif

extern void creatediscimage_open(void *hwnd);

#define ID_IS(s) wParam == wx_xrcid(s)
#define ID_RANGE(a, b) wParam >= wx_xrcid(a) && wParam <= wx_xrcid(b)

#define IDM_CDROM_REAL 1500

#define MIN_SND_BUF 50

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
int window_dosetresize = 0;
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

extern int config_selection_open(void* hwnd, int inited);
extern int shader_manager_open(void* hwnd);

extern void sdl_set_window_title(const char* title);

extern float gl3_shader_refresh_rate;
extern float gl3_input_scale;
extern int gl3_input_stretch;
extern char gl3_shader_file[20][512];

char screenshot_format[10];
int screenshot_flash = 1;
int take_screenshot = 0;

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

unsigned int get_ticks()
{
        return SDL_GetTicks();
}

void delay_ms(unsigned int ms)
{
        SDL_Delay(ms);
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
        SDL_SetThreadPriority(SDL_THREAD_PRIORITY_HIGH);

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

int dir_exists(char* path)
{
        return wx_dir_exists(path);
}

void get_pcem_path(char *s, int size)
{
#ifdef __linux
        wx_get_home_directory(s);
        strcat(s, ".pcem/");
#else
        char* path = SDL_GetBasePath();
        strcpy(s, path);
#endif
}

void set_window_title(const char *s)
{
        sdl_set_window_title(s);
}

float flash_func(float x)
{
        return 1 - pow(x, 4);
}

float flash_failed_func(float x)
{
        return fabs(sin(x*3.1415926*2));
}

void screenshot_taken(unsigned char* rgb, int width, int height)
{
        char name[512];
        char date[128];
        strcpy(name, "Screenshot from ");
        wx_date_format(date, "%Y-%m-%d %H-%M-%S");
        strcat(name, date);
        if (wx_image_save(screenshots_path, name, screenshot_format, rgb, width, height, 0))
        {
                pclog("Screenshot saved\n");
                if (screenshot_flash)
                        color_flash(flash_func, 500, 0xff, 0xff, 0xff, 0xff);
        }
        else
        {
                pclog("Screenshot was not saved\n");
                if (screenshot_flash)
                        color_flash(flash_failed_func, 500, 0xff, 0, 0, 0xff);
        }
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
        vid_resize = config_get_int(CFG_MACHINE, NULL, "vid_resize", 0);
        video_fullscreen_scale = config_get_int(CFG_MACHINE, NULL, "video_fullscreen_scale", 0);
        video_fullscreen_first = config_get_int(CFG_MACHINE, NULL, "video_fullscreen_first", 1);

        strcpy(screenshot_format, config_get_string(CFG_MACHINE, "SDL2", "screenshot_format", IMAGE_PNG));
        screenshot_flash = config_get_int(CFG_MACHINE, "SDL2", "screenshot_flash", 1);

        custom_resolution_width = config_get_int(CFG_MACHINE, "SDL2", "custom_width", custom_resolution_width);
        custom_resolution_height = config_get_int(CFG_MACHINE, "SDL2", "custom_height", custom_resolution_height);

        video_fullscreen = config_get_int(CFG_MACHINE, "SDL2", "fullscreen", video_fullscreen);
        video_fullscreen_mode = config_get_int(CFG_MACHINE, "SDL2", "fullscreen_mode", video_fullscreen_mode);
        video_scale = config_get_int(CFG_MACHINE, "SDL2", "scale", video_scale);
        video_scale_mode = config_get_int(CFG_MACHINE, "SDL2", "scale_mode", video_scale_mode);
        video_vsync = config_get_int(CFG_MACHINE, "SDL2", "vsync", video_vsync);
        video_focus_dim = config_get_int(CFG_MACHINE, "SDL2", "focus_dim", video_focus_dim);
        video_alternative_update_lock = config_get_int(CFG_MACHINE, "SDL2", "alternative_update_lock", video_alternative_update_lock);
        requested_render_driver = sdl_get_render_driver_by_name(config_get_string(CFG_MACHINE, "SDL2", "render_driver", ""), RENDERER_SOFTWARE);

        gl3_input_scale = config_get_float(CFG_MACHINE, "GL3", "input_scale", gl3_input_scale);
        gl3_input_stretch = config_get_int(CFG_MACHINE, "GL3", "input_stretch", gl3_input_stretch);
        gl3_shader_refresh_rate = config_get_float(CFG_MACHINE, "GL3", "shader_refresh_rate", gl3_shader_refresh_rate);

        memset(&gl3_shader_file, 0, sizeof(gl3_shader_file));
        int num_shaders = config_get_int(CFG_MACHINE, "GL3 Shaders", "shaders", 0);
        char s[20];
        int i;
        for (i = 0; i < num_shaders; ++i)
        {
                sprintf(s, "shader%d", i);
                strncpy(gl3_shader_file[i], config_get_string(CFG_MACHINE, "GL3 Shaders", s, ""), 511);
                gl3_shader_file[i][511] = 0;
        }
}

void sdl_saveconfig()
{
        config_set_int(CFG_MACHINE, NULL, "vid_resize", vid_resize);
        config_set_int(CFG_MACHINE, NULL, "video_fullscreen_scale", video_fullscreen_scale);
        config_set_int(CFG_MACHINE, NULL, "video_fullscreen_first", video_fullscreen_first);

        config_set_string(CFG_MACHINE, "SDL2", "screenshot_format", screenshot_format);
        config_set_int(CFG_MACHINE, "SDL2", "screenshot_flash", screenshot_flash);

        config_set_int(CFG_MACHINE, "SDL2", "custom_width", custom_resolution_width);
        config_set_int(CFG_MACHINE, "SDL2", "custom_height", custom_resolution_height);

        config_set_int(CFG_MACHINE, "SDL2", "fullscreen", video_fullscreen);
        config_set_int(CFG_MACHINE, "SDL2", "fullscreen_mode", video_fullscreen_mode);
        config_set_int(CFG_MACHINE, "SDL2", "scale", video_scale);
        config_set_int(CFG_MACHINE, "SDL2", "scale_mode", video_scale_mode);
        config_set_int(CFG_MACHINE, "SDL2", "vsync", video_vsync);
        config_set_int(CFG_MACHINE, "SDL2", "focus_dim", video_focus_dim);
        config_set_int(CFG_MACHINE, "SDL2", "alternative_update_lock", video_alternative_update_lock);
        config_set_string(CFG_MACHINE, "SDL2", "render_driver", (char*)requested_render_driver.sdl_id);

        config_set_float(CFG_MACHINE, "GL3", "input_scale", gl3_input_scale);
        config_set_int(CFG_MACHINE, "GL3", "input_stretch", gl3_input_stretch);
        config_set_float(CFG_MACHINE, "GL3", "shader_refresh_rate", gl3_shader_refresh_rate);

        char s[20];
        int i;
        for (i = 0; i < 20; ++i)
        {
                sprintf(s, "shader%d", i);
                if (strlen(gl3_shader_file[i]))
                        config_set_string(CFG_MACHINE, "GL3 Shaders", s, gl3_shader_file[i]);
                else
                        break;
        }
        config_set_int(CFG_MACHINE, "GL3 Shaders", "shaders", i);
}

void update_cdrom_menu(void* hmenu)
{
        if (cdrom_drive == CDROM_IMAGE)
                wx_checkmenuitem(menu, WX_ID("IDM_CDROM_IMAGE"), WX_MB_CHECKED);
        else if (cdrom_drive > 0)
                wx_checkmenuitem(menu, IDM_CDROM_REAL+cdrom_drive, WX_MB_CHECKED);
        else
                wx_checkmenuitem(menu, WX_ID("IDM_CDROM_EMPTY"), WX_MB_CHECKED);
}

void wx_initmenu()
{
        menu = wx_getmenu(ghwnd);

        void* cdrom_submenu = wx_getsubmenu(menu, WX_ID("IDM_CDROM"));

#ifdef __WINDOWS__
        char s[32];
        int c;
        /* Loop through each Windows drive letter and test to see if
           it's a CDROM */
        for (c='A';c<='Z';c++)
        {
                sprintf(s,"%c:\\",c);
                if (GetDriveTypeA(s)==DRIVE_CDROM)
                {
                        sprintf(s, "Host CD/DVD Drive (%c:)", c);
                        wx_appendmenu(cdrom_submenu, IDM_CDROM_REAL+c, s, wxITEM_RADIO);
                }
        }
#elif __linux__
        wx_appendmenu(cdrom_submenu, IDM_CDROM_REAL+1, "Host CD/DVD Drive (/dev/cdrom)", wxITEM_RADIO);
#elif __APPLE__
        int c;
        
        for (c = 1; c < 99; c++)
        {
                char s[80];
                int fd;
                
                sprintf(s, "disk%i", c);
                fd = opendev(s, O_RDONLY, 0, NULL);
                if (fd > 0)
                {
                        char name[255];
                        
                        close(fd);
                        
                        sprintf(name, "Host CD/DVD Drive (/dev/disk%i)", c);
                        wx_appendmenu(cdrom_submenu, IDM_CDROM_REAL+c, name, wxITEM_RADIO);
                }
        }
#endif
}

int wx_setupmenu(void* data)
{
        int c;
        update_cdrom_menu(menu);
        sprintf(menuitem, "IDM_VID_RESOLUTION[%d]", vid_resize);
        wx_checkmenuitem(menu, WX_ID(menuitem), WX_MB_CHECKED);
        wx_enablemenuitem(menu, wx_xrcid("IDM_VID_SCALE_MENU"), !vid_resize);
        sprintf(menuitem, "IDM_VID_FS[%d]", video_fullscreen_scale);
        wx_checkmenuitem(menu, WX_ID(menuitem), WX_MB_CHECKED);
        wx_checkmenuitem(menu, WX_ID("IDM_VID_FULLSCREEN"), video_fullscreen);
        wx_checkmenuitem(menu, WX_ID("IDM_VID_REMEMBER"),
                        window_remember ? WX_MB_CHECKED : WX_MB_UNCHECKED);
        wx_checkmenuitem(menu, WX_ID("IDM_BPB_DISABLE"), bpb_disable ? WX_MB_CHECKED : WX_MB_UNCHECKED);

        sprintf(menuitem, "IDM_SND_BUF[%d]", (int)(log(sound_buf_len/MIN_SND_BUF)/log(2)));
        wx_checkmenuitem(menu, WX_ID(menuitem), WX_MB_CHECKED);

        sprintf(menuitem, "IDM_SND_GAIN[%d]", (int)(sound_gain / 2));
        wx_checkmenuitem(menu, WX_ID(menuitem), WX_MB_CHECKED);

        sprintf(menuitem, "IDM_VID_SCALE_MODE[%d]", video_scale_mode);
        wx_checkmenuitem(menu, WX_ID(menuitem), WX_MB_CHECKED);
        sprintf(menuitem, "IDM_VID_SCALE[%d]", video_scale);
        wx_checkmenuitem(menu, WX_ID(menuitem), WX_MB_CHECKED);
        sprintf(menuitem, "IDM_VID_FS_MODE[%d]", video_fullscreen_mode);
        wx_checkmenuitem(menu, WX_ID(menuitem), WX_MB_CHECKED);
        wx_checkmenuitem(menu, WX_ID("IDM_VID_VSYNC"), video_vsync);
        wx_checkmenuitem(menu, WX_ID("IDM_VID_LOST_FOCUS_DIM"), video_focus_dim);
        wx_checkmenuitem(menu, WX_ID("IDM_VID_ALTERNATIVE_UPDATE_LOCK"), video_alternative_update_lock);

        int format = 0;
        if (!strcmp(screenshot_format, IMAGE_TIFF))
                format = 1;
        else if (!strcmp(screenshot_format, IMAGE_BMP))
                format = 2;
        else if (!strcmp(screenshot_format, IMAGE_JPG))
                format = 3;
        sprintf(menuitem, "IDM_SCREENSHOT_FORMAT[%d]", format);
        wx_checkmenuitem(menu, WX_ID(menuitem), WX_MB_CHECKED);
        wx_checkmenuitem(menu, WX_ID("IDM_SCREENSHOT_FLASH"), screenshot_flash);

        int num_renderers;
        sdl_render_driver* drivers = sdl_get_render_drivers(&num_renderers);
        for (c = 1; c < num_renderers; ++c)
        {
                sprintf(menuitem, "IDM_VID_RENDER_DRIVER[%d]", drivers[c].id);
                wx_enablemenuitem(menu, WX_ID(menuitem), drivers[c].renderer_available(&drivers[c]));
        }
        sprintf(menuitem, "IDM_VID_RENDER_DRIVER[%d]", requested_render_driver.id);
        wx_checkmenuitem(menu, WX_ID(menuitem), WX_MB_CHECKED);

//        wx_enablemenuitem(menu, WX_ID("IDM_VID_SDL2"), requested_render_driver.id != RENDERER_GL3);
        wx_enablemenuitem(menu, WX_ID("IDM_VID_GL3"), requested_render_driver.id == RENDERER_GL3);

        sprintf(menuitem, "IDM_VID_GL3_INPUT_STRETCH[%d]", gl3_input_stretch);
        wx_checkmenuitem(menu, WX_ID(menuitem), WX_MB_CHECKED);
        sprintf(menuitem, "IDM_VID_GL3_INPUT_SCALE[%d]", (int)((gl3_input_scale-0.5)*2));
        wx_checkmenuitem(menu, WX_ID(menuitem), WX_MB_CHECKED);
        sprintf(menuitem, "IDM_VID_GL3_SHADER_REFRESH_RATE[%g]", gl3_shader_refresh_rate);
        wx_checkmenuitem(menu, WX_ID(menuitem), WX_MB_CHECKED);

        return 1;
}

void sdl_onconfigloaded()
{
        if (ghwnd)
                wx_callback(ghwnd, wx_setupmenu, 0);

        /* create directories */
        if (!wx_dir_exists(configs_path))
                wx_create_directory(configs_path);
        if (!wx_dir_exists(nvr_path))
                wx_create_directory(nvr_path);
        if (!wx_dir_exists(logs_path))
                wx_create_directory(logs_path);
        if (!wx_dir_exists(screenshots_path))
                wx_create_directory(screenshots_path);
}

extern void wx_loadconfig();
extern void wx_saveconfig();

int pc_main(int argc, char** argv)
{
        paths_init();

#ifdef __linux__
        char s[512];
        /* create directories if they don't exist */
        if (!wx_setup(pcem_path))
                return FALSE;

        /* set up default paths */
        sprintf(s, "%s%s%c%s", pcem_path, "roms/", get_path_separator(), "/usr/share/pcem/roms/");
        set_default_roms_paths(s);
        append_filename(s, pcem_path, "nvr/", 511);
        set_default_nvr_path(s);
        append_filename(s, pcem_path, "configs/", 511);
        set_default_configs_path(s);
        append_filename(s, pcem_path, "screenshots/", 511);
        set_default_screenshots_path(s);
        append_filename(s, pcem_path, "logs/", 511);
        set_default_logs_path(s);
#endif

        add_config_callback(sdl_loadconfig, sdl_saveconfig, sdl_onconfigloaded);
        add_config_callback(wx_loadconfig, wx_saveconfig, 0);

        initpc(argc, argv);
        resetpchard();

        sound_init();

#ifdef __linux__
        /* check if cfg exists, and if not create it */
        append_filename(s, pcem_path, "pcem.cfg", 511);
        if (!wx_file_exists(s))
                saveconfig(NULL);
#endif

#ifndef __APPLE__
        display_init();
#endif
        sdl_video_init();
        joystick_init();

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
        wx_setupmenu(0);

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
        for (c = 0; c < GFX_MAX; c++)
                gfx_present[c] = video_card_available(video_old_to_new(c));

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
        int c;
        pclog("Starting emulation...\n");
        loadconfig(NULL);

        emulation_state = EMULATION_RUNNING;
        pause = 0;

        ghMutex = SDL_CreateMutex();
        mainMutex = SDL_CreateMutex();
        mainCond = SDL_CreateCond();

        if (!loadbios())
        {
                if (romset != -1)
                        wx_messagebox(ghwnd,
                                        "Configured romset not available.\nDefaulting to available romset.",
                                        "PCem error", WX_MB_OK);
                for (c = 0; c < ROM_MAX; c++)
                {
                        if (romspresent[c])
                        {
                                romset = c;
                                model = model_getmodel(romset);
                                break;
                        }
                }
        }

        if (!video_card_available(video_old_to_new(gfxcard)))
        {
                if (romset != -1)
                        wx_messagebox(ghwnd,
                                        "Configured video BIOS not available.\nDefaulting to available romset.",
                                        "PCem error", WX_MB_OK);
                for (c = GFX_MAX - 1; c >= 0; c--)
                {
                        if (gfx_present[c])
                        {
                                gfxcard = c;
                                break;
                        }
                }
        }


        loadbios();
        resetpchard();
        midi_init();

        display_start(params);
        mainthreadh = SDL_CreateThread(mainthread, "Main Thread", NULL);

        onesectimer = SDL_AddTimer(1000, timer_onesec, NULL);

        updatewindowsize(640, 480);

        timer_freq = SDL_GetPerformanceFrequency();

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

        midi_close();
        
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

int custom_resolution_callback(void* window, int message, INT_PARAM wParam, LONG_PARAM lParam)
{
        switch (message)
        {
                case WX_INITDIALOG:
                {
                        wx_sendmessage(wx_getdlgitem(window, wx_xrcid("IDC_WIDTH")), WX_UDM_SETPOS, 0, custom_resolution_width);
                        wx_sendmessage(wx_getdlgitem(window, wx_xrcid("IDC_HEIGHT")), WX_UDM_SETPOS, 0, custom_resolution_height);
                        return TRUE;
                }
                case WX_COMMAND:
                {
                        if (wParam == wxID_OK)
                        {
                                custom_resolution_width = wx_sendmessage(wx_getdlgitem(window, wx_xrcid("IDC_WIDTH")), WX_UDM_GETPOS, 0, 0);
                                custom_resolution_height = wx_sendmessage(wx_getdlgitem(window, wx_xrcid("IDC_HEIGHT")), WX_UDM_GETPOS, 0, 0);
                                wx_enddialog(window, 1);
                                return TRUE;
                        }
                        else if (wParam == wxID_CANCEL)
                        {
                                wx_enddialog(window, 0);
                                return TRUE;
                        }

                }
        }
        return 0;
}

int wx_handle_command(void* hwnd, int wParam, int checked)
{
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
                bpb_disable = !bpb_disable;
                wx_checkmenuitem(hmenu, wParam, bpb_disable);
                saveconfig(NULL);
        }
        else if (ID_IS("IDM_DISC_CREATE"))
        {
                creatediscimage_open(hwnd);
        }
        else if (ID_IS("IDM_DISC_ZIP"))
        {
                char zip_fn[256] = "";
                
                if (!getfile(hwnd,
                                "Disc image (*.img)|*.img|All files (*.*)|*.*",
                                zip_fn))
                {
                        zip_load(openfilestring);
                }
        }
        else if (ID_IS("IDM_EJECT_ZIP"))
        {
                zip_eject();
        }
        else if (ID_IS("IDM_MACHINE_TOGGLE"))
        {
                if (emulation_state != EMULATION_STOPPED)
                        wx_togglewindow(hwnd);
        }
        else if (ID_RANGE("IDM_VID_RESOLUTION[start]", "IDM_VID_RESOLUTION[end]"))
        {
                vid_resize = wParam - wx_xrcid("IDM_VID_RESOLUTION[start]");
                wx_checkmenuitem(hmenu, wParam, WX_MB_CHECKED);
                window_dosetresize = 1;
                wx_enablemenuitem(hmenu, wx_xrcid("IDM_VID_SCALE_MENU"), !vid_resize);
                saveconfig(NULL);
        }
        else if (ID_IS("IDM_VID_RESOLUTION_CUSTOM"))
        {
                if (wx_dialogbox(hwnd, "CustomResolutionDlg", custom_resolution_callback))
                {
                        if (vid_resize != 2)
                        {
                                vid_resize = 2;
                                wx_checkmenuitem(hmenu, wx_xrcid("IDM_VID_RESOLUTION[2]"), WX_MB_CHECKED);
                                wx_enablemenuitem(hmenu, wx_xrcid("IDM_VID_SCALE_MENU"), !vid_resize);
                        }
                        window_dosetresize = 1;
                }
        }
        else if (ID_IS("IDM_SCREENSHOT"))
        {
                take_screenshot = 1;
        }
        else if (ID_RANGE("IDM_SCREENSHOT_FORMAT[start]", "IDM_SCREENSHOT_FORMAT[end]"))
        {
                int format = wParam - wx_xrcid("IDM_SCREENSHOT_FORMAT[start]");
                if (format == 0)
                        strcpy(screenshot_format, IMAGE_PNG);
                else if (format == 1)
                        strcpy(screenshot_format, IMAGE_TIFF);
                else if (format == 2)
                        strcpy(screenshot_format, IMAGE_BMP);
                else if (format == 3)
                        strcpy(screenshot_format, IMAGE_JPG);
                saveconfig(NULL);
                wx_checkmenuitem(hmenu, wParam, WX_MB_CHECKED);
        }
        else if (ID_IS("IDM_SCREENSHOT_FLASH"))
        {
                screenshot_flash = !screenshot_flash;
                wx_checkmenuitem(hmenu, wParam, screenshot_flash);
                saveconfig(NULL);
        }
        else if (ID_IS("IDM_VID_REMEMBER"))
        {
                window_remember = !window_remember;
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
                video_fullscreen = !video_fullscreen;
                wx_checkmenuitem(hmenu, wParam, video_fullscreen);
                saveconfig(NULL);
        }
        else if (ID_IS("IDM_VID_FULLSCREEN_TOGGLE"))
        {
                toggle_fullscreen();
        }
        else if (ID_RANGE("IDM_VID_FS[start]", "IDM_VID_FS[end]"))
        {
                video_fullscreen_scale = wParam - wx_xrcid("IDM_VID_FS[start]");
                display_resize(video_width, video_height);
                wx_checkmenuitem(hmenu, wParam, WX_MB_CHECKED);
                saveconfig(NULL);
        }
        else if (ID_RANGE("IDM_VID_SCALE_MODE[start]", "IDM_VID_SCALE_MODE[end]"))
        {
                video_scale_mode = wParam - wx_xrcid("IDM_VID_SCALE_MODE[start]");
                renderer_doreset = 1;
                wx_checkmenuitem(hmenu, wParam, WX_MB_CHECKED);
                saveconfig(NULL);
        }
        else if (ID_RANGE("IDM_VID_SCALE[start]", "IDM_VID_SCALE[end]"))
        {
                video_scale = wParam - wx_xrcid("IDM_VID_SCALE[start]");
                wx_checkmenuitem(hmenu, wParam, WX_MB_CHECKED);
                display_resize(video_width, video_height);
                saveconfig(NULL);
        }
        else if (ID_RANGE("IDM_VID_FS_MODE[start]", "IDM_VID_FS_MODE[end]"))
        {
                video_fullscreen_mode = wParam - wx_xrcid("IDM_VID_FS_MODE[start]");
                wx_checkmenuitem(hmenu, wParam, WX_MB_CHECKED);
                saveconfig(NULL);
        }
        else if (ID_RANGE("IDM_VID_RENDER_DRIVER[start]", "IDM_VID_RENDER_DRIVER[end]"))
        {
                requested_render_driver = sdl_get_render_driver_by_id(wParam - wx_xrcid("IDM_VID_RENDER_DRIVER[start]"), RENDERER_AUTO);
                window_doreset = 1;

                /* update enabled menu-items */
//                wx_enablemenuitem(menu, WX_ID("IDM_VID_SDL2"), requested_render_driver.id != RENDERER_GL3);
                wx_enablemenuitem(menu, WX_ID("IDM_VID_GL3"), requested_render_driver.id == RENDERER_GL3);

                wx_checkmenuitem(hmenu, wParam, WX_MB_CHECKED);
                saveconfig(NULL);
        }
        else if (ID_IS("IDM_VID_VSYNC"))
        {
                video_vsync = !video_vsync;
                wx_checkmenuitem(menu, wParam, video_vsync);
                renderer_doreset = 1;
                saveconfig(NULL);
        }
        else if (ID_IS("IDM_VID_LOST_FOCUS_DIM"))
        {
                video_focus_dim = !video_focus_dim;
                wx_checkmenuitem(menu, wParam, video_focus_dim);
                saveconfig(NULL);
        }
        else if (ID_IS("IDM_VID_ALTERNATIVE_UPDATE_LOCK"))
        {
                video_alternative_update_lock = !video_alternative_update_lock;
                wx_checkmenuitem(menu, wParam, video_alternative_update_lock);
                renderer_doreset = 1;
                saveconfig(NULL);
        }
        else if (ID_RANGE("IDM_VID_GL3_INPUT_STRETCH[start]", "IDM_VID_GL3_INPUT_STRETCH[end]"))
        {
                gl3_input_stretch = wParam - wx_xrcid("IDM_VID_GL3_INPUT_STRETCH[start]");
                wx_checkmenuitem(menu, wParam, WX_MB_CHECKED);
                saveconfig(NULL);
        }
        else if (ID_RANGE("IDM_VID_GL3_INPUT_SCALE[start]", "IDM_VID_GL3_INPUT_SCALE[end]"))
        {
                int input_scale = wParam - wx_xrcid("IDM_VID_GL3_INPUT_SCALE[start]");
                gl3_input_scale = input_scale/2.0f+0.5f;
                wx_checkmenuitem(menu, wParam, WX_MB_CHECKED);
                saveconfig(NULL);
        }
        else if (ID_RANGE("IDM_VID_GL3_SHADER_REFRESH_RATE[start]", "IDM_VID_GL3_SHADER_REFRESH_RATE[end]"))
        {
                gl3_shader_refresh_rate = wParam - wx_xrcid("IDM_VID_GL3_SHADER_REFRESH_RATE[start]");
                wx_checkmenuitem(menu, wParam, WX_MB_CHECKED);
                saveconfig(NULL);
        }
        else if (ID_IS("IDM_VID_GL3_SHADER_MANAGER"))
        {
                if (shader_manager_open(hwnd))
                {
                        renderer_doreset = 1;
                        saveconfig(NULL);
                }
//                if (!getfile(hwnd, "GLSL Shaders (*.glslp;*.glsl)|*.glslp;*.glsl|All files (*.*)|*.*", gl3_shader_file))
//                {
//                        strncpy(gl3_shader_file, openfilestring, 511);
//                        gl3_shader_file[511] = 0;
//                        renderer_doreset = 1;
//                        saveconfig(NULL);
//                }

        }
        else if (ID_RANGE("IDM_SND_BUF[start]", "IDM_SND_BUF[end]"))
        {
                sound_buf_len = MIN_SND_BUF*1<<(wParam - wx_xrcid("IDM_SND_BUF[start]"));
                wx_checkmenuitem(menu, wParam, WX_MB_CHECKED);
                saveconfig(NULL);
        }
        else if (ID_RANGE("IDM_SND_GAIN[start]", "IDM_SND_GAIN[end]"))
        {
                sound_gain = 2 * (wParam - wx_xrcid("IDM_SND_GAIN[start]"));
                wx_checkmenuitem(menu, wParam, WX_MB_CHECKED);
                saveconfig(NULL);
        }
        else if (ID_IS("IDM_CDROM_EMPTY"))
        {
                if (cdrom_drive == 0)
                {
                        update_cdrom_menu(hmenu);
                        /* Switch from empty to empty. Do nothing. */
                        return 0;
                }
                atapi->exit();
                atapi_close();
                ioctl_set_drive(0);
                old_cdrom_drive = cdrom_drive;
                cdrom_drive = 0;
                saveconfig(NULL);
                update_cdrom_menu(hmenu);
        }
        else if (ID_IS("IDM_CDROM_IMAGE") || ID_IS("IDM_CDROM_IMAGE_LOAD"))
        {
                if (!getfile(hwnd,
                                "CD-ROM image (*.iso;*.cue)|*.iso;*.cue|All files (*.*)|*.*",
                                image_path))
                {
                        old_cdrom_drive = cdrom_drive;
                        strcpy(temp_image_path, openfilestring);
                        if ((strcmp(image_path, temp_image_path) == 0)
                                        && (cdrom_drive == CDROM_IMAGE))
                        {
                                /* Switching from ISO to the same ISO. Do nothing. */
                                update_cdrom_menu(hmenu);
                                return 0;
                        }
                        atapi->exit();
                        atapi_close();
                        image_open(temp_image_path);
                        cdrom_drive = CDROM_IMAGE;
                        saveconfig(NULL);
                        update_cdrom_menu(hmenu);
                } else
                        update_cdrom_menu(hmenu);
        }
        else if (wParam >= IDM_CDROM_REAL && wParam < IDM_CDROM_REAL+100)
        {
                new_cdrom_drive = wParam-IDM_CDROM_REAL;
                if (cdrom_drive == new_cdrom_drive)
                {
                        /* Switching to the same drive. Do nothing. */
                        update_cdrom_menu(hmenu);
                        return 0;
                }
                old_cdrom_drive = cdrom_drive;
                atapi->exit();
                atapi_close();
                ioctl_set_drive(new_cdrom_drive);
                cdrom_drive = new_cdrom_drive;
                saveconfig(NULL);
                update_cdrom_menu(hmenu);
        }
        return 0;
}
