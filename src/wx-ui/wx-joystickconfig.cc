#include "wx-joystickconfig.h"

#include <strings.h>
#include <vector>

extern "C" {
#include "ibm.h"
#include "gameport.h"
#include "plat-joystick.h"
}

JoystickConfigDialog::JoystickConfigDialog(wxWindow *parent, int joy_nr, int type) : wxDialog(parent, wxID_ANY,
                                           "Joystick Configuration"), joy_nr(joy_nr), type(type) {
        wxFlexGridSizer *root = new wxFlexGridSizer(0, 1, 0, 0);
        root->SetFlexibleDirection(wxBOTH);
        root->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);
        SetSizer(root);

        wxFlexGridSizer *sizer = new wxFlexGridSizer(0, 2, 0, 0);
        sizer->SetFlexibleDirection(wxBOTH);
        sizer->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);
        sizer->AddGrowableCol(1);
        root->Add(sizer, 1, wxEXPAND, 5);

        device = AddCombo(sizer, "Device:");
        device->Append("None");
        for (int c = 0; c < joysticks_present; c++)
                device->Append(plat_joystick_state[c].name);

        for (int c = 0; c < joystick_get_axis_count(type); c++)
                axes.push_back(AddCombo(sizer, wxString::Format("Axis %i:", c)));
        for (int c = 0; c < joystick_get_button_count(type); c++)
                buttons.push_back(AddCombo(sizer, wxString::Format("Button %i:", c)));
        for (int c = 0; c < joystick_get_pov_count(type) * 2; c++)
                povs.push_back(AddCombo(sizer, wxString::Format("POV %i:", c)));

        wxBoxSizer *okCancelSizer = new wxBoxSizer(wxHORIZONTAL);
        root->Add(okCancelSizer, 1, wxEXPAND, 5);
        okCancelSizer->Add(0, 0, 1, wxEXPAND, 5);
        okCancelSizer->Add(new wxButton(this, wxID_OK), 0, wxALL, 5);
        okCancelSizer->Add(new wxButton(this, wxID_CANCEL), 0, wxALL, 5);

        InitSelections();

        device->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent &) { Rebuild(); });
        Bind(wxEVT_BUTTON, &JoystickConfigDialog::OnOk, this, wxID_OK);
        Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_CANCEL); }, wxID_CANCEL);

        Fit();
}

wxComboBox *JoystickConfigDialog::AddCombo(wxFlexGridSizer *sizer, const wxString &label) {
        sizer->Add(new wxStaticText(this, wxID_ANY, label), 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
        wxBoxSizer *comboSizer = new wxBoxSizer(wxHORIZONTAL);
        sizer->Add(comboSizer, 1, wxEXPAND, 5);
        wxComboBox *cb = new wxComboBox(this, wxID_ANY);
        cb->SetEditable(false);
        comboSizer->Add(cb, 1, wxALL, 5);
        return cb;
}

void JoystickConfigDialog::AppendPovs(wxComboBox *cb, plat_joystick_t *joystick) {
        for (int d = 0; d < joystick->nr_povs; d++) {
                cb->Append(wxString::Format("%s (X axis)", joystick->pov[d].name));
                cb->Append(wxString::Format("%s (Y axis)", joystick->pov[d].name));
        }
}

void JoystickConfigDialog::AppendAxes(wxComboBox *cb, plat_joystick_t *joystick) {
        for (int d = 0; d < joystick->nr_axes; d++)
                cb->Append(joystick->axis[d].name);
}

void JoystickConfigDialog::Rebuild() {
        static const char *axis_strings[] = {"X Axis", "Y Axis", "Z Axis"};
        const int axis_strings_max = (int)(sizeof(axis_strings) / sizeof(axis_strings[0]));
        int joystick = device->GetSelection();
        plat_joystick_t *state = joystick > 0 ? &plat_joystick_state[joystick - 1] : NULL;

        for (size_t c = 0; c < axes.size(); c++) {
                wx_combobox_clear(axes[c]);
                if (state) {
                        int sel = c;
                        AppendAxes(axes[c], state);
                        for (int d = 0; d < state->nr_axes; d++) {
                                if ((int)c < axis_strings_max && !strcasecmp(axis_strings[c], state->axis[d].name))
                                        sel = d;
                        }
                        AppendPovs(axes[c], state);
                        wx_combobox_select(axes[c], sel);
                }
                axes[c]->Enable(state != NULL);
        }

        for (size_t c = 0; c < buttons.size(); c++) {
                wx_combobox_clear(buttons[c]);
                if (state) {
                        for (int d = 0; d < state->nr_buttons; d++)
                                buttons[c]->Append(state->button[d].name);
                        wx_combobox_select(buttons[c], c);
                }
                buttons[c]->Enable(state != NULL);
        }

        for (size_t c = 0; c < povs.size(); c++) {
                wx_combobox_clear(povs[c]);
                if (state) {
                        AppendPovs(povs[c], state);
                        AppendAxes(povs[c], state);
                        wx_combobox_select(povs[c], c);
                }
                povs[c]->Enable(state != NULL);
        }
}

int JoystickConfigDialog::PovIndex(int mapping, int other_count) {
        if (mapping & POV_X)
                return (mapping & 3) * 2;
        if (mapping & POV_Y)
                return (mapping & 3) * 2 + 1;
        return mapping + other_count;
}

void JoystickConfigDialog::InitSelections() {
        joystick_t *state = &joystick_state[joy_nr];
        wx_combobox_select(device, state->plat_joystick_nr);
        Rebuild();

        if (!state->plat_joystick_nr)
                return;

        plat_joystick_t *plat = &plat_joystick_state[state->plat_joystick_nr - 1];
        for (size_t c = 0; c < axes.size(); c++) {
                int mapping = state->axis_mapping[c];
                if (mapping & (POV_X | POV_Y))
                        wx_combobox_select(axes[c], plat->nr_axes + PovIndex(mapping, 0));
                else
                        wx_combobox_select(axes[c], mapping);
        }
        for (size_t c = 0; c < buttons.size(); c++)
                wx_combobox_select(buttons[c], state->button_mapping[c]);
        for (size_t c = 0; c < povs.size(); c++)
                wx_combobox_select(povs[c], PovIndex(state->pov_mapping[c / 2][c & 1], plat->nr_povs * 2));
}

int JoystickConfigDialog::GetAxis(wxComboBox *cb) {
        int axis_sel = cb->GetSelection();
        int nr_axes = plat_joystick_state[joystick_state[joy_nr].plat_joystick_nr - 1].nr_axes;

        if (axis_sel < nr_axes)
                return axis_sel;

        axis_sel -= nr_axes;
        return ((axis_sel & 1) ? POV_Y : POV_X) | (axis_sel >> 1);
}

int JoystickConfigDialog::GetPov(wxComboBox *cb) {
        int axis_sel = cb->GetSelection();
        int nr_povs = plat_joystick_state[joystick_state[joy_nr].plat_joystick_nr - 1].nr_povs * 2;

        if (axis_sel < nr_povs)
                return ((axis_sel & 1) ? POV_Y : POV_X) | (axis_sel >> 1);

        return axis_sel - nr_povs;
}

void JoystickConfigDialog::OnOk(wxCommandEvent &event) {
        joystick_t *state = &joystick_state[joy_nr];
        state->plat_joystick_nr = device->GetSelection();

        if (state->plat_joystick_nr) {
                for (size_t c = 0; c < axes.size(); c++)
                        state->axis_mapping[c] = GetAxis(axes[c]);
                for (size_t c = 0; c < buttons.size(); c++)
                        state->button_mapping[c] = buttons[c]->GetSelection();
                for (size_t c = 0; c < povs.size(); c++)
                        state->pov_mapping[c / 2][c & 1] = GetPov(povs[c]);
        }
        EndModal(wxID_OK);
}

void JoystickConfigDialog::Open(wxWindow *parent, int joy_nr, int type) {
        JoystickConfigDialog dlg(parent, joy_nr, type);
        dlg.ShowModal();
}
