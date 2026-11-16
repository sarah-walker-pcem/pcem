#ifndef _PCEM_WX_SHADER_MAN_H_
#define _PCEM_WX_SHADER_MAN_H_

#include "wx-utils.h"

#include <vector>

class ShaderManagerDialog : public wxDialog {
    public:
        static bool Open(wxWindow *parent);
        ShaderManagerDialog(wxWindow *parent);

    private:
        wxListBox *list;
        std::vector<wxString> shaders;

        void UpdateList(int selection);
        void SetShaders();
        void OnOk(wxCommandEvent &event);
        void OnApply(wxCommandEvent &event);
        void OnAdd(wxCommandEvent &event);
        void OnConfig(wxCommandEvent &event);
        void OnRemove(wxCommandEvent &event);
        void Move(int direction);
};

#endif /* _PCEM_WX_SHADER_MAN_H_ */
