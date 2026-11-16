#ifndef _PCEM_WX_HOSTCONFIG_H_
#define _PCEM_WX_HOSTCONFIG_H_

#include "wx-utils.h"

#include <string>
#include <vector>

class HostConfigDialog : public wxDialog {
    public:
        static bool Open(wxWindow *parent);
        HostConfigDialog(wxWindow *parent);

    private:
        wxComboBox *type;
        wxComboBox *device;
        std::vector<std::string> dev_names;

#ifdef USE_PCAP_NETWORKING
        static bool GetNetworkName(const char *dev_name, wxString &name);
        void PopulatePcapDevices();
#endif

        void OnOk(wxCommandEvent &event);
};

#endif /* _PCEM_WX_HOSTCONFIG_H_ */
