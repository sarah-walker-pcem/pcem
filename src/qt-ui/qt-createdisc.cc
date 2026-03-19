#include <stdint.h>

extern "C" void pclog(const char *format, ...);

#include "qt-createdisc.h"

#include "qt-utils.h"
#include "qt-dialogbox.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

#include <stdint.h>

#define IDC_CONFIG_BASE 1000

static struct {
        char name[80];
        int nr_sectors;
} disc_formats[] = {{"160 kB", 1 * 40 * 8},      {"180 kB", 1 * 40 * 9},
                    {"320 kB", 2 * 40 * 8},      {"360 kB", 2 * 40 * 9},
                    {"720 kB", 2 * 80 * 9},      {"1.2 MB", 2 * 80 * 15},
                    {"1.44 MB", 2 * 80 * 18},    {"2.88 MB", 2 * 80 * 36},
                    {"100 MB (Zip)", 2048 * 96}, {"", 0}};

static int creatediscimage_dlgproc(void *hdlg, int message, INT_PARAM wParam, LONG_PARAM lParam) {
        int c;

        switch (message) {
        case WX_INITDIALOG:
                return TRUE;

        case WX_COMMAND:
                switch (wParam) {
                case wxID_OK:
                        c = 0;

                        while (disc_formats[c].nr_sectors) {
                                QRadioButton *rb = (QRadioButton *)wx_getdlgitem(hdlg, IDC_CONFIG_BASE + c);

                                if (rb && rb->isChecked())
                                        break;
                                c++;
                        }

                        if (disc_formats[c].nr_sectors) {
                                char openfilestring[260];
                                int ret;

                                memset(openfilestring, 0, sizeof(openfilestring));
                                ret = wx_filedialog(hdlg, "Save", "", "Disc image (*.img;*.ima)|*.img;*.ima|All files (*.*)|*.*",
                                                    "img", 0, openfilestring);

                                if (!ret) {
                                        FILE *f = fopen(openfilestring, "wb");
                                        if (f) {
                                                uint8_t sector[512];
                                                int d;

                                                memset(sector, 0, 512);

                                                for (d = 0; d < disc_formats[c].nr_sectors; d++)
                                                        fwrite(sector, 512, 1, f);

                                                fclose(f);
                                        }
                                }
                        }

                        wx_enddialog(hdlg, 0);
                        return TRUE;

                case wxID_CANCEL:
                        wx_enddialog(hdlg, 0);
                        return TRUE;
                }
                break;
        }
        return FALSE;
}

void creatediscimage_open(void *hwnd) {
        int id = IDC_CONFIG_BASE;
        int c = 0;

        PCemDialogBox dialog((QWidget *)hwnd, creatediscimage_dlgproc);

        dialog.setWindowTitle("Create Disc Image");

        QVBoxLayout *root = new QVBoxLayout();
        dialog.setLayout(root);

        while (disc_formats[c].nr_sectors) {
                QRadioButton *rb = new QRadioButton(disc_formats[c].name, &dialog);
                dialog.registerWidget(id++, rb);
                root->addWidget(rb);
                c++;
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
