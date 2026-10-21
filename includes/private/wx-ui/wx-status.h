#ifndef _WX_STATUS_H_
#define _WX_STATUS_H_

#include <wx/wx.h>
#include <wx/timer.h>
#include "wx-common.h"

#define DEFAULT_WINDOW_WIDTH 350
#define DEFAULT_WINDOW_HEIGHT 25

#define SPEED_HISTORY_LENGTH 240

class StatusPane;

class StatusTimer : public wxTimer {
    public:
        StatusTimer(StatusPane *pane);
        virtual ~StatusTimer(){};

        void Notify();
        void Start();

    private:
        StatusPane *pane;
};

class StatusPane : public wxPanel {

    public:
        StatusPane(wxFrame *parent);
        virtual ~StatusPane();

        void PaintEvent(wxPaintEvent &evt);
        void PaintNow();
        void Render(wxDC &dc);

    private:
        char machineInfoText[4096];
        char statusMachineText[4096];
        char statusDeviceText[4096];
        wxLongLong lastSpeedUpdate;
        char speedHistory[SPEED_HISTORY_LENGTH];

        wxBitmap bitmapFDD[2];
        wxBitmap bitmapCDROM[2];
        wxBitmap bitmapHDD[2];

        DECLARE_EVENT_TABLE()
};

#define MAX_STATUS_DRIVES 10

/* Row of drive icons with activity indicators, shown in the main status bar */
class DriveStatusPanel : public wxPanel {
    public:
        DriveStatusPanel(wxWindow *parent);

        /* Returns true if the number of drives changed */
        bool UpdateDrives(drive_info_t *drives, int num);
        int GetPanelWidth();

    private:
        void OnPaint(wxPaintEvent &event);
        void OnMouseMove(wxMouseEvent &event);
        void OnClick(wxMouseEvent &event);
        int HitTest(int x);
        wxBitmap GetBitmap(const drive_info_t &info);

        drive_info_t drives[MAX_STATUS_DRIVES];
        int numDrives;
        int tooltipDrive;

        wxBitmap bitmapFDD[2];
        wxBitmap bitmapCDROM[2];
        wxBitmap bitmapHDD[2];
};

class MainStatusBar : public wxStatusBar {
    public:
        MainStatusBar(wxWindow *parent);
        virtual ~MainStatusBar();

    private:
        void OnTimer(wxTimerEvent &event);
        void OnSize(wxSizeEvent &event);
        void UpdateLayout();

        DriveStatusPanel *drivePanel;
        wxTimer timer;
        char machineInfoText[4096];
};

#define STATUS_WINDOW_ID 1000

class StatusFrame : public wxFrame {
    public:
        StatusFrame(wxWindow *parent);
        virtual ~StatusFrame();
        void OnQuit();

    private:
        void OnCommand(wxCommandEvent &event);
        void OnMoveWindow(wxMoveEvent &event);
        StatusPane *statusPane;
        StatusTimer *statusTimer;

        void UpdateToolbar();

        DECLARE_EVENT_TABLE()
};

#endif /* _WX_STATUS_H_ */
