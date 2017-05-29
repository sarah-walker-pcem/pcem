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
        void saveconfig(char*);
        void resetpchard();
        int deviceconfig_dlgproc(void* hdlg, int message, INT_PARAM wParam, LONG_PARAM lParam);
        device_t *config_device;
        int confirm();
}
#define IDC_CONFIG_BASE 1000


int deviceconfig_dlgproc(void* hdlg, int message, INT_PARAM wParam, LONG_PARAM lParam) {
        switch (message) {
        case WX_INITDIALOG: {
                int id = IDC_CONFIG_BASE;
                device_config_t *config = config_device->config;
                int c;

                while (config->type != -1) {
                        device_config_selection_t *selection = config->selection;
                        void* h = 0;
                        int val_int;
                        char *val_string;
                        int num;
                        char s[80];

                        switch (config->type) {
                        case CONFIG_BINARY:
                                h = wx_getdlgitem(hdlg, id);
                                val_int = config_get_int(CFG_MACHINE, config_device->name, config->name,
                                                config->default_int);

                                wx_sendmessage(h, WX_BM_SETCHECK, val_int, 0);

                                id++;
                                break;

                        case CONFIG_SELECTION:
                                h = wx_getdlgitem(hdlg, id+1);
                                val_int = config_get_int(CFG_MACHINE, config_device->name, config->name,
                                                config->default_int);

                                c = 0;
                                while (selection->description[0]) {
                                        wx_sendmessage(h, WX_CB_ADDSTRING, 0,
                                                        (LONG_PARAM) selection->description);
                                        if (val_int == selection->value)
                                                wx_sendmessage(h, WX_CB_SETCURSEL, c, 0);
                                        selection++;
                                        c++;
                                }

                                id += 2;
                                break;

                        case CONFIG_MIDI:
                                // midi ignored
//                                        val_int = config_get_int(CFG_MACHINE, NULL, config->name, config->default_int);
//
//                                        num  = midi_get_num_devs();
//                                        for (c = 0; c < num; c++)
//                                        {
//                                                midi_get_dev_name(c, s);
//                                                SendMessage(h, CB_ADDSTRING, 0, (LONG_PARAM)s);
//                                                if (val_int == c)
//                                                        SendMessage(h, CB_SETCURSEL, c, 0);
//                                        }
//
//                                        id += 2;
                                break;
                        }
                        config++;
                }
        }
                return TRUE;

        case WX_COMMAND:
                switch (wParam) {
                case wxID_OK: {
                        int id = IDC_CONFIG_BASE;
                        device_config_t *config = config_device->config;
                        int c;
                        int changed = 0;

                        while (config->type != -1) {
                                device_config_selection_t *selection = config->selection;
                                void* h = 0;
                                int val_int;
                                char *val_string;

                                switch (config->type) {
                                case CONFIG_BINARY:
                                        h = wx_getdlgitem(hdlg, id);
                                        val_int = config_get_int(CFG_MACHINE, config_device->name, config->name,
                                                        config->default_int);

                                        if (val_int != wx_sendmessage(h, WX_BM_GETCHECK, 0, 0))
                                                changed = 1;

                                        id++;
                                        break;

                                case CONFIG_SELECTION:
                                        h = wx_getdlgitem(hdlg, id+1);
                                        val_int = config_get_int(CFG_MACHINE, config_device->name, config->name,
                                                        config->default_int);

                                        c = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);

                                        for (; c > 0; c--)
                                                selection++;

                                        if (val_int != selection->value)
                                                changed = 1;

                                        id += 2;
                                        break;

                                case CONFIG_MIDI:
                                        // midi ignored
//                                      val_int = config_get_int(CFG_MACHINE, NULL, config->name,
//                                                      config->default_int);
//
//                                      c = SendMessage(h, CB_GETCURSEL, 0, 0);
//
//                                      if (val_int != c)
//                                              changed = 1;
//
//                                      id += 2;
                                        break;
                                }
                                config++;
                        }

                        if (!changed) {
                                wx_enddialog(hdlg, 0);
                                return TRUE;
                        }

                        if (has_been_inited && !confirm()) {
                                wx_enddialog(hdlg, 0);
                                return TRUE;
                        }

                        id = IDC_CONFIG_BASE;
                        config = config_device->config;

                        while (config->type != -1) {
                                device_config_selection_t *selection = config->selection;
                                void* h = 0;
                                int val_int;
                                char *val_string;

                                switch (config->type) {
                                case CONFIG_BINARY:
                                        h = wx_getdlgitem(hdlg, id);
                                        config_set_int(CFG_MACHINE, config_device->name, config->name,
                                                        wx_sendmessage(h, WX_BM_GETCHECK, 0, 0));

                                        id++;
                                        break;

                                case CONFIG_SELECTION:
                                        h = wx_getdlgitem(hdlg, id+1);
                                        c = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
                                        for (; c > 0; c--)
                                                selection++;
                                        config_set_int(CFG_MACHINE, config_device->name, config->name,
                                                        selection->value);

                                        id += 2;
                                        break;

                                case CONFIG_MIDI:
                                        // midi ignored
//                                      c = SendMessage(h, CB_GETCURSEL, 0, 0);
//                                      config_set_int(CFG_MACHINE, NULL, config->name, c);
//
//                                      id += 2;
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



void deviceconfig_open(void* hwnd, device_t *device) {
	config_device = device;

	PCemDialogBox dialog((wxWindow*) hwnd, deviceconfig_dlgproc);
//	dialog.SetWindowStyle(wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);

	device_config_t *config = device->config;

	dialog.SetTitle("Device Configuration");

        wxBoxSizer* root = new wxBoxSizer(wxVERTICAL);
        dialog.SetSizer(root);

	wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
	root->Add(sizer, 0, wxALL, 5);

	int id = IDC_CONFIG_BASE;

	while (config->type != -1) {
		switch (config->type) {
		case CONFIG_BINARY:
			sizer->Add(new wxCheckBox(&dialog, id++, config->description), 0, wxALL);
			break;

		case CONFIG_SELECTION: {
			sizer->Add(new wxStaticText(&dialog, id++, config->description), 0, wxALL);
			wxComboBox* cb = new wxComboBox(&dialog, id++);
			cb->SetEditable(false);
			sizer->Add(cb, 0, wxALL);
		} break;
		case CONFIG_MIDI:
			// midi ignored
			break;
		}

		config++;
	}

	wxBoxSizer* okCancelSizer = new wxBoxSizer(wxHORIZONTAL);

	okCancelSizer->Add(new wxButton(&dialog, wxID_OK));
	okCancelSizer->Add(new wxButton(&dialog, wxID_CANCEL));

	sizer->Add(okCancelSizer);

	dialog.Fit();
	dialog.OnInit();
	dialog.ShowModal();
	dialog.Destroy();
}
