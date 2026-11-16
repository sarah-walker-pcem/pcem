#include "wx-shaderconfig.h"

#include <wx/spinctrl.h>

#include <vector>

extern "C" {
#include "sdl2-glslp-parser.h"
#include "sdl2-display.h"
}

ShaderConfigDialog::ShaderConfigDialog(wxWindow *parent, glslp_t *glsl) : wxDialog(parent, wxID_ANY, "Shader Configuration"),
                                       glsl(glsl) {
        LoadConfig();

        wxBoxSizer *root = new wxBoxSizer(wxVERTICAL);
        SetSizer(root);

        wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
        root->Add(sizer, 0, wxALL, 5);

        wxPanel *panel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL | wxBORDER_SUNKEN);
        sizer->Add(panel);

        wxBoxSizer *panelSizer = new wxBoxSizer(wxVERTICAL);
        panel->SetSizer(panelSizer);

        wxScrolledWindow *win =
                new wxScrolledWindow(panel, wxID_ANY, wxDefaultPosition, wxSize(300, 350), wxHSCROLL | wxVSCROLL);
        win->SetScrollRate(5, 5);
        panelSizer->Add(win, 1, wxEXPAND | wxALL, 5);

        wxBoxSizer *winSizer = new wxBoxSizer(wxVERTICAL);
        win->SetSizer(winSizer);

        if (glsl->num_parameters > 0) {
                wxPanel *scrollPanel = new wxPanel(win);
                winSizer->Add(scrollPanel);

                wxBoxSizer *scrollPanelSizer = new wxBoxSizer(wxVERTICAL);
                scrollPanel->SetSizer(scrollPanelSizer);

                for (int i = 0; i < glsl->num_parameters; ++i) {
                        struct parameter *p = &glsl->parameters[i];
                        scrollPanelSizer->Add(new wxStaticText(scrollPanel, wxID_ANY, p->description), 0, wxALL);
                        wxSpinCtrlDouble *spin = new wxSpinCtrlDouble(scrollPanel, wxID_ANY);
                        spin->SetRange(p->min, p->max);
                        spin->SetIncrement(p->step);
                        spin->SetValue(p->value);
                        spin->SetDigits(2);
                        scrollPanelSizer->Add(spin, 0, wxALL);
                        spins.push_back(spin);
                }
        } else
                winSizer->Add(new wxStaticText(win, wxID_ANY, "No configuration available."), 0, wxALL);

        wxBoxSizer *buttonSizer = new wxBoxSizer(wxHORIZONTAL);
        wxButton *apply = new wxButton(this, wxID_APPLY, "Apply");
        wxButton *reset = new wxButton(this, wxID_ANY, "Reset");
        buttonSizer->Add(apply);
        buttonSizer->Add(new wxButton(this, wxID_OK));
        buttonSizer->Add(new wxButton(this, wxID_CANCEL));
        buttonSizer->Add(reset);
        sizer->Add(buttonSizer);

        apply->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { Apply(); });
        reset->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { Reset(); });
        Bind(wxEVT_BUTTON,
             [this](wxCommandEvent &) {
                     Apply();
                     EndModal(wxID_OK);
             },
             wxID_OK);
        Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_CANCEL); }, wxID_CANCEL);

        Fit();
}

void ShaderConfigDialog::SaveConfig() {
        wxString section = Section();
        for (int i = 0; i < glsl->num_parameters; ++i) {
                struct parameter *param = &glsl->parameters[i];
                wx_config_set_float(CFG_MACHINE, section.mb_str(), param->id, param->value);
        }
}

wxString ShaderConfigDialog::Section() { return wxString::Format("GL3 Shaders - %s", glsl->name); }

void ShaderConfigDialog::LoadConfig() {
        wxString section = Section();
        for (int i = 0; i < glsl->num_parameters; ++i) {
                struct parameter *param = &glsl->parameters[i];
                param->value = wx_config_get_float(CFG_MACHINE, section.mb_str(), param->id, param->default_value);
        }
}

void ShaderConfigDialog::Apply() {
        for (size_t i = 0; i < spins.size(); ++i)
                glsl->parameters[i].value = (float)spins[i]->GetValue();
        SaveConfig();
        renderer_doreset = 1;
}

void ShaderConfigDialog::Reset() {
        for (size_t i = 0; i < spins.size(); ++i)
                spins[i]->SetValue(glsl->parameters[i].default_value);
}

void ShaderConfigDialog::Open(wxWindow *parent, glslp_t *glsl) {
        ShaderConfigDialog dlg(parent, glsl);
        dlg.ShowModal();
        dlg.SaveConfig();
}
