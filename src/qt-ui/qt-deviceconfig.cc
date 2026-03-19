#include "qt-deviceconfig.h"

#include "qt-utils.h"
#include "qt-dialogbox.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

extern "C" {
#include "config.h"
#include "plat-midi.h"
void saveconfig(char *);
void resetpchard();
int deviceconfig_dlgproc(void *hdlg, int message, INT_PARAM wParam, LONG_PARAM lParam);
device_t *config_device = NULL;
int confirm();
}
#define IDC_CONFIG_BASE 1000

int deviceconfig_dlgproc(void *hdlg, int message, INT_PARAM wParam, LONG_PARAM lParam) {
        switch (message) {
        case WX_INITDIALOG: {
                int id = IDC_CONFIG_BASE;
                device_config_t *config = config_device->config;
                int c;

                while (config->type != -1) {
                        device_config_selection_t *selection = config->selection;
                        void *h = 0;
                        int val_int;
                        int num;
                        char s[100];

                        switch (config->type) {
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
                                while (selection->description[0]) {
                                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)selection->description);
                                        if (val_int == selection->value)
                                                wx_sendmessage(h, WX_CB_SETCURSEL, c, 0);
                                        selection++;
                                        c++;
                                }

                                id += 2;
                                break;

                        case CONFIG_MIDI:
                                num = midi_get_num_devs();
                                if (num > 0) {
                                        h = wx_getdlgitem(hdlg, id + 1);
                                        val_int = config_get_int(CFG_MACHINE, NULL, config->name, config->default_int);

                                        for (c = 0; c < num; c++) {
                                                midi_get_dev_name(c, s);
                                                wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)s);
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
                switch (wParam) {
                case wxID_OK: {
                        int id = IDC_CONFIG_BASE;
                        device_config_t *config = config_device->config;
                        int c;
                        int changed = 0;

                        while (config->type != -1) {
                                device_config_selection_t *selection = config->selection;
                                void *h = 0;
                                int val_int;

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
                                        h = wx_getdlgitem(hdlg, id + 1);
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
                                        if (midi_get_num_devs() > 0) {
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
                                void *h = 0;

                                switch (config->type) {
                                case CONFIG_BINARY:
                                        h = wx_getdlgitem(hdlg, id);
                                        config_set_int(CFG_MACHINE, config_device->name, config->name,
                                                       wx_sendmessage(h, WX_BM_GETCHECK, 0, 0));

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
                                        if (midi_get_num_devs() > 0) {
                                                h = wx_getdlgitem(hdlg, id + 1);
                                                c = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
                                                config_set_int(CFG_MACHINE, NULL, config->name, c);

                                                id += 2;
                                        }
                                        break;
                                }
                                config++;
                        }

                        if (has_been_inited) {
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

void deviceconfig_open(void *hwnd, device_t *device) {
        char s[257];
        config_device = device;

        PCemDialogBox dialog((QWidget *)hwnd, deviceconfig_dlgproc);

        device_config_t *config = device->config;

        dialog.setWindowTitle("Device Configuration");

        QVBoxLayout *root = new QVBoxLayout();
        dialog.setLayout(root);

        QGridLayout *sizer = new QGridLayout();
        root->addLayout(sizer);

        int id = IDC_CONFIG_BASE;
        int row = 0;

        while (config->type != -1) {
                switch (config->type) {
                case CONFIG_BINARY: {
                        QCheckBox *cb = new QCheckBox(config->description, &dialog);
                        dialog.registerWidget(id++, cb);
                        sizer->addWidget(cb, row, 1);
                        row++;
                        break;
                }

                case CONFIG_SELECTION:
                case CONFIG_MIDI: {
                        if (config->type == CONFIG_MIDI && midi_get_num_devs() == 0)
                                break;
                        sprintf(s, "%s:", config->description);
                        QLabel *label = new QLabel(s, &dialog);
                        dialog.registerWidget(id++, label);
                        sizer->addWidget(label, row, 0);
                        QComboBox *combo = new QComboBox(&dialog);
                        combo->setEditable(false);
                        dialog.registerWidget(id++, combo);
                        sizer->addWidget(combo, row, 1);
                        row++;
                        break;
                }
                }

                config++;
        }

        QHBoxLayout *okCancelSizer = new QHBoxLayout();
        root->addLayout(okCancelSizer);

        okCancelSizer->addStretch(1);
        QPushButton *okBtn = new QPushButton("OK", &dialog);
        dialog.registerWidget(wxID_OK, okBtn);
        okCancelSizer->addWidget(okBtn);
        QPushButton *cancelBtn = new QPushButton("Cancel", &dialog);
        dialog.registerWidget(wxID_CANCEL, cancelBtn);
        okCancelSizer->addWidget(cancelBtn);

        dialog.adjustSize();
        dialog.onInit();
        dialog.exec();
}
