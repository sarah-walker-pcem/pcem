#include "qt-joystickconfig.h"

#include "qt-utils.h"
#include "qt-dialogbox.h"

#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

extern "C" {
void pclog(const char *format, ...);
#include "config.h"
#include "device.h"
#include "gameport.h"
#include "plat-joystick.h"
}

#define IDC_CONFIG_BASE 1000

static int joystick_nr;
static int joystick_config_type;
#define AXIS_STRINGS_MAX 3
static char axis_strings[AXIS_STRINGS_MAX][10] = {"X Axis", "Y Axis", "Z Axis"};

static void rebuild_axis_button_selections(void *hdlg) {
        int id = IDC_CONFIG_BASE + 3;
        void *h;
        int joystick;
        int c, d;

        h = wx_getdlgitem(hdlg, IDC_CONFIG_BASE + 1);
        joystick = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);

        for (c = 0; c < joystick_get_axis_count(joystick_config_type); c++) {
                int sel = c;

                h = wx_getdlgitem(hdlg, id);
                wx_sendmessage(h, WX_CB_RESETCONTENT, 0, 0);

                if (joystick) {
                        for (d = 0; d < plat_joystick_state[joystick - 1].nr_axes; d++) {
                                wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)plat_joystick_state[joystick - 1].axis[d].name);
                                if (c < AXIS_STRINGS_MAX) {
                                        if (!strcasecmp(axis_strings[c], plat_joystick_state[joystick - 1].axis[d].name))
                                                sel = d;
                                }
                        }
                        for (d = 0; d < plat_joystick_state[joystick - 1].nr_povs; d++) {
                                char s[80];

                                sprintf(s, "%s (X axis)", plat_joystick_state[joystick - 1].pov[d].name);
                                wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)s);
                                sprintf(s, "%s (Y axis)", plat_joystick_state[joystick - 1].pov[d].name);
                                wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)s);
                        }
                        wx_sendmessage(h, WX_CB_SETCURSEL, sel, 0);
                        wx_enablewindow(h, TRUE);
                } else
                        wx_enablewindow(h, FALSE);

                id += 2;
        }

        for (c = 0; c < joystick_get_button_count(joystick_config_type); c++) {
                h = wx_getdlgitem(hdlg, id);
                wx_sendmessage(h, WX_CB_RESETCONTENT, 0, 0);

                if (joystick) {
                        for (d = 0; d < plat_joystick_state[joystick - 1].nr_buttons; d++)
                                wx_sendmessage(h, WX_CB_ADDSTRING, 0,
                                               (LONG_PARAM)plat_joystick_state[joystick - 1].button[d].name);
                        wx_sendmessage(h, WX_CB_SETCURSEL, c, 0);
                        wx_enablewindow(h, TRUE);
                } else
                        wx_enablewindow(h, FALSE);

                id += 2;
        }

        for (c = 0; c < joystick_get_pov_count(joystick_config_type) * 2; c++) {
                int sel = c;

                h = wx_getdlgitem(hdlg, id);
                wx_sendmessage(h, WX_CB_RESETCONTENT, 0, 0);

                if (joystick) {
                        for (d = 0; d < plat_joystick_state[joystick - 1].nr_povs; d++) {
                                char s[80];

                                sprintf(s, "%s (X axis)", plat_joystick_state[joystick - 1].pov[d].name);
                                wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)s);
                                sprintf(s, "%s (Y axis)", plat_joystick_state[joystick - 1].pov[d].name);
                                wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)s);
                        }
                        for (d = 0; d < plat_joystick_state[joystick - 1].nr_axes; d++) {
                                wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)plat_joystick_state[joystick - 1].axis[d].name);
                        }
                        wx_sendmessage(h, WX_CB_SETCURSEL, sel, 0);
                        wx_enablewindow(h, TRUE);
                } else
                        wx_enablewindow(h, FALSE);

                id += 2;
        }
}

static int get_axis(void *hdlg, int id) {
        void *h = wx_getdlgitem(hdlg, id);
        int axis_sel = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
        int nr_axes = plat_joystick_state[joystick_state[joystick_nr].plat_joystick_nr - 1].nr_axes;

        if (axis_sel < nr_axes)
                return axis_sel;

        axis_sel -= nr_axes;
        if (axis_sel & 1)
                return POV_Y | (axis_sel >> 1);
        else
                return POV_X | (axis_sel >> 1);
}

static int get_pov(void *hdlg, int id) {
        void *h = wx_getdlgitem(hdlg, id);
        int axis_sel = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
        int nr_povs = plat_joystick_state[joystick_state[joystick_nr].plat_joystick_nr - 1].nr_povs * 2;

        if (axis_sel < nr_povs) {
                if (axis_sel & 1)
                        return POV_Y | (axis_sel >> 1);
                else
                        return POV_X | (axis_sel >> 1);
        }

        return axis_sel - nr_povs;
}

static int joystickconfig_dlgproc(void *hdlg, int message, INT_PARAM wParam, LONG_PARAM lParam) {
        switch (message) {
        case WX_INITDIALOG: {
                void *h = wx_getdlgitem(hdlg, IDC_CONFIG_BASE + 1);
                int c;
                int id = IDC_CONFIG_BASE + 3;
                int joystick = joystick_state[joystick_nr].plat_joystick_nr;

                wx_sendmessage(h, WX_CB_SETCURSEL, joystick, 0);

                rebuild_axis_button_selections(hdlg);

                if (joystick_state[joystick_nr].plat_joystick_nr) {
                        int nr_axes = plat_joystick_state[joystick - 1].nr_axes;
                        int nr_povs = plat_joystick_state[joystick - 1].nr_povs;
                        for (c = 0; c < joystick_get_axis_count(joystick_config_type); c++) {
                                int mapping = joystick_state[joystick_nr].axis_mapping[c];

                                h = wx_getdlgitem(hdlg, id);
                                if (mapping & POV_X)
                                        wx_sendmessage(h, WX_CB_SETCURSEL, nr_axes + (mapping & 3) * 2, 0);
                                else if (mapping & POV_Y)
                                        wx_sendmessage(h, WX_CB_SETCURSEL, nr_axes + (mapping & 3) * 2 + 1, 0);
                                else
                                        wx_sendmessage(h, WX_CB_SETCURSEL, mapping, 0);
                                id += 2;
                        }
                        for (c = 0; c < joystick_get_button_count(joystick_config_type); c++) {
                                h = wx_getdlgitem(hdlg, id);
                                wx_sendmessage(h, WX_CB_SETCURSEL, joystick_state[joystick_nr].button_mapping[c], 0);
                                id += 2;
                        }
                        for (c = 0; c < joystick_get_pov_count(joystick_config_type); c++) {
                                int mapping;

                                h = wx_getdlgitem(hdlg, id);
                                mapping = joystick_state[joystick_nr].pov_mapping[c][0];
                                if (mapping & POV_X)
                                        wx_sendmessage(h, WX_CB_SETCURSEL, (mapping & 3) * 2, 0);
                                else if (mapping & POV_Y)
                                        wx_sendmessage(h, WX_CB_SETCURSEL, (mapping & 3) * 2 + 1, 0);
                                else
                                        wx_sendmessage(h, WX_CB_SETCURSEL, mapping + nr_povs * 2, 0);
                                id += 2;
                                h = wx_getdlgitem(hdlg, id);
                                mapping = joystick_state[joystick_nr].pov_mapping[c][1];
                                if (mapping & POV_X)
                                        wx_sendmessage(h, WX_CB_SETCURSEL, (mapping & 3) * 2, 0);
                                else if (mapping & POV_Y)
                                        wx_sendmessage(h, WX_CB_SETCURSEL, (mapping & 3) * 2 + 1, 0);
                                else
                                        wx_sendmessage(h, WX_CB_SETCURSEL, mapping + nr_povs * 2, 0);
                                id += 2;
                        }
                }
        }
                return TRUE;

        case WX_COMMAND:
                switch (wParam) {
                case IDC_CONFIG_BASE + 1:
                        rebuild_axis_button_selections(hdlg);
                        break;

                case wxID_OK: {
                        void *h;
                        int c;
                        int id = IDC_CONFIG_BASE + 3;

                        h = wx_getdlgitem(hdlg, IDC_CONFIG_BASE + 1);
                        joystick_state[joystick_nr].plat_joystick_nr = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);

                        if (joystick_state[joystick_nr].plat_joystick_nr) {
                                for (c = 0; c < joystick_get_axis_count(joystick_config_type); c++) {
                                        joystick_state[joystick_nr].axis_mapping[c] = get_axis(hdlg, id);
                                        id += 2;
                                }
                                for (c = 0; c < joystick_get_button_count(joystick_config_type); c++) {
                                        h = wx_getdlgitem(hdlg, id);
                                        joystick_state[joystick_nr].button_mapping[c] = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
                                        id += 2;
                                }
                                for (c = 0; c < joystick_get_pov_count(joystick_config_type); c++) {
                                        h = wx_getdlgitem(hdlg, id);
                                        joystick_state[joystick_nr].pov_mapping[c][0] = get_pov(hdlg, id);
                                        id += 2;
                                        h = wx_getdlgitem(hdlg, id);
                                        joystick_state[joystick_nr].pov_mapping[c][1] = get_pov(hdlg, id);
                                        id += 2;
                                }
                        }
                }
                case wxID_CANCEL:
                        wx_enddialog(hdlg, 0);
                        return TRUE;
                }
                break;
        }
        return FALSE;
}

void joystickconfig_open(void *hwnd, int joy_nr, int type) {
        char s[257];
        int c;

        PCemDialogBox dialog((QWidget *)hwnd, joystickconfig_dlgproc);

        joystick_nr = joy_nr;
        joystick_config_type = type;

        dialog.setWindowTitle("Joystick Configuration");

        QVBoxLayout *root = new QVBoxLayout();
        dialog.setLayout(root);

        QGridLayout *sizer = new QGridLayout();
        root->addLayout(sizer);

        int id = IDC_CONFIG_BASE;
        int row = 0;

        QLabel *deviceLabel = new QLabel("Device:", &dialog);
        dialog.registerWidget(id++, deviceLabel);
        sizer->addWidget(deviceLabel, row, 0);
        QComboBox *cb = new QComboBox(&dialog);
        cb->setEditable(false);
        dialog.registerWidget(id++, cb);
        sizer->addWidget(cb, row, 1);
        row++;

        cb->addItem("None");
        for (c = 0; c < joysticks_present; c++)
                cb->addItem(plat_joystick_state[c].name);

        for (c = 0; c < joystick_get_axis_count(type); c++) {
                sprintf(s, "Axis %i:", c);
                QLabel *label = new QLabel(s, &dialog);
                dialog.registerWidget(id++, label);
                sizer->addWidget(label, row, 0);
                QComboBox *combo = new QComboBox(&dialog);
                combo->setEditable(false);
                dialog.registerWidget(id++, combo);
                sizer->addWidget(combo, row, 1);
                row++;
        }

        for (c = 0; c < joystick_get_button_count(type); c++) {
                sprintf(s, "Button %i:", c);
                QLabel *label = new QLabel(s, &dialog);
                dialog.registerWidget(id++, label);
                sizer->addWidget(label, row, 0);
                QComboBox *combo = new QComboBox(&dialog);
                combo->setEditable(false);
                dialog.registerWidget(id++, combo);
                sizer->addWidget(combo, row, 1);
                row++;
        }

        for (c = 0; c < joystick_get_pov_count(type) * 2; c++) {
                sprintf(s, "POV %i:", c);
                QLabel *label = new QLabel(s, &dialog);
                dialog.registerWidget(id++, label);
                sizer->addWidget(label, row, 0);
                QComboBox *combo = new QComboBox(&dialog);
                combo->setEditable(false);
                dialog.registerWidget(id++, combo);
                sizer->addWidget(combo, row, 1);
                row++;
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
