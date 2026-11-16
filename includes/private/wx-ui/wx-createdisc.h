#ifndef _PCEM_WX_CREATEDISC_H_
#define _PCEM_WX_CREATEDISC_H_

#include "wx-utils.h"

#include <vector>

class CreateDiscDialog : public wxDialog {
    public:
        static void Open(wxWindow *parent);

        CreateDiscDialog(wxWindow *parent);

    private:
        struct DiscFormat {
                const char *name;
                int nr_sectors;
        };

        static const DiscFormat disc_formats[];
        static const int num_disc_formats;

        std::vector<wxRadioButton *> formats;

        void OnOk(wxCommandEvent &event);
};

#endif /* _PCEM_WX_CREATEDISC_H_ */
