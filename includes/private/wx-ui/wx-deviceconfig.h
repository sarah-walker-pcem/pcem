#ifndef _PCEM_WX_DEVICECONFIG_H_
#define _PCEM_WX_DEVICECONFIG_H_

#include "wx-utils.h"

#include <vector>

struct device_t;
struct device_config_t;

class DeviceConfigDialog : public wxDialog {
    public:
        static void Open(wxWindow *parent, struct device_t *device);
        DeviceConfigDialog(wxWindow *parent, device_t *device);

    private:
        struct Entry {
                device_config_t *config;
                wxCheckBox *check;
                wxComboBox *combo;
        };

        device_t *device;
        std::vector<Entry> entries;

        const char *Section(device_config_t *config);
        int GetValue(device_config_t *config);
        int GetNewValue(const Entry &entry);
        void OnOk(wxCommandEvent &event);
};

#endif /* _PCEM_WX_DEVICECONFIG_H_ */
