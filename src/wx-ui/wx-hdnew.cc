#include "wx-hdnew.h"
#include "wx-frontend.h"

#include <cstdint>

#include <wx/progdlg.h>

#include <atomic>
#include <stdio.h>
#include <thread>

extern "C" {
#include "minivhd/minivhd.h"
}

volatile int HdNewDialog::create_drive_pos = 0;

int HdNewDialog::ProgressDialog(wxWindow *parent, const wxString &title, const wxString &message, std::function<int()> work,
                                int range, volatile int *pos) {
        std::atomic<bool> active(true);
        int result = 0;
        std::thread worker([&]() {
                result = work();
                active = false;
        });

        wxProgressDialog dlg(title, message, range, parent, wxPD_APP_MODAL | wxPD_SMOOTH | wxPD_AUTO_HIDE);
        while (active) {
                int value = *pos;
                dlg.Update(value < range ? value : range - 1);
                wxMilliSleep(50);
        }
        worker.join();
        return result;
}

void HdNewDialog::VhdProgressCallback(uint32_t current_sector, uint32_t total_sectors) { create_drive_pos = (int)current_sector; }

void HdNewDialog::AdjustPcemGeometryForVhd(HdGeometry &g, MVHDGeom *vhd_geometry) {
        if (g.cyl <= 65535)
                return;

        int desired_sectors = g.cyl * g.hpc * g.spt;
        if (desired_sectors > 267321600)
                desired_sectors = 267321600;

        int remainder = desired_sectors % 85680;
        if (remainder > 0)
                desired_sectors += 85680 - remainder;

        g.cyl = desired_sectors / (16 * 63);
        g.hpc = 16;
        g.spt = 63;

        vhd_geometry->cyl = desired_sectors / (16 * 255);
        vhd_geometry->heads = 16;
        vhd_geometry->spt = 255;
}

MVHDGeom HdNewDialog::VhdGeometryFor(const HdGeometry &g) {
        MVHDGeom geometry;
        geometry.cyl = g.cyl;
        geometry.heads = g.hpc;
        geometry.spt = g.spt;
        return geometry;
}

bool HdNewDialog::CreateDriveVhdFixed(const char *path, HdGeometry &g) {
        MVHDGeom geometry = VhdGeometryFor(g);
        AdjustPcemGeometryForVhd(g, &geometry);

        int vhd_error = 0;
        MVHDMeta *vhd = mvhd_create_fixed(path, geometry, &vhd_error, VhdProgressCallback);
        if (!vhd)
                return false;
        mvhd_close(vhd);
        return true;
}

bool HdNewDialog::CreateDriveVhdDynamic(const char *path, HdGeometry &g, int blocksize) {
        MVHDGeom geometry = VhdGeometryFor(g);
        AdjustPcemGeometryForVhd(g, &geometry);

        MVHDCreationOptions options;
        memset(&options, 0, sizeof(options));
        options.block_size_in_sectors = blocksize;
        options.path = (char *)path;
        options.geometry = geometry;
        options.type = MVHD_TYPE_DYNAMIC;

        int vhd_error = 0;
        MVHDMeta *vhd = mvhd_create_ex(options, &vhd_error);
        if (!vhd)
                return false;
        mvhd_close(vhd);
        return true;
}

bool HdNewDialog::CreateDriveVhdDiff(const char *path, const char *parent_path, HdGeometry &g, int blocksize) {
        MVHDCreationOptions options;
        memset(&options, 0, sizeof(options));
        options.block_size_in_sectors = blocksize;
        options.path = (char *)path;
        options.parent_path = (char *)parent_path;
        options.type = MVHD_TYPE_DIFF;

        int vhd_error = 0;
        MVHDMeta *vhd = mvhd_create_ex(options, &vhd_error);
        if (!vhd)
                return false;

        MVHDGeom vhd_geom = mvhd_get_geometry(vhd);
        if (vhd_geom.spt > 63) {
                g.cyl = mvhd_calc_size_sectors(&vhd_geom) / (16 * 63);
                g.hpc = 16;
                g.spt = 63;
        } else {
                g.cyl = vhd_geom.cyl;
                g.hpc = vhd_geom.heads;
                g.spt = vhd_geom.spt;
        }
        mvhd_close(vhd);
        return true;
}

HdNewDialog::HdNewDialog(wxWindow *parent) {
        wxXmlResource::Get()->LoadDialog(this, parent, "HdNewDlg");
        spt = wx_ctrl<wxTextCtrl>(this, "IDC_EDIT1");
        hpc = wx_ctrl<wxTextCtrl>(this, "IDC_EDIT2");
        cyl = wx_ctrl<wxTextCtrl>(this, "IDC_EDIT3");
        size = wx_ctrl<wxTextCtrl>(this, "IDC_EDIT4");
        file = wx_ctrl<wxTextCtrl>(this, "IDC_EDITC");
        type = wx_ctrl<wxComboBox>(this, "IDC_HDTYPE");
        format = wx_ctrl<wxComboBox>(this, "IDC_HDFORMAT");
        blocksize = wx_ctrl<wxComboBox>(this, "IDC_HDBLOCKSIZE");
        blocksize_label = wx_ctrl<wxStaticText>(this, "IDC_HDBLOCKSIZELABEL");

        SetGeometryText("63", "16", "511", wxString::Format("%i", (((511 * 16 * 63) * 512) / 1024) / 1024));
        file->ChangeValue("");

        AppendHdTypes(type);
        type->SetSelection(0);

        format->Append("Raw (.img)");
        format->Append("Fixed-size VHD (.vhd)");
        format->Append("Dynamic-size VHD (.vhd)");
        format->Append("Differencing VHD (.vhd)");
        format->SetSelection(0);

        blocksize->Append("Large blocks (2 MB)");
        blocksize->Append("Small blocks (512 KB)");
        blocksize->SetSelection(0);
        blocksize->Show(false);
        blocksize_label->SetLabel(" ");

        Bind(wxEVT_BUTTON, &HdNewDialog::OnOk, this, wxID_OK);
        Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_CANCEL); }, wxID_CANCEL);
        Bind(wxEVT_BUTTON, &HdNewDialog::OnFile, this, XRCID("IDC_CFILE"));
        spt->Bind(wxEVT_TEXT, &HdNewDialog::OnGeometryChanged, this);
        hpc->Bind(wxEVT_TEXT, &HdNewDialog::OnGeometryChanged, this);
        cyl->Bind(wxEVT_TEXT, &HdNewDialog::OnGeometryChanged, this);
        size->Bind(wxEVT_TEXT, &HdNewDialog::OnSizeChanged, this);
        type->Bind(wxEVT_COMBOBOX, &HdNewDialog::OnTypeChanged, this);
        format->Bind(wxEVT_COMBOBOX, &HdNewDialog::OnFormatChanged, this);

        wx_fit_dialog(this);
}

void HdNewDialog::SetGeometryText(const wxString &s, const wxString &h, const wxString &c, const wxString &sz) {
        spt->ChangeValue(s);
        hpc->ChangeValue(h);
        cyl->ChangeValue(c);
        size->ChangeValue(sz);
}

void HdNewDialog::EnableGeometry(bool enable) {
        spt->Enable(enable);
        hpc->Enable(enable);
        cyl->Enable(enable);
        size->Enable(enable);
        type->Enable(enable);
}

void HdNewDialog::OnGeometryChanged(wxCommandEvent &event) {
        int s = ParseInt(spt->GetValue());
        int h = ParseInt(hpc->GetValue());
        int c = ParseInt(cyl->GetValue());

        size->ChangeValue(SizeText(c, h, s, ""));

        int hd_type = 0;
        for (int t = 0; t < NUM_HD_TYPES; t++) {
                if (s == 17 && h == hd_types[t].heads && c == hd_types[t].cylinders) {
                        hd_type = t + 1;
                        break;
                }
        }
        type->SetSelection(hd_type);
}

void HdNewDialog::OnSizeChanged(wxCommandEvent &event) {
        int mb = ParseInt(size->GetValue());
        int tracks = (int)(((int64_t)mb * 1024 * 1024) / (16 * 63 * 512));
        spt->ChangeValue("63");
        hpc->ChangeValue("16");
        cyl->ChangeValue(wxString::Format("%i", tracks));
}

void HdNewDialog::OnTypeChanged(wxCommandEvent &event) {
        int hd_type = type->GetSelection();
        if (hd_type <= 0)
                return;
        const int c = hd_types[hd_type - 1].cylinders;
        const int h = hd_types[hd_type - 1].heads;
        SetGeometryText("17", wxString::Format("%i", h), wxString::Format("%i", c), SizeText(c, h, 17, ""));
}

void HdNewDialog::OnFormatChanged(wxCommandEvent &event) {
        int hd_format = format->GetSelection();
        if (hd_format == 3) {
                SetGeometryText("(use parent)", "(use parent)", "(use parent)", "(use parent)");
                type->SetSelection(0);
                EnableGeometry(false);
        } else if (spt->GetValue().StartsWith("(use parent)")) {
                SetGeometryText("63", "16", "511", "251");
                EnableGeometry(true);
        }

        bool show_blocksize = hd_format == 2 || hd_format == 3;
        blocksize_label->SetLabel(show_blocksize ? "Block size:" : " ");
        blocksize->Show(show_blocksize);
        Layout();
}

void HdNewDialog::OnFile(wxCommandEvent &event) {
        bool raw = format->GetSelection() == 0;
        wxString path;
        if (wx_getsfile(this,
                        raw ? "Raw Hard disc image (*.img)|*.img|All files (*.*)|*.*"
                            : "VHD file (*.vhd)|*.vhd|All files (*.*)|*.*",
                        "", raw ? "img" : "vhd", path))
                file->ChangeValue(path);
}

void HdNewDialog::OnOk(wxCommandEvent &event) {
        name = file->GetValue();
        if (name.empty()) {
                wx_messagebox(this, "Please enter a valid filename", "PCem error", wxOK);
                return;
        }

        geometry.spt = ParseInt(spt->GetValue());
        geometry.hpc = ParseInt(hpc->GetValue());
        geometry.cyl = ParseInt(cyl->GetValue());
        geometry.type = 0;
        if (!ValidateGeometry(this, geometry))
                return;

        int hd_format = format->GetSelection();
        int hd_blocksize = blocksize->GetSelection() == 0 ? MVHD_BLOCK_LARGE : MVHD_BLOCK_SMALL;
        wxCharBuffer path = name.mb_str();
        int sectors = geometry.cyl * geometry.hpc * geometry.spt;

        if (hd_format == 0) {
                FILE *f = fopen(path, "wb");
                if (!f) {
                        wx_messagebox(this, "Can't open file for write", "PCem error", wxOK);
                        return;
                }
                ProgressDialog(
                        this, "PCem", "Creating drive, please wait...",
                        [f, sectors]() {
                                uint8_t buf[512];
                                memset(buf, 0, sizeof(buf));
                                for (int c = 0; c < sectors; c++) {
                                        create_drive_pos = c;
                                        fwrite(buf, sizeof(buf), 1, f);
                                }
                                fclose(f);
                                return 1;
                        },
                        sectors, &create_drive_pos);
        } else if (hd_format == 1) {
                HdGeometry *g = &geometry;
                const char *p = path;
                if (!ProgressDialog(
                            this, "PCem", "Creating drive, please wait...",
                            [p, g]() { return CreateDriveVhdFixed(p, *g) ? 1 : 0; }, sectors, &create_drive_pos)) {
                        wx_messagebox(this, "Can't create VHD", "PCem error", wxOK);
                        return;
                }
        } else if (hd_format == 2) {
                if (!CreateDriveVhdDynamic(path, geometry, hd_blocksize)) {
                        wx_messagebox(this, "Can't create VHD", "PCem error", wxOK);
                        return;
                }
        } else if (hd_format == 3) {
                wxString parent_path;
                if (!wx_getfile(this, "VHD file (*.vhd)|*.vhd|All files (*.*)|*.*", "", parent_path,
                                "Select the parent VHD"))
                        return;
                if (!CreateDriveVhdDiff(path, parent_path.mb_str(), geometry, hd_blocksize)) {
                        wx_messagebox(this, "Can't create VHD", "PCem error", wxOK);
                        return;
                }
                wx_messagebox(this,
                              "Differencing VHD image created.\n\n"
                              "WARNING: Do not open or modify the parent image(s) while this file exists.",
                              "PCem", wxOK);
        }
        if (hd_format != 3)
                wx_messagebox(this, "Drive created, remember to partition and format the new drive.", "PCem", wxOK);

        EndModal(wxID_OK);
}
