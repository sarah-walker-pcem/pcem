#include "wx-config_sel.h"
#include "wx-config.h"
#include "wx-hostconfig.h"

#include <wx/dir.h>
#include <wx/filename.h>

extern "C" {
#include "ibm.h"
#include "config.h"
#include "paths.h"
}

bool ConfigSelectionDialog::ValidConfigName(const wxString &name) { return name.find_first_of("/<>:\"\\|?*") == wxString::npos; }

wxString ConfigSelectionDialog::ConfigPath(const wxString &name) {
        char s[512];
        strcpy(s, configs_path);
        put_backslash(s);
        return wxString(s) + name + ".cfg";
}

ConfigSelectionDialog::ConfigSelectionDialog(wxWindow *parent) {
        wxXmlResource::Get()->LoadDialog(this, parent, "ConfigureSelectionDlg");
        list = wx_ctrl<wxListBox>(this, "IDC_LIST");

        UpdateList();
        if (active_config >= 0 && active_config < (int)list->GetCount())
                list->SetSelection(active_config);

        Bind(wxEVT_BUTTON, &ConfigSelectionDialog::OnOk, this, wxID_OK);
        Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_CANCEL); }, wxID_CANCEL);
        Bind(wxEVT_BUTTON, &ConfigSelectionDialog::OnNew, this, XRCID("IDC_NEW"));
        Bind(wxEVT_BUTTON, &ConfigSelectionDialog::OnConfigure, this, XRCID("IDC_CONFIG"));
        Bind(wxEVT_BUTTON, &ConfigSelectionDialog::OnRename, this, XRCID("IDC_RENAME"));
        Bind(wxEVT_BUTTON, &ConfigSelectionDialog::OnCopy, this, XRCID("IDC_COPY"));
        Bind(wxEVT_BUTTON, &ConfigSelectionDialog::OnDelete, this, XRCID("IDC_DELETE"));
        Bind(wxEVT_LISTBOX_DCLICK, &ConfigSelectionDialog::OnOk, this, XRCID("IDC_LIST"));
#ifdef USE_NETWORKING
        Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { HostConfigDialog::Open(this); }, XRCID("IDC_CONFIG_HOST"));
#endif
        wx_fit_dialog(this);
}

void ConfigSelectionDialog::UpdateList() {
        int selection = list->GetSelection();

        wxArrayString names;
        wxDir dir(configs_path);
        wxString file;
        if (dir.IsOpened()) {
                for (bool found = dir.GetFirst(&file, "*.cfg", wxDIR_FILES); found; found = dir.GetNext(&file))
                        names.Add(wxFileName(file).GetName());
        }
        names.Sort();
        list->Set(names);

        int num = list->GetCount();
        if (num > 0) {
                if (selection >= num)
                        selection = num - 1;
                else if (selection < 0)
                        selection = 0;
                list->SetSelection(selection);
        }
        FindWindow(XRCID("IDC_RENAME"))->Enable(num > 0);
        FindWindow(XRCID("IDC_DELETE"))->Enable(num > 0);
        FindWindow(XRCID("IDC_CONFIG"))->Enable(num > 0);
        FindWindow(wxID_OK)->Enable(num > 0);
}

bool ConfigSelectionDialog::AskName(const wxString &title, wxString &name) {
        while (wx_textentrydialog(this, "Enter name:", title, name, 1, 64, name)) {
                if (!ValidConfigName(name)) {
                        wx_simple_messagebox("Invalid name", "The following characters cannot be in the "
                                                             "name: /, <, >, :, \", \\, |, ?, *");
                        continue;
                }
                if (wxFileExists(ConfigPath(name))) {
                        wx_simple_messagebox("Already exists", "A configuration with that name already exists.");
                        continue;
                }
                return true;
        }
        return false;
}

void ConfigSelectionDialog::OnOk(wxCommandEvent &event) {
        int selection = list->GetSelection();
        if (selection == wxNOT_FOUND)
                return;

        wxString name = list->GetString(selection);
        active_config = selection;
        wx_strlcpy(config_file_default, ConfigPath(name), sizeof(config_file_default));
        wx_strlcpy(config_name, name, sizeof(config_name));
        pclog("Config name %s\n", config_file_default);
        EndModal(wxID_OK);
}

void ConfigSelectionDialog::OnNew(wxCommandEvent &event) {
        wxString name;
        if (!AskName("New config", name))
                return;

        wxString cfg = ConfigPath(name);
        pclog("Config %s\n", (const char *)cfg.mb_str());
        if (ConfigDialog::Open(this)) {
                saveconfig((char *)(const char *)cfg.mb_str());
                UpdateList();
                int index = list->FindString(name, true);
                if (index != wxNOT_FOUND)
                        list->SetSelection(index);
        }
}

void ConfigSelectionDialog::OnConfigure(wxCommandEvent &event) {
        int selection = list->GetSelection();
        if (selection == wxNOT_FOUND)
                return;

        wxString cfg = ConfigPath(list->GetString(selection));
        pclog("Config name %s\n", (const char *)cfg.mb_str());
        loadconfig((char *)(const char *)cfg.mb_str());
        ConfigDialog::Open(this);
        saveconfig((char *)(const char *)cfg.mb_str());
}

void ConfigSelectionDialog::OnRename(wxCommandEvent &event) {
        int selection = list->GetSelection();
        if (selection == wxNOT_FOUND)
                return;

        wxString old_name = list->GetString(selection);
        wxString name = old_name;
        if (!AskName("New name", name))
                return;

        pclog("Rename %s to %s\n", (const char *)ConfigPath(old_name).mb_str(), (const char *)ConfigPath(name).mb_str());
        wxRenameFile(ConfigPath(old_name), ConfigPath(name), false);
        UpdateList();
}

void ConfigSelectionDialog::OnCopy(wxCommandEvent &event) {
        int selection = list->GetSelection();
        if (selection == wxNOT_FOUND)
                return;

        wxString old_name = list->GetString(selection);
        wxString name = old_name;
        if (!AskName("New name", name))
                return;

        pclog("Copy %s to %s\n", (const char *)ConfigPath(old_name).mb_str(), (const char *)ConfigPath(name).mb_str());
        wxCopyFile(ConfigPath(old_name), ConfigPath(name), true);
        UpdateList();
}

void ConfigSelectionDialog::OnDelete(wxCommandEvent &event) {
        int selection = list->GetSelection();
        if (selection == wxNOT_FOUND)
                return;

        wxString name = list->GetString(selection);
        if (wx_messagebox(NULL, wxString::Format("Do you want to delete \"%s\"?", name), "PCem", wxOK | wxCANCEL) == wxOK) {
                wxRemoveFile(ConfigPath(name));
                UpdateList();
        }
}

int ConfigSelectionDialog::active_config = -1;

bool ConfigSelectionDialog::Open(wxWindow *parent, bool inited) {
        has_been_inited = inited;

        ConfigSelectionDialog dlg(parent);
        bool ret = dlg.ShowModal() == wxID_OK;

        has_been_inited = 1;
        return ret;
}
