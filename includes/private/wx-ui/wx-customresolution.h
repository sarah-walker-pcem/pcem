#ifndef _PCEM_WX_CUSTOMRESOLUTION_H_
#define _PCEM_WX_CUSTOMRESOLUTION_H_

#include "wx-utils.h"

#include <wx/spinctrl.h>

class CustomResolutionDialog : public wxDialog {
    public:
        static bool Open(wxWindow *parent);
        CustomResolutionDialog(wxWindow *parent);

    private:
        wxSpinCtrl *width;
        wxSpinCtrl *height;

        void OnOk(wxCommandEvent &event);
};

#endif /* _PCEM_WX_CUSTOMRESOLUTION_H_ */
