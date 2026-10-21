#include "wx-status.h"
#include "wx-common.h"
#include "wx-utils.h"
#include "wx-display.h"
#include <wx/artprov.h>
#include <wx/xrc/xmlres.h>
#include <sstream>

extern "C" {
int get_status(char *, char *);
extern int fps;
extern int updatestatus;
drive_info_t *get_machine_info(char *, int *);
}

int show_machine_on_start = 0;
int confirm_on_stop_emulation = 1;
int confirm_on_reset_machine = 1;
int show_status = 0;
int show_speed_history = 0;
int show_disc_activity = 1;
int show_machine_info = 1;
int show_mount_paths = 0;

#define ACTIVITY_TEXT wxT("Activity")

#define MAX(a, b) (a > b ? a : b)

#define render_disc_activity(x, y)                                                                                               \
        do {                                                                                                                     \
                if (show_disc_activity) {                                                                                        \
                        int w = 8;                                                                                               \
                        int h = 8;                                                                                               \
                        if (info.readflash) {                                                                                    \
                                dc.SetBrush(wxBrush(wxColour(0, 255, 0)));                                                       \
                        } else {                                                                                                 \
                                dc.SetBrush(wxBrush(wxColour(0, 100, 0)));                                                       \
                        }                                                                                                        \
                        dc.DrawRectangle(x + bitmap.GetWidth() - w, y, w, h);                                                    \
                }                                                                                                                \
        } while (0)

StatusTimer::StatusTimer(StatusPane *pane) { this->pane = pane; }

void StatusTimer::Notify() { pane->Refresh(); }

void StatusTimer::Start() { wxTimer::Start(10); }

BEGIN_EVENT_TABLE(StatusPane, wxPanel)
EVT_PAINT(StatusPane::PaintEvent) END_EVENT_TABLE()

        BEGIN_EVENT_TABLE(StatusFrame, wxFrame) END_EVENT_TABLE()

                StatusPane::StatusPane(wxFrame *parent)
        : wxPanel(parent) {
#ifndef __WXOSX_MAC__
        SetDoubleBuffered(true);
#endif
        machineInfoText[0] = statusMachineText[0] = statusDeviceText[0] = 0;
        lastSpeedUpdate = 0;
        memset(speedHistory, -1, sizeof(speedHistory));

        bitmapFDD[0] = wxXmlResource::Get()->LoadBitmap("BITMAP_FLOPPY");
        bitmapFDD[1] = bitmapFDD[0].ConvertToDisabled();
        bitmapHDD[0] = wxXmlResource::Get()->LoadBitmap("BITMAP_HARDDISK");
        bitmapHDD[1] = bitmapHDD[0].ConvertToDisabled();
        bitmapCDROM[0] = wxXmlResource::Get()->LoadBitmap("BITMAP_CDROM");
        bitmapCDROM[1] = bitmapCDROM[0].ConvertToDisabled();
}

StatusPane::~StatusPane() {}

void StatusPane::PaintEvent(wxPaintEvent &evt) {
        wxPaintDC dc(this);
        Render(dc);
}

void StatusPane::PaintNow() {
        wxClientDC dc(this);
        Render(dc);
}

void StatusPane::Render(wxDC &dc) {
        wxSize cSize = GetParent()->GetClientSize();

        int width = 0;
        int height = 0;
        wxLongLong millis = wxGetUTCTimeMillis();

        dc.SetBackground(wxSystemSettings::GetColour(wxSYS_COLOUR_FRAMEBK));
        dc.SetFont(wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT));
        dc.Clear();

        // draw model/cpu etc
        if (show_machine_info) {
                int num_info;
                drive_info_t *drive_info = get_machine_info(machineInfoText, &num_info);
                wxSize size = dc.GetMultiLineTextExtent(machineInfoText);
                dc.DrawText(machineInfoText, 5, height + 5);

                width = MAX(width, width + 5 + size.x);
                height = MAX(height, height + 5 + size.y);

                int x = cSize.x - 5;

                for (int i = 0; i < num_info; ++i) {
                        drive_info_t info = show_mount_paths ? drive_info[i] : drive_info[num_info - i - 1];
                        wxBitmap bitmap;
                        if (info.type == DRIVE_TYPE_FDD)
                                bitmap = info.enabled ? bitmapFDD[0] : bitmapFDD[1];
                        else if (info.type == DRIVE_TYPE_CDROM)
                                bitmap = info.enabled ? bitmapCDROM[0] : bitmapCDROM[1];
                        else
                                bitmap = info.enabled ? bitmapHDD[0] : bitmapHDD[1];
                        if (show_mount_paths) {
                                x = 5;
                                dc.DrawBitmap(bitmap, x, height + 5);
                                render_disc_activity(x, height + 5);
                                x += bitmap.GetWidth() + 5;

                                std::ostringstream ss;
                                ss << info.drive_letter << ":"
                                   << " " << info.fn;
                                std::string s = ss.str();
                                size = dc.GetTextExtent(s);
                                dc.DrawText(s, x, height + 5 + (bitmap.GetHeight() - size.y) / 2);
                                width = MAX(width, x + size.x);
                                height = height + 5 + MAX(bitmap.GetHeight(), size.y);
                        } else {
                                x -= bitmap.GetWidth();
                                dc.DrawBitmap(bitmap, x, 5);
                                render_disc_activity(x, 5);
                                x -= 5;
                                height = MAX(height, 5 + bitmap.GetHeight());
                        }
                }
        }

        if (emulation_state != EMULATION_STOPPED) {
                // draw status text
                if (show_status) {
                        char charMac = statusMachineText[0];
                        char charDev = statusDeviceText[0];
                        if (updatestatus) {
                                updatestatus = 0;
                                get_status(statusMachineText, statusDeviceText);
                        }
                        if (charMac != 0 || statusMachineText[0] != charMac) {
                                int statusX = 5;
                                int statusY = height + 5;
                                wxSize size = dc.GetMultiLineTextExtent(statusMachineText);
                                dc.DrawText(statusMachineText, statusX, statusY);
                                width = MAX(width, statusX + size.GetWidth());
                                height = MAX(height, statusY + size.GetHeight());
                                if (charDev != 0 || statusDeviceText[0] != charDev) {
                                        wxSize dSize = dc.GetMultiLineTextExtent(statusDeviceText);
                                        dc.DrawText(statusDeviceText, statusX + ceil((size.GetWidth() + 50) / 100.0) * 100,
                                                    statusY);
                                        width = MAX(width, statusX + ceil((size.GetWidth() + 50) / 100.0) * 100 +
                                                                   ceil((dSize.GetWidth() + 50) / 100.0) * 100);
                                        height = MAX(height, statusY + dSize.GetHeight());
                                }
                        }
                }

                if (emulation_state == EMULATION_RUNNING && (millis - lastSpeedUpdate > 500)) {
                        lastSpeedUpdate = millis;
                        memmove(speedHistory + 1, speedHistory, SPEED_HISTORY_LENGTH - 2);
                        speedHistory[0] = fps;
                }
                // draw speed history
                if (show_speed_history) {
                        int speedGraphBorder = 5;
                        int speedGraphX = 5;
                        int speedGraphY = height + 5;
                        int speedGraphWidth = MAX(2 * SPEED_HISTORY_LENGTH, width - 5);
                        double lineLength = (double)(speedGraphWidth - 2) / (SPEED_HISTORY_LENGTH - 1);
                        int speedGraphHeight = 125;
                        dc.SetBrush(wxBrush(wxColour(255, 255, 255)));
                        dc.DrawRectangle(speedGraphX, speedGraphY, speedGraphWidth, speedGraphHeight + speedGraphBorder * 2);
                        dc.SetPen(wxPen(wxColour(200, 200, 200)));
                        dc.DrawLine(speedGraphX, speedGraphY + speedGraphBorder + speedGraphHeight - 0,
                                    speedGraphX + speedGraphWidth, speedGraphY + speedGraphHeight + speedGraphBorder - 0); // 0
                        dc.DrawLine(speedGraphX, speedGraphY + speedGraphBorder + speedGraphHeight - 50,
                                    speedGraphX + speedGraphWidth, speedGraphY + speedGraphHeight + speedGraphBorder - 50); // 50
                        dc.DrawLine(speedGraphX, speedGraphY + speedGraphBorder + speedGraphHeight - 100,
                                    speedGraphX + speedGraphWidth,
                                    speedGraphY + speedGraphHeight + speedGraphBorder - 100); // 100
                        dc.DrawLine(speedGraphX, speedGraphY + speedGraphBorder + speedGraphHeight - 125,
                                    speedGraphX + speedGraphWidth,
                                    speedGraphY + speedGraphHeight + speedGraphBorder - 125); // 125
                        dc.SetPen(wxPen(wxColour(0, 0, 0)));
                        for (int i = 0; i < SPEED_HISTORY_LENGTH - 1; ++i) {
                                int v0 = speedHistory[i];
                                int v1 = speedHistory[i + 1];
                                if (v0 >= 0 && v1 >= 0) {
                                        dc.DrawLine(speedGraphX + speedGraphWidth - i * lineLength - 1,
                                                    speedGraphY + speedGraphHeight + speedGraphBorder - v0,
                                                    speedGraphX + speedGraphWidth - (i + 1) * lineLength - 1,
                                                    speedGraphY + speedGraphHeight + speedGraphBorder - v1);
                                }
                        }
                        dc.SetBrush(wxBrush(wxColour(255, 255, 255), wxBrushStyle(wxBRUSHSTYLE_TRANSPARENT)));
                        dc.DrawRectangle(speedGraphX, speedGraphY, speedGraphWidth, speedGraphHeight + speedGraphBorder * 2);

                        width = MAX(width, (speedGraphX + speedGraphWidth));
                        height = MAX(height, speedGraphY + speedGraphHeight + speedGraphBorder * 2);
                }
        }

        width += 5;
        height += 5;
        width = width < DEFAULT_WINDOW_WIDTH ? DEFAULT_WINDOW_WIDTH : width;
        height = height < DEFAULT_WINDOW_HEIGHT ? DEFAULT_WINDOW_HEIGHT : height;
        if (cSize.GetWidth() != width || cSize.GetHeight() != height) {
                GetParent()->SetClientSize(width, height);
        }
}

#define DRIVE_ICON_SIZE 16
#define DRIVE_ICON_SPACING 4
#define DRIVE_LED_SIZE 5

static bool is_zip_drive(const drive_info_t &info) { return info.type == DRIVE_TYPE_FDD && info.drive_letter >= 'C'; }

DriveStatusPanel::DriveStatusPanel(wxWindow *parent) : wxPanel(parent, wxID_ANY) {
#ifndef __WXOSX_MAC__
        SetDoubleBuffered(true);
#endif
        numDrives = 0;
        tooltipDrive = -1;
        memset(drives, 0, sizeof(drives));

        bitmapFDD[0] = wxXmlResource::Get()->LoadBitmap("BITMAP_FLOPPY");
        bitmapFDD[1] = bitmapFDD[0].ConvertToDisabled();
        bitmapHDD[0] = wxXmlResource::Get()->LoadBitmap("BITMAP_HARDDISK");
        bitmapHDD[1] = bitmapHDD[0].ConvertToDisabled();
        bitmapCDROM[0] = wxXmlResource::Get()->LoadBitmap("BITMAP_CDROM");
        bitmapCDROM[1] = bitmapCDROM[0].ConvertToDisabled();

        Bind(wxEVT_PAINT, &DriveStatusPanel::OnPaint, this);
        Bind(wxEVT_MOTION, &DriveStatusPanel::OnMouseMove, this);
        Bind(wxEVT_LEFT_UP, &DriveStatusPanel::OnClick, this);
        Bind(wxEVT_RIGHT_UP, &DriveStatusPanel::OnClick, this);
}

bool DriveStatusPanel::UpdateDrives(drive_info_t *info, int num) {
        if (num > MAX_STATUS_DRIVES)
                num = MAX_STATUS_DRIVES;

        bool countChanged = num != numDrives;
        if (!countChanged && (!num || !memcmp(drives, info, num * sizeof(drive_info_t))))
                return false;

        if (num)
                memcpy(drives, info, num * sizeof(drive_info_t));
        numDrives = num;
        tooltipDrive = -1;
        Refresh();

        return countChanged;
}

int DriveStatusPanel::GetPanelWidth() { return numDrives * (DRIVE_ICON_SIZE + DRIVE_ICON_SPACING) + DRIVE_ICON_SPACING; }

wxBitmap DriveStatusPanel::GetBitmap(const drive_info_t &info) {
        if (info.type == DRIVE_TYPE_FDD)
                return info.enabled ? bitmapFDD[0] : bitmapFDD[1];
        if (info.type == DRIVE_TYPE_CDROM)
                return info.enabled ? bitmapCDROM[0] : bitmapCDROM[1];
        return bitmapHDD[0];
}

int DriveStatusPanel::HitTest(int x) {
        int i = (x - DRIVE_ICON_SPACING / 2) / (DRIVE_ICON_SIZE + DRIVE_ICON_SPACING);
        return (x >= 0 && i < numDrives) ? i : -1;
}

void DriveStatusPanel::OnPaint(wxPaintEvent &event) {
        wxPaintDC dc(this);
        int y = (GetClientSize().GetHeight() - DRIVE_ICON_SIZE) / 2;

        for (int i = 0; i < numDrives; ++i) {
                int x = DRIVE_ICON_SPACING + i * (DRIVE_ICON_SIZE + DRIVE_ICON_SPACING);
                dc.DrawBitmap(GetBitmap(drives[i]), x, y, true);

                if (show_disc_activity) {
                        dc.SetPen(wxPen(wxColour(0, 0, 0)));
                        dc.SetBrush(wxBrush(drives[i].readflash ? wxColour(0, 255, 0) : wxColour(0, 100, 0)));
                        dc.DrawRectangle(x + DRIVE_ICON_SIZE - DRIVE_LED_SIZE, y + DRIVE_ICON_SIZE - DRIVE_LED_SIZE,
                                         DRIVE_LED_SIZE, DRIVE_LED_SIZE);
                }
        }
}

void DriveStatusPanel::OnMouseMove(wxMouseEvent &event) {
        int i = HitTest(event.GetX());
        if (i == tooltipDrive)
                return;
        tooltipDrive = i;
        if (i < 0) {
                UnsetToolTip();
                return;
        }

        const drive_info_t &info = drives[i];
        wxString name;
        if (info.type == DRIVE_TYPE_CDROM)
                name = "CD-ROM";
        else if (info.type == DRIVE_TYPE_HDD)
                name = "Hard disk";
        else if (is_zip_drive(info))
                name = "ZIP drive";
        else
                name = "Floppy drive";

        wxString fn = info.fn;
        if (fn.IsEmpty())
                fn = info.enabled ? "(disk inserted)" : "(empty)";
        SetToolTip(wxString::Format("%s %c: %s", name, info.drive_letter, fn));
}

void DriveStatusPanel::OnClick(wxMouseEvent &event) {
        int i = HitTest(event.GetX());
        if (i < 0)
                return;

        const drive_info_t &info = drives[i];
        wxMenu menu;
        if (info.type == DRIVE_TYPE_CDROM) {
                menu.Append(XRCID("IDM_CDROM_IMAGE_LOAD"), "Load CD-ROM image...");
                menu.Append(XRCID("IDM_CDROM_EMPTY"), "Eject CD-ROM")->Enable(info.enabled);
        } else if (is_zip_drive(info)) {
                menu.Append(XRCID("IDM_DISC_ZIP"), "Load ZIP disk...");
                menu.Append(XRCID("IDM_EJECT_ZIP"), "Eject ZIP disk")->Enable(info.enabled);
        } else if (info.type == DRIVE_TYPE_FDD) {
                menu.Append(XRCID(info.drive ? "IDM_DISC_B" : "IDM_DISC_A"),
                            wxString::Format("Change drive %c:...", info.drive_letter));
                menu.Append(XRCID(info.drive ? "IDM_EJECT_B" : "IDM_EJECT_A"),
                            wxString::Format("Eject drive %c:", info.drive_letter))
                        ->Enable(info.enabled);
        } else
                return;

        PopupMenu(&menu, event.GetPosition());
}

MainStatusBar::MainStatusBar(wxWindow *parent) : wxStatusBar(parent, wxID_ANY), timer(this) {
        machineInfoText[0] = 0;
        drivePanel = new DriveStatusPanel(this);

        SetFieldsCount(3);
        UpdateLayout();

        Bind(wxEVT_TIMER, &MainStatusBar::OnTimer, this);
        Bind(wxEVT_SIZE, &MainStatusBar::OnSize, this);
        timer.Start(100);
}

MainStatusBar::~MainStatusBar() { timer.Stop(); }

void MainStatusBar::UpdateLayout() {
        int widths[3] = {drivePanel->GetPanelWidth(), -1, 110};
        SetStatusWidths(3, widths);

        wxRect rect;
        if (GetFieldRect(0, rect))
                drivePanel->SetSize(rect);
}

void MainStatusBar::OnSize(wxSizeEvent &event) {
        UpdateLayout();
        event.Skip();
}

void MainStatusBar::OnTimer(wxTimerEvent &event) {
        wxString hint, speed;

        if (emulation_state == EMULATION_STOPPED) {
                if (drivePanel->UpdateDrives(NULL, 0))
                        UpdateLayout();
                speed = "Stopped";
        } else {
                int num_drives;
                drive_info_t *drives = get_machine_info(machineInfoText, &num_drives);
                if (drivePanel->UpdateDrives(drives, num_drives))
                        UpdateLayout();

                if (emulation_state == EMULATION_PAUSED)
                        speed = "Paused";
                else
                        speed = wxString::Format("Speed: %d%%", fps);
                hint = mousecapture ? "Press Ctrl+End to release mouse" : "Click to capture mouse";
        }

        if (GetStatusText(1) != hint)
                SetStatusText(hint, 1);
        if (GetStatusText(2) != speed)
                SetStatusText(speed, 2);
}

extern "C" {
extern int window_remember;
void wx_handle_command(void *, int, int);

void resume_emulation();
void pause_emulation();
int stop_emulation_confirm();
void reset_emulation();

void hdconf_open(void *hwnd);
void config_open(void *hwnd);
}

int wx_window_x = 0;
int wx_window_y = 0;

StatusFrame::StatusFrame(wxWindow *parent)
        : wxFrame(parent, STATUS_WINDOW_ID, "PCem Machine", wxPoint(0, 0), wxSize(DEFAULT_WINDOW_WIDTH, 200),
                  wxCAPTION | wxCLOSE_BOX) {
        SetMenuBar(wxXmlResource::Get()->LoadMenuBar(wxT("status_menu")));
        SetToolBar(wxXmlResource::Get()->LoadToolBar(this, wxT("tool_bar")));

        this->statusPane = new StatusPane(this);
        wxBoxSizer *sizer = new wxBoxSizer(wxHORIZONTAL);
        sizer->Add(statusPane, 1, wxEXPAND);
        SetSizer(sizer);

        Bind(wxEVT_MENU, &StatusFrame::OnCommand, this);
        Bind(wxEVT_MOVE, &StatusFrame::OnMoveWindow, this);

        statusTimer = new StatusTimer(statusPane);
        statusTimer->Start();

        if (window_remember)
                SetPosition(wxPoint(wx_window_x, wx_window_y));
        else
                CenterOnScreen();

        UpdateToolbar();
        GetMenuBar()->FindItem(XRCID("IDM_SHOW_STATUS"))->Check(show_status);
        GetMenuBar()->FindItem(XRCID("IDM_SPEED_HISTORY"))->Check(show_speed_history);
        GetMenuBar()->FindItem(XRCID("IDM_DISC_ACTIVITY"))->Check(show_disc_activity);
        GetMenuBar()->FindItem(XRCID("IDM_MACHINE_MOUNT_PATHS"))->Check(show_mount_paths);
        GetMenuBar()->FindItem(XRCID("IDM_SHOW_MACHINE_ON_START"))->Check(show_machine_on_start);
}

StatusFrame::~StatusFrame() {
        statusTimer->Stop();
        delete statusTimer;
}

void StatusFrame::UpdateToolbar() {
        GetToolBar()->EnableTool(XRCID("TOOLBAR_RUN"), emulation_state != EMULATION_RUNNING);
        GetToolBar()->EnableTool(XRCID("TOOLBAR_PAUSE"), emulation_state != EMULATION_PAUSED);
}

void StatusFrame::OnCommand(wxCommandEvent &event) {
        if (event.GetId() == XRCID("TOOLBAR_RUN")) {
                resume_emulation();
                UpdateToolbar();
        } else if (event.GetId() == XRCID("TOOLBAR_PAUSE")) {
                pause_emulation();
                UpdateToolbar();
        } else if (event.GetId() == XRCID("TOOLBAR_RESET")) {
                int ret = wxID_OK;
                if (confirm_on_reset_machine) {
                        wxDialog dlg;
                        wxXmlResource::Get()->LoadDialog(&dlg, this, "ConfirmRememberDlg");
                        dlg.FindWindow("IDC_CONFIRM_LABEL")->SetLabel("Reset machine?");
                        dlg.FindWindow("IDC_CONFIRM_REMEMBER")->SetLabel("Do not ask again");
                        dlg.Fit();
                        ret = dlg.ShowModal();
                        if (ret == wxID_OK)
                                confirm_on_reset_machine = !((wxCheckBox *)dlg.FindWindow("IDC_CONFIRM_REMEMBER"))->IsChecked();
                }
                if (ret == wxID_OK) {
                        reset_emulation();
                        UpdateToolbar();
                }
        } else if (event.GetId() == XRCID("TOOLBAR_STOP"))
                wx_stop_emulation(GetParent());
        else if (event.GetId() == XRCID("IDM_SHOW_STATUS"))
                show_status = event.IsChecked();
        else if (event.GetId() == XRCID("IDM_SPEED_HISTORY"))
                show_speed_history = event.IsChecked();
        else if (event.GetId() == XRCID("IDM_DISC_ACTIVITY"))
                show_disc_activity = event.IsChecked();
        else if (event.GetId() == XRCID("IDM_MACHINE_MOUNT_PATHS"))
                show_mount_paths = event.IsChecked();
        else if (event.GetId() == XRCID("IDM_SHOW_MACHINE_ON_START"))
                show_machine_on_start = event.IsChecked();
        //        else if (event.GetId() == XRCID("IDM_HDCONF"))
        //                hdconf_open(this);
        else if (event.GetId() == XRCID("IDM_CONFIG"))
                config_open(this);
        else if (event.GetId() == XRCID("IDM_RESET_CONFIRMATION_DIALOGS")) {
                confirm_on_reset_machine = 1;
                confirm_on_stop_emulation = 1;
                wxMessageBox("Confirmation dialogs has been reset.", "PCem");
        } else if (event.GetId() == XRCID("IDM_ABOUT")) {
                wxDialog dlg;
                wxXmlResource::Get()->LoadDialog(&dlg, this, "AboutDlg");
                wxString version = dlg.FindWindow("VERSION_TEXT")->GetLabel();
                version.Replace("PCEM-VERSION-STRING", PCEM_VERSION_STRING);
                dlg.FindWindow("VERSION_TEXT")->SetLabel(version);
                dlg.Fit();
                dlg.ShowModal();
        }
}

void StatusFrame::OnMoveWindow(wxMoveEvent &event) {
        if (window_remember) {
                wx_window_x = GetScreenPosition().x;
                wx_window_y = GetScreenPosition().y;
        }
}
