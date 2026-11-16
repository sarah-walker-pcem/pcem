#include "wx-shader_man.h"
#include "wx-shaderconfig.h"

#include <vector>

extern "C" {
#include "sdl2-display.h"
#include "sdl2-glslp-parser.h"
#include "sdl2-emu.h"
#include "config.h"
}

#define MAX_USER_SHADERS 20

ShaderManagerDialog::ShaderManagerDialog(wxWindow *parent) {
        wxXmlResource::Get()->LoadDialog(this, parent, "ShaderManagerDlg");
        list = wx_ctrl<wxListBox>(this, "IDC_LIST");

        for (int c = 0; c < MAX_USER_SHADERS; c++) {
                if (strlen(gl3_shader_file[c]))
                        shaders.push_back(gl3_shader_file[c]);
        }
        UpdateList(shaders.size() - 1);

        Bind(wxEVT_BUTTON, &ShaderManagerDialog::OnOk, this, wxID_OK);
        Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_CANCEL); }, wxID_CANCEL);
        Bind(wxEVT_BUTTON, &ShaderManagerDialog::OnApply, this, XRCID("IDC_APPLY"));
        Bind(wxEVT_BUTTON, &ShaderManagerDialog::OnAdd, this, XRCID("IDC_ADD"));
        Bind(wxEVT_BUTTON, &ShaderManagerDialog::OnConfig, this, XRCID("IDC_CONFIG"));
        Bind(wxEVT_BUTTON, &ShaderManagerDialog::OnRemove, this, XRCID("IDC_REMOVE"));
        Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { Move(-1); }, XRCID("IDC_MOVE_UP"));
        Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { Move(1); }, XRCID("IDC_MOVE_DOWN"));
        wx_fit_dialog(this);
}

void ShaderManagerDialog::UpdateList(int selection) {
        list->Clear();
        for (size_t c = 0; c < shaders.size(); c++) {
                char path[512];
                wx_strlcpy(path, shaders[c], sizeof(path));
                list->Append(get_filename(path));
        }
        if (selection >= 0 && selection < (int)shaders.size())
                list->SetSelection(selection);
}

void ShaderManagerDialog::SetShaders() {
        for (int i = 0; i < MAX_USER_SHADERS; ++i) {
                if (!strlen(gl3_shader_file[i]))
                        continue;
                bool available = false;
                for (size_t j = 0; j < shaders.size(); ++j) {
                        if (shaders[j] == gl3_shader_file[i])
                                available = true;
                }
                if (!available) {
                        char name[64];
                        get_glslp_name(gl3_shader_file[i], name, sizeof(name));
                        wx_config_free_section(CFG_MACHINE, wxString::Format("GL3 Shaders - %s", name).mb_str());
                }
        }
        for (int i = 0; i < MAX_USER_SHADERS; ++i) {
                if (i < (int)shaders.size())
                        wx_strlcpy(gl3_shader_file[i], shaders[i], sizeof(gl3_shader_file[i]));
                else
                        gl3_shader_file[i][0] = 0;
        }
}

void ShaderManagerDialog::OnOk(wxCommandEvent &event) {
        SetShaders();
        EndModal(wxID_OK);
}

void ShaderManagerDialog::OnApply(wxCommandEvent &event) {
        SetShaders();
        renderer_doreset = 1;
}

void ShaderManagerDialog::OnAdd(wxCommandEvent &event) {
        if (shaders.size() >= MAX_USER_SHADERS) {
                wx_simple_messagebox("Error", "Can't add more shaders.");
                return;
        }

        int c = list->GetSelection();
        wxString file;
        if (!wx_filedialog(this, "Open", c >= 0 ? shaders[c] : wxString(),
                           "GLSL Shaders (*.glslp;*.glsl)|*.glslp;*.glsl|All files (*.*)|*.*", wxEmptyString, true, file))
                return;

        shaders.insert(shaders.begin() + (c + 1), file);
        UpdateList(c + 1);
}

void ShaderManagerDialog::OnConfig(wxCommandEvent &event) {
        int c = list->GetSelection();
        if (c < 0)
                return;
        glslp_t *glsl = glslp_parse(shaders[c].mb_str());
        if (glsl) {
                ShaderConfigDialog::Open(this, glsl);
                glslp_free(glsl);
        }
}

void ShaderManagerDialog::OnRemove(wxCommandEvent &event) {
        int c = list->GetSelection();
        if (c < 0)
                return;
        shaders.erase(shaders.begin() + c);
        UpdateList(c > 0 ? c - 1 : 0);
}

void ShaderManagerDialog::Move(int direction) {
        int c = list->GetSelection();
        int other = c + direction;
        if (c < 0 || other < 0 || other >= (int)shaders.size())
                return;
        std::swap(shaders[c], shaders[other]);
        UpdateList(other);
}

bool ShaderManagerDialog::Open(wxWindow *parent) {
        ShaderManagerDialog dlg(parent);
        return dlg.ShowModal() == wxID_OK;
}
