#ifndef _PCEM_WX_HDNEW_H_
#define _PCEM_WX_HDNEW_H_

#include "wx-hdgeometry.h"

#include <functional>
#include <stdint.h>

struct MVHDGeom;

class HdNewDialog : public HdGeometryDialog {
    public:
        wxString name;
        HdGeometry geometry;

        HdNewDialog(wxWindow *parent);

    private:
        static int ProgressDialog(wxWindow *parent, const wxString &title, const wxString &message, std::function<int()> work,
                                  int range, volatile int *pos);

        static volatile int create_drive_pos;

        static void VhdProgressCallback(uint32_t current_sector, uint32_t total_sectors);
        static void AdjustPcemGeometryForVhd(HdGeometry &g, MVHDGeom *vhd_geometry);
        static MVHDGeom VhdGeometryFor(const HdGeometry &g);
        static bool CreateDriveVhdFixed(const char *path, HdGeometry &g);
        static bool CreateDriveVhdDynamic(const char *path, HdGeometry &g, int blocksize);
        static bool CreateDriveVhdDiff(const char *path, const char *parent_path, HdGeometry &g, int blocksize);

        wxTextCtrl *spt, *hpc, *cyl, *size, *file;
        wxComboBox *type, *format, *blocksize;
        wxStaticText *blocksize_label;

        void SetGeometryText(const wxString &s, const wxString &h, const wxString &c, const wxString &sz);
        void EnableGeometry(bool enable);
        void OnGeometryChanged(wxCommandEvent &event);
        void OnSizeChanged(wxCommandEvent &event);
        void OnTypeChanged(wxCommandEvent &event);
        void OnFormatChanged(wxCommandEvent &event);
        void OnFile(wxCommandEvent &event);
        void OnOk(wxCommandEvent &event);
};

#endif /* _PCEM_WX_HDNEW_H_ */
