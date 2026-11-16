#include "wx-hdsize.h"

HdSizeDialog::HdSizeDialog(wxWindow *parent, HdGeometry &geometry) : geometry(geometry) {
        wxXmlResource::Get()->LoadDialog(this, parent, "HdSizeDlg");
        spt = wx_ctrl<wxTextCtrl>(this, "IDC_EDIT1");
        hpc = wx_ctrl<wxTextCtrl>(this, "IDC_EDIT2");
        cyl = wx_ctrl<wxTextCtrl>(this, "IDC_EDIT3");
        size = wx_ctrl<wxStaticText>(this, "IDC_TEXT1");
        wxComboBox *type = wx_ctrl<wxComboBox>(this, "IDC_HDTYPE");

        AppendHdTypes(type);
        wx_combobox_select(type, geometry.type);
        spt->ChangeValue(wxString::Format("%i", geometry.spt));
        hpc->ChangeValue(wxString::Format("%i", geometry.hpc));
        cyl->ChangeValue(wxString::Format("%i", geometry.cyl));
        UpdateSize();

        Bind(wxEVT_BUTTON, &HdSizeDialog::OnOk, this, wxID_OK);
        Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_CANCEL); }, wxID_CANCEL);
        spt->Bind(wxEVT_TEXT, [this](wxCommandEvent &) { UpdateSize(); });
        hpc->Bind(wxEVT_TEXT, [this](wxCommandEvent &) { UpdateSize(); });
        cyl->Bind(wxEVT_TEXT, [this](wxCommandEvent &) { UpdateSize(); });

        wx_fit_dialog(this);
}

void HdSizeDialog::UpdateSize() {
        size->SetLabel(SizeText(ParseInt(cyl->GetValue()), ParseInt(hpc->GetValue()), ParseInt(spt->GetValue()), "mb"));
}

void HdSizeDialog::OnOk(wxCommandEvent &event) {
        HdGeometry g = geometry;
        g.spt = ParseInt(spt->GetValue());
        g.hpc = ParseInt(hpc->GetValue());
        g.cyl = ParseInt(cyl->GetValue());
        if (!ValidateGeometry(this, g))
                return;
        geometry = g;
        EndModal(wxID_OK);
}
