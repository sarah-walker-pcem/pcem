#include "wx-utils.h"

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/spinctrl.h>
#include <wx/xrc/xmlres.h>
#include <wx/stdpaths.h>
#include <wx/fileconf.h>
#include <wx/wfstream.h>
#include <wx/progdlg.h>
#include <wx/choicebk.h>
#include <wx/combobox.h>

#include "wx-dialogbox.h"
#include "wx-app.h"
#include "wx-common.h"
#include "wx-status.h"

extern "C"
{
#include "thread.h"
}

int confirm()
{
        if (emulation_state != EMULATION_STOPPED) {
                return wx_messagebox(NULL, "This will reset PCem!\nOkay to continue?", "PCem", WX_MB_OKCANCEL) == WX_IDOK;
        }
        return 1;
}

int wx_messagebox(void* window, const char* message, const char* title = NULL, int style = 5)
{
        return wxMessageBox(message, title, style | wxCENTRE | wxSTAY_ON_TOP, (wxWindow*) window);
}

void wx_simple_messagebox(const char* title, const char *format, ...)
{
        char message[2048];
        va_list ap;
        va_start(ap, format);
        vsnprintf(message, 2047, format, ap);
        message[2047] = 0;
        va_end(ap);
        wx_messagebox(0, message, title, WX_MB_OK);
}


int wx_textentrydialog(void* window, const char* message, const char* title, const char* value, unsigned int min_length, unsigned int max_length, LONG_PARAM result)
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
                                strcpy((char*)result, value.mb_str());
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
                        open ? (wxFD_OPEN | wxFD_FILE_MUST_EXIST) : (wxFD_SAVE | wxFD_OVERWRITE_PROMPT));
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
                        strcpy(file, p.mb_str());
                }
                else
                        strcpy(file, p.mb_str());
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

void* wx_getnativemenu(void* menu)
{
#ifdef _WIN32
        return ((wxMenu*)menu)->GetHMenu();
#endif
        return 0;
}

void* wx_getnativewindow(void* window)
{
#ifdef _WIN32
        return ((wxWindow*)window)->GetHWND();
#endif
        return 0;
}

#ifdef _WIN32
void wx_winsendmessage(void* window, int msg, INT_PARAM wParam, LONG_PARAM lParam)
{
        WinSendMessageEvent* event = new WinSendMessageEvent(wx_getnativewindow(window), msg, wParam, lParam);
        wxQueueEvent((wxWindow*)window, event);
}
#endif

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

int wx_iswindowvisible(void* window)
{
        return ((wxWindow*) window)->IsShown();
}

void wx_togglewindow(void* window)
{
        wx_showwindow(window, -1);
}

void wx_callback(void* window, WX_CALLBACK callback, void* data)
{
        CallbackEvent* event = new CallbackEvent(callback, data);
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

int wx_sendmessage(void* window, int type, LONG_PARAM param1, LONG_PARAM param2)
{
        switch (type)
        {
        case WX_CB_ADDSTRING:
                ((wxComboBox*) window)->Append((char*) param2);
                break;
        case WX_CB_SETCURSEL:
                if (param1 >= 0 && param1 < (int)((wxComboBox*) window)->GetCount())
                        ((wxComboBox*) window)->Select(param1);
                break;
        case WX_CB_GETCURSEL:
                return ((wxComboBox*) window)->GetCurrentSelection();
                break;
        case WX_CB_GETLBTEXT:
                strcpy((char*) param2, ((wxComboBox*) window)->GetString(param1).mb_str());
                break;
        case WX_CB_RESETCONTENT:
        {
#ifndef __WXOSX_MAC__
                ((wxComboBox*) window)->SetValue("");
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
                        strcpy((char*) param2, ((wxTextCtrl*) window)->GetValue().mb_str());
                else
                        strcpy((char*) param2, ((wxStaticText*) window)->GetLabel().mb_str());
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
        case WX_LB_SETCURSEL:
        {
                ((wxListBox*) window)->SetSelection(param1);
                break;
        }
        case WX_LB_GETTEXT:
        {
                strcpy((char*) param2, ((wxListBox*) window)->GetString(param1).mb_str());
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
        case WX_LB_RESETCONTENT:
        {
                ((wxListBox*) window)->Clear();
                break;
        }
        case WX_CHB_SETPAGETEXT:
        {
                ((wxChoicebook*)window)->SetPageText(param1, (char *)param2);
                break;
        }
        case WX_CHB_ADDPAGE:
        {
                ((wxChoicebook*)window)->AddPage((wxWindow *)param1, (char *)param2);
                break;
        }
        case WX_CHB_REMOVEPAGE:
        {
                ((wxChoicebook*)window)->RemovePage(param1);
                break;
        }
        case WX_CHB_GETPAGECOUNT:
        {
                return ((wxChoicebook*)window)->GetPageCount();
        }
        case WX_REPARENT:
        {
                ((wxWindow *)window)->Reparent((wxWindow *)param1);
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

void wx_get_home_directory(char* path)
{
        wxString home = wxFileName::GetHomeDir();
        if (!home.EndsWith(wxFileName::GetPathSeparator())) {
                home.Append(wxFileName::GetPathSeparator());
        }
        strcpy(path, home.mb_str());
}

int wx_create_directory(char* path)
{
        return wxFileName::Mkdir(path);
}

int wx_setup(char* path)
{
        wxFileName p(path);
        if (!p.DirExists())
        {
                if (!p.Mkdir())
                        return FALSE;
                wxFileName configs(p);
                configs.AppendDir("configs");
                if (!configs.DirExists() && !configs.Mkdir())
                        return FALSE;

                wxFileName roms(p);
                roms.AppendDir("roms");
                if (!roms.DirExists() && !roms.Mkdir())
                        return FALSE;

                wxFileName nvr(p);
                nvr.AppendDir("nvr");
                if (!nvr.DirExists() && !nvr.Mkdir())
                        return FALSE;

                wxFileName screenshots(p);
                screenshots.AppendDir("screenshots");
                if (!screenshots.DirExists() && !screenshots.Mkdir())
                        return FALSE;

                wxFileName logs(p);
                logs.AppendDir("logs");
                if (!logs.DirExists() && !logs.Mkdir())
                        return FALSE;
        }

        return TRUE;
}

int wx_file_exists(char* path)
{
        wxFileName p(path);
        return p.Exists();
}

int wx_dir_exists(char* path)
{
        wxFileName p(path);
        return p.DirExists();
}

int wx_copy_file(char* from, char* to, int overwrite)
{
        return wxCopyFile(from, to, overwrite);
}

int wx_image_save(const char* path, const char* name, const char* format, unsigned char* rgba, int width, int height, int alpha)
{
        int x, y;
        wxLogNull logNull;
        wxImage image(width, height);
        if (alpha)
        {
                unsigned char* rgb = (unsigned char*)malloc(width*height*3);
                unsigned char* a = (unsigned char*)malloc(width*height);
                for (x = 0; x < width; ++x)
                {
                        for (y = 0; y < height; ++y)
                        {
                                rgb[(y*width+x)*3+0] = rgba[(y*width+x)*4+0];
                                rgb[(y*width+x)*3+1] = rgba[(y*width+x)*4+1];
                                rgb[(y*width+x)*3+2] = rgba[(y*width+x)*4+2];
                                a[y*width+x] = rgba[(y*width+x)*4+3];
                        }
                }
                image.SetData(rgb);
                image.SetAlpha(a);
        }
        else
                image.SetData(rgba, true);

        wxImageHandler* h;
        wxString ext;

        if (!strcmp(format, IMAGE_TIFF))
        {
                h = new wxTIFFHandler();
                ext = "tif";
        }
        else if (!strcmp(format, IMAGE_BMP))
        {
                h = new wxBMPHandler();
                ext = "bmp";
        }
        else if (!strcmp(format, IMAGE_JPG))
        {
                h = new wxJPEGHandler();
                ext = "jpg";
        }
        else
        {
                h = new wxPNGHandler();
                ext = "png";
        }

        int res = 0;
        if (h)
        {
                wxString p(path);
                if (!p.EndsWith("/"))
                        p = p.Append("/");
                p = p.Append(name).Append(".").Append(ext);

                wxFileOutputStream stream(p);
                res = h->SaveFile(&image, stream, false);
                delete h;
        }

        return res;
}

void* wx_image_load(const char* path)
{
        wxLogNull logNull; /* removes sRGB-warnings on Windows */
        wxImage* image = new wxImage(path);
        if (image->IsOk())
                return image;
        delete image;
        return 0;
}

void wx_image_free(void* image)
{
        delete (wxImage*)image;
}

void wx_image_rescale(void* image, int width, int height)
{
        ((wxImage*)image)->Rescale(width, height);
}

void wx_image_get_size(void* image, int* width, int* height)
{
        *width = ((wxImage*)image)->GetWidth();
        *height = ((wxImage*)image)->GetHeight();
}

unsigned char* wx_image_get_data(void* image)
{
        return ((wxImage*)image)->GetData();
}

unsigned char* wx_image_get_alpha(void* image)
{
        return ((wxImage*)image)->GetAlpha();
}

/* wxFileConfig */

void* wx_config_load(const char* path)
{
        if (!wxFileExists(path))
                return 0;
        wxFileInputStream stream(path);
        if (stream.IsOk())
                return new wxFileConfig(stream);
        return 0;
}
int wx_config_get_string(void* config, const char* name, char* dst, int size, const char* defVal)
{
        wxFileConfig* c = ((wxFileConfig*)config);
        wxString val;
        bool res = c->Read(name, &val, wxString(defVal));
        strncpy(dst, val.GetData().AsChar(), size-1);
        dst[size-1] = 0;
        return res;
}
int wx_config_get_int(void* config, const char* name, int* dst, int defVal)
{
        wxFileConfig* c = ((wxFileConfig*)config);
        return c->Read(name, dst, defVal);
}

int wx_config_get_float(void* config, const char* name, float* dst, float defVal)
{
        wxFileConfig* c = ((wxFileConfig*)config);
        return c->Read(name, dst, defVal);

}

int wx_config_get_bool(void* config, const char* name, int* dst, int defVal)
{
        wxFileConfig* c = ((wxFileConfig*)config);
        wxString val;
        bool res = 1;
        if (c->Read(name, &val))
        {
                val.LowerCase();
                if (val.StartsWith("true"))
                        *dst = 1;
                else if (val.StartsWith("false"))
                        *dst = 0;
                else
                        res = 0;
        }
        if (!res)
                res = c->Read(name, (bool*)dst, (bool)defVal);

        return res;
}

int wx_config_has_entry(void* config, const char* name)
{
        wxFileConfig* c = ((wxFileConfig*)config);
        return c->HasEntry(name);
}

void wx_config_free(void* config)
{
        delete (wxFileConfig*)config;
}

typedef struct progress_data_t {
        WX_CALLBACK callback;
        void* data;
        int active;
        int result;
} progress_data_t;

static void progress_callback(void* data)
{
        progress_data_t* d = (progress_data_t*) data;
        d->result = d->callback(d->data);
        d->active = 0;
}

int wx_progressdialog(void* window, const char* title, const char* message, WX_CALLBACK callback, void* data, int range, volatile int *pos)
{
        struct progress_data_t pdata;
        pdata.callback = callback;
        pdata.data = data;
        pdata.active = 1;
        pdata.result = 0;

        thread_t* t = thread_create(progress_callback, &pdata);

        wxProgressDialog dlg(title, message, range, (wxWindow*)window, wxPD_APP_MODAL | wxPD_SMOOTH | wxPD_AUTO_HIDE);
        while (pdata.active)
        {
                dlg.Update(*pos);
                wxMilliSleep(50);
        }
        thread_kill(t);

        return pdata.result;
}

void wx_date_format(char* s, const char* format)
{
        wxString res = wxDateTime::Now().Format(format);
        strcpy(s, res.mb_str());
}
