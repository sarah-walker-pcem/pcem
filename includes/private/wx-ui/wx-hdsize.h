#ifndef _PCEM_WX_HDSIZE_H_
#define _PCEM_WX_HDSIZE_H_

#include "wx-hdgeometry.h"

class HdSizeDialog : public HdGeometryDialog {
    public:
        HdSizeDialog(wxWindow *parent, HdGeometry &geometry);

    private:
        HdGeometry &geometry;
        wxTextCtrl *spt, *hpc, *cyl;
        wxStaticText *size;

        void UpdateSize();
        void OnOk(wxCommandEvent &event);
};

#endif /* _PCEM_WX_HDSIZE_H_ */
