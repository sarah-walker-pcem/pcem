#include "wx-display.h"
#include "wx-utils.h"
#include "wx-app.h"
#include "wx-common.h"

#ifdef __WXOSX__
#include <math.h>
#include <objc/message.h>
#include <objc/runtime.h>
#endif

#ifdef __WXGTK__
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#endif

#define DISPLAY_DRIVES_NAME "pcem_display_drives"
#define DISPLAY_DRIVE_ICON_SPACING 4
#define DISPLAY_MAX_DRIVES 10

extern "C" drive_info_t *get_machine_info(char *, int *);

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

        int GetRequiredWidth() { return count * (IconSize().x + DISPLAY_DRIVE_ICON_SPACING) + DISPLAY_DRIVE_ICON_SPACING; }

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
                wxMenu *cdrom = (frame && frame->GetMenuBar()) ? wx_getsubmenu(frame->GetMenuBar(), XRCID("IDM_CDROM")) : NULL;
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
                                wxMenuItem *copy =
                                        popup->Append(item->GetId(), item->GetItemLabel(), wxEmptyString, item->GetKind());
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
                        char letter = drives[i].drive_letter;
                        popup.Append(XRCID(b ? "IDM_DISC_B" : "IDM_DISC_A"), wxString::Format("Change drive %c:...", letter));
                        popup.Append(XRCID(b ? "IDM_EJECT_B" : "IDM_EJECT_A"), wxString::Format("Eject drive %c:", letter))
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

DisplayFrame::DisplayFrame(wxWindow *owner, wxMenuBar *menubar, const char *title, SDL_DISPLAY_CALLBACK callback)
        : wxFrame(owner, wxID_ANY, title, wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE), callback(callback) {
        wx_set_app_icon(this);
        if (menubar)
                SetMenuBar(menubar);

        wxStatusBar *status = CreateStatusBar(4);
        int widths[4] = {-1, DISPLAY_DRIVE_ICON_SPACING, 90, 90};
        status->SetStatusWidths(4, widths);
        drives = new DriveStatusPanel(status);
        status->Bind(wxEVT_SIZE, [this](wxSizeEvent &event) {
                LayoutStatus();
                event.Skip();
        });

        canvas = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS);
        canvas->SetBackgroundStyle(wxBG_STYLE_PAINT);
        canvas->SetBackgroundColour(*wxBLACK);
        wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
        sizer->Add(canvas, 1, wxEXPAND);
        SetSizer(sizer);
        SetClientSize(640, 480);

        Bind(wxEVT_MENU, [this, owner](wxCommandEvent &event) {
                if (event.GetId() == wxID_EXIT)
                        this->callback(SDL_DISPLAY_CLOSE, 0, 0);
                else
                        owner->GetEventHandler()->ProcessEvent(event);
        });
        Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent &event) {
                if (event.CanVeto()) {
                        event.Veto();
                        this->callback(SDL_DISPLAY_CLOSE, 0, 0);
                } else
                        event.Skip();
        });
        Bind(wxEVT_ACTIVATE, [this](wxActivateEvent &event) {
                this->callback(event.GetActive() ? SDL_DISPLAY_ACTIVATE : SDL_DISPLAY_DEACTIVATE, 0, 0);
                event.Skip();
        });
        Bind(wxEVT_DESTROY, [this](wxWindowDestroyEvent &event) {
                if (event.GetEventObject() == this)
                        this->callback(SDL_DISPLAY_DESTROY, 0, 0);
                event.Skip();
        });
        canvas->Bind(wxEVT_SIZE, [this](wxSizeEvent &event) {
                wxSize size = canvas->GetClientSize();
                this->callback(SDL_DISPLAY_SIZE, size.x, size.y);
                event.Skip();
        });
        canvas->Bind(wxEVT_PAINT, [this](wxPaintEvent &event) { wxPaintDC dc(canvas); });
        canvas->Bind(wxEVT_LEFT_UP, [this](wxMouseEvent &event) { this->callback(SDL_DISPLAY_MOUSE_UP, 1, 0); });
        canvas->Bind(wxEVT_MIDDLE_UP, [this](wxMouseEvent &event) { this->callback(SDL_DISPLAY_MOUSE_UP, 2, 0); });
        canvas->Bind(wxEVT_RIGHT_UP, [this](wxMouseEvent &event) { this->callback(SDL_DISPLAY_MOUSE_UP, 3, 0); });
#ifdef __WXGTK__
        canvas->Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent &event) { this->callback(SDL_DISPLAY_KEY, event.GetRawKeyFlags(), 1); });
        canvas->Bind(wxEVT_KEY_UP, [this](wxKeyEvent &event) { this->callback(SDL_DISPLAY_KEY, event.GetRawKeyFlags(), 0); });
#else
        canvas->Bind(wxEVT_KEY_DOWN, [](wxKeyEvent &event) {});
        canvas->Bind(wxEVT_KEY_UP, [](wxKeyEvent &event) {});
#endif
        canvas->Bind(wxEVT_CHAR, [](wxKeyEvent &event) {});
}

void DisplayFrame::LayoutStatus() {
        wxStatusBar *status = GetStatusBar();
        if (!status)
                return;
        int widths[4] = {-1, drives->GetRequiredWidth(), 90, 90};
        status->SetStatusWidths(4, widths);
        wxRect rect;
        if (status->GetFieldRect(1, rect))
                drives->SetSize(rect);
}

void *DisplayFrame::NativeCanvas() {
#ifdef __WXGTK__
        GdkWindow *gdk_window = canvas->GTKGetDrawingWindow();
        if (!gdk_window || !GDK_IS_X11_WINDOW(gdk_window))
                return NULL;
        return (void *)(uintptr_t)gdk_x11_window_get_xid(gdk_window);
#else
        return (void *)canvas->GetHandle();
#endif
}

void DisplayFrame::SetDisplaySize(int width, int height, bool resizable) {
        if (IsFullScreen())
                return;
        SetMinSize(wxDefaultSize);
        SetMaxSize(wxDefaultSize);
        SetClientSize(width, height);
        Layout();
        if (!resizable) {
                wxSize size = GetSize();
                SetMinSize(size);
                SetMaxSize(size);
        }
}

void DisplayFrame::ShowDisplay(bool show) {
        if (!show && IsFullScreen())
                ShowFullScreen(false);
        Show(show);
        if (show) {
                Raise();
                canvas->SetFocus();
        }
}

int DisplayFrame::TopSafeInset() {
#ifdef __WXOSX__
        typedef struct {
                double top, left, bottom, right;
        } edge_insets_t;
        void *nswindow = (void *)GetWXWindow();
        if (!nswindow)
                return 0;
        void *screen = ((void *(*)(void *, SEL))objc_msgSend)(nswindow, sel_registerName("screen"));
        SEL sel = sel_registerName("safeAreaInsets");
        if (!screen || !((BOOL(*)(void *, SEL, SEL))objc_msgSend)(screen, sel_registerName("respondsToSelector:"), sel))
                return 0;
        edge_insets_t insets;
#if defined(__x86_64__)
        ((void (*)(edge_insets_t *, void *, SEL))objc_msgSend_stret)(&insets, screen, sel);
#else
        insets = ((edge_insets_t(*)(void *, SEL))objc_msgSend)(screen, sel);
#endif
        return insets.top > 0 ? (int)ceil(insets.top) : 0;
#else
        return 0;
#endif
}

void DisplayFrame::SetDisplayFullscreen(bool fullscreen) {
        if (fullscreen) {
                SetMinSize(wxDefaultSize);
                SetMaxSize(wxDefaultSize);
#ifdef __WXGTK__
                GdkWindow *gdk_window = gtk_widget_get_window((GtkWidget *)GetHandle());
                if (gdk_window) {
                        GdkGeometry geometry = {};
                        gdk_window_set_geometry_hints(gdk_window, &geometry, (GdkWindowHints)0);
                        gdk_display_flush(gdk_window_get_display(gdk_window));
                }
#endif
        }
        ShowFullScreen(fullscreen, wxFULLSCREEN_ALL);

        wxStatusBar *status = GetStatusBar();
        if (status && status->IsShown() != !fullscreen)
                status->Show(!fullscreen);
#ifdef __WXOSX__
        wxSizerItem *item = GetSizer() ? GetSizer()->GetItem(canvas) : NULL;
        if (item) {
                item->SetFlag(wxEXPAND | wxTOP);
                item->SetBorder(fullscreen ? TopSafeInset() : 0);
        }
        SetBackgroundColour(fullscreen ? *wxBLACK : wxNullColour);
        Refresh();
#endif
        SendSizeEvent();
        if (fullscreen)
                Raise();
}

void DisplayFrame::SetStatusField(int field, const wxString &text) {
        wxStatusBar *status = GetStatusBar();
        if (!status || field >= status->GetFieldsCount())
                return;
        if (status->GetStatusText(field) != text)
                status->SetStatusText(text, field);
}

void DisplayFrame::SetStatus(const char *message, int speed, int fps) {
        SetStatusField(0, message);
        SetStatusField(2, wxString::Format("Speed: %d%%", speed));
        SetStatusField(3, wxString::Format("FPS: %d", fps));

        wxStatusBar *status = GetStatusBar();
        if (!status)
                return;
        if (drives->UpdateDrives())
                LayoutStatus();
#ifdef __WXOSX__
        status->Update();
#endif
}

void *DisplayFrame::HostNativeCanvas(void *frame) { return ((DisplayFrame *)frame)->NativeCanvas(); }

void DisplayFrame::HostCanvasSize(void *frame, int *width, int *height) {
        wxSize size = ((DisplayFrame *)frame)->canvas->GetClientSize();
        *width = size.x;
        *height = size.y;
}

void DisplayFrame::HostSetSize(void *frame, int width, int height, int resizable) {
        ((DisplayFrame *)frame)->SetDisplaySize(width, height, resizable != 0);
}

void DisplayFrame::HostGetRect(void *frame, int *x, int *y, int *width, int *height) {
        DisplayFrame *f = (DisplayFrame *)frame;
        wxPoint pos = f->GetPosition();
        wxSize size = f->GetClientSize();
        *x = pos.x;
        *y = pos.y;
        *width = size.x;
        *height = size.y;
}

void DisplayFrame::HostSetPosition(void *frame, int x, int y) { ((DisplayFrame *)frame)->SetPosition(wxPoint(x, y)); }

void DisplayFrame::HostCenter(void *frame) { ((DisplayFrame *)frame)->CenterOnScreen(); }

void DisplayFrame::HostShow(void *frame, int show) { ((DisplayFrame *)frame)->ShowDisplay(show != 0); }

void DisplayFrame::HostSetFullscreen(void *frame, int fullscreen) {
        ((DisplayFrame *)frame)->SetDisplayFullscreen(fullscreen != 0);
}

void DisplayFrame::HostSetTitle(void *frame, const char *title) { ((DisplayFrame *)frame)->SetTitle(title); }

void DisplayFrame::HostSetStatus(void *frame, const char *message, int speed, int fps) {
        ((DisplayFrame *)frame)->SetStatus(message, speed, fps);
}
