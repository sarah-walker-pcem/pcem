#include "wx-app.h"

#include <wx/xrc/xmlres.h>
#include <wx/event.h>
#include "wx-utils.h"
#include "wx-common.h"
#include "wx-frontend.h"
#include "wx-status.h"
#include "logging-internal.h"

extern "C" {
#include "sdl2-emu.h"
}

extern int config_override;

extern void InitXmlResource();

wxDEFINE_EVENT(WX_CALLBACK_EVENT, CallbackEvent);
wxDEFINE_EVENT(WX_EXIT_EVENT, wxCommandEvent);
wxDEFINE_EVENT(WX_STOP_EMULATION_EVENT, wxCommandEvent);
wxDEFINE_EVENT(WX_STOP_EMULATION_NOW_EVENT, wxCommandEvent);
wxDEFINE_EVENT(WX_EXIT_COMPLETE_EVENT, wxCommandEvent);
wxDEFINE_EVENT(WX_SHOW_WINDOW_EVENT, wxCommandEvent);

wxBEGIN_EVENT_TABLE(Frame, wxFrame) wxEND_EVENT_TABLE()

        wxIMPLEMENT_APP_NO_MAIN(App);

static void bind_quit_handlers(wxApp *app) {
#ifdef __WXOSX__
        app->Bind(wxEVT_CHAR_HOOK, [](wxKeyEvent &event) {
                wxDialog *dlg = wxDynamicCast(wxGetTopLevelParent((wxWindow *)event.GetEventObject()), wxDialog);
                if (event.GetKeyCode() == 'Q' && event.GetModifiers() == wxMOD_CMD && dlg &&
                    dlg->GetName() == "ConfigureSelectionDlg" && dlg->IsModal())
                        dlg->EndModal(0);
                else
                        event.Skip();
        });
#endif
}

App::App() { this->frame = NULL; }

bool App::OnInit() {
        SetAppDisplayName("PCem");
        wxImage::AddHandler(new wxPNGHandler);
        wxXmlResource::Get()->InitAllHandlers();
        InitXmlResource();

        bind_quit_handlers(this);

        frame = new Frame(this, "null frame", wxPoint(500, 500), wxSize(100, 100));
        frame->Start();
        return true;
}

int App::OnRun() { return wxApp::OnRun(); }

#include <sstream>

Frame::Frame(App *app, const wxString &title, const wxPoint &pos, const wxSize &size)
        : wxFrame(NULL, wxID_ANY, title, pos, size, 0) // wxDEFAULT_FRAME_STYLE & ~(wxRESIZE_BORDER))
{
        this->closing = false;
        this->menu = wxXmlResource::Get()->LoadMenuBar(wxT("main_menu"));

        Bind(wxEVT_CLOSE_WINDOW, &Frame::OnClose, this);
        Bind(wxEVT_MENU, &Frame::OnCommand, this);
        Bind(wxEVT_TOOL, &Frame::OnCommand, this);
        Bind(WX_SHOW_WINDOW_EVENT, &Frame::OnShowWindowEvent, this);
        Bind(WX_EXIT_EVENT, &Frame::OnExitEvent, this);
        Bind(WX_EXIT_COMPLETE_EVENT, &Frame::OnExitCompleteEvent, this);
        Bind(WX_STOP_EMULATION_EVENT, &Frame::OnStopEmulationEvent, this);
        Bind(WX_STOP_EMULATION_NOW_EVENT, &Frame::OnStopEmulationNowEvent, this);
        Bind(WX_CALLBACK_EVENT, &Frame::OnCallbackEvent, this);

        wx_set_app_icon(this);
        CenterOnScreen();
}

void wx_set_app_icon(wxTopLevelWindow *win) {
#ifdef __WXMSW__
        static wxIconBundle icons(wxT("MAIN_ICO"), 0);
        if (icons.IsOk())
                win->SetIcons(icons);
#else
        (void)win;
#endif
}

void Frame::Start() {
        if (wx_start(this))
                ShowConfigSelection();
        else
                Quit(0);
}

void Frame::ShowConfigSelection() {
        if (wx_load_config(this))
                start_emulation(this);
        else
                Quit(1);
}

void Frame::OnCallbackEvent(CallbackEvent &event) {
        WX_CALLBACK callback = event.GetCallback();
        callback(event.GetData());
}

void Frame::OnStopEmulationEvent(wxCommandEvent &event) {
        if (emulation_state != EMULATION_STOPPED) {
                pause_emulation();
                int ret = wxID_OK;
                pclog_flush();
                if (confirm_on_stop_emulation) {
                        wxDialog dlg;
                        wxXmlResource::Get()->LoadDialog(&dlg, this, "ConfirmRememberDlg");
                        dlg.FindWindow("IDC_CONFIRM_LABEL")->SetLabel("Stop emulation?");
                        dlg.FindWindow("IDC_CONFIRM_REMEMBER")->SetLabel("Do not ask again");
                        dlg.Fit();
                        ret = dlg.ShowModal();
                        if (ret == wxID_OK)
                                confirm_on_stop_emulation = !((wxCheckBox *)dlg.FindWindow("IDC_CONFIRM_REMEMBER"))->IsChecked();
                }

                if (ret == wxID_OK) {
                        stop_emulation();
                        if (!config_override)
                                ShowConfigSelection();
                        else
                                Quit(1);
                } else
                        resume_emulation();
        }
}

void Frame::OnStopEmulationNowEvent(wxCommandEvent &event) {
        stop_emulation();
        pclog_flush();
        if (!config_override)
                ShowConfigSelection();
        else
                Quit(1);
}

void Frame::OnShowWindowEvent(wxCommandEvent &event) {
        wxWindow *window = (wxWindow *)event.GetEventObject();
        int shown = window->IsShown();
        int value = event.GetInt();
        if (value < 0)
                window->Show(!shown);
        else
                window->Show(value > 0);
        if (!shown)
                window->Refresh();
}

void Frame::OnCommand(wxCommandEvent &event) { wx_handle_command(this, event.GetId(), event.IsChecked()); }

void Frame::OnClose(wxCloseEvent &event) {
        pclog_end();
        QueueCommand(WX_EXIT_EVENT, 0);
}

wxMenuBar *Frame::GetMenu() { return menu; }

void Frame::QueueCommand(wxEventType type, int value) {
        wxCommandEvent *event = new wxCommandEvent(type, wxID_ANY);
        event->SetEventObject(this);
        event->SetInt(value);
        wxQueueEvent(this, event);
}

void Frame::QueueCallback(WX_CALLBACK callback, void *data) { wxQueueEvent(this, new CallbackEvent(callback, data)); }

void Frame::StopEmulation() { QueueCommand(WX_STOP_EMULATION_EVENT, 0); }

void Frame::StopEmulationNow() { QueueCommand(WX_STOP_EMULATION_NOW_EVENT, 0); }

void Frame::ToggleVisible() { QueueCommand(WX_SHOW_WINDOW_EVENT, -1); }

void Frame::ShowStatus() {
        if (!FindWindowById(STATUS_WINDOW_ID))
                (new StatusFrame(this))->Show();
}

void Frame::CloseStatus() {
        wxWindow *status = FindWindowById(STATUS_WINDOW_ID);
        if (status)
                status->Close();
}

/* Exit */

void Frame::Quit(bool stop_emulator) {
        if (closing)
                return;
        closing = true;
        if (stop_emulator) {
                if (!wx_stop(this)) {
                        // cancel quit
                        closing = false;
                        return;
                }
        }
        pclog_end();
        Destroy();
}

void Frame::OnExitEvent(wxCommandEvent &event) {
        if (closing)
                return;
        closing = true;
        CExitThread *exitThread = new CExitThread(this);
        exitThread->Run();
}

void Frame::OnExitCompleteEvent(wxCommandEvent &event) {
        if (event.GetInt()) {
                pclog_end();
                Destroy();
        } else
                closing = false;
}

CExitThread::CExitThread(Frame *frame) { this->frame = frame; }

wxThread::ExitCode CExitThread::Entry() {
        wxCommandEvent *event = new wxCommandEvent(WX_EXIT_COMPLETE_EVENT, wxID_ANY);
        event->SetInt(wx_stop(frame));
        wxQueueEvent(frame, event);
        return 0;
}

