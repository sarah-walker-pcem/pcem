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

#ifdef _WIN32
class WinSendMessageEvent;
wxDECLARE_EVENT(WX_WIN_SEND_MESSAGE_EVENT, WinSendMessageEvent);
class WinSendMessageEvent: public wxCommandEvent
{
public:
        WinSendMessageEvent(void* hwnd, int message, INT_PARAM wParam, LONG_PARAM lParam) : wxCommandEvent(WX_WIN_SEND_MESSAGE_EVENT)
        {
                this->hwnd = hwnd;
                this->message = message;
                this->wParam = wParam;
                this->lParam = lParam;
        }
        WinSendMessageEvent(const WinSendMessageEvent& event) : wxCommandEvent(event)
        {
                this->hwnd = event.GetHWND();
                this->message = event.GetMessage();
                this->wParam = event.GetWParam();
                this->lParam = event.GetLParam();
        }

        wxEvent* Clone() const { return new WinSendMessageEvent(*this); }

        void* GetHWND() const { return hwnd; }
        int GetMessage() const { return message; }
        INT_PARAM GetWParam() const { return wParam; }
        LONG_PARAM GetLParam() const { return lParam; }

private:
        void* hwnd;
        int message;
        INT_PARAM wParam;
        LONG_PARAM lParam;
};
#endif

class CallbackEvent: public wxCommandEvent
{
public:
        CallbackEvent(WX_CALLBACK callback, void* data) : wxCommandEvent(WX_CALLBACK_EVENT)
        {
                this->callback = callback;
                this->data = data;
        }
        CallbackEvent(const CallbackEvent& event) : wxCommandEvent(event)
        {
                this->callback = event.GetCallback();
                this->data = event.GetData();
        }

        wxEvent* Clone() const { return new CallbackEvent(*this); }

        WX_CALLBACK GetCallback() const { return callback; }
        void* GetData() const { return data; }

private:
        WX_CALLBACK callback;
        void* data;


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
#ifdef _WIN32
        void OnWinSendMessageEvent(WinSendMessageEvent& event);
#endif

        void ShowConfigSelection();
        wxMenu* menu;

        bool closing;

        void Quit(bool stop_emulator = 1);

        wxDECLARE_EVENT_TABLE();
};

#endif /* SRC_WX_APP_H_ */
