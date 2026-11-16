#ifndef SRC_WX_APP_H_
#define SRC_WX_APP_H_

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include "wx-utils.h"

typedef int (*WX_CALLBACK)(void *data);

class CallbackEvent;

wxDECLARE_EVENT(WX_CALLBACK_EVENT, CallbackEvent);
wxDECLARE_EVENT(WX_EXIT_EVENT, wxCommandEvent);
wxDECLARE_EVENT(WX_STOP_EMULATION_EVENT, wxCommandEvent);
wxDECLARE_EVENT(WX_STOP_EMULATION_NOW_EVENT, wxCommandEvent);
wxDECLARE_EVENT(WX_EXIT_COMPLETE_EVENT, wxCommandEvent);
wxDECLARE_EVENT(WX_SHOW_WINDOW_EVENT, wxCommandEvent);


class CallbackEvent : public wxCommandEvent {
    public:
        CallbackEvent(WX_CALLBACK callback, void *data) : wxCommandEvent(WX_CALLBACK_EVENT) {
                this->callback = callback;
                this->data = data;
        }
        CallbackEvent(const CallbackEvent &event) : wxCommandEvent(event) {
                this->callback = event.GetCallback();
                this->data = event.GetData();
        }

        wxEvent *Clone() const { return new CallbackEvent(*this); }

        WX_CALLBACK GetCallback() const { return callback; }
        void *GetData() const { return data; }

    private:
        WX_CALLBACK callback;
        void *data;
};

class Frame;

class App : public wxApp {
    public:
        App();
        virtual bool OnInit();
        virtual int OnRun();
        Frame *GetFrame() { return frame; }

    private:
        Frame *frame;
};

class CExitThread : public wxThread {
    public:
        CExitThread(Frame *frame);
        virtual ~CExitThread() {}

    private:
        wxThread::ExitCode Entry();
        Frame *frame;
};

class Frame : public wxFrame {
    public:
        Frame(App *app, const wxString &title, const wxPoint &pos, const wxSize &size);

        virtual ~Frame() {}

        void Start();

        wxMenuBar *GetMenu();

        void QueueCallback(WX_CALLBACK callback, void *data);
        void StopEmulation();
        void StopEmulationNow();
        void ToggleVisible();
        void ShowStatus();
        void CloseStatus();

    private:
        void OnCommand(wxCommandEvent &event);
        void OnExitEvent(wxCommandEvent &event);
        void OnExitCompleteEvent(wxCommandEvent &event);
        void OnStopEmulationEvent(wxCommandEvent &event);
        void OnStopEmulationNowEvent(wxCommandEvent &event);
        void OnShowWindowEvent(wxCommandEvent &event);
        void OnCallbackEvent(CallbackEvent &event);
        void OnClose(wxCloseEvent &event);

        void ShowConfigSelection();
        void QueueCommand(wxEventType type, int value);
        wxMenuBar *menu;

        bool closing;

        void Quit(bool stop_emulator = 1);

        wxDECLARE_EVENT_TABLE();
};

void wx_set_app_icon(wxTopLevelWindow *win);

#endif /* SRC_WX_APP_H_ */
