#ifndef SRC_WX_STATUS_H_
#define SRC_WX_STATUS_H_

#include <wx/wx.h>
#include <wx/timer.h>

#define DEFAULT_WINDOW_WIDTH 350
#define DEFAULT_WINDOW_HEIGHT 25

#define SPEED_HISTORY_LENGTH 240

class StatusPane;

class StatusTimer : public wxTimer
{
public:
        StatusTimer(StatusPane* pane);
        virtual ~StatusTimer() {};

        void Notify();
        void Start();
private:
        StatusPane* pane;
};

class StatusPane : public wxPanel
{

public:
    StatusPane(wxFrame* parent);
    virtual ~StatusPane();

    void PaintEvent(wxPaintEvent& evt);
    void PaintNow();
    void Render( wxDC& dc );

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

#define STATUS_WINDOW_ID 1000

class StatusFrame : public wxFrame {
public:
        StatusFrame(wxWindow* parent);
        virtual ~StatusFrame();
        void OnQuit();
private:
        void OnCommand(wxCommandEvent &event);
        void OnMoveWindow(wxMoveEvent& event);
        StatusPane* statusPane;
        StatusTimer* statusTimer;

        void UpdateToolbar();

        DECLARE_EVENT_TABLE()

};

#endif /* SRC_WX_STATUS_H_ */
