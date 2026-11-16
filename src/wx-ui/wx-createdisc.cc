#include "wx-createdisc.h"

#include <stdint.h>
#include <stdio.h>
#include <vector>

CreateDiscDialog::CreateDiscDialog(wxWindow *parent) : wxDialog(parent, wxID_ANY, "Create Disc Image") {
        wxFlexGridSizer *root = new wxFlexGridSizer(0, 1, 0, 0);
        root->SetFlexibleDirection(wxBOTH);
        root->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);
        SetSizer(root);

        wxFlexGridSizer *sizer = new wxFlexGridSizer(0, 1, 0, 0);
        sizer->SetFlexibleDirection(wxBOTH);
        sizer->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);
        root->Add(sizer, 1, wxEXPAND, 5);

        for (int c = 0; c < num_disc_formats; c++) {
                wxBoxSizer *radioSizer = new wxBoxSizer(wxHORIZONTAL);
                sizer->Add(radioSizer, 1, wxEXPAND, 5);
                wxRadioButton *rb = new wxRadioButton(this, wxID_ANY, disc_formats[c].name, wxDefaultPosition,
                                                      wxDefaultSize, c ? 0 : wxRB_GROUP);
                radioSizer->Add(rb, 1, wxALL, 5);
                formats.push_back(rb);
        }

        wxBoxSizer *okCancelSizer = new wxBoxSizer(wxHORIZONTAL);
        root->Add(okCancelSizer, 1, wxEXPAND, 5);
        okCancelSizer->Add(0, 0, 1, wxEXPAND, 5);
        okCancelSizer->Add(new wxButton(this, wxID_OK), 0, wxALL, 5);
        okCancelSizer->Add(new wxButton(this, wxID_CANCEL), 0, wxALL, 5);

        Bind(wxEVT_BUTTON, &CreateDiscDialog::OnOk, this, wxID_OK);
        Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_CANCEL); }, wxID_CANCEL);

        Fit();
}

void CreateDiscDialog::OnOk(wxCommandEvent &event) {
        int c;
        for (c = 0; c < num_disc_formats; c++) {
                if (formats[c]->GetValue())
                        break;
        }

        wxString file;
        if (c < num_disc_formats &&
            wx_filedialog(this, "Save", "", "Disc image (*.img;*.ima)|*.img;*.ima|All files (*.*)|*.*", "img", false, file)) {
                FILE *f = fopen(file.mb_str(), "wb");
                if (f) {
                        uint8_t sector[512];
                        memset(sector, 0, sizeof(sector));
                        for (int d = 0; d < disc_formats[c].nr_sectors; d++)
                                fwrite(sector, sizeof(sector), 1, f);
                        fclose(f);
                }
        }
        EndModal(wxID_OK);
}

const CreateDiscDialog::DiscFormat CreateDiscDialog::disc_formats[] = {
        {"160 kB", 1 * 40 * 8},   {"180 kB", 1 * 40 * 9},   {"320 kB", 2 * 40 * 8},       {"360 kB", 2 * 40 * 9},
        {"720 kB", 2 * 80 * 9},   {"1.2 MB", 2 * 80 * 15},  {"1.44 MB", 2 * 80 * 18},     {"2.88 MB", 2 * 80 * 36},
        {"100 MB (Zip)", 2048 * 96}};

const int CreateDiscDialog::num_disc_formats = sizeof(disc_formats) / sizeof(disc_formats[0]);

void CreateDiscDialog::Open(wxWindow *parent) {
        CreateDiscDialog dlg(parent);
        dlg.ShowModal();
}
