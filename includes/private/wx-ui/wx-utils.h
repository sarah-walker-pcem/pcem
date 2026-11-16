#ifndef _PCEM_WX_UTILS_H_
#define _PCEM_WX_UTILS_H_

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif
#include <wx/xrc/xmlres.h>

#include <string.h>

#include <pcem/api.h>
#include "sdl2-common.h"

extern "C" {
#include <pcem/config.h>
}

static inline int wx_config_get_int(int global, const char *head, const char *name, int def) {
        return config_get_int(global, (char *)head, (char *)name, def);
}
static inline float wx_config_get_float(int global, const char *head, const char *name, float def) {
        return config_get_float(global, (char *)head, (char *)name, def);
}
static inline const char *wx_config_get_string(int global, const char *head, const char *name, const char *def) {
        return config_get_string(global, (char *)head, (char *)name, (char *)def);
}
static inline void wx_config_set_int(int global, const char *head, const char *name, int val) {
        config_set_int(global, (char *)head, (char *)name, val);
}
static inline void wx_config_set_float(int global, const char *head, const char *name, float val) {
        config_set_float(global, (char *)head, (char *)name, val);
}
static inline void wx_config_set_string(int global, const char *head, const char *name, const char *val) {
        config_set_string(global, (char *)head, (char *)name, (char *)val);
}
static inline void wx_config_free_section(int global, const char *head) { config_free_section(global, (char *)head); }

template <typename T> T *wx_ctrl(wxWindow *parent, const wxString &name) {
        return wxStaticCast(parent->FindWindow(XRCID(name)), T);
}

static inline void wx_strlcpy(char *dst, const wxString &src, size_t size) {
        strncpy(dst, src.mb_str(), size - 1);
        dst[size - 1] = 0;
}

int wx_messagebox(wxWindow *parent, const wxString &message, const wxString &title, int style = wxOK);
void wx_simple_messagebox(const char *title, const char *format, ...);
bool wx_textentrydialog(wxWindow *parent, const wxString &message, const wxString &title, const wxString &value,
                        unsigned int min_length, unsigned int max_length, wxString &result);
bool wx_filedialog(wxWindow *parent, const wxString &title, const wxString &path, const wxString &wildcard,
                   const wxString &default_ext, bool open, wxString &result);
int confirm();
void wx_fit_dialog(wxDialog *dlg);

void wx_combobox_clear(wxComboBox *cb);
void wx_combobox_select(wxComboBox *cb, int index);

wxMenu *wx_getsubmenu(wxMenuBar *menubar, int id);

#endif /* _PCEM_WX_UTILS_H_ */
