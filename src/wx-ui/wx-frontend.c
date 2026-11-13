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
#include <stdlib.h>
#include <math.h>
#include "ibm.h"
#include "cassette.h"
#include "cdrom-ioctl.h"
#include "cdrom-image.h"
#include "config.h"
#include "video.h"
#include "ide.h"
#include "scsi_zip.h"
#include "sound.h"
#include "disc.h"
#include "disc_img.h"
#include "nvr.h"

#include "sdl2-video.h"
#include "sdl2-display.h"
#include "sdl2-host.h"
#include "wx-utils.h"
#include "wx-common.h"
#include "wx-frontend.h"

#include "viewer.h"

#if __APPLE__
#include <util.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

extern void creatediscimage_open(void *hwnd);

#define IDM_CDROM_REAL 1500
#define IDM_CDROM_DEVICE 1501

#define MIN_SND_BUF 50

void *ghwnd = 0;

void *menu;

char menuitem[60];

extern int config_selection_open(void *hwnd, int inited);
extern int shader_manager_open(void *hwnd);

extern void wx_loadconfig();
extern void wx_saveconfig();

void update_cdrom_menu(void *hmenu) {
        if (cdrom_drive == CDROM_IMAGE)
                wx_checkmenuitem(menu, WX_ID("IDM_CDROM_IMAGE"), WX_MB_CHECKED);
        else if (cdrom_drive > 0)
#if __linux__
                wx_checkmenuitem(menu, IDM_CDROM_DEVICE, WX_MB_CHECKED);
#else
                wx_checkmenuitem(menu, IDM_CDROM_REAL + cdrom_drive, WX_MB_CHECKED);
#endif
        else
                wx_checkmenuitem(menu, WX_ID("IDM_CDROM_EMPTY"), WX_MB_CHECKED);
}

void wx_initmenu() {
        menu = wx_getmenu(ghwnd);

        void *cdrom_submenu = wx_getsubmenu(menu, WX_ID("IDM_CDROM"));

#ifdef __WINDOWS__
        char s[32];
        int c;
        /* Loop through each Windows drive letter and test to see if
           it's a CDROM */
        for (c = 'A'; c <= 'Z'; c++) {
                sprintf(s, "%c:\\", c);
                if (GetDriveTypeA(s) == DRIVE_CDROM) {
                        sprintf(s, "Host CD/DVD Drive (%c:)", c);
                        wx_appendmenu(cdrom_submenu, IDM_CDROM_REAL + c, s, wxITEM_RADIO);
                }
        }
#elif __linux__
        wx_appendmenu(cdrom_submenu, IDM_CDROM_DEVICE, "Host CD/DVD Drive...", wxITEM_RADIO);
#elif __APPLE__
        int c;

        for (c = 1; c < 99; c++) {
                char s[80];
                int fd;

                sprintf(s, "disk%i", c);
                fd = opendev(s, O_RDONLY, 0, NULL);
                if (fd > 0) {
                        char name[255];

                        close(fd);

                        sprintf(name, "Host CD/DVD Drive (/dev/disk%i)", c);
                        wx_appendmenu(cdrom_submenu, IDM_CDROM_REAL + c, name, wxITEM_RADIO);
                }
        }
#endif
}

int wx_setupmenu(void *data) {
        int c;
        update_cdrom_menu(menu);
        update_viewers_menu(menu);
        sprintf(menuitem, "IDM_VID_RESOLUTION[%d]", vid_resize);
        wx_checkmenuitem(menu, WX_ID(menuitem), WX_MB_CHECKED);
        wx_enablemenuitem(menu, wx_xrcid("IDM_VID_SCALE_MENU"), !vid_resize);
        sprintf(menuitem, "IDM_VID_FS[%d]", video_fullscreen_scale);
        wx_checkmenuitem(menu, WX_ID(menuitem), WX_MB_CHECKED);
        wx_checkmenuitem(menu, WX_ID("IDM_VID_FULLSCREEN"), video_fullscreen);
        wx_checkmenuitem(menu, WX_ID("IDM_VID_REMEMBER"), window_remember ? WX_MB_CHECKED : WX_MB_UNCHECKED);
        wx_checkmenuitem(menu, WX_ID("IDM_BPB_DISABLE"), bpb_disable ? WX_MB_CHECKED : WX_MB_UNCHECKED);

        sprintf(menuitem, "IDM_SND_BUF[%d]", (int)(log(sound_buf_len / MIN_SND_BUF) / log(2)));
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
        sdl_render_driver *drivers = sdl_get_render_drivers(&num_renderers);
        for (c = 1; c < num_renderers; ++c) {
                sprintf(menuitem, "IDM_VID_RENDER_DRIVER[%d]", drivers[c].id);
                wx_enablemenuitem(menu, WX_ID(menuitem), drivers[c].renderer_available(&drivers[c]));
        }
        sprintf(menuitem, "IDM_VID_RENDER_DRIVER[%d]", requested_render_driver.id);
        wx_checkmenuitem(menu, WX_ID(menuitem), WX_MB_CHECKED);

        //        wx_enablemenuitem(menu, WX_ID("IDM_VID_SDL2"), requested_render_driver.id != RENDERER_GL3);
        wx_enablemenuitem(menu, WX_ID("IDM_VID_GL3"), requested_render_driver.id == RENDERER_GL3);

        sprintf(menuitem, "IDM_VID_GL3_INPUT_STRETCH[%d]", gl3_input_stretch);
        wx_checkmenuitem(menu, WX_ID(menuitem), WX_MB_CHECKED);
        sprintf(menuitem, "IDM_VID_GL3_INPUT_SCALE[%d]", (int)((gl3_input_scale - 0.5) * 2));
        wx_checkmenuitem(menu, WX_ID(menuitem), WX_MB_CHECKED);
        sprintf(menuitem, "IDM_VID_GL3_SHADER_REFRESH_RATE[%g]", gl3_shader_refresh_rate);
        wx_checkmenuitem(menu, WX_ID(menuitem), WX_MB_CHECKED);

        return 1;
}

static void wx_host_message(const char *title, const char *message) { wx_messagebox(0, message, title, WX_MB_OK); }

static void wx_host_ui_call(SDL_HOST_CALLBACK callback, void *data) { wx_callback(ghwnd, callback, data); }

#ifdef PCEM_RENDER_WITH_TIMER
static void *render_timer = NULL;
static void (*render_loop)(void) = NULL;

static void wx_host_render_timer() {
        if (render_loop)
                render_loop();
}

static void wx_host_start_render_loop(void (*loop)(void)) {
        render_loop = loop;
        if (render_timer)
                wx_destroytimer(render_timer);
        render_timer = wx_createtimer(wx_host_render_timer);
        wx_starttimer(render_timer, 500, 1);
}
#endif

static void wx_host_request_stop(void) { wx_stop_emulation(ghwnd); }

static void wx_host_request_stop_now(void) { wx_stop_emulation_now(ghwnd); }

static void wx_host_config_loaded(void) {
        if (ghwnd)
                wx_callback(ghwnd, wx_setupmenu, 0);
}

static void wx_host_emulation_started(void) {
        if (show_machine_on_start)
                wx_show_status(ghwnd);
}

static void wx_host_emulation_paused(void) { viewer_notify_pause(); }

static void wx_host_emulation_resumed(void) { viewer_notify_resume(); }

static void wx_host_emulation_stopped(void) {
        viewer_close_all();
        wx_close_status(ghwnd);
}

static int wx_host_image_save(const char *path, const char *name, const char *format, unsigned char *rgb, int width, int height) {
        return wx_image_save(path, name, format, rgb, width, height, 0);
}

static void *wx_host_panel_create(void *parent, const char *title, SDL_DISPLAY_CALLBACK callback) {
        menu = wx_getmenu(parent);
        return wx_display_create(parent, menu, title, callback);
}

static void wx_host_panel_set_title(void *panel, const char *title) { wx_setwindowtitle(panel, (char *)title); }

static const sdl_host_t wx_host = {
#ifdef __APPLE__
        .defer_display_init = 1,
#endif
        .message = wx_host_message,
#ifdef PCEM_RENDER_WITH_TIMER
        .start_render_loop = wx_host_start_render_loop,
#else
        .ui_call = wx_host_ui_call,
#endif
        .request_stop = wx_host_request_stop,
        .request_stop_now = wx_host_request_stop_now,
        .config_loaded = wx_host_config_loaded,
        .emulation_started = wx_host_emulation_started,
        .emulation_paused = wx_host_emulation_paused,
        .emulation_resumed = wx_host_emulation_resumed,
        .emulation_stopped = wx_host_emulation_stopped,
        .image_save = wx_host_image_save,
        .image_load = wx_image_load_rgba,
        .panel_create = wx_host_panel_create,
        .panel_native = wx_display_getnativecanvas,
        .panel_canvas_size = wx_display_getcanvassize,
        .panel_set_size = wx_display_setsize,
        .panel_get_rect = wx_display_getrect,
        .panel_set_position = wx_setwindowposition,
        .panel_center = wx_display_center,
        .panel_show = wx_display_show,
        .panel_set_fullscreen = wx_display_setfullscreen,
        .panel_set_title = wx_host_panel_set_title,
        .panel_set_status = wx_display_setstatus,
};

void wx_frontend_init(void) {
        sdl_host_set(&wx_host);
        add_config_callback(wx_loadconfig, wx_saveconfig, 0);
}

int wx_load_config(void *hwnd) {
        if (!config_override) {
                if (!config_selection_open(NULL, 0))
                        return FALSE;
        }

        return TRUE;
}

int wx_start(void *hwnd) {
        ghwnd = hwnd;

#ifdef __APPLE__
        display_init();
#endif

        wx_initmenu();
        wx_setupmenu(0);

        return emulator_check_roms();
}

int wx_stop(void *window) { return emulator_shutdown(); }

char openfilestring[260];
int getfile(void *hwnd, char *f, char *fn) {
        int ret = wx_filedialog(hwnd, "Open", fn, f, 0, 1, openfilestring);
#ifdef __APPLE__
        /* wxWidgets on OSX may mess up the SDL-window somehow, so just in case we reset it here */
        window_doreset = 1;
#endif
        return ret;
}

int getsfile(void *hwnd, char *f, char *fn, char *dir, char *ext) {
        int ret = wx_filedialog(hwnd, "Save", dir, f, ext, 0, openfilestring);
#ifdef __APPLE__
        window_doreset = 1;
#endif
        return ret;
}

int getfilewithcaption(void *hwnd, char *f, char *fn, char *caption) {
        int ret = wx_filedialog(hwnd, caption, fn, f, 0, 1, openfilestring);
#ifdef __APPLE__
        /* wxWidgets on OSX may mess up the SDL-window somehow, so just in case we reset it here */
        window_doreset = 1;
#endif
        return ret;
}

int custom_resolution_callback(void *window, int message, INT_PARAM wParam, LONG_PARAM lParam) {
        switch (message) {
        case WX_INITDIALOG: {
                wx_sendmessage(wx_getdlgitem(window, wx_xrcid("IDC_WIDTH")), WX_UDM_SETPOS, 0, custom_resolution_width);
                wx_sendmessage(wx_getdlgitem(window, wx_xrcid("IDC_HEIGHT")), WX_UDM_SETPOS, 0, custom_resolution_height);
                return TRUE;
        }
        case WX_COMMAND: {
                if (wParam == wxID_OK) {
                        custom_resolution_width =
                                wx_sendmessage(wx_getdlgitem(window, wx_xrcid("IDC_WIDTH")), WX_UDM_GETPOS, 0, 0);
                        custom_resolution_height =
                                wx_sendmessage(wx_getdlgitem(window, wx_xrcid("IDC_HEIGHT")), WX_UDM_GETPOS, 0, 0);
                        wx_enddialog(window, 1);
                        return TRUE;
                } else if (wParam == wxID_CANCEL) {
                        wx_enddialog(window, 0);
                        return TRUE;
                }
        }
        }
        return 0;
}

int wx_handle_command(void *hwnd, int wParam, int checked) {
        void *hmenu;
        char temp_image_path[1024];
        int new_cdrom_drive;
        hmenu = wx_getmenu(hwnd);

        if (ID_IS("IDM_STATUS")) {
                wx_show_status(hwnd);
        } else if (ID_IS("IDM_FILE_RESET")) {
                pauseEmulation = 1;
                SDL_Delay(100);
                savenvr();
                resetpc();
                pauseEmulation = 0;
        } else if (ID_IS("IDM_FILE_HRESET")) {
                pauseEmulation = 1;
                SDL_Delay(100);
                savenvr();
                resetpchard();
                pauseEmulation = 0;
        } else if (ID_IS("IDM_FILE_RESET_CAD")) {
                pauseEmulation = 1;
                SDL_Delay(100);
                savenvr();
                resetpc_cad();
                pauseEmulation = 0;
        } else if (ID_IS("IDM_FILE_EXIT")) {
                //                wx_exit(hwnd, 0);
                wx_stop_emulation(hwnd);
        } else if (ID_IS("IDM_DISC_A")) {
                if (!getfile(hwnd, "Disc image (*.img;*.ima;*.fdi)|*.img;*.ima;*.fdi|All files (*.*)|*.*", discfns[0])) {
                        disc_close(0);
                        disc_load(0, openfilestring);
                        saveconfig(NULL);
                }
        } else if (ID_IS("IDM_DISC_B")) {
                if (!getfile(hwnd, "Disc image (*.img;*.ima;*.fdi)|*.img;*.ima;*.fdi|All files (*.*)|*.*", discfns[1])) {
                        disc_close(1);
                        disc_load(1, openfilestring);
                        saveconfig(NULL);
                }
        } else if (ID_IS("IDM_EJECT_A")) {
                disc_close(0);
                saveconfig(NULL);
        } else if (ID_IS("IDM_EJECT_B")) {
                disc_close(1);
                saveconfig(NULL);
        } else if (ID_IS("IDM_BPB_DISABLE")) {
                bpb_disable = !bpb_disable;
                wx_checkmenuitem(hmenu, wParam, bpb_disable);
                saveconfig(NULL);
        } else if (ID_IS("IDM_DISC_CREATE")) {
                creatediscimage_open(hwnd);
        } else if (ID_IS("IDM_DISC_ZIP")) {
                char zip_fn[256] = "";

                if (!getfile(hwnd, "Disc image (*.img)|*.img|All files (*.*)|*.*", zip_fn)) {
                        zip_load(openfilestring);
                }
        } else if (ID_IS("IDM_EJECT_ZIP")) {
                zip_eject();
        } else if (ID_IS("IDM_CASSETTE_LOAD")) {
                if (!getfile(hwnd, "Tape image (*.pzxi;*.pzx)|*.pzxi;*.pzx|All files (*.*)|*.*", cassettefn)) {
                        cassette_eject();
                        cassette_load(openfilestring);
                        saveconfig(NULL);
                }
        } else if (ID_IS("IDM_CASSETTE_EJECT")) {
                cassette_eject();
                saveconfig(NULL);
        } else if (ID_IS("IDM_MACHINE_TOGGLE")) {
                if (emulation_state != EMULATION_STOPPED)
                        wx_togglewindow(hwnd);
        } else if (ID_RANGE("IDM_VID_RESOLUTION[start]", "IDM_VID_RESOLUTION[end]")) {
                vid_resize = wParam - wx_xrcid("IDM_VID_RESOLUTION[start]");
                wx_checkmenuitem(hmenu, wParam, WX_MB_CHECKED);
                window_dosetresize = 1;
                wx_enablemenuitem(hmenu, wx_xrcid("IDM_VID_SCALE_MENU"), !vid_resize);
                saveconfig(NULL);
        } else if (ID_IS("IDM_VID_RESOLUTION_CUSTOM")) {
                if (wx_dialogbox(hwnd, "CustomResolutionDlg", custom_resolution_callback)) {
                        if (vid_resize != 2) {
                                vid_resize = 2;
                                wx_checkmenuitem(hmenu, wx_xrcid("IDM_VID_RESOLUTION[2]"), WX_MB_CHECKED);
                                wx_enablemenuitem(hmenu, wx_xrcid("IDM_VID_SCALE_MENU"), !vid_resize);
                        }
                        window_dosetresize = 1;
                }
        } else if (ID_IS("IDM_SCREENSHOT")) {
                take_screenshot = 1;
        } else if (ID_RANGE("IDM_SCREENSHOT_FORMAT[start]", "IDM_SCREENSHOT_FORMAT[end]")) {
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
        } else if (ID_IS("IDM_SCREENSHOT_FLASH")) {
                screenshot_flash = !screenshot_flash;
                wx_checkmenuitem(hmenu, wParam, screenshot_flash);
                saveconfig(NULL);
        } else if (ID_IS("IDM_VID_REMEMBER")) {
                window_remember = !window_remember;
                wx_checkmenuitem(hmenu, WX_ID("IDM_VID_REMEMBER"), window_remember ? WX_MB_CHECKED : WX_MB_UNCHECKED);
                window_doremember = 1;
                saveconfig(NULL);
        } else if (ID_IS("IDM_VID_FULLSCREEN")) {
                if (video_fullscreen_first) {
                        video_fullscreen_first = 0;
                        wx_messagebox(hwnd, "Use CTRL + ALT + PAGE DOWN to return to windowed mode", "PCem", WX_MB_OK);
                }
                video_fullscreen = !video_fullscreen;
                wx_checkmenuitem(hmenu, wParam, video_fullscreen);
                saveconfig(NULL);
        } else if (ID_IS("IDM_VID_FULLSCREEN_TOGGLE")) {
                toggle_fullscreen();
        } else if (ID_RANGE("IDM_VID_FS[start]", "IDM_VID_FS[end]")) {
                video_fullscreen_scale = wParam - wx_xrcid("IDM_VID_FS[start]");
                display_resize(video_width, video_height);
                wx_checkmenuitem(hmenu, wParam, WX_MB_CHECKED);
                saveconfig(NULL);
        } else if (ID_RANGE("IDM_VID_SCALE_MODE[start]", "IDM_VID_SCALE_MODE[end]")) {
                video_scale_mode = wParam - wx_xrcid("IDM_VID_SCALE_MODE[start]");
                renderer_doreset = 1;
                wx_checkmenuitem(hmenu, wParam, WX_MB_CHECKED);
                saveconfig(NULL);
        } else if (ID_RANGE("IDM_VID_SCALE[start]", "IDM_VID_SCALE[end]")) {
                video_scale = wParam - wx_xrcid("IDM_VID_SCALE[start]");
                wx_checkmenuitem(hmenu, wParam, WX_MB_CHECKED);
                display_resize(video_width, video_height);
                saveconfig(NULL);
        } else if (ID_RANGE("IDM_VID_FS_MODE[start]", "IDM_VID_FS_MODE[end]")) {
                video_fullscreen_mode = wParam - wx_xrcid("IDM_VID_FS_MODE[start]");
                wx_checkmenuitem(hmenu, wParam, WX_MB_CHECKED);
                saveconfig(NULL);
        } else if (ID_RANGE("IDM_VID_RENDER_DRIVER[start]", "IDM_VID_RENDER_DRIVER[end]")) {
                requested_render_driver =
                        sdl_get_render_driver_by_id(wParam - wx_xrcid("IDM_VID_RENDER_DRIVER[start]"), RENDERER_AUTO);
                window_doreset = 1;

                /* update enabled menu-items */
                //                wx_enablemenuitem(menu, WX_ID("IDM_VID_SDL2"), requested_render_driver.id != RENDERER_GL3);
                wx_enablemenuitem(menu, WX_ID("IDM_VID_GL3"), requested_render_driver.id == RENDERER_GL3);

                wx_checkmenuitem(hmenu, wParam, WX_MB_CHECKED);
                saveconfig(NULL);
        } else if (ID_IS("IDM_VID_VSYNC")) {
                video_vsync = !video_vsync;
                wx_checkmenuitem(menu, wParam, video_vsync);
                renderer_doreset = 1;
                saveconfig(NULL);
        } else if (ID_IS("IDM_VID_LOST_FOCUS_DIM")) {
                video_focus_dim = !video_focus_dim;
                wx_checkmenuitem(menu, wParam, video_focus_dim);
                saveconfig(NULL);
        } else if (ID_IS("IDM_VID_ALTERNATIVE_UPDATE_LOCK")) {
                video_alternative_update_lock = !video_alternative_update_lock;
                wx_checkmenuitem(menu, wParam, video_alternative_update_lock);
                renderer_doreset = 1;
                saveconfig(NULL);
        } else if (ID_RANGE("IDM_VID_GL3_INPUT_STRETCH[start]", "IDM_VID_GL3_INPUT_STRETCH[end]")) {
                gl3_input_stretch = wParam - wx_xrcid("IDM_VID_GL3_INPUT_STRETCH[start]");
                wx_checkmenuitem(menu, wParam, WX_MB_CHECKED);
                saveconfig(NULL);
        } else if (ID_RANGE("IDM_VID_GL3_INPUT_SCALE[start]", "IDM_VID_GL3_INPUT_SCALE[end]")) {
                int input_scale = wParam - wx_xrcid("IDM_VID_GL3_INPUT_SCALE[start]");
                gl3_input_scale = input_scale / 2.0f + 0.5f;
                wx_checkmenuitem(menu, wParam, WX_MB_CHECKED);
                saveconfig(NULL);
        } else if (ID_RANGE("IDM_VID_GL3_SHADER_REFRESH_RATE[start]", "IDM_VID_GL3_SHADER_REFRESH_RATE[end]")) {
                gl3_shader_refresh_rate = wParam - wx_xrcid("IDM_VID_GL3_SHADER_REFRESH_RATE[start]");
                wx_checkmenuitem(menu, wParam, WX_MB_CHECKED);
                saveconfig(NULL);
        } else if (ID_IS("IDM_VID_GL3_SHADER_MANAGER")) {
                if (shader_manager_open(hwnd)) {
                        renderer_doreset = 1;
                        saveconfig(NULL);
                }
                //                if (!getfile(hwnd, "GLSL Shaders (*.glslp;*.glsl)|*.glslp;*.glsl|All files (*.*)|*.*",
                //                gl3_shader_file))
                //                {
                //                        strncpy(gl3_shader_file, openfilestring, 511);
                //                        gl3_shader_file[511] = 0;
                //                        renderer_doreset = 1;
                //                        saveconfig(NULL);
                //                }
        } else if (ID_RANGE("IDM_SND_BUF[start]", "IDM_SND_BUF[end]")) {
                sound_buf_len = MIN_SND_BUF * 1 << (wParam - wx_xrcid("IDM_SND_BUF[start]"));
                wx_checkmenuitem(menu, wParam, WX_MB_CHECKED);
                saveconfig(NULL);
        } else if (ID_RANGE("IDM_SND_GAIN[start]", "IDM_SND_GAIN[end]")) {
                sound_gain = 2 * (wParam - wx_xrcid("IDM_SND_GAIN[start]"));
                wx_checkmenuitem(menu, wParam, WX_MB_CHECKED);
                saveconfig(NULL);
        } else if (ID_IS("IDM_CDROM_EMPTY")) {
                if (cdrom_drive == 0) {
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
        } else if (ID_IS("IDM_CDROM_IMAGE") || ID_IS("IDM_CDROM_IMAGE_LOAD")) {
                if (!getfile(hwnd, "CD-ROM image (*.iso;*.cue)|*.iso;*.cue|All files (*.*)|*.*", image_path)) {
                        old_cdrom_drive = cdrom_drive;
                        strcpy(temp_image_path, openfilestring);
                        if ((strcmp(image_path, temp_image_path) == 0) && (cdrom_drive == CDROM_IMAGE)) {
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
#if __linux__
        } else if (wParam == IDM_CDROM_DEVICE) {
                char new_device_path[1024];

                if (wx_textentrydialog(hwnd, "CD-ROM device path:", "PCem", cdrom_device_path,
                                       1, sizeof(new_device_path) - 1, (LONG_PARAM)new_device_path)) {
                        old_cdrom_drive = cdrom_drive;
                        atapi->exit();
                        atapi_close();
                        strcpy(cdrom_device_path, new_device_path);
                        ioctl_set_drive(1);
                        cdrom_drive = 1;
                        saveconfig(NULL);
                        update_cdrom_menu(hmenu);
                }
#else
        } else if (wParam >= IDM_CDROM_REAL && wParam < IDM_CDROM_REAL + 100) {
                new_cdrom_drive = wParam - IDM_CDROM_REAL;
                if (cdrom_drive == new_cdrom_drive) {
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
#endif
        } else if (wParam >= IDM_VIEWER && wParam < IDM_VIEWER_MAX) {
                viewer_open(hwnd, wParam - IDM_VIEWER);
        }
        return 0;
}
