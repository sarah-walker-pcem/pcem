#ifndef _PCEM_WX_SHADERCONFIG_H_
#define _PCEM_WX_SHADERCONFIG_H_

#include "wx-utils.h"

#include <wx/spinctrl.h>

#include <vector>

struct glslp_t;

class ShaderConfigDialog : public wxDialog {
    public:
        static void Open(wxWindow *parent, struct glslp_t *glsl);
        ShaderConfigDialog(wxWindow *parent, glslp_t *glsl);
        void SaveConfig();

    private:
        glslp_t *glsl;
        std::vector<wxSpinCtrlDouble *> spins;

        wxString Section();
        void LoadConfig();
        void Apply();
        void Reset();
};

#endif /* _PCEM_WX_SHADERCONFIG_H_ */
