#include "qt-utils.h"
#include "qt-dialogbox.h"

#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

#include "qt-glslp-parser.h"
#include "qt-shaderconfig.h"

extern "C" {
#include "config.h"
void saveconfig(char *);
void resetpchard();
int deviceconfig_dlgproc(void *hdlg, int message, INT_PARAM wParam, LONG_PARAM lParam);
void write_shader_config(glslp_t *shader);
struct glslp_t *current_glsl = NULL;
int confirm();
}
#define IDC_CONFIG_BASE 1000
#define ID_APPLY 500
#define ID_RESET 501
extern int renderer_doreset;

static int shaderconfig_dlgproc(void *hdlg, int message, INT_PARAM wParam, LONG_PARAM lParam) {
        int i;
        switch (message) {
        case WX_INITDIALOG: {
                return TRUE;
        }
        case WX_COMMAND:
                switch (wParam) {
                case ID_APPLY: {
                        for (i = 0; i < current_glsl->num_parameters; ++i) {
                                struct parameter *p = &current_glsl->parameters[i];
                                QDoubleSpinBox *h = (QDoubleSpinBox *)wx_getdlgitem(hdlg, IDC_CONFIG_BASE + i);
                                p->value = (float)h->value();
                        }
                        write_shader_config(current_glsl);
                        renderer_doreset = 1;
                        return TRUE;
                }
                case wxID_OK: {
                        for (i = 0; i < current_glsl->num_parameters; ++i) {
                                struct parameter *p = &current_glsl->parameters[i];
                                QDoubleSpinBox *h = (QDoubleSpinBox *)wx_getdlgitem(hdlg, IDC_CONFIG_BASE + i);
                                p->value = (float)h->value();
                        }
                        write_shader_config(current_glsl);
                        renderer_doreset = 1;
                        wx_enddialog(hdlg, 0);
                        return TRUE;
                }
                case wxID_CANCEL:
                        wx_enddialog(hdlg, 0);
                        return TRUE;
                case ID_RESET: {
                        for (i = 0; i < current_glsl->num_parameters; ++i) {
                                struct parameter *p = &current_glsl->parameters[i];
                                QDoubleSpinBox *h = (QDoubleSpinBox *)wx_getdlgitem(hdlg, IDC_CONFIG_BASE + i);
                                h->setValue(p->default_value);
                        }
                        return TRUE;
                }
                }
        }
        return TRUE;
}

void shaderconfig_open(void *hwnd, struct glslp_t *glsl) {
        int i;
        current_glsl = glsl;

        PCemDialogBox dialog((QWidget *)hwnd, shaderconfig_dlgproc);

        dialog.setWindowTitle("Shader Configuration");

        QVBoxLayout *root = new QVBoxLayout();
        dialog.setLayout(root);

        QScrollArea *scrollArea = new QScrollArea(&dialog);
        scrollArea->setWidgetResizable(true);
        scrollArea->setMinimumSize(300, 350);
        scrollArea->setFrameShape(QFrame::StyledPanel);
        root->addWidget(scrollArea);

        QWidget *scrollContent = new QWidget();
        QVBoxLayout *scrollLayout = new QVBoxLayout(scrollContent);

        if (glsl->num_parameters > 0) {
                for (i = 0; i < glsl->num_parameters; ++i) {
                        struct parameter *p = &glsl->parameters[i];
                        scrollLayout->addWidget(new QLabel(p->description, scrollContent));
                        QDoubleSpinBox *spin = new QDoubleSpinBox(scrollContent);
                        spin->setRange(p->min, p->max);
                        spin->setSingleStep(p->step);
                        spin->setValue(p->value);
                        spin->setDecimals(2);
                        dialog.registerWidget(IDC_CONFIG_BASE + i, spin);
                        scrollLayout->addWidget(spin);
                }
        } else {
                scrollLayout->addWidget(new QLabel("No configuration available.", scrollContent));
        }

        scrollLayout->addStretch();
        scrollArea->setWidget(scrollContent);

        QHBoxLayout *buttonSizer = new QHBoxLayout();

        QPushButton *applyBtn = new QPushButton("Apply", &dialog);
        dialog.registerWidget(ID_APPLY, applyBtn);
        buttonSizer->addWidget(applyBtn);

        QPushButton *okBtn = new QPushButton("OK", &dialog);
        dialog.registerWidget(wxID_OK, okBtn);
        buttonSizer->addWidget(okBtn);

        QPushButton *cancelBtn = new QPushButton("Cancel", &dialog);
        dialog.registerWidget(wxID_CANCEL, cancelBtn);
        buttonSizer->addWidget(cancelBtn);

        QPushButton *resetBtn = new QPushButton("Reset", &dialog);
        dialog.registerWidget(ID_RESET, resetBtn);
        buttonSizer->addWidget(resetBtn);

        root->addLayout(buttonSizer);

        dialog.adjustSize();
        dialog.onInit();
        dialog.exec();
}
