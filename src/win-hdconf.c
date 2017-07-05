#define BITMAP WINDOWS_BITMAP
#include <windows.h>
#include <windowsx.h>
#undef BITMAP

#include "ibm.h"
#include "hdd.h"
#include "ide.h"
#include "resources.h"
#include "win.h"

static struct
{
        int cylinders;
        int heads;
} hd_types[] =
{
        {306,  4},  {615, 4},   {615,  6},  {940,  8},
        {940,  6},  {615, 4},   {462,  8},  {733,  5},
        {900, 15},  {820, 3},   {855,  5},  {855,  7},
        {306,  8},  {733, 7},     {0,  0},  {612,  4},
        {977,  5},  {977, 7},  {1024,  7},  {733,  5},
        {733,  7},  {733, 5},   {306,  4},  {925,  7},
        {925,  9},  {754, 7},   {754, 11},  {699,  7},
        {823, 10},  {918, 7},  {1024, 11}, {1024, 15},
        {1024, 5},  {612, 2},  {1024,  9}, {1024,  8},
        {615,  8},  {987, 3},   {462,  7},  {820,  6},
        {977,  5},  {981, 5},   {830,  7},  {830, 10},
        {917, 15}, {1224, 15}
};

static int hd_changed = 0;

static char hd_new_name[512];
static int hd_new_spt, hd_new_hpc, hd_new_cyl;
static int hd_new_type;
static int new_cdrom_channel;

static void check_hd_type(off64_t sz)
{
        int c;
        
        hd_new_type = 0;

        if (hdd_controller_current_is_mfm())
        {
                for (c = 0; c < 46; c++)
                {
                        if ((hd_types[c].cylinders * hd_types[c].heads * 17 * 512) == sz)
                        {
                                hd_new_spt = 17;
                                hd_new_hpc = hd_types[c].heads;
                                hd_new_cyl = hd_types[c].cylinders;
                                hd_new_type = c+1;
                                return;
                        }                
                }
                hd_new_spt = 63;
                hd_new_hpc = 16;
                hd_new_cyl = ((sz / 512) / 16) / 63;
        }
        else
        {
                if ((sz % 17) == 0 && sz <= 142606336)
                {
                        hd_new_spt = 17;

                        if (sz <= 26738688)
                                hd_new_hpc = 4;
                        else if ((sz % 3072) == 0 && sz <= 53477376)
                                hd_new_hpc = 6;
                        else
                        {
                                for (c=5;c<16;c++)
                                {
                                        if((sz % (c * 512)) == 0 && sz <= 1024*c*17*512) break;
                                        if (c == 5) c++;
                                }
                                hd_new_hpc = c;
                        }
                        hd_new_cyl = (int)((sz / 512) / hd_new_hpc) / 17;
                }
                else
                {
                        hd_new_spt = 63;
                        hd_new_hpc = 16;
                        hd_new_cyl = ((sz / 512) / 16) / 63;
                }
        }
}

static void update_hdd_cdrom(HWND hdlg)
{
        HWND h;
        
        h = GetDlgItem(hdlg, IDC_CHDD);
        SendMessage(h, BM_SETCHECK, (new_cdrom_channel == 0) ? 0 : 1, 0);
        h = GetDlgItem(hdlg, IDC_CCDROM);
        SendMessage(h, BM_SETCHECK, (new_cdrom_channel == 0) ? 1 : 0, 0);
        h = GetDlgItem(hdlg, IDC_DHDD);
        SendMessage(h, BM_SETCHECK, (new_cdrom_channel == 1) ? 0 : 1, 0);
        h = GetDlgItem(hdlg, IDC_DCDROM);
        SendMessage(h, BM_SETCHECK, (new_cdrom_channel == 1) ? 1 : 0, 0);
        h = GetDlgItem(hdlg, IDC_EHDD);
        SendMessage(h, BM_SETCHECK, (new_cdrom_channel == 2) ? 0 : 1, 0);
        h = GetDlgItem(hdlg, IDC_ECDROM);
        SendMessage(h, BM_SETCHECK, (new_cdrom_channel == 2) ? 1 : 0, 0);
        h = GetDlgItem(hdlg, IDC_FHDD);
        SendMessage(h, BM_SETCHECK, (new_cdrom_channel == 3) ? 0 : 1, 0);
        h = GetDlgItem(hdlg, IDC_FCDROM);
        SendMessage(h, BM_SETCHECK, (new_cdrom_channel == 3) ? 1 : 0, 0);
}

static BOOL CALLBACK hdnew_dlgproc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
{
        char s[260];
        HWND h;
        int c;
        PcemHDC hd[4];
        FILE *f;
        uint8_t buf[512];
        int hd_type;
        
        switch (message)
        {
                case WM_INITDIALOG:
                h = GetDlgItem(hdlg, IDC_EDIT1);
                sprintf(s, "%i", 63);
                SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                h = GetDlgItem(hdlg, IDC_EDIT2);
                sprintf(s, "%i", 16);
                SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                h = GetDlgItem(hdlg, IDC_EDIT3);
                sprintf(s, "%i", 511);
                SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);

                h = GetDlgItem(hdlg, IDC_EDITC);
                SendMessage(h, WM_SETTEXT, 0, (LPARAM)"");
                
                h = GetDlgItem(hdlg, IDC_TEXT1);
                sprintf(s, "Size : %imb", (((511*16*63)*512)/1024)/1024);
                SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);

//                hd_type = 0;
                h = GetDlgItem(hdlg, IDC_HDTYPE);
                SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)"Custom type");
                for (c = 1; c <= 46; c++)
                {
                        sprintf(s, "Type %02i : cylinders=%i, heads=%i, size=%iMB", c, hd_types[c-1].cylinders, hd_types[c-1].heads, (hd_types[c-1].cylinders * hd_types[c-1].heads * 17 * 512) / (1024 * 1024));
                        SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)s);
                }
                SendMessage(h, CB_SETCURSEL, 0, 0);

                return TRUE;
                case WM_COMMAND:
                switch (LOWORD(wParam))
                {
                        case IDOK:
                        h = GetDlgItem(hdlg, IDC_EDITC);
                        SendMessage(h, WM_GETTEXT, 511, (LPARAM)hd_new_name);
                        if (!hd_new_name[0])
                        {
                                MessageBox(ghwnd,"Please enter a valid filename","PCem error",MB_OK);
                                return TRUE;
                        }
                        h = GetDlgItem(hdlg, IDC_EDIT1);
                        SendMessage(h, WM_GETTEXT, 255, (LPARAM)s);
                        sscanf(s, "%i", &hd_new_spt);
                        h = GetDlgItem(hdlg, IDC_EDIT2);
                        SendMessage(h, WM_GETTEXT, 255, (LPARAM)s);
                        sscanf(s, "%i", &hd_new_hpc);
                        h = GetDlgItem(hdlg, IDC_EDIT3);
                        SendMessage(h, WM_GETTEXT, 255, (LPARAM)s);
                        sscanf(s, "%i", &hd_new_cyl);
                        
                        if (hd_new_spt > 63)
                        {
                                MessageBox(ghwnd, "Drive has too many sectors (maximum is 63)", "PCem error", MB_OK);
                                return TRUE;
                        }
                        if (hd_new_hpc > 16)
                        {
                                MessageBox(ghwnd, "Drive has too many heads (maximum is 16)", "PCem error", MB_OK);
                                return TRUE;
                        }
                        if (hd_new_cyl > 16383)
                        {
                                MessageBox(ghwnd, "Drive has too many cylinders (maximum is 16383)", "PCem error", MB_OK);
                                return TRUE;
                        }
                        
                        f = fopen64(hd_new_name, "wb");
                        if (!f)
                        {
                                MessageBox(ghwnd, "Can't open file for write", "PCem error", MB_OK);
                                return TRUE;
                        }
                        memset(buf, 0, 512);
                        for (c = 0; c < (hd_new_cyl * hd_new_hpc * hd_new_spt); c++)
                            fwrite(buf, 512, 1, f);
                        fclose(f);
                        
                        MessageBox(ghwnd, "Remember to partition and format the new drive", "PCem", MB_OK);
                        
                        EndDialog(hdlg, 1);
                        return TRUE;
                        case IDCANCEL:
                        EndDialog(hdlg, 0);
                        return TRUE;

                        case IDC_CFILE:
                        if (!getsfile(hdlg, "Hard disc image (*.IMG)\0*.IMG\0All files (*.*)\0*.*\0", "", NULL, "img"))
                        {
                                h = GetDlgItem(hdlg, IDC_EDITC);
                                SendMessage(h, WM_SETTEXT, 0, (LPARAM)openfilestring);
                        }
                        return TRUE;
                        
                        case IDC_EDIT1: case IDC_EDIT2: case IDC_EDIT3:
                        h = GetDlgItem(hdlg, IDC_EDIT1);
                        SendMessage(h, WM_GETTEXT, 255, (LPARAM)s);
                        pclog("EDIT1: %s\n", s);
                        sscanf(s, "%i", &hd[0].spt);
                        h = GetDlgItem(hdlg, IDC_EDIT2);
                        SendMessage(h, WM_GETTEXT, 255, (LPARAM)s);
                        pclog("EDIT2: %s\n", s);
                        sscanf(s, "%i", &hd[0].hpc);
                        h = GetDlgItem(hdlg, IDC_EDIT3);
                        SendMessage(h, WM_GETTEXT, 255, (LPARAM)s);
                        pclog("EDIT3: %s\n", s);
                        sscanf(s, "%i", &hd[0].tracks);

                        h = GetDlgItem(hdlg, IDC_TEXT1);
                        sprintf(s, "Size : %imb", (int)(((((uint64_t)hd[0].tracks*(uint64_t)hd[0].hpc)*(uint64_t)hd[0].spt)*512)/1024)/1024);
                        SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                        
                        hd_type = 0;
                        for (c = 1; c <= 46; c++)
                        {
                                pclog("Compare %i,%i %i,%i %i,%i\n", hd[0].spt,17, hd[0].hpc,hd_types[c-1].heads, hd[0].tracks,hd_types[c-1].cylinders);
                                if (hd[0].spt == 17 && hd[0].hpc == hd_types[c-1].heads && hd[0].tracks == hd_types[c-1].cylinders)
                                {
                                        hd_type = c;
                                        break;
                                }
                        }
                        h = GetDlgItem(hdlg, IDC_HDTYPE);
                        SendMessage(h, CB_SETCURSEL, hd_type, 0);
                        
                        return TRUE;

                        case IDC_HDTYPE:
                        if (HIWORD(wParam) == CBN_SELCHANGE)
                        {
                                h = GetDlgItem(hdlg, IDC_HDTYPE);
                                hd_type = SendMessage(h, CB_GETCURSEL, 0, 0);
                                if (hd_type)
                                {
                                        h = GetDlgItem(hdlg, IDC_EDIT3);
                                        sprintf(s, "%i", hd_types[hd_type-1].cylinders);
                                        SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                                        h = GetDlgItem(hdlg, IDC_EDIT2);
                                        sprintf(s, "%i", hd_types[hd_type-1].heads);
                                        SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                                        h = GetDlgItem(hdlg, IDC_EDIT1);
                                        SendMessage(h, WM_SETTEXT, 0, (LPARAM)"17");

                                        h = GetDlgItem(hdlg, IDC_TEXT1);
                                        sprintf(s, "Size : %imb", (int)(((uint64_t)hd_types[hd_type-1].cylinders*hd_types[hd_type-1].heads*17*512)/1024)/1024);
                                        SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                                }
                        }
                        return TRUE;
                }
                break;

        }
        return FALSE;
}

BOOL CALLBACK hdsize_dlgproc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
{
        char s[260];
        HWND h;
        PcemHDC hd[2];
        int c;
        
        switch (message)
        {
                case WM_INITDIALOG:
                h = GetDlgItem(hdlg, IDC_HDTYPE);
                SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)"Custom type");
                for (c = 1; c <= 46; c++)
                {
                        sprintf(s, "Type %02i : cylinders=%i, heads=%i, size=%iMB", c, hd_types[c-1].cylinders, hd_types[c-1].heads, (hd_types[c-1].cylinders * hd_types[c-1].heads * 17 * 512) / (1024 * 1024));
                        SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)s);
                }
                SendMessage(h, CB_SETCURSEL, hd_new_type, 0);

                h = GetDlgItem(hdlg, IDC_EDIT1);
                sprintf(s, "%i", hd_new_spt);
                SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                h = GetDlgItem(hdlg, IDC_EDIT2);
                sprintf(s, "%i", hd_new_hpc);
                SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                h = GetDlgItem(hdlg, IDC_EDIT3);
                sprintf(s, "%i", hd_new_cyl);
                SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);

                h = GetDlgItem(hdlg, IDC_TEXT1);
                sprintf(s, "Size : %imb", (int)((((uint64_t)hd_new_spt*(uint64_t)hd_new_hpc*(uint64_t)hd_new_cyl)*512)/1024)/1024);
                SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);

                return TRUE;
                case WM_COMMAND:
                switch (LOWORD(wParam))
                {
                        case IDOK:
                        h = GetDlgItem(hdlg, IDC_EDIT1);
                        SendMessage(h, WM_GETTEXT, 255, (LPARAM)s);
                        sscanf(s, "%i", &hd_new_spt);
                        h = GetDlgItem(hdlg, IDC_EDIT2);
                        SendMessage(h, WM_GETTEXT, 255, (LPARAM)s);
                        sscanf(s, "%i", &hd_new_hpc);
                        h = GetDlgItem(hdlg, IDC_EDIT3);
                        SendMessage(h, WM_GETTEXT, 255, (LPARAM)s);
                        sscanf(s, "%i", &hd_new_cyl);
                        
                        if (hd_new_spt > 63)
                        {
                                MessageBox(ghwnd,"Drive has too many sectors (maximum is 63)","PCem error",MB_OK);
                                return TRUE;
                        }
                        if (hd_new_hpc > 16)
                        {
                                MessageBox(ghwnd,"Drive has too many heads (maximum is 16)","PCem error",MB_OK);
                                return TRUE;
                        }
                        if (hd_new_cyl > 16383)
                        {
                                MessageBox(ghwnd,"Drive has too many cylinders (maximum is 16383)","PCem error",MB_OK);
                                return TRUE;
                        }
                        
                        EndDialog(hdlg,1);
                        return TRUE;
                        case IDCANCEL:
                        EndDialog(hdlg,0);
                        return TRUE;

                        case IDC_EDIT1: case IDC_EDIT2: case IDC_EDIT3:
                        h = GetDlgItem(hdlg, IDC_EDIT1);
                        SendMessage(h, WM_GETTEXT, 255, (LPARAM)s);
                        sscanf(s, "%i", &hd[0].spt);
                        h = GetDlgItem(hdlg, IDC_EDIT2);
                        SendMessage(h, WM_GETTEXT, 255, (LPARAM)s);
                        sscanf(s, "%i", &hd[0].hpc);
                        h = GetDlgItem(hdlg, IDC_EDIT3);
                        SendMessage(h, WM_GETTEXT, 255, (LPARAM)s);
                        sscanf(s, "%i", &hd[0].tracks);

                        h = GetDlgItem(hdlg, IDC_TEXT1);
                        sprintf(s, "Size : %imb", (int)(((((uint64_t)hd[0].tracks*(uint64_t)hd[0].hpc)*(uint64_t)hd[0].spt)*512)/1024)/1024);
                        SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                        return TRUE;
                }
                break;

        }
        return FALSE;
}

static BOOL CALLBACK hdconf_dlgproc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
{
        char s[260];
        HWND h;
        PcemHDC hd[4];
        FILE *f;
        off64_t sz;
        switch (message)
        {
                case WM_INITDIALOG:
                pause = 1;
                hd[0] = hdc[0];
                hd[1] = hdc[1];
                hd[2] = hdc[2];
                hd[3] = hdc[3];
                hd_changed = 0;
                
                h = GetDlgItem(hdlg, IDC_EDIT_C_SPT);
                sprintf(s, "%i", hdc[0].spt);
                SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                h = GetDlgItem(hdlg, IDC_EDIT_C_HPC);
                sprintf(s, "%i", hdc[0].hpc);
                SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                h = GetDlgItem(hdlg, IDC_EDIT_C_CYL);
                sprintf(s, "%i", hdc[0].tracks);
                SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                h = GetDlgItem(hdlg, IDC_EDIT_C_FN);
                SendMessage(h, WM_SETTEXT, 0, (LPARAM)ide_fn[0]);

                h = GetDlgItem(hdlg, IDC_EDIT_D_SPT);
                sprintf(s, "%i", hdc[1].spt);
                SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                h = GetDlgItem(hdlg, IDC_EDIT_D_HPC);
                sprintf(s, "%i", hdc[1].hpc);
                SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                h = GetDlgItem(hdlg, IDC_EDIT_D_CYL);
                sprintf(s, "%i", hdc[1].tracks);
                SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                h=  GetDlgItem(hdlg, IDC_EDIT_D_FN);
                SendMessage(h, WM_SETTEXT, 0, (LPARAM)ide_fn[1]);
                
                h = GetDlgItem(hdlg, IDC_EDIT_E_SPT);
                sprintf(s, "%i", hdc[2].spt);
                SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                h = GetDlgItem(hdlg, IDC_EDIT_E_HPC);
                sprintf(s, "%i", hdc[2].hpc);
                SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                h = GetDlgItem(hdlg, IDC_EDIT_E_CYL);
                sprintf(s, "%i", hdc[2].tracks);
                SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                h=  GetDlgItem(hdlg, IDC_EDIT_E_FN);
                SendMessage(h, WM_SETTEXT, 0, (LPARAM)ide_fn[2]);
                
                h = GetDlgItem(hdlg, IDC_EDIT_F_SPT);
                sprintf(s, "%i", hdc[3].spt);
                SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                h = GetDlgItem(hdlg, IDC_EDIT_F_HPC);
                sprintf(s, "%i", hdc[3].hpc);
                SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                h = GetDlgItem(hdlg, IDC_EDIT_F_CYL);
                sprintf(s, "%i", hdc[3].tracks);
                SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                h=  GetDlgItem(hdlg, IDC_EDIT_F_FN);
                SendMessage(h, WM_SETTEXT, 0, (LPARAM)ide_fn[3]);
                
                h = GetDlgItem(hdlg, IDC_TEXT_C_SIZE);
                sprintf(s, "Size : %imb", (int)(((((uint64_t)hd[0].tracks*(uint64_t)hd[0].hpc)*(uint64_t)hd[0].spt)*512)/1024)/1024);
                SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);

                h = GetDlgItem(hdlg, IDC_TEXT_D_SIZE);
                sprintf(s, "Size : %imb", (int)(((((uint64_t)hd[1].tracks*(uint64_t)hd[1].hpc)*(uint64_t)hd[1].spt)*512)/1024)/1024);
                SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);

                h = GetDlgItem(hdlg, IDC_TEXT_E_SIZE);
                sprintf(s, "Size : %imb", (int)(((((uint64_t)hd[2].tracks*(uint64_t)hd[2].hpc)*(uint64_t)hd[2].spt)*512)/1024)/1024);
                SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);

                h = GetDlgItem(hdlg, IDC_TEXT_F_SIZE);
                sprintf(s, "Size : %imb", (int)(((((uint64_t)hd[3].tracks*(uint64_t)hd[3].hpc)*(uint64_t)hd[3].spt)*512)/1024)/1024);
                SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);

                new_cdrom_channel = cdrom_channel;

                update_hdd_cdrom(hdlg);
                return TRUE;
                
                case WM_COMMAND:
                switch (LOWORD(wParam))
                {
                        case IDOK:
                        if (hd_changed || cdrom_channel != new_cdrom_channel)
                        {                     
                                if (!has_been_inited || MessageBox(NULL, "This will reset PCem!\nOkay to continue?", "PCem", MB_OKCANCEL) == IDOK)
                                {
                                        h = GetDlgItem(hdlg, IDC_EDIT_C_SPT);
                                        SendMessage(h, WM_GETTEXT, 255, (LPARAM)s);
                                        sscanf(s, "%i", &hd[0].spt);
                                        h = GetDlgItem(hdlg, IDC_EDIT_C_HPC);
                                        SendMessage(h, WM_GETTEXT, 255, (LPARAM)s);
                                        sscanf(s, "%i", &hd[0].hpc);
                                        h = GetDlgItem(hdlg, IDC_EDIT_C_CYL);
                                        SendMessage(h, WM_GETTEXT, 255, (LPARAM)s);
                                        sscanf(s, "%i", &hd[0].tracks);
                                        h = GetDlgItem(hdlg, IDC_EDIT_C_FN);
                                        SendMessage(h, WM_GETTEXT, 511, (LPARAM)ide_fn[0]);

                                        h = GetDlgItem(hdlg, IDC_EDIT_D_SPT);
                                        SendMessage(h, WM_GETTEXT, 255, (LPARAM)s);
                                        sscanf(s, "%i", &hd[1].spt);
                                        h = GetDlgItem(hdlg, IDC_EDIT_D_HPC);
                                        SendMessage(h, WM_GETTEXT, 255, (LPARAM)s);
                                        sscanf(s, "%i", &hd[1].hpc);
                                        h = GetDlgItem(hdlg, IDC_EDIT_D_CYL);
                                        SendMessage(h, WM_GETTEXT, 255, (LPARAM)s);
                                        sscanf(s, "%i", &hd[1].tracks);
                                        h = GetDlgItem(hdlg, IDC_EDIT_D_FN);
                                        SendMessage(h, WM_GETTEXT, 511, (LPARAM)ide_fn[1]);
                                        
                                        h = GetDlgItem(hdlg, IDC_EDIT_E_SPT);
                                        SendMessage(h, WM_GETTEXT, 255, (LPARAM)s);
                                        sscanf(s, "%i", &hd[2].spt);
                                        h = GetDlgItem(hdlg, IDC_EDIT_E_HPC);
                                        SendMessage(h, WM_GETTEXT, 255, (LPARAM)s);
                                        sscanf(s, "%i", &hd[2].hpc);
                                        h = GetDlgItem(hdlg, IDC_EDIT_E_CYL);
                                        SendMessage(h, WM_GETTEXT, 255, (LPARAM)s);
                                        sscanf(s, "%i", &hd[2].tracks);
                                        h = GetDlgItem(hdlg, IDC_EDIT_E_FN);
                                        SendMessage(h, WM_GETTEXT, 511, (LPARAM)ide_fn[2]);
                                        
                                        h = GetDlgItem(hdlg, IDC_EDIT_F_SPT);
                                        SendMessage(h, WM_GETTEXT, 255, (LPARAM)s);
                                        sscanf(s, "%i", &hd[3].spt);
                                        h = GetDlgItem(hdlg, IDC_EDIT_F_HPC);
                                        SendMessage(h, WM_GETTEXT, 255, (LPARAM)s);
                                        sscanf(s, "%i", &hd[3].hpc);
                                        h = GetDlgItem(hdlg, IDC_EDIT_F_CYL);
                                        SendMessage(h, WM_GETTEXT, 255, (LPARAM)s);
                                        sscanf(s, "%i", &hd[3].tracks);
                                        h = GetDlgItem(hdlg, IDC_EDIT_F_FN);
                                        SendMessage(h, WM_GETTEXT, 511, (LPARAM)ide_fn[3]);
                                        
                                        hdc[0] = hd[0];
                                        hdc[1] = hd[1];
                                        hdc[2] = hd[2];
                                        hdc[3] = hd[3];

                                        cdrom_channel = new_cdrom_channel;
                                       
                                        if (has_been_inited)
                                        {
                                                saveconfig(NULL);
                                                                                
                                                resetpchard();
                                        }
                                }                                
                        }
                        case IDCANCEL:
                        EndDialog(hdlg, 0);
                        pause = 0;
                        return TRUE;

                        case IDC_EJECTC:
                        hd[0].spt = 0;
                        hd[0].hpc = 0;
                        hd[0].tracks = 0;
                        ide_fn[0][0] = 0;
                        SetDlgItemText(hdlg, IDC_EDIT_C_SPT, "0");
                        SetDlgItemText(hdlg, IDC_EDIT_C_HPC, "0");
                        SetDlgItemText(hdlg, IDC_EDIT_C_CYL, "0");
                        SetDlgItemText(hdlg, IDC_EDIT_C_FN, "");
                        hd_changed = 1;
                        return TRUE;
                        case IDC_EJECTD:
                        hd[1].spt = 0;
                        hd[1].hpc = 0;
                        hd[1].tracks = 0;
                        ide_fn[1][0] = 0;
                        SetDlgItemText(hdlg, IDC_EDIT_D_SPT, "0");
                        SetDlgItemText(hdlg, IDC_EDIT_D_HPC, "0");
                        SetDlgItemText(hdlg, IDC_EDIT_D_CYL, "0");
                        SetDlgItemText(hdlg, IDC_EDIT_D_FN, "");
                        hd_changed = 1;
                        return TRUE;
                        case IDC_EJECTE:
                        hd[2].spt = 0;
                        hd[2].hpc = 0;
                        hd[2].tracks = 0;
                        ide_fn[2][0] = 0;
                        SetDlgItemText(hdlg, IDC_EDIT_E_SPT, "0");
                        SetDlgItemText(hdlg, IDC_EDIT_E_HPC, "0");
                        SetDlgItemText(hdlg, IDC_EDIT_E_CYL, "0");
                        SetDlgItemText(hdlg, IDC_EDIT_E_FN, "");
                        hd_changed = 1;
                        return TRUE;
                        case IDC_EJECTF:
                        hd[3].spt = 0;
                        hd[3].hpc = 0;
                        hd[3].tracks = 0;
                        ide_fn[3][0] = 0;
                        SetDlgItemText(hdlg, IDC_EDIT_F_SPT, "0");
                        SetDlgItemText(hdlg, IDC_EDIT_F_HPC, "0");
                        SetDlgItemText(hdlg, IDC_EDIT_F_CYL, "0");
                        SetDlgItemText(hdlg, IDC_EDIT_F_FN, "");
                        hd_changed = 1;
                        return TRUE;
                        
                        case IDC_CNEW:
                        if (DialogBox(hinstance, hdd_controller_current_is_mfm() ? TEXT("HdNewDlgMfm") : TEXT("HdNewDlg"), hdlg, (DLGPROC)hdnew_dlgproc) == 1)
                        {
                                h = GetDlgItem(hdlg, IDC_EDIT_C_SPT);
                                sprintf(s, "%i", hd_new_spt);
                                SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                                h = GetDlgItem(hdlg, IDC_EDIT_C_HPC);
                                sprintf(s, "%i", hd_new_hpc);
                                SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                                h = GetDlgItem(hdlg, IDC_EDIT_C_CYL);
                                sprintf(s, "%i", hd_new_cyl);
                                SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                                h = GetDlgItem(hdlg, IDC_EDIT_C_FN);
                                SendMessage(h, WM_SETTEXT, 0, (LPARAM)hd_new_name);

                                h = GetDlgItem(hdlg, IDC_TEXT_C_SIZE);
                                sprintf(s, "Size : %imb", (int)(((((uint64_t)hd_new_cyl*(uint64_t)hd_new_hpc)*(uint64_t)hd_new_spt)*512)/1024)/1024);
                                SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);

                                hd_changed = 1;
                        }                              
                        return TRUE;
                        
                        case IDC_CFILE:
                        if (!getfile(hdlg, "Hard disc image (*.IMG)\0*.IMG\0All files (*.*)\0*.*\0", ""))
                        {
                                f = fopen64(openfilestring, "rb");
                                if (!f)
                                {
                                        MessageBox(ghwnd,"Can't open file for read","PCem error",MB_OK);
                                        return TRUE;
                                }
                                fseeko64(f, -1, SEEK_END);
                                sz = ftello64(f) + 1;
                                fclose(f);
                                check_hd_type(sz);
                                
                                if (DialogBox(hinstance, hdd_controller_current_is_mfm() ? TEXT("HdSizeDlgMfm") : TEXT("HdSizeDlg"), hdlg, (DLGPROC)hdsize_dlgproc) == 1)
                                {
                                        h = GetDlgItem(hdlg, IDC_EDIT_C_SPT);
                                        sprintf(s, "%i", hd_new_spt);
                                        SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                                        h = GetDlgItem(hdlg, IDC_EDIT_C_HPC);
                                        sprintf(s, "%i", hd_new_hpc);
                                        SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                                        h = GetDlgItem(hdlg, IDC_EDIT_C_CYL);
                                        sprintf(s, "%i", hd_new_cyl);
                                        SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                                        h = GetDlgItem(hdlg, IDC_EDIT_C_FN);
                                        SendMessage(h, WM_SETTEXT, 0, (LPARAM)openfilestring);

                                        h=  GetDlgItem(hdlg, IDC_TEXT_C_SIZE);
                                        sprintf(s, "Size : %imb", (int)(((((uint64_t)hd_new_cyl*(uint64_t)hd_new_hpc)*(uint64_t)hd_new_spt)*512)/1024)/1024);
                                        SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
        
                                        hd_changed = 1;
                                }
                        }
                        return TRUE;
                                
                        case IDC_DNEW:
                        if (DialogBox(hinstance, hdd_controller_current_is_mfm() ? TEXT("HdNewDlgMfm") : TEXT("HdNewDlg"), hdlg, (DLGPROC)hdnew_dlgproc) == 1)
                        {
                                h = GetDlgItem(hdlg, IDC_EDIT_D_SPT);
                                sprintf(s, "%i", hd_new_spt);
                                SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                                h = GetDlgItem(hdlg, IDC_EDIT_D_HPC);
                                sprintf(s, "%i", hd_new_hpc);
                                SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                                h = GetDlgItem(hdlg, IDC_EDIT_D_CYL);
                                sprintf(s, "%i", hd_new_cyl);
                                SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                                h = GetDlgItem(hdlg, IDC_EDIT_D_FN);
                                SendMessage(h, WM_SETTEXT, 0, (LPARAM)hd_new_name);

                                h=  GetDlgItem(hdlg, IDC_TEXT_D_SIZE);
                                sprintf(s, "Size : %imb", (int)(((((uint64_t)hd_new_cyl*(uint64_t)hd_new_hpc)*(uint64_t)hd_new_spt)*512)/1024)/1024);
                                SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);

                                hd_changed = 1;
                        }                              
                        return TRUE;
                        
                        case IDC_DFILE:
                        if (!getfile(hdlg, "Hard disc image (*.IMG)\0*.IMG\0All files (*.*)\0*.*\0", ""))
                        {
                                f = fopen64(openfilestring, "rb");
                                if (!f)
                                {
                                        MessageBox(ghwnd,"Can't open file for read","PCem error",MB_OK);
                                        return TRUE;
                                }
                                fseeko64(f, -1, SEEK_END);
                                sz = ftello64(f) + 1;
                                fclose(f);
                                check_hd_type(sz);
                                
                                if (DialogBox(hinstance, hdd_controller_current_is_mfm() ? TEXT("HdSizeDlgMfm") : TEXT("HdSizeDlg"), hdlg, (DLGPROC)hdsize_dlgproc) == 1)
                                {
                                        h = GetDlgItem(hdlg, IDC_EDIT_D_SPT);
                                        sprintf(s, "%i", hd_new_spt);
                                        SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                                        h = GetDlgItem(hdlg, IDC_EDIT_D_HPC);
                                        sprintf(s, "%i", hd_new_hpc);
                                        SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                                        h = GetDlgItem(hdlg, IDC_EDIT_D_CYL);
                                        sprintf(s, "%i", hd_new_cyl);
                                        SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                                        h = GetDlgItem(hdlg, IDC_EDIT_D_FN);
                                        SendMessage(h, WM_SETTEXT, 0, (LPARAM)openfilestring);

                                        h = GetDlgItem(hdlg, IDC_TEXT_D_SIZE);
                                        sprintf(s, "Size : %imb", (int)(((((uint64_t)hd_new_cyl*(uint64_t)hd_new_hpc)*(uint64_t)hd_new_spt)*512)/1024)/1024);
                                        SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
        
                                        hd_changed = 1;
                                }
                        }
                        return TRUE;

                        case IDC_ENEW:
                        if (DialogBox(hinstance, TEXT("HdNewDlg"), hdlg, (DLGPROC)hdnew_dlgproc) == 1)
                        {
                                h = GetDlgItem(hdlg, IDC_EDIT_E_SPT);
                                sprintf(s, "%i", hd_new_spt);
                                SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                                h = GetDlgItem(hdlg, IDC_EDIT_E_HPC);
                                sprintf(s, "%i", hd_new_hpc);
                                SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                                h = GetDlgItem(hdlg, IDC_EDIT_E_CYL);
                                sprintf(s, "%i", hd_new_cyl);
                                SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                                h = GetDlgItem(hdlg, IDC_EDIT_E_FN);
                                SendMessage(h, WM_SETTEXT, 0, (LPARAM)hd_new_name);

                                h=  GetDlgItem(hdlg, IDC_TEXT_E_SIZE);
                                sprintf(s, "Size : %imb", (int)(((((uint64_t)hd_new_cyl*(uint64_t)hd_new_hpc)*(uint64_t)hd_new_spt)*512)/1024)/1024);
                                SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);

                                hd_changed = 1;
                        }                              
                        return TRUE;
                        
                        case IDC_EFILE:
                        if (!getfile(hdlg, "Hard disc image (*.IMG)\0*.IMG\0All files (*.*)\0*.*\0", ""))
                        {
                                f = fopen64(openfilestring, "rb");
                                if (!f)
                                {
                                        MessageBox(ghwnd,"Can't open file for read","PCem error",MB_OK);
                                        return TRUE;
                                }
                                fseeko64(f, -1, SEEK_END);
                                sz = ftello64(f) + 1;
                                fclose(f);
                                check_hd_type(sz);
                                
                                if (DialogBox(hinstance, TEXT("HdSizeDlg"), hdlg, (DLGPROC)hdsize_dlgproc) == 1)
                                {
                                        h = GetDlgItem(hdlg, IDC_EDIT_E_SPT);
                                        sprintf(s, "%i", hd_new_spt);
                                        SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                                        h = GetDlgItem(hdlg, IDC_EDIT_E_HPC);
                                        sprintf(s, "%i", hd_new_hpc);
                                        SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                                        h = GetDlgItem(hdlg, IDC_EDIT_E_CYL);
                                        sprintf(s, "%i", hd_new_cyl);
                                        SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                                        h = GetDlgItem(hdlg, IDC_EDIT_E_FN);
                                        SendMessage(h, WM_SETTEXT, 0, (LPARAM)openfilestring);

                                        h = GetDlgItem(hdlg, IDC_TEXT_E_SIZE);
                                        sprintf(s, "Size : %imb", (int)(((((uint64_t)hd_new_cyl*(uint64_t)hd_new_hpc)*(uint64_t)hd_new_spt)*512)/1024)/1024);
                                        SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
        
                                        hd_changed = 1;
                                }
                        }
                        return TRUE;

                        case IDC_FNEW:
                        if (DialogBox(hinstance, TEXT("HdNewDlg"), hdlg, (DLGPROC)hdnew_dlgproc) == 1)
                        {
                                h = GetDlgItem(hdlg, IDC_EDIT_F_SPT);
                                sprintf(s, "%i", hd_new_spt);
                                SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                                h = GetDlgItem(hdlg, IDC_EDIT_F_HPC);
                                sprintf(s, "%i", hd_new_hpc);
                                SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                                h = GetDlgItem(hdlg, IDC_EDIT_F_CYL);
                                sprintf(s, "%i", hd_new_cyl);
                                SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                                h = GetDlgItem(hdlg, IDC_EDIT_F_FN);
                                SendMessage(h, WM_SETTEXT, 0, (LPARAM)hd_new_name);

                                h=  GetDlgItem(hdlg, IDC_TEXT_F_SIZE);
                                sprintf(s, "Size : %imb", (int)(((((uint64_t)hd_new_cyl*(uint64_t)hd_new_hpc)*(uint64_t)hd_new_spt)*512)/1024)/1024);
                                SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);

                                hd_changed = 1;
                        }                              
                        return TRUE;
                        
                        case IDC_FFILE:
                        if (!getfile(hdlg, "Hard disc image (*.IMG)\0*.IMG\0All files (*.*)\0*.*\0", ""))
                        {
                                f = fopen64(openfilestring, "rb");
                                if (!f)
                                {
                                        MessageBox(ghwnd,"Can't open file for read","PCem error",MB_OK);
                                        return TRUE;
                                }
                                fseeko64(f, -1, SEEK_END);
                                sz = ftello64(f) + 1;
                                fclose(f);
                                check_hd_type(sz);
                                
                                if (DialogBox(hinstance, TEXT("HdSizeDlg"), hdlg, (DLGPROC)hdsize_dlgproc) == 1)
                                {
                                        h = GetDlgItem(hdlg, IDC_EDIT_F_SPT);
                                        sprintf(s, "%i", hd_new_spt);
                                        SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                                        h = GetDlgItem(hdlg, IDC_EDIT_F_HPC);
                                        sprintf(s, "%i", hd_new_hpc);
                                        SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                                        h = GetDlgItem(hdlg, IDC_EDIT_F_CYL);
                                        sprintf(s, "%i", hd_new_cyl);
                                        SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                                        h = GetDlgItem(hdlg, IDC_EDIT_F_FN);
                                        SendMessage(h, WM_SETTEXT, 0, (LPARAM)openfilestring);

                                        h = GetDlgItem(hdlg, IDC_TEXT_F_SIZE);
                                        sprintf(s, "Size : %imb", (int)(((((uint64_t)hd_new_cyl*(uint64_t)hd_new_hpc)*(uint64_t)hd_new_spt)*512)/1024)/1024);
                                        SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
        
                                        hd_changed = 1;
                                }
                        }
                        return TRUE;

                        case IDC_EDIT_C_SPT: case IDC_EDIT_C_HPC: case IDC_EDIT_C_CYL:
                        h = GetDlgItem(hdlg, IDC_EDIT_C_SPT);
                        SendMessage(h, WM_GETTEXT, 255, (LPARAM)s);
                        sscanf(s, "%i", &hd[0].spt);
                        h = GetDlgItem(hdlg, IDC_EDIT_C_HPC);
                        SendMessage(h, WM_GETTEXT, 255, (LPARAM)s);
                        sscanf(s, "%i", &hd[0].hpc);
                        h = GetDlgItem(hdlg, IDC_EDIT_C_CYL);
                        SendMessage(h, WM_GETTEXT, 255, (LPARAM)s);
                        sscanf(s, "%i", &hd[0].tracks);

                        h = GetDlgItem(hdlg, IDC_TEXT_C_SIZE);
                        sprintf(s, "Size : %imb", ((((hd[0].tracks*hd[0].hpc)*hd[0].spt)*512)/1024)/1024);
                        SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                        return TRUE;

                        case IDC_EDIT_D_SPT: case IDC_EDIT_D_HPC: case IDC_EDIT_D_CYL:
                        h = GetDlgItem(hdlg, IDC_EDIT_D_SPT);
                        SendMessage(h, WM_GETTEXT, 255, (LPARAM)s);
                        sscanf(s, "%i", &hd[1].spt);
                        h = GetDlgItem(hdlg, IDC_EDIT_D_HPC);
                        SendMessage(h, WM_GETTEXT, 255, (LPARAM)s);
                        sscanf(s, "%i", &hd[1].hpc);
                        h = GetDlgItem(hdlg, IDC_EDIT_D_CYL);
                        SendMessage(h, WM_GETTEXT, 255, (LPARAM)s);
                        sscanf(s, "%i", &hd[1].tracks);

                        h = GetDlgItem(hdlg, IDC_TEXT_D_SIZE);
                        sprintf(s, "Size : %imb", ((((hd[1].tracks*hd[1].hpc)*hd[1].spt)*512)/1024)/1024);
                        SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                        return TRUE;

                        case IDC_EDIT_E_SPT: case IDC_EDIT_E_HPC: case IDC_EDIT_E_CYL:
                        h = GetDlgItem(hdlg, IDC_EDIT_E_SPT);
                        SendMessage(h, WM_GETTEXT, 255, (LPARAM)s);
                        sscanf(s, "%i", &hd[2].spt);
                        h = GetDlgItem(hdlg, IDC_EDIT_E_HPC);
                        SendMessage(h, WM_GETTEXT, 255, (LPARAM)s);
                        sscanf(s, "%i", &hd[2].hpc);
                        h = GetDlgItem(hdlg, IDC_EDIT_E_CYL);
                        SendMessage(h, WM_GETTEXT, 255, (LPARAM)s);
                        sscanf(s, "%i", &hd[2].tracks);

                        h = GetDlgItem(hdlg, IDC_TEXT_E_SIZE);
                        sprintf(s, "Size : %imb", ((((hd[2].tracks*hd[2].hpc)*hd[2].spt)*512)/1024)/1024);
                        SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                        return TRUE;

                        case IDC_EDIT_F_SPT: case IDC_EDIT_F_HPC: case IDC_EDIT_F_CYL:
                        h = GetDlgItem(hdlg, IDC_EDIT_F_SPT);
                        SendMessage(h, WM_GETTEXT, 255, (LPARAM)s);
                        sscanf(s, "%i", &hd[3].spt);
                        h = GetDlgItem(hdlg, IDC_EDIT_F_HPC);
                        SendMessage(h, WM_GETTEXT, 255, (LPARAM)s);
                        sscanf(s, "%i", &hd[3].hpc);
                        h = GetDlgItem(hdlg, IDC_EDIT_F_CYL);
                        SendMessage(h, WM_GETTEXT, 255, (LPARAM)s);
                        sscanf(s, "%i", &hd[3].tracks);

                        h = GetDlgItem(hdlg, IDC_TEXT_F_SIZE);
                        sprintf(s, "Size : %imb", ((((hd[3].tracks*hd[3].hpc)*hd[3].spt)*512)/1024)/1024);
                        SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
                        return TRUE;

                        case IDC_CHDD:
                        if (new_cdrom_channel == 0)
                                new_cdrom_channel = -1;
                        update_hdd_cdrom(hdlg);
                        return TRUE;
                        case IDC_DHDD:
                        if (new_cdrom_channel == 1)
                                new_cdrom_channel = -1;
                        update_hdd_cdrom(hdlg);
                        return TRUE;
                        case IDC_EHDD:
                        if (new_cdrom_channel == 2)
                                new_cdrom_channel = -1;
                        update_hdd_cdrom(hdlg);
                        return TRUE;
                        case IDC_FHDD:
                        if (new_cdrom_channel == 3)
                                new_cdrom_channel = -1;
                        update_hdd_cdrom(hdlg);
                        return TRUE;
                        
                        case IDC_CCDROM:
                        new_cdrom_channel = 0;
                        update_hdd_cdrom(hdlg);
                        return TRUE;
                        case IDC_DCDROM:
                        new_cdrom_channel = 1;
                        update_hdd_cdrom(hdlg);
                        return TRUE;
                        case IDC_ECDROM:
                        new_cdrom_channel = 2;
                        update_hdd_cdrom(hdlg);
                        return TRUE;
                        case IDC_FCDROM:
                        new_cdrom_channel = 3;
                        update_hdd_cdrom(hdlg);
                        return TRUE;                        
                }
                break;

        }
        return FALSE;
}

void hdconf_open(HWND hwnd)
{
        if (hdd_controller_current_is_mfm())
                DialogBox(hinstance, TEXT("HdConfDlgMfm"), hwnd, (DLGPROC)hdconf_dlgproc);
        else
                DialogBox(hinstance, TEXT("HdConfDlg"), hwnd, (DLGPROC)hdconf_dlgproc);
}        
