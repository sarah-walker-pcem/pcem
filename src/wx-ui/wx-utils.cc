#include "wx-utils.h"

#include <wx/xml/xml.h>
#include <wx/filename.h>

int confirm() {
        if (emulation_state != EMULATION_STOPPED)
                return wx_messagebox(NULL, "This will reset PCem!\nOkay to continue?", "PCem", wxOK | wxCANCEL) == wxOK;
        return 1;
}

int wx_messagebox(wxWindow *parent, const wxString &message, const wxString &title, int style) {
        return wxMessageBox(message, title, style | wxCENTRE | wxSTAY_ON_TOP, parent);
}

void wx_simple_messagebox(const char *title, const char *format, ...) {
        char message[2048];
        va_list ap;
        va_start(ap, format);
        vsnprintf(message, sizeof(message), format, ap);
        va_end(ap);
        wx_messagebox(NULL, message, title, wxOK);
}

bool wx_textentrydialog(wxWindow *parent, const wxString &message, const wxString &title, const wxString &value,
                        unsigned int min_length, unsigned int max_length, wxString &result) {
        wxString current = value;
        while (true) {
                wxTextEntryDialog dlg(parent, message, title, current);
                if (max_length > 0)
                        dlg.SetMaxLength(max_length);
                if (dlg.ShowModal() != wxID_OK)
                        return false;
                current = dlg.GetValue();
                if (current.Length() >= min_length) {
                        result = current;
                        return true;
                }
        }
}

bool wx_filedialog(wxWindow *parent, const wxString &title, const wxString &path, const wxString &wildcard,
                   const wxString &default_ext, bool open, wxString &result) {
        wxFileDialog dlg(parent, title, "", path, wildcard,
                         open ? (wxFD_OPEN | wxFD_FILE_MUST_EXIST) : (wxFD_SAVE | wxFD_OVERWRITE_PROMPT));
        if (dlg.ShowModal() != wxID_OK)
                return false;
        result = dlg.GetPath();
        if (!open && !default_ext.empty() && !wxFileName(result).HasExt()) {
                if (!result.EndsWith("."))
                        result += ".";
                result += default_ext;
        }
        return true;
}

void wx_fit_dialog(wxDialog *dlg) {
        wxWindow *root = dlg->FindWindow(XRCID("ROOT_PANEL"));
        if (root)
                root->Fit();
        dlg->Fit();
}

void wx_combobox_clear(wxComboBox *cb) {
#ifndef __WXOSX_MAC__
        cb->SetValue("");
        cb->Clear();
#else
        for (int count = cb->GetCount(); count > 0; count--)
                cb->Delete(0);
#endif
}

void wx_combobox_select(wxComboBox *cb, int index) {
        if (index >= 0 && index < (int)cb->GetCount())
                cb->SetSelection(index);
}

wxMenu *wx_getsubmenu(wxMenuBar *menubar, int id) {
        const wxXmlNode *node = wxXmlResource::Get()->GetResourceNode("main_menu");
        size_t index = 0;
        for (const wxXmlNode *child = node ? node->GetChildren() : NULL; child; child = child->GetNext()) {
                if (child->GetType() != wxXML_ELEMENT_NODE || child->GetAttribute("class") != "wxMenu")
                        continue;
                wxString name = child->GetAttribute("name");
                if (!name.empty() && XRCID(name.utf8_str()) == id && index < menubar->GetMenuCount())
                        return menubar->GetMenu(index);
                index++;
        }
        wxMenuItem *item = menubar->FindItem(id);
        return item ? item->GetSubMenu() : NULL;
}
