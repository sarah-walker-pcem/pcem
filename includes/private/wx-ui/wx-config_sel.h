#ifndef _PCEM_WX_CONFIG_SEL_H_
#define _PCEM_WX_CONFIG_SEL_H_

#include "wx-utils.h"

class ConfigSelectionDialog : public wxDialog {
    public:
        static bool Open(wxWindow *parent, bool inited);

        ConfigSelectionDialog(wxWindow *parent);

    private:
        static int active_config;

        static bool ValidConfigName(const wxString &name);
        static wxString ConfigPath(const wxString &name);

        wxListBox *list;

        void UpdateList();
        bool AskName(const wxString &title, wxString &name);
        void OnOk(wxCommandEvent &event);
        void OnNew(wxCommandEvent &event);
        void OnConfigure(wxCommandEvent &event);
        void OnRename(wxCommandEvent &event);
        void OnCopy(wxCommandEvent &event);
        void OnDelete(wxCommandEvent &event);
};

#endif /* _PCEM_WX_CONFIG_SEL_H_ */
