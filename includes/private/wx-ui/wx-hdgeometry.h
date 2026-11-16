#ifndef _PCEM_WX_HDGEOMETRY_H_
#define _PCEM_WX_HDGEOMETRY_H_

#include "wx-utils.h"

#define MAX_CYLINDERS 265264
#define NUM_HD_TYPES 46

class HdGeometryDialog : public wxDialog {
    public:
        struct HdType {
                int cylinders;
                int heads;
        };

        struct HdGeometry {
                int spt;
                int hpc;
                int cyl;
                int type;
        };

        static const HdType hd_types[NUM_HD_TYPES];

        static int ParseInt(const wxString &s);
        static wxString SizeText(int cyl, int hpc, int spt, const char *suffix);
        static void AppendHdTypes(wxComboBox *cb);
        static bool ValidateGeometry(wxWindow *parent, const HdGeometry &g);
        static void AdjustVhdGeometryForPcem(HdGeometry &g);
};

#endif /* _PCEM_WX_HDGEOMETRY_H_ */
