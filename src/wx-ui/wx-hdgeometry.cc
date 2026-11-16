#include "wx-hdgeometry.h"

#include <stdio.h>

const HdGeometryDialog::HdType HdGeometryDialog::hd_types[NUM_HD_TYPES] = {
        {306, 4}, {615, 4}, {615, 6}, {940, 8}, {940, 6}, {615, 4}, {462, 8}, {733, 5},
        {900, 15}, {820, 3}, {855, 5}, {855, 7}, {306, 8}, {733, 7}, {0, 0}, {612, 4},
        {977, 5}, {977, 7}, {1024, 7}, {733, 5}, {733, 7}, {733, 5}, {306, 4}, {925, 7},
        {925, 9}, {754, 7}, {754, 11}, {699, 7}, {823, 10}, {918, 7}, {1024, 11}, {1024, 15},
        {1024, 5}, {612, 2}, {1024, 9}, {1024, 8}, {615, 8}, {987, 3}, {462, 7}, {820, 6},
        {977, 5}, {981, 5}, {830, 7}, {830, 10}, {917, 15}, {1224, 15}};

int HdGeometryDialog::ParseInt(const wxString &s) {
        int value = 0;
        sscanf(s.mb_str(), "%i", &value);
        return value;
}

wxString HdGeometryDialog::SizeText(int cyl, int hpc, int spt, const char *suffix) {
        return wxString::Format("%i%s", (int)((((uint64_t)cyl * (uint64_t)hpc * (uint64_t)spt) * 512) / 1024 / 1024), suffix);
}

void HdGeometryDialog::AppendHdTypes(wxComboBox *cb) {
        cb->Append("Custom type");
        for (int c = 0; c < NUM_HD_TYPES; c++)
                cb->Append(wxString::Format("Type %02i : cylinders=%i, heads=%i, size=%iMB", c + 1, hd_types[c].cylinders,
                                            hd_types[c].heads,
                                            (hd_types[c].cylinders * hd_types[c].heads * 17 * 512) / (1024 * 1024)));
}

bool HdGeometryDialog::ValidateGeometry(wxWindow *parent, const HdGeometry &g) {
        if (g.spt > 63) {
                wx_messagebox(parent, "Drive has too many sectors (maximum is 63)", "PCem error", wxOK);
                return false;
        }
        if (g.hpc > 16) {
                wx_messagebox(parent, "Drive has too many heads (maximum is 16)", "PCem error", wxOK);
                return false;
        }
        if (g.cyl > MAX_CYLINDERS) {
                wx_messagebox(parent, wxString::Format("Drive has too many cylinders (maximum is %i)", MAX_CYLINDERS),
                              "PCem error", wxOK);
                return false;
        }
        return true;
}

void HdGeometryDialog::AdjustVhdGeometryForPcem(HdGeometry &g) {
        if (g.spt <= 63)
                return;

        int desired_sectors = g.cyl * g.hpc * g.spt;
        if (desired_sectors > 267321600)
                desired_sectors = 267321600;

        int remainder = desired_sectors % 85680;
        if (remainder > 0)
                desired_sectors -= remainder;

        g.cyl = desired_sectors / (16 * 63);
        g.hpc = 16;
        g.spt = 63;
}
