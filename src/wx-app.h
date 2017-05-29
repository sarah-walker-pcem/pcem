#ifndef SRC_WX_APP_H_
#define SRC_WX_APP_H_

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include "wx-utils.h"

class CallbackEvent;
class PopupMenuEvent;

wxDECLARE_EVENT(WX_CALLBACK_EVENT, CallbackEvent);
wxDECLARE_EVENT(WX_EXIT_EVENT, wxCommandEvent);
wxDECLARE_EVENT(WX_STOP_EMULATION_EVENT, wxCommandEvent);
wxDECLARE_EVENT(WX_EXIT_COMPLETE_EVENT, wxCommandEvent);
wxDECLARE_EVENT(WX_SHOW_WINDOW_EVENT, wxCommandEvent);
wxDECLARE_EVENT(WX_POPUP_MENU_EVENT, PopupMenuEvent);

class CallbackEvent: public wxCommandEvent
{
public:
        CallbackEvent(WX_CALLBACK callback) : wxCommandEvent(WX_CALLBACK_EVENT)
        {
                this->callback = callback;
        }
        CallbackEvent(const CallbackEvent& event) : wxCommandEvent(event)
        {
                this->callback = event.GetCallback();
        }

        wxEvent* Clone() const { return new CallbackEvent(*this); }

        WX_CALLBACK GetCallback() const { return callback; }

private:
        WX_CALLBACK callback;


};

class PopupMenuEvent: public wxCommandEvent
{
public:
        PopupMenuEvent(wxWindow* window, wxMenu* menu, int* x, int* y) : wxCommandEvent(WX_POPUP_MENU_EVENT)
        {
                this->window = window;
                this->menu = menu;
                this->x = x;
                this->y = y;
        }
        PopupMenuEvent(const PopupMenuEvent& event) : wxCommandEvent(event)
        {
                this->window = event.GetWindow();
                this->menu = event.GetMenu();
                this->x = event.GetX();
                this->y = event.GetY();
        }

        wxEvent* Clone() const { return new PopupMenuEvent(*this); }

        wxWindow*  GetWindow() const { return window; }
        wxMenu* GetMenu() const { return menu; }
        int* GetX() const { return x; }
        int* GetY() const { return y; }

private:
        int* x;
        int* y;
        wxMenu* menu;
        wxWindow* window;
};


class Frame;

class App: public wxApp
{
public:
        App();
        virtual bool OnInit();
        virtual int OnRun();
        Frame* GetFrame()
        {
                return frame;
        }
private:
        Frame* frame;
};

class CExitThread: public wxThread
{
public:
	CExitThread(Frame* frame);
    virtual ~CExitThread() {}
private:
    wxThread::ExitCode Entry();
    Frame* frame;
};

class Frame: public wxFrame
{
public:
        Frame(App* app, const wxString& title, const wxPoint& pos,
                        const wxSize& size);

        virtual ~Frame() {}

        void Start();

        wxMenu* GetMenu();

private:
        void OnCommand(wxCommandEvent& event);
        void OnExitEvent(wxCommandEvent& event);
        void OnExitCompleteEvent(wxCommandEvent& event);
        void OnStopEmulationEvent(wxCommandEvent& event);
        void OnShowWindowEvent(wxCommandEvent& event);
        void OnPopupMenuEvent(PopupMenuEvent& event);
        void OnCallbackEvent(CallbackEvent& event);
        void OnClose(wxCloseEvent& event);

        void ShowConfigSelection();
        wxMenu* menu;

        bool closing;

        void Quit(bool stop_emulator = 1);

        wxDECLARE_EVENT_TABLE();
};

#endif /* SRC_WX_APP_H_ */
