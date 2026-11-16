#ifndef _WX_FRONTEND_H_
#define _WX_FRONTEND_H_

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

void wx_frontend_init();
bool wx_load_config(wxWindow *hwnd);
int wx_start(wxWindow *hwnd);
int wx_stop(wxWindow *hwnd);
void wx_handle_command(wxWindow *hwnd, int id, bool checked);

bool wx_getfile(wxWindow *parent, const wxString &wildcard, const wxString &path, wxString &result,
                const wxString &caption = "Open");
bool wx_getsfile(wxWindow *parent, const wxString &wildcard, const wxString &path, const wxString &ext, wxString &result);

#endif /* _WX_FRONTEND_H_ */
