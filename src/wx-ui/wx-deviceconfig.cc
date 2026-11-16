#include "wx-deviceconfig.h"
#include "wx-config.h"

#include <vector>

extern "C" {
#include "ibm.h"
#include "device.h"
#include "plat-midi.h"
}

DeviceConfigDialog::DeviceConfigDialog(wxWindow *parent, device_t *device) : wxDialog(parent, wxID_ANY, "Device Configuration"),
                                       device(device) {
        wxFlexGridSizer *root = new wxFlexGridSizer(0, 1, 0, 0);
        root->SetFlexibleDirection(wxBOTH);
        root->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);
        SetSizer(root);

        wxFlexGridSizer *sizer = new wxFlexGridSizer(0, 2, 0, 0);
        sizer->SetFlexibleDirection(wxBOTH);
        sizer->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);
        sizer->AddGrowableCol(1);
        root->Add(sizer, 1, wxEXPAND, 5);

        for (device_config_t *config = device->config; config->type != -1; config++) {
                Entry entry = {config, NULL, NULL};

                switch (config->type) {
                case CONFIG_BINARY:
                        entry.check = new wxCheckBox(this, wxID_ANY, config->description);
                        entry.check->SetValue(GetValue(config) != 0);
                        sizer->Add(0, 0, 1, wxEXPAND, 5);
                        sizer->Add(entry.check, 0, wxALL, 5);
                        break;

                case CONFIG_SELECTION:
                case CONFIG_MIDI: {
                        int num_midi = midi_get_num_devs();
                        if (config->type == CONFIG_MIDI && num_midi == 0)
                                continue;

                        sizer->Add(new wxStaticText(this, wxID_ANY, wxString::Format("%s:", config->description)), 0,
                                   wxALL | wxALIGN_CENTER_VERTICAL, 5);
                        wxBoxSizer *comboSizer = new wxBoxSizer(wxHORIZONTAL);
                        sizer->Add(comboSizer, 1, wxEXPAND, 5);
                        entry.combo = new wxComboBox(this, wxID_ANY);
                        entry.combo->SetEditable(false);
                        comboSizer->Add(entry.combo, 1, wxALL, 5);

                        int value = GetValue(config);
                        if (config->type == CONFIG_SELECTION) {
                                for (int c = 0; config->selection[c].description[0]; c++) {
                                        entry.combo->Append(config->selection[c].description);
                                        if (value == config->selection[c].value)
                                                entry.combo->SetSelection(c);
                                }
                        } else {
                                for (int c = 0; c < num_midi; c++) {
                                        char s[100];
                                        midi_get_dev_name(c, s);
                                        entry.combo->Append(s);
                                        if (value == c)
                                                entry.combo->SetSelection(c);
                                }
                        }
                        break;
                }

                default:
                        continue;
                }

                entries.push_back(entry);
        }

        wxBoxSizer *okCancelSizer = new wxBoxSizer(wxHORIZONTAL);
        root->Add(okCancelSizer, 1, wxEXPAND, 5);
        okCancelSizer->Add(0, 0, 1, wxEXPAND, 5);
        okCancelSizer->Add(new wxButton(this, wxID_OK), 0, wxALL, 5);
        okCancelSizer->Add(new wxButton(this, wxID_CANCEL), 0, wxALL, 5);

        Bind(wxEVT_BUTTON, &DeviceConfigDialog::OnOk, this, wxID_OK);
        Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_CANCEL); }, wxID_CANCEL);

        Fit();
}

const char *DeviceConfigDialog::Section(device_config_t *config) { return config->type == CONFIG_MIDI ? NULL : device->name; }

int DeviceConfigDialog::GetValue(device_config_t *config) {
        return wx_config_get_int(CFG_MACHINE, Section(config), config->name, config->default_int);
}

int DeviceConfigDialog::GetNewValue(const Entry &entry) {
        if (entry.check)
                return entry.check->GetValue();
        int c = entry.combo->GetSelection();
        if (entry.config->type == CONFIG_MIDI)
                return c;
        return entry.config->selection[c > 0 ? c : 0].value;
}

void DeviceConfigDialog::OnOk(wxCommandEvent &event) {
        bool changed = false;
        for (size_t i = 0; i < entries.size(); i++) {
                if (GetNewValue(entries[i]) != GetValue(entries[i].config))
                        changed = true;
        }

        if (!changed || (has_been_inited && !confirm())) {
                EndModal(wxID_CANCEL);
                return;
        }

        for (size_t i = 0; i < entries.size(); i++)
                wx_config_set_int(CFG_MACHINE, Section(entries[i].config), entries[i].config->name, GetNewValue(entries[i]));

        if (has_been_inited) {
                saveconfig(NULL);
                resetpchard();
        }
        EndModal(wxID_OK);
}

void DeviceConfigDialog::Open(wxWindow *parent, device_t *device) {
        if (!device)
                return;
        DeviceConfigDialog dlg(parent, device);
        dlg.ShowModal();
}
