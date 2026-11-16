#ifndef _PCEM_WX_JOYSTICKCONFIG_H_
#define _PCEM_WX_JOYSTICKCONFIG_H_

#include "wx-utils.h"

#include <vector>

struct plat_joystick_t;

class JoystickConfigDialog : public wxDialog {
    public:
        static void Open(wxWindow *parent, int joy_nr, int type);
        JoystickConfigDialog(wxWindow *parent, int joy_nr, int type);

    private:
        int joy_nr;
        int type;
        wxComboBox *device;
        std::vector<wxComboBox *> axes;
        std::vector<wxComboBox *> buttons;
        std::vector<wxComboBox *> povs;

        wxComboBox *AddCombo(wxFlexGridSizer *sizer, const wxString &label);
        static void AppendPovs(wxComboBox *cb, plat_joystick_t *joystick);
        static void AppendAxes(wxComboBox *cb, plat_joystick_t *joystick);
        void Rebuild();
        static int PovIndex(int mapping, int other_count);
        void InitSelections();
        int GetAxis(wxComboBox *cb);
        int GetPov(wxComboBox *cb);
        void OnOk(wxCommandEvent &event);
};

#endif /* _PCEM_WX_JOYSTICKCONFIG_H_ */
