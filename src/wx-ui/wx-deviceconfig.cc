#include "wx-deviceconfig.h"

#include "wx-utils.h"
#include "wx/wxprec.h"

#include "wx-dialogbox.h"

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#include "wx/xrc/xmlres.h"

extern "C"
{
#include "config.h"
#include "plat-midi.h"
void saveconfig(char*);
void resetpchard();
int deviceconfig_dlgproc(void* hdlg, int message, INT_PARAM wParam,
                LONG_PARAM lParam);
device_t *config_device;
int confirm();
}
#define IDC_CONFIG_BASE 1000

int deviceconfig_dlgproc(void* hdlg, int message, INT_PARAM wParam,
                LONG_PARAM lParam)
{
        switch (message)
        {
        case WX_INITDIALOG:
        {
                int id = IDC_CONFIG_BASE;
                device_config_t *config = config_device->config;
                int c;

                while (config->type != -1)
                {
                        device_config_selection_t *selection = config->selection;
                        void* h = 0;
                        int val_int;
                        int num;
                        char s[100];

                        switch (config->type)
                        {
                        case CONFIG_BINARY:
                                h = wx_getdlgitem(hdlg, id);
                                val_int = config_get_int(CFG_MACHINE, config_device->name, config->name, config->default_int);

                                wx_sendmessage(h, WX_BM_SETCHECK, val_int, 0);

                                id++;
                                break;

                        case CONFIG_SELECTION:
                                h = wx_getdlgitem(hdlg, id + 1);
                                val_int = config_get_int(CFG_MACHINE, config_device->name, config->name, config->default_int);

                                c = 0;
                                while (selection->description[0])
                                {
                                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM) selection->description);
                                        if (val_int == selection->value)
                                                wx_sendmessage(h, WX_CB_SETCURSEL, c, 0);
                                        selection++;
                                        c++;
                                }

                                id += 2;
                                break;

                        case CONFIG_MIDI:
                                num = midi_get_num_devs();
                                if (num > 0)
                                {
                                        h = wx_getdlgitem(hdlg, id + 1);
                                        val_int = config_get_int(CFG_MACHINE, NULL, config->name, config->default_int);

                                        for (c = 0; c < num; c++)
                                        {
                                                midi_get_dev_name(c, s);
                                                wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM) s);
                                                if (val_int == c)
                                                        wx_sendmessage(h, WX_CB_SETCURSEL, c, 0);
                                        }
                                        id += 2;
                                }
                                break;
                        }
                        config++;
                }
        }
                return TRUE;

        case WX_COMMAND:
                switch (wParam)
                {
                case wxID_OK:
                {
                        int id = IDC_CONFIG_BASE;
                        device_config_t *config = config_device->config;
                        int c;
                        int changed = 0;

                        while (config->type != -1)
                        {
                                device_config_selection_t *selection = config->selection;
                                void* h = 0;
                                int val_int;

                                switch (config->type)
                                {
                                case CONFIG_BINARY:
                                        h = wx_getdlgitem(hdlg, id);
                                        val_int = config_get_int(CFG_MACHINE, config_device->name, config->name, config->default_int);

                                        if (val_int != wx_sendmessage(h, WX_BM_GETCHECK, 0, 0))
                                                changed = 1;

                                        id++;
                                        break;

                                case CONFIG_SELECTION:
                                        h = wx_getdlgitem(hdlg, id + 1);
                                        val_int = config_get_int(CFG_MACHINE, config_device->name, config->name, config->default_int);

                                        c = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);

                                        for (; c > 0; c--)
                                                selection++;

                                        if (val_int != selection->value)
                                                changed = 1;

                                        id += 2;
                                        break;

                                case CONFIG_MIDI:
                                        if (midi_get_num_devs() > 0)
                                        {
                                                h = wx_getdlgitem(hdlg, id + 1);
                                                val_int = config_get_int(CFG_MACHINE, NULL, config->name, config->default_int);

                                                c = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);

                                                if (val_int != c)
                                                        changed = 1;
                                                id += 2;
                                        }
                                        break;
                                }
                                config++;
                        }

                        if (!changed)
                        {
                                wx_enddialog(hdlg, 0);
                                return TRUE;
                        }

                        if (has_been_inited && !confirm())
                        {
                                wx_enddialog(hdlg, 0);
                                return TRUE;
                        }

                        id = IDC_CONFIG_BASE;
                        config = config_device->config;

                        while (config->type != -1)
                        {
                                device_config_selection_t *selection = config->selection;
                                void* h = 0;

                                switch (config->type)
                                {
                                case CONFIG_BINARY:
                                        h = wx_getdlgitem(hdlg, id);
                                        config_set_int(CFG_MACHINE, config_device->name, config->name, wx_sendmessage(h, WX_BM_GETCHECK, 0, 0));

                                        id++;
                                        break;

                                case CONFIG_SELECTION:
                                        h = wx_getdlgitem(hdlg, id + 1);
                                        c = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
                                        for (; c > 0; c--)
                                                selection++;
                                        config_set_int(CFG_MACHINE, config_device->name, config->name, selection->value);

                                        id += 2;
                                        break;

                                case CONFIG_MIDI:
                                        if (midi_get_num_devs() > 0)
                                        {
                                                h = wx_getdlgitem(hdlg, id + 1);
                                                c = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
                                                config_set_int(CFG_MACHINE, NULL, config->name, c);

                                                id += 2;
                                        }
                                        break;
                                }
                                config++;
                        }

                        if (has_been_inited)
                        {
                                saveconfig(NULL);
                                resetpchard();
                        }

                        wx_enddialog(hdlg, 0);
                        return TRUE;
                }
                case wxID_CANCEL:
                        wx_enddialog(hdlg, 0);
                        return TRUE;
                }
                break;
        }
        return FALSE;
}

void deviceconfig_open(void* hwnd, device_t *device)
{
        char s[257];
        config_device = device;

        PCemDialogBox dialog((wxWindow*) hwnd, deviceconfig_dlgproc);
//	dialog.SetWindowStyle(wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);

        device_config_t *config = device->config;

        dialog.SetTitle("Device Configuration");

        wxFlexGridSizer* root = new wxFlexGridSizer(0, 1, 0, 0);
        root->SetFlexibleDirection(wxBOTH);
        root->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);
        dialog.SetSizer(root);

        wxFlexGridSizer* sizer = new wxFlexGridSizer(0, 2, 0, 0);
        sizer->SetFlexibleDirection(wxBOTH);
        sizer->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);
        sizer->AddGrowableCol(1);
        root->Add(sizer, 1, wxEXPAND, 5);

        int id = IDC_CONFIG_BASE;

        while (config->type != -1)
        {
                switch (config->type)
                {
                case CONFIG_BINARY:
                        sizer->Add(0, 0, 1, wxEXPAND, 5);
                        sizer->Add(new wxCheckBox(&dialog, id++, config->description), 0, wxALL, 5);
                        break;

                case CONFIG_SELECTION:
                case CONFIG_MIDI:
                {
                        if (config->type == CONFIG_MIDI && midi_get_num_devs() == 0)
                                break;
                        sprintf(s, "%s:", config->description);
                        sizer->Add(new wxStaticText(&dialog, id++, s), 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
                        wxBoxSizer* comboSizer = new wxBoxSizer(wxHORIZONTAL);
                        sizer->Add(comboSizer, 1, wxEXPAND, 5);
                        wxComboBox* cb = new wxComboBox(&dialog, id++);
                        cb->SetEditable(false);
                        comboSizer->Add(cb, 1, wxALL, 5);
                        break;
                }
                }

                config++;
        }

        wxBoxSizer* okCancelSizer = new wxBoxSizer(wxHORIZONTAL);
        root->Add(okCancelSizer, 1, wxEXPAND, 5);

        okCancelSizer->Add(0, 0, 1, wxEXPAND, 5);
        okCancelSizer->Add(new wxButton(&dialog, wxID_OK), 0, wxALL, 5);
        okCancelSizer->Add(new wxButton(&dialog, wxID_CANCEL), 0, wxALL, 5);

        dialog.Fit();
        dialog.OnInit();
        dialog.ShowModal();
        dialog.Destroy();
}
