#include "wx-customresolution.h"

#include <SDL2/SDL.h>

extern "C" {
#include "ibm.h"
#include "video.h"
#include "sdl2-video.h"
}

CustomResolutionDialog::CustomResolutionDialog(wxWindow *parent) {
        wxXmlResource::Get()->LoadDialog(this, parent, "CustomResolutionDlg");
        width = wx_ctrl<wxSpinCtrl>(this, "IDC_WIDTH");
        height = wx_ctrl<wxSpinCtrl>(this, "IDC_HEIGHT");
        width->SetValue(custom_resolution_width);
        height->SetValue(custom_resolution_height);
        Bind(wxEVT_BUTTON, &CustomResolutionDialog::OnOk, this, wxID_OK);
        wx_fit_dialog(this);
}

void CustomResolutionDialog::OnOk(wxCommandEvent &event) {
        custom_resolution_width = width->GetValue();
        custom_resolution_height = height->GetValue();
        EndModal(wxID_OK);
}

bool CustomResolutionDialog::Open(wxWindow *parent) {
        CustomResolutionDialog dlg(parent);
        return dlg.ShowModal() == wxID_OK;
}
