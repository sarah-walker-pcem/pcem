#include "wx-app.h"

#include <wx/xrc/xmlres.h>
#include <wx/event.h>
#include "wx-utils.h"
#include "wx-common.h"

#ifdef _WIN32
#define BITMAP WINDOWS_BITMAP
#include <windows.h>
#include <windowsx.h>
#undef BITMAP
#endif

extern "C"
{
        int wx_load_config(void*);
        int wx_start(void*);
        int wx_stop(void*);
        void wx_show(void*);
        void wx_handle_command(void*, int, int);

        int start_emulation(void*);
        int resume_emulation();
        int pause_emulation();
        int stop_emulation();
}

extern int config_override;

extern void InitXmlResource();

wxDEFINE_EVENT(WX_CALLBACK_EVENT, CallbackEvent);
wxDEFINE_EVENT(WX_EXIT_EVENT, wxCommandEvent);
wxDEFINE_EVENT(WX_STOP_EMULATION_EVENT, wxCommandEvent);
wxDEFINE_EVENT(WX_EXIT_COMPLETE_EVENT, wxCommandEvent);
wxDEFINE_EVENT(WX_SHOW_WINDOW_EVENT, wxCommandEvent);
wxDEFINE_EVENT(WX_POPUP_MENU_EVENT, PopupMenuEvent);
#ifdef _WIN32
wxDEFINE_EVENT(WX_WIN_SEND_MESSAGE_EVENT, WinSendMessageEvent);
#endif

wxBEGIN_EVENT_TABLE(Frame, wxFrame)
wxEND_EVENT_TABLE()

wxIMPLEMENT_APP_NO_MAIN(App);

App::App()
{
        this->frame = NULL;
}

bool App::OnInit()
{
        wxImage::AddHandler( new wxPNGHandler );
        wxXmlResource::Get()->InitAllHandlers();
//        if (!wxXmlResource::Get()->Load("src/pc.xrc"))
//        {
//                std::cout << "Could not load resource file" << std::endl;
//                return false;
//        }
        InitXmlResource();

        frame = new Frame(this, "null frame", wxPoint(500, 500),
                        wxSize(100, 100));
        frame->Start();
        return true;
}

int App::OnRun()
{
        return wxApp::OnRun();
}

#include <sstream>

Frame::Frame(App* app, const wxString& title, const wxPoint& pos,
                const wxSize& size) :
                wxFrame(NULL, wxID_ANY, title, pos, size, 0)//wxDEFAULT_FRAME_STYLE & ~(wxRESIZE_BORDER))
{
        this->closing = false;
        this->menu = wxXmlResource::Get()->LoadMenu(wxT("main_menu"));

        Bind(wxEVT_CLOSE_WINDOW, &Frame::OnClose, this);
        Bind(wxEVT_MENU, &Frame::OnCommand, this);
        Bind(wxEVT_TOOL, &Frame::OnCommand, this);
        Bind(WX_SHOW_WINDOW_EVENT, &Frame::OnShowWindowEvent, this);
        Bind(WX_POPUP_MENU_EVENT, &Frame::OnPopupMenuEvent, this);
        Bind(WX_EXIT_EVENT, &Frame::OnExitEvent, this);
        Bind(WX_EXIT_COMPLETE_EVENT, &Frame::OnExitCompleteEvent, this);
        Bind(WX_STOP_EMULATION_EVENT, &Frame::OnStopEmulationEvent, this);
        Bind(WX_CALLBACK_EVENT, &Frame::OnCallbackEvent, this);
#ifdef _WIN32
        Bind(WX_WIN_SEND_MESSAGE_EVENT, &Frame::OnWinSendMessageEvent, this);
#endif

        CenterOnScreen();
}

void Frame::Start()
{
        if (wx_start(this))
                ShowConfigSelection();
        else
                Quit(0);
}

void Frame::ShowConfigSelection()
{
        if (wx_load_config(this))
                start_emulation(this);
        else
                Quit(1);
}

void Frame::OnCallbackEvent(CallbackEvent& event)
{
        WX_CALLBACK callback = event.GetCallback();
        callback(event.GetData());
}

void Frame::OnStopEmulationEvent(wxCommandEvent& event)
{
        if (emulation_state != EMULATION_STOPPED)
        {
                pause_emulation();
                int ret = wxID_OK;

                if (confirm_on_stop_emulation)
                {
                        wxDialog dlg;
                        wxXmlResource::Get()->LoadDialog(&dlg, this, "ConfirmRememberDlg");
                        dlg.FindWindow("IDC_CONFIRM_LABEL")->SetLabel("Stop emulation?");
                        dlg.FindWindow("IDC_CONFIRM_REMEMBER")->SetLabel("Do not ask again");
                        dlg.Fit();
                        ret = dlg.ShowModal();
                        if (ret == wxID_OK)
                                confirm_on_stop_emulation = !((wxCheckBox*)dlg.FindWindow("IDC_CONFIRM_REMEMBER"))->IsChecked();
                }

                if (ret == wxID_OK)
                {
                        stop_emulation();
                        if (!config_override)
                                ShowConfigSelection();
                        else
                                Quit(1);
                }
                else
                        resume_emulation();
        }
}

void Frame::OnShowWindowEvent(wxCommandEvent& event)
{
        wxWindow* window = (wxWindow*)event.GetEventObject();
        int shown = window->IsShown();
        int value = event.GetInt();
        if (value < 0)
                window->Show(!shown);
        else
                window->Show(value > 0);
        if (!shown)
                window->Refresh();
}

void Frame::OnPopupMenuEvent(PopupMenuEvent& event)
{
        wxWindow* window = event.GetWindow();
        wxMenu* menu = event.GetMenu();
        int* x = event.GetX();
        int* y = event.GetY();

        if (x && y)
                window->PopupMenu(menu, *x, *y);
        else
                window->PopupMenu(menu);
}

void Frame::OnCommand(wxCommandEvent& event)
{
        wx_handle_command(this, event.GetId(), event.IsChecked());
}

void Frame::OnClose(wxCloseEvent& event)
{
        wx_exit(this, 0);
}

wxMenu* Frame::GetMenu()
{
        return menu;
}

/* Exit */

void Frame::Quit(bool stop_emulator)
{
        if (closing)
                return;
        closing = true;
        if (stop_emulator)
        {
                if (!wx_stop(this))
                {
                        // cancel quit
                        closing = false;
                        return;
                }
        }
        Destroy();
}

void Frame::OnExitEvent(wxCommandEvent& event)
{
        if (closing)
                return;
        closing = true;
        CExitThread* exitThread = new CExitThread(this);
        exitThread->Run();
}

void Frame::OnExitCompleteEvent(wxCommandEvent& event)
{
        if (event.GetInt())
                Destroy();
        else
                closing = false;
}

CExitThread::CExitThread(Frame* frame)
{
	this->frame = frame;
}

wxThread::ExitCode CExitThread::Entry()
{
        wxCommandEvent* event = new wxCommandEvent(WX_EXIT_COMPLETE_EVENT, wxID_ANY);
        event->SetInt(wx_stop(frame));
        wxQueueEvent(frame, event);
	return 0;
}

#ifdef _WIN32
void Frame::OnWinSendMessageEvent(WinSendMessageEvent& event)
{
        SendMessage((HWND)event.GetHWND(), event.GetMessage(), event.GetWParam(), event.GetLParam());
}
#endif
