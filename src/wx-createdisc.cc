#include <stdint.h>

extern "C" void pclog(const char *format, ...);

#include "wx-createdisc.h"

#include "wx-utils.h"
#include "wx/wxprec.h"

#include "wx-dialogbox.h"
#include <stdint.h>

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#include "wx/xrc/xmlres.h"

#define IDC_CONFIG_BASE 1000

static struct
{
        char name[80];
        int nr_sectors;
} disc_formats[] =
{
        {"160 kB", 1*40*8},
        {"180 kB", 1*40*9},
        {"320 kB", 2*40*8},
        {"360 kB", 2*40*9},
        {"720 kB", 2*80*9},
        {"1.44 MB", 2*80*18},
        {"2.88 MB", 2*80*36},
        {"100 MB (Zip)", 2048*96},
        {"", 0}
};

static int creatediscimage_dlgproc(void *hdlg, int message, INT_PARAM wParam,
                LONG_PARAM lParam)
{
        wxWindow *window = (wxWindow *)hdlg;
        int c;
        
        switch (message)
        {
                case WX_INITDIALOG:
                return TRUE;

                case WX_COMMAND:
                switch (wParam)
                {
                        case wxID_OK:
                        c = 0;
                        
                        while (disc_formats[c].nr_sectors)
                        {
                                wxWindow *rb_win = window->FindWindow(IDC_CONFIG_BASE + c);
                                wxRadioButton *rb = wxDynamicCast(rb_win, wxRadioButton);
                                
                                if (rb->GetValue())
                                        break;
                                c++;
                        }
                        
                        if (disc_formats[c].nr_sectors)
                        {
                                char openfilestring[260];
                                int ret;
                                
                                memset(openfilestring, 0, sizeof(openfilestring));
                                ret = wx_filedialog(hdlg, "Save", "",
                                                        "Disc image (*.img;*.ima)|*.img;*.ima|All files (*.*)|*.*",
                                                        ".img",
                                                        0, openfilestring);
                                
                                if (!ret)
                                {
                                        FILE *f = fopen(openfilestring, "wb");
                                        if (f)
                                        {
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

void creatediscimage_open(void *hwnd)
{
        int id = IDC_CONFIG_BASE;
        int c = 0;

        PCemDialogBox dialog((wxWindow *)hwnd, creatediscimage_dlgproc);

        dialog.SetTitle("Create Disc Image");

        wxFlexGridSizer* root = new wxFlexGridSizer(0, 1, 0, 0);
        root->SetFlexibleDirection(wxBOTH);
        root->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);
        dialog.SetSizer(root);

        wxFlexGridSizer* sizer = new wxFlexGridSizer(0, 1, 0, 0);
        sizer->SetFlexibleDirection(wxBOTH);
        sizer->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);
        root->Add(sizer, 1, wxEXPAND, 5);

        while (disc_formats[c].nr_sectors)
        {
                wxBoxSizer *radioSizer = new wxBoxSizer(wxHORIZONTAL);
                sizer->Add(radioSizer, 1, wxEXPAND, 5);
                wxRadioButton *rb = new wxRadioButton(&dialog, id++, disc_formats[c].name);
                radioSizer->Add(rb, 1, wxALL, 5);
                
                c++;
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
