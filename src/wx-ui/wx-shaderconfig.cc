#include "wx-utils.h"
#include <wx/wxprec.h>

#include "wx-dialogbox.h"

#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/xrc/xmlres.h>
#include <wx/spinctrl.h>

#include "wx-glslp-parser.h"
#include "wx-shaderconfig.h"

extern "C"
{
        #include "config.h"
        void saveconfig(char*);
        void resetpchard();
        int deviceconfig_dlgproc(void* hdlg, int message, INT_PARAM wParam, LONG_PARAM lParam);
        void write_shader_config(glslp_t* shader);
        struct glslp_t* current_glsl;
        int confirm();
}
#define IDC_CONFIG_BASE 1000
#define ID_APPLY 500
#define ID_RESET 501
extern int renderer_doreset;

static int shaderconfig_dlgproc(void* hdlg, int message, INT_PARAM wParam, LONG_PARAM lParam) {
        int i;
        switch (message) {
        case WX_INITDIALOG: {
                        return TRUE;
                }
        case WX_COMMAND:
                switch (wParam) {
                case ID_APPLY: {
                        for (i = 0; i < current_glsl->num_parameters; ++i)
                        {
                                struct parameter* p = &current_glsl->parameters[i];
                                wxSpinCtrlDouble* h = (wxSpinCtrlDouble*)wx_getdlgitem(hdlg, IDC_CONFIG_BASE+i);
                                p->value = (float)h->GetValue();
                        }
                        write_shader_config(current_glsl);
                        renderer_doreset = 1;
                        return TRUE;
                }
                case wxID_OK: {
                        for (i = 0; i < current_glsl->num_parameters; ++i)
                        {
                                struct parameter* p = &current_glsl->parameters[i];
                                wxSpinCtrlDouble* h = (wxSpinCtrlDouble*)wx_getdlgitem(hdlg, IDC_CONFIG_BASE+i);
                                p->value = (float)h->GetValue();
                        }
                        write_shader_config(current_glsl);
                        renderer_doreset = 1;
                        wx_enddialog(hdlg, 0);
                        return TRUE;
                }
                case wxID_CANCEL:
                        wx_enddialog(hdlg, 0);
                        return TRUE;
                case ID_RESET: {
                        for (i = 0; i < current_glsl->num_parameters; ++i)
                        {
                                struct parameter* p = &current_glsl->parameters[i];
                                wxSpinCtrlDouble* h = (wxSpinCtrlDouble*)wx_getdlgitem(hdlg, IDC_CONFIG_BASE+i);
                                h->SetValue(p->default_value);
                        }
                        return TRUE;
                }
                }
        }
        return TRUE;
}



void shaderconfig_open(void* hwnd, struct glslp_t* glsl) {
        int i;
        current_glsl = glsl;

        PCemDialogBox dialog((wxWindow*) hwnd, shaderconfig_dlgproc);
//      dialog.SetWindowStyle(wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);

        dialog.SetTitle("Shader Configuration");

        wxBoxSizer* root = new wxBoxSizer(wxVERTICAL);
        dialog.SetSizer(root);

        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
        root->Add(sizer, 0, wxALL, 5);

        wxPanel* panel = new wxPanel(&dialog, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL | wxBORDER_SUNKEN);
        sizer->Add(panel);

        wxBoxSizer* panelSizer = new wxBoxSizer(wxVERTICAL);
        panel->SetSizer(panelSizer);

        wxScrolledWindow* win = new wxScrolledWindow(panel, wxID_ANY, wxDefaultPosition, wxSize(300, 350), wxHSCROLL|wxVSCROLL);
        win->SetScrollRate( 5, 5 );
        panelSizer->Add(win, 1, wxEXPAND | wxALL, 5);

        wxBoxSizer* winSizer = new wxBoxSizer(wxVERTICAL);
        win->SetSizer(winSizer);

        if (glsl->num_parameters > 0)
        {
                wxPanel* scrollPanel = new wxPanel(win);
                winSizer->Add(scrollPanel);

                wxBoxSizer* scollPanelSizer = new wxBoxSizer(wxVERTICAL);
                scrollPanel->SetSizer(scollPanelSizer);

                for (i = 0; i < glsl->num_parameters; ++i)
                {
                        struct parameter* p = &glsl->parameters[i];
                        scollPanelSizer->Add(new wxStaticText(scrollPanel, wxID_ANY, p->description), 0, wxALL);
                        wxSpinCtrlDouble* spin = new wxSpinCtrlDouble(scrollPanel, IDC_CONFIG_BASE+i);
                        spin->SetRange(p->min, p->max);
                        spin->SetIncrement(p->step);
                        spin->SetValue(p->value);
                        spin->SetDigits(2);

                        scollPanelSizer->Add(spin, 0, wxALL);
                }
        }
        else
                winSizer->Add(new wxStaticText(win, wxID_ANY, "No configuration available."), 0, wxALL);

        wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);

        buttonSizer->Add(new wxButton(&dialog, ID_APPLY, "Apply"));
        buttonSizer->Add(new wxButton(&dialog, wxID_OK));
        buttonSizer->Add(new wxButton(&dialog, wxID_CANCEL));
        buttonSizer->Add(new wxButton(&dialog, ID_RESET, "Reset"));

        sizer->Add(buttonSizer);

        dialog.Fit();
        dialog.OnInit();
        dialog.ShowModal();
        dialog.Destroy();
}
