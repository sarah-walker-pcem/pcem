#include <SDL2/SDL.h>

#include "wx-utils.h"
#include "wx-app.h"
#include "wx-common.h"
#include "wx-config_sel.h"
#include "wx-shader_man.h"
#include "wx-customresolution.h"
#include "wx-createdisc.h"
#include "wx-frontend.h"
#include "wx-display.h"

#include <wx/spinctrl.h>
#include <wx/wfstream.h>

#include <iostream>
#include <math.h>
#include <string.h>

#ifdef _WIN32
#define BITMAP WINDOWS_BITMAP
#include <windows.h>
#undef BITMAP
#endif

#if __APPLE__
#include <util.h>
#include <fcntl.h>
#include <unistd.h>
#endif

extern "C" {
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
#include "sdl2-emu.h"
#include "viewer.h"
}

#define IDM_CDROM_REAL 1500
#define IDM_CDROM_DEVICE 1501

#define MIN_SND_BUF 50

static Frame *ghwnd = NULL;
static wxMenuBar *menu = NULL;

static void wx_checkmenuitem(wxMenuBar *menubar, int id, bool checked) {
        wxMenuItem *item = menubar->FindItem(id);
        if (item)
                item->Check(checked);
#ifndef RELEASE_BUILD
        else
                std::cout << "Menu item not found: " << id << std::endl;
#endif
}

static void wx_enablemenuitem(wxMenuBar *menubar, int id, bool enable) {
        wxMenuItem *item = menubar->FindItem(id);
        if (item)
                item->Enable(enable);
#ifndef RELEASE_BUILD
        else
                std::cout << "Menu item not found: " << id << std::endl;
#endif
}

static int xrcid_index(const char *name, int index) { return XRCID(wxString::Format("%s[%d]", name, index).utf8_str()); }

static bool id_in_range(int id, const char *name, int *index) {
        int start = XRCID((wxString(name) + "[start]").utf8_str());
        int end = XRCID((wxString(name) + "[end]").utf8_str());
        if (id < start || id > end)
                return false;
        *index = id - start;
        return true;
}

static void wx_loadconfig() {
        show_machine_info = wx_config_get_int(CFG_GLOBAL, "wxWidgets", "show_machine_info", show_machine_info);
        show_disc_activity = wx_config_get_int(CFG_GLOBAL, "wxWidgets", "show_disc_activity", show_disc_activity);
        show_speed_history = wx_config_get_int(CFG_GLOBAL, "wxWidgets", "show_speed_history", show_speed_history);
        show_mount_paths = wx_config_get_int(CFG_GLOBAL, "wxWidgets", "show_mount_paths", show_mount_paths);
        show_status = wx_config_get_int(CFG_GLOBAL, "wxWidgets", "show_status", show_status);
        show_machine_on_start = wx_config_get_int(CFG_GLOBAL, "wxWidgets", "show_machine_on_start", show_machine_on_start);

        confirm_on_reset_machine =
                wx_config_get_int(CFG_GLOBAL, "wxWidgets", "confirm_on_reset_machine", confirm_on_reset_machine);
        confirm_on_stop_emulation =
                wx_config_get_int(CFG_GLOBAL, "wxWidgets", "confirm_on_stop_emulation", confirm_on_stop_emulation);

        wx_window_x = wx_config_get_int(CFG_GLOBAL, "wxWidgets", "window_x", wx_window_x);
        wx_window_y = wx_config_get_int(CFG_GLOBAL, "wxWidgets", "window_y", wx_window_y);
}

static void wx_saveconfig() {
        wx_config_set_int(CFG_GLOBAL, "wxWidgets", "show_machine_info", show_machine_info);
        wx_config_set_int(CFG_GLOBAL, "wxWidgets", "show_disc_activity", show_disc_activity);
        wx_config_set_int(CFG_GLOBAL, "wxWidgets", "show_speed_history", show_speed_history);
        wx_config_set_int(CFG_GLOBAL, "wxWidgets", "show_mount_paths", show_mount_paths);
        wx_config_set_int(CFG_GLOBAL, "wxWidgets", "show_status", show_status);
        wx_config_set_int(CFG_GLOBAL, "wxWidgets", "show_machine_on_start", show_machine_on_start);

        wx_config_set_int(CFG_GLOBAL, "wxWidgets", "confirm_on_reset_machine", confirm_on_reset_machine);
        wx_config_set_int(CFG_GLOBAL, "wxWidgets", "confirm_on_stop_emulation", confirm_on_stop_emulation);

        wx_config_set_int(CFG_GLOBAL, "wxWidgets", "window_x", wx_window_x);
        wx_config_set_int(CFG_GLOBAL, "wxWidgets", "window_y", wx_window_y);
}

static void update_cdrom_menu() {
        if (cdrom_drive == CDROM_IMAGE)
                wx_checkmenuitem(menu, XRCID("IDM_CDROM_IMAGE"), true);
        else if (cdrom_drive > 0)
#if __linux__
                wx_checkmenuitem(menu, IDM_CDROM_DEVICE, true);
#else
                wx_checkmenuitem(menu, IDM_CDROM_REAL + cdrom_drive, true);
#endif
        else
                wx_checkmenuitem(menu, XRCID("IDM_CDROM_EMPTY"), true);
}

static void wx_initmenu() {
        menu = ghwnd->GetMenu();

        wxMenu *cdrom_submenu = wx_getsubmenu(menu, XRCID("IDM_CDROM"));
        if (!cdrom_submenu)
                return;

#ifdef _WIN32
        for (int c = 'A'; c <= 'Z'; c++) {
                char s[32];
                snprintf(s, sizeof(s), "%c:\\", c);
                if (GetDriveTypeA(s) == DRIVE_CDROM)
                        cdrom_submenu->Append(IDM_CDROM_REAL + c, wxString::Format("Host CD/DVD Drive (%c:)", c), wxEmptyString,
                                              wxITEM_RADIO);
        }
#elif __linux__
        cdrom_submenu->Append(IDM_CDROM_DEVICE, "Host CD/DVD Drive...", wxEmptyString, wxITEM_RADIO);
#elif __APPLE__
        for (int c = 1; c < 99; c++) {
                char s[80];
                snprintf(s, sizeof(s), "disk%i", c);
                int fd = opendev(s, O_RDONLY, 0, NULL);
                if (fd > 0) {
                        close(fd);
                        cdrom_submenu->Append(IDM_CDROM_REAL + c, wxString::Format("Host CD/DVD Drive (/dev/disk%i)", c),
                                              wxEmptyString, wxITEM_RADIO);
                }
        }
#endif
}

static int wx_setupmenu(void *data) {
        update_cdrom_menu();
        update_viewers_menu(menu);
        wx_checkmenuitem(menu, xrcid_index("IDM_VID_RESOLUTION", vid_resize), true);
        wx_enablemenuitem(menu, XRCID("IDM_VID_SCALE_MENU"), !vid_resize);
        wx_checkmenuitem(menu, xrcid_index("IDM_VID_FS", video_fullscreen_scale), true);
        wx_checkmenuitem(menu, XRCID("IDM_VID_FULLSCREEN"), video_fullscreen);
        wx_checkmenuitem(menu, XRCID("IDM_VID_REMEMBER"), window_remember);
        wx_checkmenuitem(menu, XRCID("IDM_BPB_DISABLE"), bpb_disable);

        wx_checkmenuitem(menu, xrcid_index("IDM_SND_BUF", (int)(log(sound_buf_len / MIN_SND_BUF) / log(2))), true);
        wx_checkmenuitem(menu, xrcid_index("IDM_SND_GAIN", (int)(sound_gain / 2)), true);

        wx_checkmenuitem(menu, xrcid_index("IDM_VID_SCALE_MODE", video_scale_mode), true);
        wx_checkmenuitem(menu, xrcid_index("IDM_VID_SCALE", video_scale), true);
        wx_checkmenuitem(menu, xrcid_index("IDM_VID_FS_MODE", video_fullscreen_mode), true);
        wx_checkmenuitem(menu, XRCID("IDM_VID_VSYNC"), video_vsync);
        wx_checkmenuitem(menu, XRCID("IDM_VID_LOST_FOCUS_DIM"), video_focus_dim);
        wx_checkmenuitem(menu, XRCID("IDM_VID_ALTERNATIVE_UPDATE_LOCK"), video_alternative_update_lock);

        int format = 0;
        if (!strcmp(screenshot_format, IMAGE_TIFF))
                format = 1;
        else if (!strcmp(screenshot_format, IMAGE_BMP))
                format = 2;
        else if (!strcmp(screenshot_format, IMAGE_JPG))
                format = 3;
        wx_checkmenuitem(menu, xrcid_index("IDM_SCREENSHOT_FORMAT", format), true);
        wx_checkmenuitem(menu, XRCID("IDM_SCREENSHOT_FLASH"), screenshot_flash);

        int num_renderers;
        sdl_render_driver *drivers = sdl_get_render_drivers(&num_renderers);
        for (int c = 1; c < num_renderers; ++c)
                wx_enablemenuitem(menu, xrcid_index("IDM_VID_RENDER_DRIVER", drivers[c].id),
                                  drivers[c].renderer_available(&drivers[c]));
        wx_checkmenuitem(menu, xrcid_index("IDM_VID_RENDER_DRIVER", requested_render_driver.id), true);

        wx_enablemenuitem(menu, XRCID("IDM_VID_GL3"), requested_render_driver.id == RENDERER_GL3);

        wx_checkmenuitem(menu, xrcid_index("IDM_VID_GL3_INPUT_STRETCH", gl3_input_stretch), true);
        wx_checkmenuitem(menu, xrcid_index("IDM_VID_GL3_INPUT_SCALE", (int)((gl3_input_scale - 0.5) * 2)), true);
        wx_checkmenuitem(menu, XRCID(wxString::Format("IDM_VID_GL3_SHADER_REFRESH_RATE[%g]", gl3_shader_refresh_rate).utf8_str()), true);

        return 1;
}

static void wx_host_message(const char *title, const char *message) { wx_messagebox(NULL, message, title, wxOK); }

static void wx_host_ui_call(SDL_HOST_CALLBACK callback, void *data) { ghwnd->QueueCallback(callback, data); }

#ifdef PCEM_RENDER_WITH_TIMER
static wxTimer *render_timer = NULL;
static void (*render_loop)(void) = NULL;

static void wx_host_start_render_loop(void (*loop)(void)) {
        render_loop = loop;
        if (!render_timer) {
                render_timer = new wxTimer();
                render_timer->Bind(wxEVT_TIMER, [](wxTimerEvent &event) {
                        if (render_loop)
                                render_loop();
                });
        }
        render_timer->Stop();
        render_timer->StartOnce(500);
}
#endif

static void wx_host_request_stop(void) { ghwnd->StopEmulation(); }

static void wx_host_request_stop_now(void) { ghwnd->StopEmulationNow(); }

static void wx_host_config_loaded(void) {
        if (ghwnd)
                ghwnd->QueueCallback(wx_setupmenu, NULL);
}

static void wx_host_emulation_started(void) {
        if (menu)
                update_viewers_menu(menu);
        if (show_machine_on_start)
                ghwnd->ShowStatus();
}

static void wx_host_emulation_paused(void) { viewer_notify_pause(); }

static void wx_host_emulation_resumed(void) { viewer_notify_resume(); }

static void wx_host_emulation_stopped(void) {
        viewer_close_all();
        ghwnd->CloseStatus();
}

static int wx_host_image_save(const char *path, const char *name, const char *format, unsigned char *rgb, int width, int height) {
        wxLogNull logNull;
        wxImage image(width, height);
        image.SetData(rgb, true);

        wxImageHandler *h;
        wxString ext;

        if (!strcmp(format, IMAGE_TIFF)) {
                h = new wxTIFFHandler();
                ext = "tif";
        } else if (!strcmp(format, IMAGE_BMP)) {
                h = new wxBMPHandler();
                ext = "bmp";
        } else if (!strcmp(format, IMAGE_JPG)) {
                h = new wxJPEGHandler();
                ext = "jpg";
        } else {
                h = new wxPNGHandler();
                ext = "png";
        }

        wxString p(path);
        if (!p.EndsWith("/"))
                p += "/";
        p += wxString(name) + "." + ext;

        wxFileOutputStream stream(p);
        int res = h->SaveFile(&image, stream, false);
        delete h;
        return res;
}

static unsigned char *wx_host_image_load(const char *path, int *width, int *height) {
        wxLogNull logNull;
        wxImage image(path);
        if (!image.IsOk())
                return NULL;
        int w = image.GetWidth();
        int h = image.GetHeight();
        unsigned char *rgb = image.GetData();
        unsigned char *alpha = image.HasAlpha() ? image.GetAlpha() : NULL;
        unsigned char *data = (unsigned char *)malloc(w * h * 4);
        if (!data)
                return NULL;
        for (int i = 0; i < w * h; ++i) {
                data[i * 4 + 0] = rgb[i * 3 + 0];
                data[i * 4 + 1] = rgb[i * 3 + 1];
                data[i * 4 + 2] = rgb[i * 3 + 2];
                data[i * 4 + 3] = alpha ? alpha[i] : 0xff;
        }
        *width = w;
        *height = h;
        return data;
}

static void *wx_host_panel_create(void *parent, const char *title, SDL_DISPLAY_CALLBACK callback) {
        menu = ((Frame *)parent)->GetMenu();
        return new DisplayFrame((wxWindow *)parent, menu, title, callback);
}

static sdl_host_t wx_host;

void wx_frontend_init() {
        memset(&wx_host, 0, sizeof(wx_host));
#ifdef __APPLE__
        wx_host.defer_display_init = 1;
#endif
        wx_host.message = wx_host_message;
#ifdef PCEM_RENDER_WITH_TIMER
        wx_host.start_render_loop = wx_host_start_render_loop;
#else
        wx_host.ui_call = wx_host_ui_call;
#endif
        wx_host.request_stop = wx_host_request_stop;
        wx_host.request_stop_now = wx_host_request_stop_now;
        wx_host.config_loaded = wx_host_config_loaded;
        wx_host.emulation_started = wx_host_emulation_started;
        wx_host.emulation_paused = wx_host_emulation_paused;
        wx_host.emulation_resumed = wx_host_emulation_resumed;
        wx_host.emulation_stopped = wx_host_emulation_stopped;
        wx_host.image_save = wx_host_image_save;
        wx_host.image_load = wx_host_image_load;
        wx_host.panel_create = wx_host_panel_create;
        wx_host.panel_native = DisplayFrame::HostNativeCanvas;
        wx_host.panel_canvas_size = DisplayFrame::HostCanvasSize;
        wx_host.panel_set_size = DisplayFrame::HostSetSize;
        wx_host.panel_get_rect = DisplayFrame::HostGetRect;
        wx_host.panel_set_position = DisplayFrame::HostSetPosition;
        wx_host.panel_center = DisplayFrame::HostCenter;
        wx_host.panel_show = DisplayFrame::HostShow;
        wx_host.panel_set_fullscreen = DisplayFrame::HostSetFullscreen;
        wx_host.panel_set_title = DisplayFrame::HostSetTitle;
        wx_host.panel_set_status = DisplayFrame::HostSetStatus;

        sdl_host_set(&wx_host);
        add_config_callback(wx_loadconfig, wx_saveconfig, NULL);
}

bool wx_load_config(wxWindow *hwnd) { return config_override || ConfigSelectionDialog::Open(NULL, false); }

int wx_start(wxWindow *hwnd) {
        ghwnd = (Frame *)hwnd;

#ifdef __APPLE__
        display_init();
#endif

        wx_initmenu();
        wx_setupmenu(NULL);

        return emulator_check_roms();
}

int wx_stop(wxWindow *hwnd) { return emulator_shutdown(); }

static bool file_dialog(wxWindow *parent, const wxString &title, const wxString &path, const wxString &wildcard,
                        const wxString &ext, bool open, wxString &result) {
        bool ret = wx_filedialog(parent, title, path, wildcard, ext, open, result);
#ifdef __APPLE__
        window_doreset = 1;
#endif
        return ret;
}

bool wx_getfile(wxWindow *parent, const wxString &wildcard, const wxString &path, wxString &result, const wxString &caption) {
        return file_dialog(parent, caption, path, wildcard, wxEmptyString, true, result);
}

bool wx_getsfile(wxWindow *parent, const wxString &wildcard, const wxString &path, const wxString &ext, wxString &result) {
        return file_dialog(parent, "Save", path, wildcard, ext, false, result);
}


static void pause_and_reset(void (*reset)(void)) {
        pauseEmulation = 1;
        SDL_Delay(100);
        savenvr();
        reset();
        pauseEmulation = 0;
}

static void load_disc(wxWindow *hwnd, int drive) {
        wxString file;
        if (wx_getfile(hwnd, "Disc image (*.img;*.ima;*.fdi)|*.img;*.ima;*.fdi|All files (*.*)|*.*", discfns[drive], file)) {
                disc_close(drive);
                disc_load(drive, (char *)(const char *)file.mb_str());
                saveconfig(NULL);
        }
}

static void eject_cdrom() {
        atapi->exit();
        atapi_close();
}

static bool hotkey_held(wxKeyCode key) {
#ifdef __WXMSW__
        return wxGetKeyState(WXK_CONTROL) && wxGetKeyState(WXK_ALT) && wxGetKeyState(key);
#else
        return false;
#endif
}

void wx_handle_command(wxWindow *hwnd, int id, bool checked) {
        int index;

        if (id == XRCID("IDM_STATUS")) {
                ((Frame *)hwnd)->ShowStatus();
        } else if (id == XRCID("IDM_FILE_RESET")) {
                pause_and_reset(resetpc);
        } else if (id == XRCID("IDM_FILE_HRESET")) {
                pause_and_reset(resetpchard);
                update_viewers_menu(menu);
        } else if (id == XRCID("IDM_FILE_RESET_CAD")) {
                pause_and_reset(resetpc_cad);
        } else if (id == XRCID("IDM_FILE_EXIT")) {
                ((Frame *)hwnd)->StopEmulation();
        } else if (id == XRCID("IDM_DISC_A")) {
                load_disc(hwnd, 0);
        } else if (id == XRCID("IDM_DISC_B")) {
                load_disc(hwnd, 1);
        } else if (id == XRCID("IDM_EJECT_A")) {
                disc_close(0);
                saveconfig(NULL);
        } else if (id == XRCID("IDM_EJECT_B")) {
                disc_close(1);
                saveconfig(NULL);
        } else if (id == XRCID("IDM_BPB_DISABLE")) {
                bpb_disable = !bpb_disable;
                wx_checkmenuitem(menu, id, bpb_disable);
                saveconfig(NULL);
        } else if (id == XRCID("IDM_DISC_CREATE")) {
                CreateDiscDialog::Open(hwnd);
        } else if (id == XRCID("IDM_DISC_ZIP")) {
                wxString file;
                if (wx_getfile(hwnd, "Disc image (*.img)|*.img|All files (*.*)|*.*", "", file))
                        zip_load((char *)(const char *)file.mb_str());
        } else if (id == XRCID("IDM_EJECT_ZIP")) {
                zip_eject();
        } else if (id == XRCID("IDM_CASSETTE_LOAD")) {
                wxString file;
                if (wx_getfile(hwnd, "Tape image (*.pzxi;*.pzx)|*.pzxi;*.pzx|All files (*.*)|*.*", cassettefn, file)) {
                        cassette_eject();
                        cassette_load((char *)(const char *)file.mb_str());
                        saveconfig(NULL);
                }
        } else if (id == XRCID("IDM_CASSETTE_EJECT")) {
                cassette_eject();
                saveconfig(NULL);
        } else if (id == XRCID("IDM_MACHINE_TOGGLE")) {
                if (emulation_state != EMULATION_STOPPED)
                        ((Frame *)hwnd)->ToggleVisible();
        } else if (id_in_range(id, "IDM_VID_RESOLUTION", &index)) {
                vid_resize = index;
                wx_checkmenuitem(menu, id, true);
                window_dosetresize = 1;
                wx_enablemenuitem(menu, XRCID("IDM_VID_SCALE_MENU"), !vid_resize);
                saveconfig(NULL);
        } else if (id == XRCID("IDM_VID_RESOLUTION_CUSTOM")) {
                if (CustomResolutionDialog::Open(hwnd)) {
                        if (vid_resize != 2) {
                                vid_resize = 2;
                                wx_checkmenuitem(menu, xrcid_index("IDM_VID_RESOLUTION", 2), true);
                                wx_enablemenuitem(menu, XRCID("IDM_VID_SCALE_MENU"), !vid_resize);
                        }
                        window_dosetresize = 1;
                }
        } else if (id == XRCID("IDM_SCREENSHOT")) {
                if (!hotkey_held(WXK_PAGEUP))
                        take_screenshot = 1;
        } else if (id_in_range(id, "IDM_SCREENSHOT_FORMAT", &index)) {
                static const char *formats[] = {IMAGE_PNG, IMAGE_TIFF, IMAGE_BMP, IMAGE_JPG};
                if (index < 4)
                        strcpy(screenshot_format, formats[index]);
                saveconfig(NULL);
                wx_checkmenuitem(menu, id, true);
        } else if (id == XRCID("IDM_SCREENSHOT_FLASH")) {
                screenshot_flash = !screenshot_flash;
                wx_checkmenuitem(menu, id, screenshot_flash);
                saveconfig(NULL);
        } else if (id == XRCID("IDM_VID_REMEMBER")) {
                window_remember = !window_remember;
                wx_checkmenuitem(menu, id, window_remember);
                window_doremember = 1;
                saveconfig(NULL);
        } else if (id == XRCID("IDM_VID_FULLSCREEN")) {
                if (video_fullscreen_first) {
                        video_fullscreen_first = 0;
                        wx_messagebox(hwnd, "Use CTRL + ALT + PAGE DOWN to return to windowed mode", "PCem", wxOK);
                }
                video_fullscreen = !video_fullscreen;
                wx_checkmenuitem(menu, id, video_fullscreen);
                saveconfig(NULL);
        } else if (id == XRCID("IDM_VID_FULLSCREEN_TOGGLE")) {
                if (!hotkey_held(WXK_PAGEDOWN))
                        toggle_fullscreen();
        } else if (id_in_range(id, "IDM_VID_FS", &index)) {
                video_fullscreen_scale = index;
                display_resize(video_width, video_height);
                wx_checkmenuitem(menu, id, true);
                saveconfig(NULL);
        } else if (id_in_range(id, "IDM_VID_SCALE_MODE", &index)) {
                video_scale_mode = index;
                renderer_doreset = 1;
                wx_checkmenuitem(menu, id, true);
                saveconfig(NULL);
        } else if (id_in_range(id, "IDM_VID_SCALE", &index)) {
                video_scale = index;
                wx_checkmenuitem(menu, id, true);
                display_resize(video_width, video_height);
                saveconfig(NULL);
        } else if (id_in_range(id, "IDM_VID_FS_MODE", &index)) {
                video_fullscreen_mode = index;
                wx_checkmenuitem(menu, id, true);
                saveconfig(NULL);
        } else if (id_in_range(id, "IDM_VID_RENDER_DRIVER", &index)) {
                requested_render_driver = sdl_get_render_driver_by_id(index, RENDERER_AUTO);
                window_doreset = 1;
                wx_enablemenuitem(menu, XRCID("IDM_VID_GL3"), requested_render_driver.id == RENDERER_GL3);
                wx_checkmenuitem(menu, id, true);
                saveconfig(NULL);
        } else if (id == XRCID("IDM_VID_VSYNC")) {
                video_vsync = !video_vsync;
                wx_checkmenuitem(menu, id, video_vsync);
                renderer_doreset = 1;
                saveconfig(NULL);
        } else if (id == XRCID("IDM_VID_LOST_FOCUS_DIM")) {
                video_focus_dim = !video_focus_dim;
                wx_checkmenuitem(menu, id, video_focus_dim);
                saveconfig(NULL);
        } else if (id == XRCID("IDM_VID_ALTERNATIVE_UPDATE_LOCK")) {
                video_alternative_update_lock = !video_alternative_update_lock;
                wx_checkmenuitem(menu, id, video_alternative_update_lock);
                renderer_doreset = 1;
                saveconfig(NULL);
        } else if (id_in_range(id, "IDM_VID_GL3_INPUT_STRETCH", &index)) {
                gl3_input_stretch = index;
                wx_checkmenuitem(menu, id, true);
                saveconfig(NULL);
        } else if (id_in_range(id, "IDM_VID_GL3_INPUT_SCALE", &index)) {
                gl3_input_scale = index / 2.0f + 0.5f;
                wx_checkmenuitem(menu, id, true);
                saveconfig(NULL);
        } else if (id_in_range(id, "IDM_VID_GL3_SHADER_REFRESH_RATE", &index)) {
                gl3_shader_refresh_rate = index;
                wx_checkmenuitem(menu, id, true);
                saveconfig(NULL);
        } else if (id == XRCID("IDM_VID_GL3_SHADER_MANAGER")) {
                if (ShaderManagerDialog::Open(hwnd)) {
                        renderer_doreset = 1;
                        saveconfig(NULL);
                }
        } else if (id_in_range(id, "IDM_SND_BUF", &index)) {
                sound_buf_len = MIN_SND_BUF * 1 << index;
                wx_checkmenuitem(menu, id, true);
                saveconfig(NULL);
        } else if (id_in_range(id, "IDM_SND_GAIN", &index)) {
                sound_gain = 2 * index;
                wx_checkmenuitem(menu, id, true);
                saveconfig(NULL);
        } else if (id == XRCID("IDM_CDROM_EMPTY")) {
                if (cdrom_drive != 0) {
                        eject_cdrom();
                        ioctl_set_drive(0);
                        old_cdrom_drive = cdrom_drive;
                        cdrom_drive = 0;
                        saveconfig(NULL);
                }
                update_cdrom_menu();
        } else if (id == XRCID("IDM_CDROM_IMAGE") || id == XRCID("IDM_CDROM_IMAGE_LOAD")) {
                wxString file;
                if (wx_getfile(hwnd, "CD-ROM image (*.iso;*.cue)|*.iso;*.cue|All files (*.*)|*.*", image_path, file)) {
                        char temp_image_path[1024];
                        wx_strlcpy(temp_image_path, file, sizeof(temp_image_path));
                        old_cdrom_drive = cdrom_drive;
                        if (strcmp(image_path, temp_image_path) || cdrom_drive != CDROM_IMAGE) {
                                eject_cdrom();
                                image_open(temp_image_path);
                                cdrom_drive = CDROM_IMAGE;
                                saveconfig(NULL);
                        }
                }
                update_cdrom_menu();
#if __linux__
        } else if (id == IDM_CDROM_DEVICE) {
                wxString path;
                if (wx_textentrydialog(hwnd, "CD-ROM device path:", "PCem", cdrom_device_path, 1, sizeof(cdrom_device_path) - 1,
                                       path)) {
                        old_cdrom_drive = cdrom_drive;
                        eject_cdrom();
                        wx_strlcpy(cdrom_device_path, path, sizeof(cdrom_device_path));
                        ioctl_set_drive(1);
                        cdrom_drive = 1;
                        saveconfig(NULL);
                        update_cdrom_menu();
                }
#else
        } else if (id >= IDM_CDROM_REAL && id < IDM_CDROM_REAL + 100) {
                int new_cdrom_drive = id - IDM_CDROM_REAL;
                if (cdrom_drive != new_cdrom_drive) {
                        old_cdrom_drive = cdrom_drive;
                        eject_cdrom();
                        ioctl_set_drive(new_cdrom_drive);
                        cdrom_drive = new_cdrom_drive;
                        saveconfig(NULL);
                }
                update_cdrom_menu();
#endif
        } else if (id >= IDM_VIEWER && id < IDM_VIEWER_MAX) {
                viewer_open(hwnd, id - IDM_VIEWER);
        }
}
