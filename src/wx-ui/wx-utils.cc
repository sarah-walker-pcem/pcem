#include "wx-utils.h"

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/spinctrl.h>
#include <wx/xrc/xmlres.h>
#include <wx/xml/xml.h>
#include <wx/stdpaths.h>
#include <wx/fileconf.h>
#include <wx/wfstream.h>
#include <wx/progdlg.h>
#include <wx/choicebk.h>
#include <wx/combobox.h>
#include <wx/simplebook.h>

#include "wx-dialogbox.h"
#include "wx-app.h"
#include "wx-common.h"
#include "wx-status.h"

extern "C" {
#include "thread.h"
}

int confirm() {
        if (emulation_state != EMULATION_STOPPED) {
                return wx_messagebox(NULL, "This will reset PCem!\nOkay to continue?", "PCem", WX_MB_OKCANCEL) == WX_IDOK;
        }
        return 1;
}

int wx_messagebox(void *window, const char *message, const char *title = NULL, int style = 5) {
        return wxMessageBox(message, title, style | wxCENTRE | wxSTAY_ON_TOP, (wxWindow *)window);
}

void wx_simple_messagebox(const char *title, const char *format, ...) {
        char message[2048];
        va_list ap;
        va_start(ap, format);
        vsnprintf(message, 2047, format, ap);
        message[2047] = 0;
        va_end(ap);
        wx_messagebox(0, message, title, WX_MB_OK);
}

int wx_textentrydialog(void *window, const char *message, const char *title, const char *value, unsigned int min_length,
                       unsigned int max_length, LONG_PARAM result) {
        while (1) {
                wxTextEntryDialog dlg((wxWindow *)window, message, title, value);
                if (max_length > 0)
                        dlg.SetMaxLength(max_length);
                if (dlg.ShowModal() == wxID_OK) {
                        wxString value = dlg.GetValue();
                        if (value.Length() >= min_length) {
                                strcpy((char *)result, value.mb_str());
                                return TRUE;
                        }
                } else
                        return FALSE;
        }
        return FALSE;
}

void wx_setwindowtitle(void *window, char *s) { ((wxFrame *)window)->SetTitle(s); }

int wx_xrcid(const char *s) { return XRCID(s); }

int wx_filedialog(void *window, const char *title, const char *path, const char *extensions, const char *extension, int open,
                  char *file) {
        wxFileDialog dlg((wxWindow *)window, title, "", path, extensions,
                         open ? (wxFD_OPEN | wxFD_FILE_MUST_EXIST) : (wxFD_SAVE | wxFD_OVERWRITE_PROMPT));
        if (dlg.ShowModal() == wxID_OK) {
                wxString p = dlg.GetPath();
                wxFileName f(p);
                if (!open && !f.HasExt() && extension) {
                        wxString e = extension;
                        if (!p.EndsWith("."))
                                p += ".";
                        p += extension;
                        strcpy(file, p.mb_str());
                } else
                        strcpy(file, p.mb_str());
                return 0;
        }
        return 1;
}

void wx_checkmenuitem(void *menu, int id, int checked) {
        wxMenuItem *item = NULL;
        if (((wxObject *)menu)->GetClassInfo()->IsKindOf(CLASSINFO(wxMenuBar)))
                item = ((wxMenuBar *)menu)->FindItem(id);
        else
                item = ((wxMenu *)menu)->FindItem(id);

        if (item)
                item->Check(checked);
#ifndef RELEASE_BUILD
        else
                std::cout << "Menu item not found: " << id << std::endl;
#endif
}

void wx_enablemenuitem(void *menu, int id, int enable) {
        wxMenuItem *item = NULL;
        if (((wxObject *)menu)->GetClassInfo()->IsKindOf(CLASSINFO(wxMenuBar)))
                item = ((wxMenuBar *)menu)->FindItem(id);
        else
                item = ((wxMenu *)menu)->FindItem(id);

        if (item)
                item->Enable(enable);
#ifndef RELEASE_BUILD
        else
                std::cout << "Menu item not found: " << id << std::endl;
#endif
}

void *wx_getsubmenu(void *menu, int id) {
        wxMenuItem *m;
        if (((wxObject *)menu)->GetClassInfo()->IsKindOf(CLASSINFO(wxMenuBar))) {
                wxMenuBar *menubar = (wxMenuBar *)menu;
                const wxXmlNode *node = wxXmlResource::Get()->GetResourceNode("main_menu");
                size_t index = 0;
                for (const wxXmlNode *child = node ? node->GetChildren() : NULL; child; child = child->GetNext()) {
                        if (child->GetType() != wxXML_ELEMENT_NODE || child->GetAttribute("class") != "wxMenu")
                                continue;
                        wxString name = child->GetAttribute("name");
                        if (!name.empty() && XRCID(name.c_str()) == id && index < menubar->GetMenuCount())
                                return menubar->GetMenu(index);
                        index++;
                }
                m = menubar->FindItem(id);
        } else
                m = ((wxMenu *)menu)->FindItem(id);
        if (m)
                return m->GetSubMenu();

        return 0;
}

void *wx_getnativemenu(void *menu) {
#ifdef _WIN32
        return ((wxMenu *)menu)->GetHMenu();
#endif
        return 0;
}

void *wx_getnativewindow(void *window) {
#ifdef _WIN32
        return ((wxWindow *)window)->GetHWND();
#endif
        return 0;
}

#ifdef _WIN32
void wx_winsendmessage(void *window, int msg, INT_PARAM wParam, LONG_PARAM lParam) {
        WinSendMessageEvent *event = new WinSendMessageEvent(wx_getnativewindow(window), msg, wParam, lParam);
        wxQueueEvent((wxWindow *)window, event);
}
#endif

void wx_appendmenu(void *sub_menu, int id, const char *title, enum wxItemKind type) {
        ((wxMenu *)sub_menu)->Append(id, title, wxEmptyString, type);
}

void wx_enabletoolbaritem(void *toolbar, int id, int enable) { ((wxToolBar *)toolbar)->EnableTool(id, enable); }

void *wx_getmenu(void *window) { return ((Frame *)window)->GetMenu(); }

void *wx_getmenubar(void *window) { return ((wxFrame *)window)->GetMenuBar(); }

void *wx_gettoolbar(void *window) { return ((wxFrame *)window)->GetToolBar(); }

void *wx_getdlgitem(void *window, int id) { return ((wxWindow *)window)->FindWindow(id); }

void wx_setdlgitemtext(void *window, int id, char *str) {
        wxWindow *w = ((wxWindow *)window)->FindWindow(id);
        if (w->GetClassInfo()->IsKindOf(CLASSINFO(wxTextCtrl)))
                ((wxTextCtrl *)w)->SetValue(str);
        else
                ((wxStaticText *)w)->SetLabel(str);
}

void wx_enablewindow(void *window, int enabled) { ((wxWindow *)window)->Enable(enabled); }

void wx_showwindow(void *window, int show) {
        wxCommandEvent *event = new wxCommandEvent(WX_SHOW_WINDOW_EVENT, wxID_ANY);
        event->SetEventObject((wxWindow *)window);
        event->SetInt(show);
        wxQueueEvent((wxWindow *)window, event);
}

int wx_iswindowvisible(void *window) { return ((wxWindow *)window)->IsShown(); }

void wx_togglewindow(void *window) { wx_showwindow(window, -1); }

void wx_callback(void *window, WX_CALLBACK callback, void *data) {
        CallbackEvent *event = new CallbackEvent(callback, data);
        wxQueueEvent((wxWindow *)window, event);
}

void wx_enddialog(void *window, int ret_code) { ((wxDialog *)window)->EndModal(ret_code); }

int wx_dlgdirlist(void *window, const char *path, int id, int static_path, int file_type) {
        wxWindow *w = ((wxWindow *)window)->FindWindow(id);
        if (w->GetClassInfo()->IsKindOf(CLASSINFO(wxListBox))) {
                wxListBox *list = (wxListBox *)w;
                list->Clear();
                wxArrayString items;
                wxString f = wxFindFirstFile(path);
                while (!f.empty()) {
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

int wx_dlgdirselectex(void *window, LONG_PARAM path, int count, int id) {
        wxWindow *w = ((wxWindow *)window)->FindWindow(id);
        if (w->GetClassInfo()->IsKindOf(CLASSINFO(wxListBox))) {
                wxListBox *list = (wxListBox *)w;
                strcpy((char *)path, (list->GetStringSelection() + ".").GetData().AsChar());
                return TRUE;
        }
        return FALSE;
}

int wx_sendmessage(void *window, int type, LONG_PARAM param1, LONG_PARAM param2) {
        switch (type) {
        case WX_CB_ADDSTRING:
                ((wxComboBox *)window)->Append((char *)param2);
                break;
        case WX_CB_SETCURSEL:
                if (param1 >= 0 && param1 < (int)((wxComboBox *)window)->GetCount())
                        ((wxComboBox *)window)->Select(param1);
                break;
        case WX_CB_GETCURSEL:
                return ((wxComboBox *)window)->GetCurrentSelection();
                break;
        case WX_CB_GETLBTEXT:
                strcpy((char *)param2, ((wxComboBox *)window)->GetString(param1).mb_str());
                break;
        case WX_CB_RESETCONTENT: {
#ifndef __WXOSX_MAC__
                ((wxComboBox *)window)->SetValue("");
                ((wxComboBox *)window)->Clear();
#else
                /* Clear() does not work on OSX */
                wxComboBox *cb = (wxComboBox *)window;
                int count = cb->GetCount();
                while (count--)
                        cb->Delete(0);
#endif
                break;
        }
        case WX_BM_SETCHECK:
                ((wxCheckBox *)window)->SetValue(param1);
                break;
        case WX_BM_GETCHECK:
                return ((wxCheckBox *)window)->GetValue();
                break;
        case WX_WM_SETTEXT: {
                if (((wxWindow *)window)->GetClassInfo()->IsKindOf(CLASSINFO(wxTextCtrl)))
                        ((wxTextCtrl *)window)->SetValue((char *)param2);
                else
                        ((wxStaticText *)window)->SetLabel((char *)param2);
        } break;
        case WX_WM_GETTEXT:
                if (((wxWindow *)window)->GetClassInfo()->IsKindOf(CLASSINFO(wxTextCtrl)))
                        strcpy((char *)param2, ((wxTextCtrl *)window)->GetValue().mb_str());
                else
                        strcpy((char *)param2, ((wxStaticText *)window)->GetLabel().mb_str());
                break;
        case WX_UDM_SETPOS:
                ((wxSpinCtrl *)window)->SetValue(param2);
                break;
        case WX_UDM_GETPOS:
                return ((wxSpinCtrl *)window)->GetValue();
        case WX_UDM_SETRANGE: {
                int min = (param2 >> 16) & 0xffff;
                int max = param2 & 0xffff;
                ((wxSpinCtrl *)window)->SetRange(min, max);
                break;
        }
        case WX_UDM_SETINCR: {
                /* SetIncrement-method may not exist on all platforms */
#ifdef __WXGTK__
                ((wxSpinCtrl *)window)->SetIncrement(param2);
#endif
                break;
        }
        case WX_LB_GETCOUNT: {
                return ((wxListBox *)window)->GetCount();
        }
        case WX_LB_GETCURSEL: {
                return ((wxListBox *)window)->GetSelection();
        }
        case WX_LB_SETCURSEL: {
                ((wxListBox *)window)->SetSelection(param1);
                break;
        }
        case WX_LB_GETTEXT: {
                strcpy((char *)param2, ((wxListBox *)window)->GetString(param1).mb_str());
                break;
        }
        case WX_LB_INSERTSTRING: {
                ((wxListBox *)window)->Insert((char *)param2, param1);
                break;
        }
        case WX_LB_DELETESTRING: {
                ((wxListBox *)window)->Delete(param1);
                break;
        }
        case WX_LB_RESETCONTENT: {
                ((wxListBox *)window)->Clear();
                break;
        }
        case WX_CHB_SETPAGETEXT: {
                ((wxChoicebook *)window)->SetPageText(param1, (char *)param2);
                break;
        }
        case WX_CHB_ADDPAGE: {
                ((wxChoicebook *)window)->AddPage((wxWindow *)param1, (char *)param2);
                break;
        }
        case WX_CHB_REMOVEPAGE: {
                ((wxChoicebook *)window)->RemovePage(param1);
                break;
        }
        case WX_CHB_GETPAGECOUNT: {
                return ((wxChoicebook *)window)->GetPageCount();
        }
        case WX_REPARENT: {
                ((wxWindow *)window)->Reparent((wxWindow *)param1);
                break;
        }
        case WX_WM_ENABLE: {
                ((wxWindow *)window)->Enable((bool)param2);
                break;
        }
        case WX_WM_SHOW: {
                ((wxWindow *)window)->Show((bool)param2);
                break;
        }
        case WX_WM_LAYOUT: {
                ((wxWindow *)window)->Layout();
                break;
        }
        case WX_SB_SETCURSEL:
                if (param1 >= 0 && param1 < (int)((wxSimplebook *)window)->GetChildren().GetCount())
                        ((wxSimplebook *)window)->SetSelection(param1);
                break;
        }

        ((wxWindow *)window)->Fit();

        return 0;
}

int wx_dialogbox(void *window, const char *name,
                 int (*callback)(void *window, int message, INT_PARAM param1, LONG_PARAM param2)) {
        PCemDialogBox dlg((wxWindow *)window, name, callback);
        dlg.OnInit();
        dlg.Fit();
        int ret = dlg.ShowModal();
        dlg.Destroy();
        return ret;
}

void wx_exit(void *window, int value) {
        wxCommandEvent *event = new wxCommandEvent(WX_EXIT_EVENT, wxID_ANY);
        event->SetEventObject((wxWindow *)window);
        event->SetInt(value);
        wxQueueEvent((wxWindow *)window, event);
}

void wx_stop_emulation(void *window) {
        wxCommandEvent *event = new wxCommandEvent(WX_STOP_EMULATION_EVENT, wxID_ANY);
        event->SetEventObject((wxWindow *)window);
        wxQueueEvent((wxWindow *)window, event);
}

void wx_stop_emulation_now(void *window) {
        wxCommandEvent *event = new wxCommandEvent(WX_STOP_EMULATION_NOW_EVENT, wxID_ANY);
        event->SetEventObject((wxWindow *)window);
        wxQueueEvent((wxWindow *)window, event);
}

int wx_yield() { return wxYield(); }

/* Timer stuff */

class PCemTimer : public wxTimer {
    public:
        PCemTimer(void (*fn)()) { this->fn = fn; }
        virtual ~PCemTimer(){};

        void Notify() { fn(); }

    private:
        void (*fn)();
};

void *wx_createtimer(void (*fn)()) { return new PCemTimer(fn); }

void wx_starttimer(void *timer, int milliseconds, int once) {
        wxTimer *t = (wxTimer *)timer;
        t->Start(milliseconds, once);
}

void wx_stoptimer(void *timer) {
        wxTimer *t = (wxTimer *)timer;
        t->Stop();
}

void wx_destroytimer(void *timer) {
        wx_stoptimer(timer);
        delete (wxTimer *)timer;
}

void wx_popupmenu(void *window, void *menu, int *x, int *y) {
        PopupMenuEvent *event = new PopupMenuEvent((wxWindow *)window, (wxMenu *)menu, x, y);
        wxQueueEvent((wxWindow *)window, event);
}

void *wx_create_status_frame(void *window) {
        StatusFrame *frame = new StatusFrame((wxWindow *)window);
        return frame;
}

void wx_destroy_status_frame(void *window) {
        StatusFrame *frame = (StatusFrame *)window;
        delete frame;
}

#define DISPLAY_CANVAS_NAME "pcem_display_canvas"
#define DISPLAY_DRIVES_NAME "pcem_display_drives"
#define DISPLAY_DRIVE_ICON_SPACING 4
#define DISPLAY_MAX_DRIVES 10

extern "C" drive_info_t *get_machine_info(char *, int *);
extern int show_disc_activity;

class DriveStatusPanel : public wxPanel {
    public:
        DriveStatusPanel(wxWindow *parent)
                : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE, DISPLAY_DRIVES_NAME) {
                count = 0;
                hover = -1;
                memset(drives, 0, sizeof(drives));
                memset(flash, 0, sizeof(flash));
                bitmapFDD[0] = wxXmlResource::Get()->LoadBitmap("BITMAP_FLOPPY");
                bitmapFDD[1] = bitmapFDD[0].ConvertToDisabled();
                bitmapHDD[0] = wxXmlResource::Get()->LoadBitmap("BITMAP_HARDDISK");
                bitmapHDD[1] = bitmapHDD[0].ConvertToDisabled();
                bitmapCDROM[0] = wxXmlResource::Get()->LoadBitmap("BITMAP_CDROM");
                bitmapCDROM[1] = bitmapCDROM[0].ConvertToDisabled();
                SetBackgroundStyle(wxBG_STYLE_PAINT);
                Bind(wxEVT_PAINT, &DriveStatusPanel::OnPaint, this);
                Bind(wxEVT_LEFT_UP, &DriveStatusPanel::OnClick, this);
                Bind(wxEVT_RIGHT_UP, &DriveStatusPanel::OnClick, this);
                Bind(wxEVT_MOTION, &DriveStatusPanel::OnMotion, this);
                Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent &event) { hover = -1; });
        }

        int GetRequiredWidth() {
                return count * (IconSize().x + DISPLAY_DRIVE_ICON_SPACING) + DISPLAY_DRIVE_ICON_SPACING;
        }

        bool UpdateDrives() {
                char text[4096];
                int num = 0;
                drive_info_t *info = get_machine_info(text, &num);
                bool count_changed = num != count;
                bool changed = count_changed;

                if (num > DISPLAY_MAX_DRIVES)
                        num = DISPLAY_MAX_DRIVES;
                for (int i = 0; i < num; i++) {
                        int new_flash = info[i].readflash ? 1 : 0;
                        if (i >= count || drives[i].type != info[i].type || drives[i].enabled != info[i].enabled ||
                            drives[i].drive_letter != info[i].drive_letter || strcmp(drives[i].fn, info[i].fn) ||
                            flash[i] != new_flash)
                                changed = true;
                        drives[i] = info[i];
                        flash[i] = new_flash;
                }
                count = num;
                if (changed) {
                        Refresh();
                        if (hover >= 0 && hover < count)
                                SetToolTip(DriveDescription(hover));
                }
                return count_changed;
        }

    private:
        drive_info_t drives[DISPLAY_MAX_DRIVES];
        int flash[DISPLAY_MAX_DRIVES];
        int count;
        int hover;
        wxBitmap bitmapFDD[2];
        wxBitmap bitmapCDROM[2];
        wxBitmap bitmapHDD[2];

        wxSize IconSize() { return bitmapFDD[0].IsOk() ? bitmapFDD[0].GetSize() : wxSize(16, 16); }

        bool IsZip(int i) { return drives[i].type == DRIVE_TYPE_FDD && drives[i].drive_letter >= 'C'; }

        int HitTest(int x) {
                int step = IconSize().x + DISPLAY_DRIVE_ICON_SPACING;
                if (x < DISPLAY_DRIVE_ICON_SPACING)
                        return -1;
                int i = (x - DISPLAY_DRIVE_ICON_SPACING) / step;
                return (i < count) ? i : -1;
        }

        wxString DriveDescription(int i) {
                wxString s;
                if (IsZip(i))
                        s = wxString::Format("ZIP (%c:)", drives[i].drive_letter);
                else if (drives[i].type == DRIVE_TYPE_CDROM)
                        s = wxString::Format("CD-ROM (%c:)", drives[i].drive_letter);
                else if (drives[i].type == DRIVE_TYPE_HDD)
                        s = wxString::Format("Hard disk (%c:)", drives[i].drive_letter);
                else
                        s = wxString::Format("Floppy drive %c:", drives[i].drive_letter);
                if (strlen(drives[i].fn))
                        s += wxString(" - ") + wxString(drives[i].fn);
                else if (!drives[i].enabled)
                        s += " - empty";
                return s;
        }

        void OnPaint(wxPaintEvent &event) {
                wxPaintDC dc(this);
                wxSize size = GetClientSize();
                wxSize icon = IconSize();
                dc.SetBackground(wxBrush(GetParent()->GetBackgroundColour()));
                dc.Clear();
                dc.SetPen(*wxTRANSPARENT_PEN);
                for (int i = 0; i < count; i++) {
                        int x = DISPLAY_DRIVE_ICON_SPACING + i * (icon.x + DISPLAY_DRIVE_ICON_SPACING);
                        int y = (size.y - icon.y) / 2;
                        int disabled = drives[i].enabled ? 0 : 1;
                        wxBitmap *bitmap;
                        if (drives[i].type == DRIVE_TYPE_CDROM)
                                bitmap = &bitmapCDROM[disabled];
                        else if (drives[i].type == DRIVE_TYPE_HDD)
                                bitmap = &bitmapHDD[disabled];
                        else
                                bitmap = &bitmapFDD[disabled];
                        if (bitmap->IsOk())
                                dc.DrawBitmap(*bitmap, x, y, true);
                        if (show_disc_activity) {
                                dc.SetBrush(wxBrush(flash[i] ? wxColour(0, 255, 0) : wxColour(0, 100, 0)));
                                dc.DrawRectangle(x + icon.x - 5, y + icon.y - 5, 5, 5);
                        }
                }
        }

        void OnMotion(wxMouseEvent &event) {
                int i = HitTest(event.GetX());
                if (i != hover) {
                        hover = i;
                        if (i >= 0)
                                SetToolTip(DriveDescription(i));
                        else
                                UnsetToolTip();
                }
        }

        void AppendCDROMItems(wxMenu *popup) {
                wxFrame *frame = wxDynamicCast(wxGetTopLevelParent(this), wxFrame);
                wxMenu *cdrom = (frame && frame->GetMenuBar()) ? (wxMenu *)wx_getsubmenu(frame->GetMenuBar(), XRCID("IDM_CDROM"))
                                                               : NULL;
                if (!cdrom) {
                        popup->Append(XRCID("IDM_CDROM_IMAGE_LOAD"), "Load image...");
                        popup->Append(XRCID("IDM_CDROM_EMPTY"), "Eject");
                        return;
                }
                wxMenuItemList &items = cdrom->GetMenuItems();
                for (wxMenuItemList::iterator it = items.begin(); it != items.end(); ++it) {
                        wxMenuItem *item = *it;
                        if (item->IsSeparator())
                                popup->AppendSeparator();
                        else if (!item->IsSubMenu()) {
                                wxMenuItem *copy = popup->Append(item->GetId(), item->GetItemLabel(), wxEmptyString, item->GetKind());
                                if (item->IsCheckable())
                                        copy->Check(item->IsChecked());
                                copy->Enable(item->IsEnabled());
                        }
                }
        }

        void OnClick(wxMouseEvent &event) {
                int i = HitTest(event.GetX());
                if (i < 0)
                        return;

                wxMenu popup;
                if (IsZip(i)) {
                        popup.Append(XRCID("IDM_DISC_ZIP"), "Load ZIP disc...");
                        popup.Append(XRCID("IDM_EJECT_ZIP"), "Eject ZIP disc")->Enable(drives[i].enabled != 0);
                } else if (drives[i].type == DRIVE_TYPE_FDD) {
                        int b = drives[i].drive == 1;
                        popup.Append(XRCID(b ? "IDM_DISC_B" : "IDM_DISC_A"), wxString::Format("Change drive %c:...", drives[i].drive_letter));
                        popup.Append(XRCID(b ? "IDM_EJECT_B" : "IDM_EJECT_A"), wxString::Format("Eject drive %c:", drives[i].drive_letter))
                                ->Enable(drives[i].enabled != 0);
                        popup.AppendSeparator();
                        popup.Append(XRCID("IDM_DISC_CREATE"), "Create blank disc image...");
                } else if (drives[i].type == DRIVE_TYPE_CDROM)
                        AppendCDROMItems(&popup);
                else
                        popup.Append(wxID_ANY, DriveDescription(i))->Enable(false);

                PopupMenu(&popup, DISPLAY_DRIVE_ICON_SPACING + i * (IconSize().x + DISPLAY_DRIVE_ICON_SPACING), 0);
        }
};

static void display_layout_status(wxFrame *frame) {
        wxStatusBar *status = frame->GetStatusBar();
        DriveStatusPanel *drives = status ? (DriveStatusPanel *)status->FindWindow(DISPLAY_DRIVES_NAME) : NULL;
        if (!drives)
                return;
        int widths[4] = {-1, drives->GetRequiredWidth(), 90, 90};
        status->SetStatusWidths(4, widths);
        wxRect rect;
        if (status->GetFieldRect(1, rect))
                drives->SetSize(rect);
}


void *wx_display_create(void *parent, void *menubar, const char *title, WX_DISPLAY_CALLBACK callback) {
        wxFrame *frame = new wxFrame((wxWindow *)parent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE);
        if (menubar)
                frame->SetMenuBar((wxMenuBar *)menubar);
        wxStatusBar *status = frame->CreateStatusBar(4);
        int widths[4] = {-1, DISPLAY_DRIVE_ICON_SPACING, 90, 90};
        status->SetStatusWidths(4, widths);
        new DriveStatusPanel(status);
        status->Bind(wxEVT_SIZE, [frame](wxSizeEvent &event) {
                display_layout_status(frame);
                event.Skip();
        });

        wxPanel *canvas = new wxPanel(frame, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS, DISPLAY_CANVAS_NAME);
        canvas->SetBackgroundStyle(wxBG_STYLE_PAINT);
        canvas->SetBackgroundColour(*wxBLACK);
        wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
        sizer->Add(canvas, 1, wxEXPAND);
        frame->SetSizer(sizer);
        frame->SetClientSize(640, 480);

        wxWindow *owner = (wxWindow *)parent;
        frame->Bind(wxEVT_MENU, [owner, callback](wxCommandEvent &event) {
                if (event.GetId() == wxID_EXIT)
                        callback(WX_DISPLAY_CLOSE, 0, 0);
                else
                        owner->GetEventHandler()->ProcessEvent(event);
        });
        frame->Bind(wxEVT_CLOSE_WINDOW, [callback](wxCloseEvent &event) {
                if (event.CanVeto()) {
                        event.Veto();
                        callback(WX_DISPLAY_CLOSE, 0, 0);
                } else
                        event.Skip();
        });
        frame->Bind(wxEVT_ACTIVATE, [callback](wxActivateEvent &event) {
                callback(event.GetActive() ? WX_DISPLAY_ACTIVATE : WX_DISPLAY_DEACTIVATE, 0, 0);
                event.Skip();
        });
        frame->Bind(wxEVT_DESTROY, [frame, callback](wxWindowDestroyEvent &event) {
                if (event.GetEventObject() == frame)
                        callback(WX_DISPLAY_DESTROY, 0, 0);
                event.Skip();
        });
        canvas->Bind(wxEVT_SIZE, [canvas, callback](wxSizeEvent &event) {
                wxSize size = canvas->GetClientSize();
                callback(WX_DISPLAY_SIZE, size.x, size.y);
                event.Skip();
        });
        canvas->Bind(wxEVT_PAINT, [canvas](wxPaintEvent &event) { wxPaintDC dc(canvas); });
        canvas->Bind(wxEVT_LEFT_UP, [callback](wxMouseEvent &event) { callback(WX_DISPLAY_MOUSE_UP, 1, 0); });
        canvas->Bind(wxEVT_MIDDLE_UP, [callback](wxMouseEvent &event) { callback(WX_DISPLAY_MOUSE_UP, 2, 0); });
        canvas->Bind(wxEVT_RIGHT_UP, [callback](wxMouseEvent &event) { callback(WX_DISPLAY_MOUSE_UP, 3, 0); });
        canvas->Bind(wxEVT_KEY_DOWN, [](wxKeyEvent &event) {});
        canvas->Bind(wxEVT_KEY_UP, [](wxKeyEvent &event) {});
        canvas->Bind(wxEVT_CHAR, [](wxKeyEvent &event) {});

        return frame;
}

void *wx_display_getnativecanvas(void *frame) {
        wxWindow *canvas = ((wxWindow *)frame)->FindWindow(DISPLAY_CANVAS_NAME);
        if (!canvas)
                return NULL;
#ifdef __WXGTK__
        return canvas->GTKGetDrawingWindow();
#else
        return (void *)canvas->GetHandle();
#endif
}

void wx_display_getcanvassize(void *frame, int *width, int *height) {
        wxWindow *canvas = ((wxWindow *)frame)->FindWindow(DISPLAY_CANVAS_NAME);
        wxSize size = canvas ? canvas->GetClientSize() : wxSize(0, 0);
        *width = size.x;
        *height = size.y;
}

void wx_display_setsize(void *frame, int width, int height, int resizable) {
        wxFrame *f = (wxFrame *)frame;
        if (f->IsFullScreen())
                return;
        f->SetMinSize(wxDefaultSize);
        f->SetMaxSize(wxDefaultSize);
        f->SetClientSize(width, height);
        f->Layout();
        if (!resizable) {
                wxSize size = f->GetSize();
                f->SetMinSize(size);
                f->SetMaxSize(size);
        }
}

void wx_display_getrect(void *frame, int *x, int *y, int *width, int *height) {
        wxFrame *f = (wxFrame *)frame;
        wxPoint pos = f->GetPosition();
        wxSize size = f->GetClientSize();
        *x = pos.x;
        *y = pos.y;
        *width = size.x;
        *height = size.y;
}

void wx_display_center(void *frame) { ((wxFrame *)frame)->CenterOnScreen(); }

void wx_display_show(void *frame, int show) {
        wxFrame *f = (wxFrame *)frame;
        if (!show && f->IsFullScreen())
                f->ShowFullScreen(false);
        f->Show(show != 0);
        if (show) {
                f->Raise();
                wxWindow *canvas = f->FindWindow(DISPLAY_CANVAS_NAME);
                if (canvas)
                        canvas->SetFocus();
        }
}

void wx_display_setfullscreen(void *frame, int fullscreen) {
        wxFrame *f = (wxFrame *)frame;
        if (fullscreen) {
                f->SetMinSize(wxDefaultSize);
                f->SetMaxSize(wxDefaultSize);
        }
        f->ShowFullScreen(fullscreen != 0, wxFULLSCREEN_ALL);
        if (fullscreen)
                f->Raise();
}

void wx_display_update(void *frame) {
        wxFrame *f = (wxFrame *)frame;
        wxStatusBar *status = f->GetStatusBar();
        if (!status)
                return;
        DriveStatusPanel *drives = (DriveStatusPanel *)status->FindWindow(DISPLAY_DRIVES_NAME);
        if (drives && drives->UpdateDrives())
                display_layout_status(f);
#ifdef __WXOSX__
        status->Update();
#endif
}

void wx_display_setstatustext(void *frame, int field, const char *text) {
        wxStatusBar *status = ((wxFrame *)frame)->GetStatusBar();
        if (!status || field >= status->GetFieldsCount())
                return;
        wxString s(text);
        if (status->GetStatusText(field) != s)
                status->SetStatusText(s, field);
}

void wx_setwindowposition(void *window, int x, int y) { ((wxFrame *)window)->SetPosition(wxPoint(x, y)); }

void wx_setwindowsize(void *window, int width, int height) { ((wxFrame *)window)->SetSize(wxSize(width, height)); }

void wx_show_status(void *window) {
        wxWindow *status = ((wxWindow *)window)->FindWindowById(STATUS_WINDOW_ID);
        if (!status) {
                StatusFrame *statusFrame = new StatusFrame((wxWindow *)window);
                statusFrame->Show();
        }
}

void wx_close_status(void *window) {
        wxWindow *status = ((wxWindow *)window)->FindWindowById(STATUS_WINDOW_ID);
        if (status) {
                status->Close();
        }
}

int wx_setup(char *path) {
        wxFileName p(path);
        if (!p.DirExists()) {
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

int wx_file_exists(char *path) {
        wxFileName p(path);
        return p.Exists();
}

int wx_copy_file(char *from, char *to, int overwrite) { return wxCopyFile(from, to, overwrite); }

int wx_image_save(const char *path, const char *name, const char *format, unsigned char *rgba, int width, int height, int alpha) {
        int x, y;
        wxLogNull logNull;
        wxImage image(width, height);
        if (alpha) {
                unsigned char *rgb = (unsigned char *)malloc(width * height * 3);
                unsigned char *a = (unsigned char *)malloc(width * height);
                for (x = 0; x < width; ++x) {
                        for (y = 0; y < height; ++y) {
                                rgb[(y * width + x) * 3 + 0] = rgba[(y * width + x) * 4 + 0];
                                rgb[(y * width + x) * 3 + 1] = rgba[(y * width + x) * 4 + 1];
                                rgb[(y * width + x) * 3 + 2] = rgba[(y * width + x) * 4 + 2];
                                a[y * width + x] = rgba[(y * width + x) * 4 + 3];
                        }
                }
                image.SetData(rgb);
                image.SetAlpha(a);
        } else
                image.SetData(rgba, true);

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

        int res = 0;
        if (h) {
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

void *wx_image_load(const char *path) {
        wxLogNull logNull; /* removes sRGB-warnings on Windows */
        wxImage *image = new wxImage(path);
        if (image->IsOk())
                return image;
        delete image;
        return 0;
}

void wx_image_free(void *image) { delete (wxImage *)image; }

void wx_image_rescale(void *image, int width, int height) { ((wxImage *)image)->Rescale(width, height); }

void wx_image_get_size(void *image, int *width, int *height) {
        *width = ((wxImage *)image)->GetWidth();
        *height = ((wxImage *)image)->GetHeight();
}

unsigned char *wx_image_get_data(void *image) { return ((wxImage *)image)->GetData(); }

unsigned char *wx_image_get_alpha(void *image) { return ((wxImage *)image)->GetAlpha(); }

/* wxFileConfig */

void *wx_config_load(const char *path) {
        if (!wxFileExists(path))
                return 0;
        wxFileInputStream stream(path);
        if (stream.IsOk())
                return new wxFileConfig(stream);
        return 0;
}
int wx_config_get_string(void *config, const char *name, char *dst, int size, const char *defVal) {
        wxFileConfig *c = ((wxFileConfig *)config);
        wxString val;
        bool res = c->Read(name, &val, wxString(defVal));
        strncpy(dst, val.GetData().AsChar(), size - 1);
        dst[size - 1] = 0;
        return res;
}
int wx_config_get_int(void *config, const char *name, int *dst, int defVal) {
        wxFileConfig *c = ((wxFileConfig *)config);
        return c->Read(name, dst, defVal);
}

int wx_config_get_float(void *config, const char *name, float *dst, float defVal) {
        wxFileConfig *c = ((wxFileConfig *)config);
        return c->Read(name, dst, defVal);
}

int wx_config_get_bool(void *config, const char *name, int *dst, int defVal) {
        wxFileConfig *c = ((wxFileConfig *)config);
        wxString val;
        bool res = 1;
        if (c->Read(name, &val)) {
                val.LowerCase();
                if (val.StartsWith("true"))
                        *dst = 1;
                else if (val.StartsWith("false"))
                        *dst = 0;
                else
                        res = 0;
        }
        if (!res)
                res = c->Read(name, (bool *)dst, (bool)defVal);

        return res;
}

int wx_config_has_entry(void *config, const char *name) {
        wxFileConfig *c = ((wxFileConfig *)config);
        return c->HasEntry(name);
}

void wx_config_free(void *config) { delete (wxFileConfig *)config; }

typedef struct progress_data_t {
        WX_CALLBACK callback;
        void *data;
        int active;
        int result;
} progress_data_t;

static void progress_callback(void *data) {
        progress_data_t *d = (progress_data_t *)data;
        d->result = d->callback(d->data);
        d->active = 0;
}

int wx_progressdialog(void *window, const char *title, const char *message, WX_CALLBACK callback, void *data, int range,
                      volatile int *pos) {
        struct progress_data_t pdata;
        pdata.callback = callback;
        pdata.data = data;
        pdata.active = 1;
        pdata.result = 0;

        thread_t *t = thread_create(progress_callback, &pdata);

        wxProgressDialog dlg(title, message, range, (wxWindow *)window, wxPD_APP_MODAL | wxPD_SMOOTH | wxPD_AUTO_HIDE);
        while (pdata.active) {
                dlg.Update(*pos);
                wxMilliSleep(50);
        }
        thread_kill(t);

        return pdata.result;
}

void wx_date_format(char *s, const char *format) {
        wxString res = wxDateTime::Now().Format(format);
        strcpy(s, res.mb_str());
}
