#include <wx/defs.h>

#ifndef LONG_PARAM
#define LONG_PARAM wxIntPtr
#endif

#ifndef INT_PARAM
#define INT_PARAM wxInt32
#endif

typedef int (*WX_CALLBACK)(void* data);

#ifdef __cplusplus
extern "C" {
#endif
        int wx_messagebox(void* nothing, const char* message, const char* title, int style);
        void wx_simple_messagebox(const char* title, const char *format, ...);

        int wx_xrcid(const char* s);
        int wx_filedialog(void* window, const char* title, const char* path, const char* extensions, const char* extension, int open, char* file);
        void wx_checkmenuitem(void* window, int id, int checked);
        void wx_enablemenuitem(void* menu, int id, int enable);
        void* wx_getmenu(void* window);
        void* wx_getmenubar(void* window);
        void wx_enabletoolbaritem(void* toolbar, int id, int enable);
        void* wx_gettoolbar(void* window);

        void* wx_getsubmenu(void* window, int id);
        void wx_appendmenu(void* sub_menu, int id, const char* title, enum wxItemKind type);
        void* wx_getnativemenu(void* menu);

        int wx_textentrydialog(void* window, const char* message, const char* title, const char* value, unsigned int min_length, unsigned int max_length, LONG_PARAM result);

        int wx_dlgdirlist(void* window, const char* path, int id, int static_path, int file_type);
        int wx_dlgdirselectex(void* window, LONG_PARAM path, int count, int id);

        void wx_setwindowtitle(void* window, char* s);
        int wx_sendmessage(void* window, int type, LONG_PARAM param1, LONG_PARAM param2);
        void* wx_getdlgitem(void* window, int id);
        void wx_setdlgitemtext(void* window, int id, char* str);
        void wx_enablewindow(void* window, int enabled);
        void wx_showwindow(void* window, int show);
        int wx_iswindowvisible(void* window);
        void* wx_getnativewindow(void* window);
        void wx_callback(void* window, WX_CALLBACK callback, void* data);
        void wx_togglewindow(void* window);
        int wx_progressdialog(void* window, const char* title, const char* message, WX_CALLBACK callback, void* data, int range, volatile int *pos);

        void wx_enddialog(void* window, int ret_code);

        int wx_dialogbox(void* window, const char* name, int(*callback)(void* window, int message, INT_PARAM param1, LONG_PARAM param2));

        void wx_exit(void* window, int value);
        void wx_stop_emulation(void* window);

        void* wx_createtimer(void (*fn)());
        void wx_starttimer(void* timer, int milliseconds, int once);
        void wx_stoptimer(void* timer);
        void wx_destroytimer(void* timer);

        void wx_popupmenu(void* window, void* menu, int* x, int* y);

        void* wx_create_status_frame(void* window);
        void wx_destroy_status_frame(void* window);

        int wx_yield();

        void wx_setwindowposition(void* window, int x, int y);
        void wx_setwindowsize(void* window, int width, int height);

        void wx_show_status(void* window);
        void wx_close_status(void* window);

        void wx_get_home_directory(char* path);
        int wx_create_directory(char* path);

        int wx_setup(char* path);
        int wx_file_exists(char* path);
        int wx_dir_exists(char* path);
        int wx_copy_file(char* from, char* to, int overwrite);

        void wx_date_format(char* s, const char* format);

        int wx_image_save(const char* path, const char* name, const char* format, unsigned char* rgba, int width, int height, int alpha);

        void* wx_image_load(const char* path);
        void wx_image_rescale(void* image, int width, int height);
        void wx_image_get_size(void* image, int* width, int* height);
        unsigned char* wx_image_get_data(void* image);
        unsigned char* wx_image_get_alpha(void* image);
        void wx_image_free(void* image);

        void* wx_config_load(const char* path);
        int wx_config_get_string(void* config, const char* name, char* dst, int size, const char* defVal);
        int wx_config_get_int(void* config, const char* name, int* dst, int defVal);
        int wx_config_get_float(void* config, const char* name, float* dst, float defVal);
        int wx_config_get_bool(void* config, const char* name, int* dst, int defVal);
        int wx_config_has_entry(void* config, const char* name);
        void wx_config_free(void* config);

        int confirm();

#ifdef _WIN32
        void wx_winsendmessage(void* window, int msg, INT_PARAM wParam, LONG_PARAM lParam);
#endif
#ifdef __cplusplus
}
#endif

extern int (*wx_keydown_func)(void* window, void* event, int keycode, int modifiers);
extern int (*wx_keyup_func)(void* window, void* event, int keycode, int modifiers);
extern void (*wx_idle_func)(void* window, void* event);

#define WX_ID wx_xrcid

#define WX_MB_CHECKED 1
#define WX_MB_UNCHECKED 0

#define WX_INITDIALOG 1
#define WX_COMMAND 2

#define WX_CB_ADDSTRING 1
#define WX_CB_SETCURSEL 2
#define WX_CB_GETCURSEL 3
#define WX_CB_RESETCONTENT 4
#define WX_CB_GETLBTEXT 5

#define WX_BM_SETCHECK 20
#define WX_BM_GETCHECK 21

#define WX_WM_SETTEXT 40
#define WX_WM_GETTEXT 41

#define WX_UDM_SETPOS 50
#define WX_UDM_GETPOS 51
#define WX_UDM_SETINCR 52
#define WX_UDM_SETRANGE 53

#define WX_LB_GETCOUNT 60
#define WX_LB_GETCURSEL 61
#define WX_LB_GETTEXT 62
#define WX_LB_DELETESTRING 63
#define WX_LB_INSERTSTRING 64
#define WX_LB_RESETCONTENT 65
#define WX_LB_SETCURSEL 66
#define WX_LBN_DBLCLK 67

#define WX_CHB_SETPAGETEXT 68
#define WX_CHB_ADDPAGE 69
#define WX_CHB_REMOVEPAGE 70
#define WX_CHB_GETPAGECOUNT 71

#define WX_REPARENT 72


#define WX_MB_OK wxOK
#define WX_MB_OKCANCEL wxOK|wxCANCEL
#define WX_IDOK wxOK

#define IMAGE_JPG "jpg"
#define IMAGE_PNG "png"
#define IMAGE_BMP "bmp"
#define IMAGE_TIFF "tiff"

extern int has_been_inited;

#define ID_IS(s) wParam == wx_xrcid(s)
#define ID_RANGE(a, b) wParam >= wx_xrcid(a) && wParam <= wx_xrcid(b)
