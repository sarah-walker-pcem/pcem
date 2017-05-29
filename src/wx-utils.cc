#include "wx-utils.h"

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/spinctrl.h>
#include <wx/xrc/xmlres.h>
#include <wx/stdpaths.h>

#include "wx-dialogbox.h"
#include "wx-app.h"
#include "wx-common.h"
#include "wx-status.h"

int confirm()
{
        if (emulation_state != EMULATION_STOPPED) {
                return wx_messagebox(NULL, "This will reset PCem!\nOkay to continue?", "PCem", WX_MB_OKCANCEL) == WX_IDOK;
        }
        return 1;
}

int wx_messagebox(void* window, const char* message, const char* title = NULL, int style = 5)
{
        return wxMessageBox(message, title, style | wxCENTRE, (wxWindow*) window);
}

int wx_textentrydialog(void* window, const char* message, const char* title, const char* value, int min_length, int max_length, LONG_PARAM result)
{
        while (1)
        {
                wxTextEntryDialog dlg((wxWindow*)window, message, title, value);
                if (max_length > 0)
                        dlg.SetMaxLength(max_length);
                if (dlg.ShowModal() == wxID_OK)
                {
                        wxString value = dlg.GetValue();
                        if (value.Length() >= min_length)
                        {
                                strcpy((char*)result, value);
                                return TRUE;
                        }
                }
                else
                        return FALSE;
        }
        return FALSE;
}

void wx_setwindowtitle(void* window, char* s)
{
        ((wxFrame*) window)->SetTitle(s);
}

int wx_xrcid(const char* s)
{
        return XRCID(s);
}

int wx_filedialog(void* window, const char* title, const char* path,
                const char* extensions, const char* extension, int open, char* file)
{
        wxFileDialog dlg((wxWindow*) window, title, "", path, extensions,
                        open ? wxFD_OPEN : wxFD_SAVE);
        if (dlg.ShowModal() == wxID_OK)
        {
                wxString p = dlg.GetPath();
                wxFileName f(p);
                if (!open && !f.HasExt() && extension)
                {
                        wxString e = extension;
                        if (!p.EndsWith("."))
                                p += ".";
                        p += extension;
                        strcpy(file, p);
                }
                else
                        strcpy(file, p);
                return 0;
        }
        return 1;
}

void wx_checkmenuitem(void* menu, int id, int checked)
{
        wxMenuItem* item = NULL;
        if (((wxObject*)menu)->GetClassInfo()->IsKindOf(CLASSINFO(wxMenuBar)))
                item = ((wxMenuBar*) menu)->FindItem(id);
        else
                item = ((wxMenu*) menu)->FindItem(id);

        if (item)
                item->Check(checked);
        else
                std::cout << "Menu item not found: " << id << std::endl;
}

void wx_enablemenuitem(void* menu, int id, int enable)
{
        wxMenuItem* item = NULL;
        if (((wxObject*)menu)->GetClassInfo()->IsKindOf(CLASSINFO(wxMenuBar)))
                item = ((wxMenuBar*) menu)->FindItem(id);
        else
                item = ((wxMenu*) menu)->FindItem(id);

        if (item)
                item->Enable(enable);
        else
                std::cout << "Menu item not found: " << id << std::endl;
}

void* wx_getsubmenu(void* menu, int id)
{
        wxMenuItem* m = ((wxMenu*) menu)->FindItem(id);
        if (m)
                return m->GetSubMenu();

        return 0;
}

void wx_appendmenu(void* sub_menu, int id, const char* title, enum wxItemKind type)
{
        ((wxMenu*) sub_menu)->Append(id, title, wxEmptyString, type);
}


void wx_enabletoolbaritem(void* toolbar, int id, int enable)
{
        ((wxToolBar*) toolbar)->EnableTool(id, enable);
}

void* wx_getmenu(void* window)
{
        return ((Frame*) window)->GetMenu();
}

void* wx_getmenubar(void* window)
{
        return ((wxFrame*) window)->GetMenuBar();
}

void* wx_gettoolbar(void* window)
{
        return ((wxFrame*) window)->GetToolBar();
}

void* wx_getdlgitem(void* window, int id)
{
        return ((wxWindow*) window)->FindWindow(id);
}

void wx_setdlgitemtext(void* window, int id, char* str)
{
        wxWindow* w = ((wxWindow*) window)->FindWindow(id);
        if (w->GetClassInfo()->IsKindOf(CLASSINFO(wxTextCtrl)))
                ((wxTextCtrl*) w)->SetValue(str);
        else
                ((wxStaticText*) w)->SetLabel(str);
}

void wx_enablewindow(void* window, int enabled)
{
        ((wxWindow*) window)->Enable(enabled);
}

void wx_showwindow(void* window, int show)
{
        wxCommandEvent* event = new wxCommandEvent(WX_SHOW_WINDOW_EVENT, wxID_ANY);
        event->SetEventObject((wxWindow*)window);
        event->SetInt(show);
        wxQueueEvent((wxWindow*)window, event);
}

void wx_togglewindow(void* window)
{
        wx_showwindow(window, -1);
}

void wx_callback(void* window, WX_CALLBACK callback)
{
        CallbackEvent* event = new CallbackEvent(callback);
        wxQueueEvent((wxWindow*)window, event);
}

void wx_enddialog(void* window, int ret_code)
{
        ((wxDialog*) window)->EndModal(ret_code);
}

int wx_dlgdirlist(void* window, const char* path, int id, int static_path, int file_type)
{
        wxWindow* w = ((wxWindow*) window)->FindWindow(id);
        if (w->GetClassInfo()->IsKindOf(CLASSINFO(wxListBox)))
        {
                wxListBox* list = (wxListBox*)w;
                list->Clear();
                wxArrayString items;
                wxString f = wxFindFirstFile(path);
                while (!f.empty())
                {
                        wxFileName file(f);
                        items.Add(file.GetName());
                        f = wxFindNextFile();
                }
                items.Sort();
                list->Set(items);
                return TRUE;
        }
        return FALSE;
}

int wx_dlgdirselectex(void* window, LONG_PARAM path, int count, int id)
{
        wxWindow* w = ((wxWindow*) window)->FindWindow(id);
        if (w->GetClassInfo()->IsKindOf(CLASSINFO(wxListBox)))
        {
                wxListBox* list = (wxListBox*)w;
                strcpy((char*)path, (list->GetStringSelection() + ".").GetData().AsChar());
                return TRUE;
        }
        return FALSE;
}

int wx_sendmessage(void* window, int type, INT_PARAM param1, LONG_PARAM param2)
{
        switch (type)
        {
        case WX_CB_ADDSTRING:
                ((wxComboBox*) window)->Append((char*) param2);
                break;
        case WX_CB_SETCURSEL:
                ((wxComboBox*) window)->Select(param1);
                break;
        case WX_CB_GETCURSEL:
                return ((wxComboBox*) window)->GetCurrentSelection();
                break;
        case WX_CB_GETLBTEXT:
                strcpy((char*) param2, ((wxComboBox*) window)->GetString(param1));
                break;
        case WX_CB_RESETCONTENT:
        {
#ifndef __WXOSX_MAC__
                ((wxComboBox*) window)->Clear();
#else
                /* Clear() does not work on OSX */
                wxComboBox* cb = (wxComboBox*) window;
                int count = cb->GetCount();
                while (count--)
                        cb->Delete(0);
#endif
                break;
        }
        case WX_BM_SETCHECK:
                ((wxCheckBox*) window)->SetValue(param1);
                break;
        case WX_BM_GETCHECK:
                return ((wxCheckBox*) window)->GetValue();
                break;
        case WX_WM_SETTEXT:
        {
                if (((wxWindow*) window)->GetClassInfo()->IsKindOf(CLASSINFO(wxTextCtrl)))
                        ((wxTextCtrl*) window)->SetValue((char*) param2);
                else
                        ((wxStaticText*) window)->SetLabel((char*) param2);
        }
                break;
        case WX_WM_GETTEXT:
                if (((wxWindow*) window)->GetClassInfo()->IsKindOf(CLASSINFO(wxTextCtrl)))
                        strcpy((char*) param2, ((wxTextCtrl*) window)->GetValue());
                else
                        strcpy((char*) param2, ((wxStaticText*) window)->GetLabel());
                break;
        case WX_UDM_SETPOS:
                ((wxSpinCtrl*) window)->SetValue(param2);
                break;
        case WX_UDM_GETPOS:
                return ((wxSpinCtrl*) window)->GetValue();
        case WX_UDM_SETRANGE:
        {
                int min = (param2 >> 16) & 0xffff;
                int max = param2 & 0xffff;
                ((wxSpinCtrl*) window)->SetRange(min, max);
                break;
        }
        case WX_UDM_SETINCR:
        {
                /* SetIncrement-method may not exist on all platforms */
#ifdef __WXGTK__
                ((wxSpinCtrl*) window)->SetIncrement(param2);
#endif
                break;
        }
        case WX_LB_GETCOUNT:
        {
                return ((wxListBox*) window)->GetCount();
        }
        case WX_LB_GETCURSEL:
        {
                return ((wxListBox*) window)->GetSelection();
        }
        case WX_LB_GETTEXT:
        {
                strcpy((char*) param2, ((wxListBox*) window)->GetString(param1));
                break;
        }
        case WX_LB_INSERTSTRING:
        {
                ((wxListBox*) window)->Insert((char*) param2, param1);
                break;
        }
        case WX_LB_DELETESTRING:
        {
                ((wxListBox*) window)->Delete(param1);
                break;
        }
        }

        ((wxWindow*)window)->Fit();

        return 0;
}

int wx_dialogbox(void* window, const char* name, int (*callback)(void* window, int message, INT_PARAM param1, LONG_PARAM param2))
{
        PCemDialogBox dlg((wxWindow*) window, name, callback);
        dlg.OnInit();
        dlg.Fit();
        int ret = dlg.ShowModal();
        dlg.Destroy();
        return ret;
}

void wx_exit(void* window, int value)
{
        wxCommandEvent* event = new wxCommandEvent(WX_EXIT_EVENT, wxID_ANY);
        event->SetEventObject((wxWindow*)window);
        event->SetInt(value);
        wxQueueEvent((wxWindow*)window, event);
}

void wx_stop_emulation(void* window)
{
        wxCommandEvent* event = new wxCommandEvent(WX_STOP_EMULATION_EVENT, wxID_ANY);
        event->SetEventObject((wxWindow*)window);
        wxQueueEvent((wxWindow*)window, event);
}

int wx_yield()
{
	return wxYield();
}

/* Timer stuff */

class PCemTimer : public wxTimer
{
public:
        PCemTimer(void (*fn)())
        {
                this->fn = fn;
        }
        virtual ~PCemTimer() {};

        void Notify()
        {
                fn();
        }
private:
        void (*fn)();
};

void* wx_createtimer(void (*fn)())
{
        return new PCemTimer(fn);
}

void wx_starttimer(void* timer, int milliseconds, int once)
{
        wxTimer* t = (wxTimer*)timer;
        t->Start(milliseconds, once);
}

void wx_stoptimer(void* timer)
{
        wxTimer* t = (wxTimer*)timer;
        t->Stop();
}

void wx_destroytimer(void* timer)
{
        wx_stoptimer(timer);
        delete (wxTimer*)timer;
}

void wx_popupmenu(void* window, void* menu, int* x, int* y)
{
        PopupMenuEvent* event = new PopupMenuEvent((wxWindow*)window, (wxMenu*)menu, x, y);
        wxQueueEvent((wxWindow*)window, event);
}

void* wx_create_status_frame(void* window)
{
        StatusFrame* frame = new StatusFrame((wxWindow*)window);
        return frame;
}

void wx_destroy_status_frame(void* window)
{
        StatusFrame* frame = (StatusFrame*)window;
        delete frame;
}

void wx_setwindowposition(void* window, int x, int y)
{
        ((wxFrame*) window)->SetPosition(wxPoint(x, y));
}

void wx_setwindowsize(void* window, int width, int height)
{
        ((wxFrame*) window)->SetSize(wxSize(width, height));
}

void wx_show_status(void* window)
{
        wxWindow* status = ((wxWindow*)window)->FindWindowById(STATUS_WINDOW_ID);
        if (!status)
        {
                StatusFrame* statusFrame = new StatusFrame((wxWindow*) window);
                statusFrame->Show();
        }
}

void wx_close_status(void* window)
{
        wxWindow* status = ((wxWindow*)window)->FindWindowById(STATUS_WINDOW_ID);
        if (status)
        {
                status->Close();
        }
}
