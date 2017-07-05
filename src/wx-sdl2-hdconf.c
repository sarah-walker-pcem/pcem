#include "wx-utils.h"
#include "wx-sdl2.h"

#include "ibm.h"
#include "ide.h"
#include "hdd.h"

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

extern int pause;

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
                if ((sz % 17) == 0 && sz <= 133693440)
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

#define ID_IS(s) wParam == wx_xrcid(s)
#define ID_RANGE(a, b) wParam >= wx_xrcid(a) && wParam <= wx_xrcid(b)

static void update_hdd_cdrom(void* hdlg)
{
        void* h;

        h = wx_getdlgitem(hdlg, WX_ID("IDC_HDD[0]"));
        wx_sendmessage(h, WX_BM_SETCHECK, (new_cdrom_channel == 0) ? 0 : 1, 0);
        h = wx_getdlgitem(hdlg, WX_ID("IDC_CDROM[0]"));
        wx_sendmessage(h, WX_BM_SETCHECK, (new_cdrom_channel == 0) ? 1 : 0, 0);
        h = wx_getdlgitem(hdlg, WX_ID("IDC_HDD[1]"));
        wx_sendmessage(h, WX_BM_SETCHECK, (new_cdrom_channel == 1) ? 0 : 1, 0);
        h = wx_getdlgitem(hdlg, WX_ID("IDC_CDROM[1]"));
        wx_sendmessage(h, WX_BM_SETCHECK, (new_cdrom_channel == 1) ? 1 : 0, 0);
        h = wx_getdlgitem(hdlg, WX_ID("IDC_HDD[2]"));
        wx_sendmessage(h, WX_BM_SETCHECK, (new_cdrom_channel == 2) ? 0 : 1, 0);
        h = wx_getdlgitem(hdlg, WX_ID("IDC_CDROM[2]"));
        wx_sendmessage(h, WX_BM_SETCHECK, (new_cdrom_channel == 2) ? 1 : 0, 0);
        h = wx_getdlgitem(hdlg, WX_ID("IDC_HDD[3]"));
        wx_sendmessage(h, WX_BM_SETCHECK, (new_cdrom_channel == 3) ? 0 : 1, 0);
        h = wx_getdlgitem(hdlg, WX_ID("IDC_CDROM[3]"));
        wx_sendmessage(h, WX_BM_SETCHECK, (new_cdrom_channel == 3) ? 1 : 0, 0);
}

static int hdnew_dlgproc(void* hdlg, int message, INT_PARAM wParam, LONG_PARAM lParam)
{
        char s[260];
        void* h;
        int c;
        PcemHDC hd[4];
        FILE *f;
        uint8_t buf[512];
        int hd_type;

        switch (message)
        {
                case WX_INITDIALOG:
                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT1"));
                sprintf(s, "%i", 63);
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT2"));
                sprintf(s, "%i", 16);
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT3"));
                sprintf(s, "%i", 511);
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);

                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDITC"));
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)"");

                h = wx_getdlgitem(hdlg, WX_ID("IDC_TEXT1"));
                sprintf(s, "Size : %imb", (((511*16*63)*512)/1024)/1024);
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);

//                hd_type = 0;
                h = wx_getdlgitem(hdlg, WX_ID("IDC_HDTYPE"));
                wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"Custom type");
                for (c = 1; c <= 46; c++)
                {
                        sprintf(s, "Type %02i : cylinders=%i, heads=%i, size=%iMB", c, hd_types[c-1].cylinders, hd_types[c-1].heads, (hd_types[c-1].cylinders * hd_types[c-1].heads * 17 * 512) / (1024 * 1024));
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)s);
                }
                wx_sendmessage(h, WX_CB_SETCURSEL, 0, 0);

                return TRUE;
                case WX_COMMAND:
                if (wParam == wxID_OK) {
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDITC"));
                        wx_sendmessage(h, WX_WM_GETTEXT, 511, (LONG_PARAM)hd_new_name);
                        if (!hd_new_name[0])
                        {
                                wx_messagebox(NULL,"Please enter a valid filename","PCem error",WX_MB_OK);
                                return TRUE;
                        }
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT1"));
                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                        sscanf(s, "%i", &hd_new_spt);
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT2"));
                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                        sscanf(s, "%i", &hd_new_hpc);
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT3"));
                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                        sscanf(s, "%i", &hd_new_cyl);

                        if (hd_new_spt > 63)
                        {
                                wx_messagebox(NULL, "Drive has too many sectors (maximum is 63)", "PCem error", WX_MB_OK);
                                return TRUE;
                        }
                        if (hd_new_hpc > 16)
                        {
                                wx_messagebox(NULL, "Drive has too many heads (maximum is 16)", "PCem error", WX_MB_OK);
                                return TRUE;
                        }
                        if (hd_new_cyl > 16383)
                        {
                                wx_messagebox(NULL, "Drive has too many cylinders (maximum is 16383)", "PCem error", WX_MB_OK);
                                return TRUE;
                        }

                        f = fopen64(hd_new_name, "wb");
                        if (!f)
                        {
                                wx_messagebox(NULL, "Can't open file for write", "PCem error", WX_MB_OK);
                                return TRUE;
                        }
                        memset(buf, 0, 512);
                        for (c = 0; c < (hd_new_cyl * hd_new_hpc * hd_new_spt); c++)
                            fwrite(buf, 512, 1, f);
                        fclose(f);

                        wx_messagebox(NULL, "Remember to partition and format the new drive", "PCem", WX_MB_OK);

                        wx_enddialog(hdlg, 1);
                        return TRUE;
                } else if (wParam == wxID_CANCEL) {
                        wx_enddialog(hdlg, 0);
                        return TRUE;
                } else if (ID_IS("IDC_CFILE")) {
                        if (!getsfile(hdlg, "Hard disc image (*.img)|*.img|All files (*.*)|*.*", "", NULL, "img"))
                        {
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDITC"));
                                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM) openfilestring);
                        }
                        return TRUE;
                } else if (ID_IS("IDC_EDIT1") || ID_IS("IDC_EDIT2") || ID_IS("IDC_EDIT3")) {
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT1"));
                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                        pclog("EDIT1: %s\n", s);
                        sscanf(s, "%i", &hd[0].spt);
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT2"));
                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                        pclog("EDIT2: %s\n", s);
                        sscanf(s, "%i", &hd[0].hpc);
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT3"));
                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                        pclog("EDIT3: %s\n", s);
                        sscanf(s, "%i", &hd[0].tracks);

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_TEXT1"));
                        sprintf(s, "Size : %imb", (int)(((((uint64_t)hd[0].tracks*(uint64_t)hd[0].hpc)*(uint64_t)hd[0].spt)*512)/1024)/1024);
                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);

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
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_HDTYPE"));
                        wx_sendmessage(h, WX_CB_SETCURSEL, hd_type, 0);

                        return TRUE;
                } else if (ID_IS("IDC_HDTYPE")) {
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_HDTYPE"));
                        hd_type = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
                        if (hd_type)
                        {
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT3"));
                                sprintf(s, "%i", hd_types[hd_type-1].cylinders);
                                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT2"));
                                sprintf(s, "%i", hd_types[hd_type-1].heads);
                                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT1"));
                                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)"17");

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_TEXT1"));
                                sprintf(s, "Size : %imb", (int)(((uint64_t)hd_types[hd_type-1].cylinders*hd_types[hd_type-1].heads*17*512)/1024)/1024);
                                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                        }
                        return TRUE;
                }
                break;

        }
        return FALSE;
}

static int hdsize_dlgproc(void* hdlg, int message, INT_PARAM wParam, LONG_PARAM lParam)
{
        char s[260];
        void* h;
        PcemHDC hd[2];
        int c;

        switch (message)
        {
                case WX_INITDIALOG:
                h = wx_getdlgitem(hdlg, WX_ID("IDC_HDTYPE"));
                wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"Custom type");
                for (c = 1; c <= 46; c++)
                {
                        sprintf(s, "Type %02i : cylinders=%i, heads=%i, size=%iMB", c, hd_types[c-1].cylinders, hd_types[c-1].heads, (hd_types[c-1].cylinders * hd_types[c-1].heads * 17 * 512) / (1024 * 1024));
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)s);
                }
                wx_sendmessage(h, WX_CB_SETCURSEL, hd_new_type, 0);

                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT1"));
                sprintf(s, "%i", hd_new_spt);
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT2"));
                sprintf(s, "%i", hd_new_hpc);
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT3"));
                sprintf(s, "%i", hd_new_cyl);
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);

                h = wx_getdlgitem(hdlg, WX_ID("IDC_TEXT1"));
                sprintf(s, "Size : %imb", (int)((((uint64_t)hd_new_spt*(uint64_t)hd_new_hpc*(uint64_t)hd_new_cyl)*512)/1024)/1024);
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);

                return TRUE;
                case WX_COMMAND:
                if (wParam == wxID_OK) {
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT1"));
                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                        sscanf(s, "%i", &hd_new_spt);
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT2"));
                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                        sscanf(s, "%i", &hd_new_hpc);
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT3"));
                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                        sscanf(s, "%i", &hd_new_cyl);

                        if (hd_new_spt > 63)
                        {
                                wx_messagebox(NULL,"Drive has too many sectors (maximum is 63)","PCem error",WX_MB_OK);
                                return TRUE;
                        }
                        if (hd_new_hpc > 16)
                        {
                                wx_messagebox(NULL,"Drive has too many heads (maximum is 16)","PCem error",WX_MB_OK);
                                return TRUE;
                        }
                        if (hd_new_cyl > 16383)
                        {
                                wx_messagebox(NULL,"Drive has too many cylinders (maximum is 16383)","PCem error",WX_MB_OK);
                                return TRUE;
                        }

                        wx_enddialog(hdlg,1);
                        return TRUE;
                } else if (wParam == wxID_CANCEL) {
                        wx_enddialog(hdlg,0);
                        return TRUE;
                } else if (ID_IS("IDC_EDIT1") || ID_IS("IDC_EDIT2") || ID_IS("IDC_EDIT3")) {
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT1"));
                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                        sscanf(s, "%i", &hd[0].spt);
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT2"));
                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                        sscanf(s, "%i", &hd[0].hpc);
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT3"));
                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                        sscanf(s, "%i", &hd[0].tracks);

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_TEXT1"));
                        sprintf(s, "Size : %imb", (int)(((((uint64_t)hd[0].tracks*(uint64_t)hd[0].hpc)*(uint64_t)hd[0].spt)*512)/1024)/1024);
                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                        return TRUE;
                }

        }
        return FALSE;
}

static int hdconf_dlgproc(void* hdlg, int message, INT_PARAM wParam, LONG_PARAM lParam)
{
        char s[260];
        void* h;
        PcemHDC hd[4];
        FILE *f;
        off64_t sz;
        switch (message)
        {
                case WX_INITDIALOG:
                pause = 1;
                hd[0] = hdc[0];
                hd[1] = hdc[1];
                hd[2] = hdc[2];
                hd[3] = hdc[3];
                hd_changed = 0;

                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_SPT[0]"));
                sprintf(s, "%i", hdc[0].spt);
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_HPC[0]"));
                sprintf(s, "%i", hdc[0].hpc);
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_CYL[0]"));
                sprintf(s, "%i", hdc[0].tracks);
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_FN[0]"));
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)ide_fn[0]);

                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_SPT[1]"));
                sprintf(s, "%i", hdc[1].spt);
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_HPC[1]"));
                sprintf(s, "%i", hdc[1].hpc);
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_CYL[1]"));
                sprintf(s, "%i", hdc[1].tracks);
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                h=  wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_FN[1]"));
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)ide_fn[1]);

                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_SPT[2]"));
                sprintf(s, "%i", hdc[2].spt);
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_HPC[2]"));
                sprintf(s, "%i", hdc[2].hpc);
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_CYL[2]"));
                sprintf(s, "%i", hdc[2].tracks);
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                h=  wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_FN[2]"));
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)ide_fn[2]);

                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_SPT[3]"));
                sprintf(s, "%i", hdc[3].spt);
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_HPC[3]"));
                sprintf(s, "%i", hdc[3].hpc);
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_CYL[3]"));
                sprintf(s, "%i", hdc[3].tracks);
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                h=  wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_FN[3]"));
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)ide_fn[3]);

                h = wx_getdlgitem(hdlg, WX_ID("IDC_TEXT_SIZE[0]"));
                sprintf(s, "Size : %imb", (int)(((((uint64_t)hd[0].tracks*(uint64_t)hd[0].hpc)*(uint64_t)hd[0].spt)*512)/1024)/1024);
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);

                h = wx_getdlgitem(hdlg, WX_ID("IDC_TEXT_SIZE[1]"));
                sprintf(s, "Size : %imb", (int)(((((uint64_t)hd[1].tracks*(uint64_t)hd[1].hpc)*(uint64_t)hd[1].spt)*512)/1024)/1024);
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);

                h = wx_getdlgitem(hdlg, WX_ID("IDC_TEXT_SIZE[2]"));
                sprintf(s, "Size : %imb", (int)(((((uint64_t)hd[2].tracks*(uint64_t)hd[2].hpc)*(uint64_t)hd[2].spt)*512)/1024)/1024);
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);

                h = wx_getdlgitem(hdlg, WX_ID("IDC_TEXT_SIZE[3]"));
                sprintf(s, "Size : %imb", (int)(((((uint64_t)hd[3].tracks*(uint64_t)hd[3].hpc)*(uint64_t)hd[3].spt)*512)/1024)/1024);
                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);

                new_cdrom_channel = cdrom_channel;

                update_hdd_cdrom(hdlg);

                if (hdd_controller_current_is_mfm()) {
                        wx_enablewindow(wx_getdlgitem(hdlg, WX_ID("IDC_PANEL[2]")), 0);
                        wx_enablewindow(wx_getdlgitem(hdlg, WX_ID("IDC_PANEL[3]")), 0);
                        wx_enablewindow(wx_getdlgitem(hdlg, WX_ID("IDC_HDD[0]")), 0);
                        wx_enablewindow(wx_getdlgitem(hdlg, WX_ID("IDC_HDD[1]")), 0);
                        wx_enablewindow(wx_getdlgitem(hdlg, WX_ID("IDC_CDROM[0]")), 0);
                        wx_enablewindow(wx_getdlgitem(hdlg, WX_ID("IDC_CDROM[1]")), 0);
                }
                return TRUE;

                case WX_COMMAND:
                if (wParam == wxID_OK)
                {
                        if (hd_changed || cdrom_channel != new_cdrom_channel)
                        {
                                if (!has_been_inited || confirm())
                                {
                                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_SPT[0]"));
                                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                                        sscanf(s, "%i", &hd[0].spt);
                                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_HPC[0]"));
                                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                                        sscanf(s, "%i", &hd[0].hpc);
                                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_CYL[0]"));
                                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                                        sscanf(s, "%i", &hd[0].tracks);
                                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_FN[0]"));
                                        wx_sendmessage(h, WX_WM_GETTEXT, 511, (LONG_PARAM)ide_fn[0]);

                                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_SPT[1]"));
                                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                                        sscanf(s, "%i", &hd[1].spt);
                                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_HPC[1]"));
                                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                                        sscanf(s, "%i", &hd[1].hpc);
                                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_CYL[1]"));
                                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                                        sscanf(s, "%i", &hd[1].tracks);
                                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_FN[1]"));
                                        wx_sendmessage(h, WX_WM_GETTEXT, 511, (LONG_PARAM)ide_fn[1]);

                                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_SPT[2]"));
                                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                                        sscanf(s, "%i", &hd[2].spt);
                                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_HPC[2]"));
                                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                                        sscanf(s, "%i", &hd[2].hpc);
                                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_CYL[2]"));
                                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                                        sscanf(s, "%i", &hd[2].tracks);
                                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_FN[2]"));
                                        wx_sendmessage(h, WX_WM_GETTEXT, 511, (LONG_PARAM)ide_fn[2]);

                                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_SPT[3]"));
                                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                                        sscanf(s, "%i", &hd[3].spt);
                                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_HPC[3]"));
                                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                                        sscanf(s, "%i", &hd[3].hpc);
                                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_CYL[3]"));
                                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                                        sscanf(s, "%i", &hd[3].tracks);
                                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_FN[3]"));
                                        wx_sendmessage(h, WX_WM_GETTEXT, 511, (LONG_PARAM)ide_fn[3]);

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
                }
                if (wParam == wxID_OK || wParam == wxID_CANCEL)
                {
                        wx_enddialog(hdlg, 0);
                        pause = 0;
                        return TRUE;
                } else if (ID_IS("IDC_EJECT[0]"))
                {
                        hd[0].spt = 0;
                        hd[0].hpc = 0;
                        hd[0].tracks = 0;
                        ide_fn[0][0] = 0;
                        wx_setdlgitemtext(hdlg, WX_ID("IDC_EDIT_SPT[0]"), "0");
                        wx_setdlgitemtext(hdlg, WX_ID("IDC_EDIT_HPC[0]"), "0");
                        wx_setdlgitemtext(hdlg, WX_ID("IDC_EDIT_CYL[0]"), "0");
                        wx_setdlgitemtext(hdlg, WX_ID("IDC_EDIT_FN[0]"), "");
                        hd_changed = 1;
                        return TRUE;
                } else if (ID_IS("IDC_EJECT[1]"))
                {
                        hd[1].spt = 0;
                        hd[1].hpc = 0;
                        hd[1].tracks = 0;
                        ide_fn[1][0] = 0;
                        wx_setdlgitemtext(hdlg, WX_ID("IDC_EDIT_SPT[1]"), "0");
                        wx_setdlgitemtext(hdlg, WX_ID("IDC_EDIT_HPC[1]"), "0");
                        wx_setdlgitemtext(hdlg, WX_ID("IDC_EDIT_CYL[1]"), "0");
                        wx_setdlgitemtext(hdlg, WX_ID("IDC_EDIT_FN[1]"), "");
                        hd_changed = 1;
                        return TRUE;
                } else if (ID_IS("IDC_EJECT[2]"))
                {
                        hd[2].spt = 0;
                        hd[2].hpc = 0;
                        hd[2].tracks = 0;
                        ide_fn[2][0] = 0;
                        wx_setdlgitemtext(hdlg, WX_ID("IDC_EDIT_SPT[2]"), "0");
                        wx_setdlgitemtext(hdlg, WX_ID("IDC_EDIT_HPC[2]"), "0");
                        wx_setdlgitemtext(hdlg, WX_ID("IDC_EDIT_CYL[2]"), "0");
                        wx_setdlgitemtext(hdlg, WX_ID("IDC_EDIT_FN[2]"), "");
                        hd_changed = 1;
                        return TRUE;
                } else if (ID_IS("IDC_EJECT[3]"))
                {
                        hd[3].spt = 0;
                        hd[3].hpc = 0;
                        hd[3].tracks = 0;
                        ide_fn[3][0] = 0;
                        wx_setdlgitemtext(hdlg, WX_ID("IDC_EDIT_SPT[3]"), "0");
                        wx_setdlgitemtext(hdlg, WX_ID("IDC_EDIT_HPC[3]"), "0");
                        wx_setdlgitemtext(hdlg, WX_ID("IDC_EDIT_CYL[3]"), "0");
                        wx_setdlgitemtext(hdlg, WX_ID("IDC_EDIT_FN[3]"), "");
                        hd_changed = 1;
                        return TRUE;
                }
                else if (ID_IS("IDC_NEW[0]"))
                {
                        if (wx_dialogbox(hdlg, "HdNewDlg", hdnew_dlgproc) == 1)
                        {
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_SPT[0]"));
                                sprintf(s, "%i", hd_new_spt);
                                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_HPC[0]"));
                                sprintf(s, "%i", hd_new_hpc);
                                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_CYL[0]"));
                                sprintf(s, "%i", hd_new_cyl);
                                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_FN[0]"));
                                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)hd_new_name);

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_TEXT_SIZE[0]"));
                                sprintf(s, "Size : %imb", (int)(((((uint64_t)hd_new_cyl*(uint64_t)hd_new_hpc)*(uint64_t)hd_new_spt)*512)/1024)/1024);
                                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);

                                hd_changed = 1;
                        }
                        return TRUE;
                }
                else if (ID_IS("IDC_FILE[0]"))
                {
                        if (!getfile(hdlg, "Hard disc image (*.img)|*.img|All files (*.*)|*.*", ""))
                        {
                                f = fopen64(openfilestring, "rb");
                                if (!f)
                                {
                                        wx_messagebox(NULL,"Can't open file for read","PCem error",WX_MB_OK);
                                        return TRUE;
                                }
                                fseeko64(f, -1, SEEK_END);
                                sz = ftello64(f) + 1;
                                fclose(f);
                                check_hd_type(sz);

                                if (wx_dialogbox(hdlg, "HdSizeDlg", hdsize_dlgproc) == 1)
                                {
                                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_SPT[0]"));
                                        sprintf(s, "%i", hd_new_spt);
                                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_HPC[0]"));
                                        sprintf(s, "%i", hd_new_hpc);
                                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_CYL[0]"));
                                        sprintf(s, "%i", hd_new_cyl);
                                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_FN[0]"));
                                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)openfilestring);

                                        h=  wx_getdlgitem(hdlg, WX_ID("IDC_TEXT_SIZE[0]"));
                                        sprintf(s, "Size : %imb", (int)(((((uint64_t)hd_new_cyl*(uint64_t)hd_new_hpc)*(uint64_t)hd_new_spt)*512)/1024)/1024);
                                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);

                                        hd_changed = 1;
                                }
                        }
                        return TRUE;

                }
                else if (ID_IS("IDC_NEW[1]"))
                {
                        if (wx_dialogbox(hdlg, "HdNewDlg", hdnew_dlgproc) == 1)
                        {
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_SPT[1]"));
                                sprintf(s, "%i", hd_new_spt);
                                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_HPC[1]"));
                                sprintf(s, "%i", hd_new_hpc);
                                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_CYL[1]"));
                                sprintf(s, "%i", hd_new_cyl);
                                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_FN[1]"));
                                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)hd_new_name);

                                h=  wx_getdlgitem(hdlg, WX_ID("IDC_TEXT_SIZE[1]"));
                                sprintf(s, "Size : %imb", (int)(((((uint64_t)hd_new_cyl*(uint64_t)hd_new_hpc)*(uint64_t)hd_new_spt)*512)/1024)/1024);
                                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);

                                hd_changed = 1;
                        }
                        return TRUE;
                }
                else if (ID_IS("IDC_FILE[1]"))
                {
                        if (!getfile(hdlg, "Hard disc image (*.img)|*.img|All files (*.*)|*.*", ""))
                        {
                                f = fopen64(openfilestring, "rb");
                                if (!f)
                                {
                                        wx_messagebox(NULL,"Can't open file for read","PCem error",WX_MB_OK);
                                        return TRUE;
                                }
                                fseeko64(f, -1, SEEK_END);
                                sz = ftello64(f) + 1;
                                fclose(f);
                                check_hd_type(sz);

                                if (wx_dialogbox(hdlg, "HdSizeDlg", hdsize_dlgproc) == 1)
                                {
                                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_SPT[1]"));
                                        sprintf(s, "%i", hd_new_spt);
                                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_HPC[1]"));
                                        sprintf(s, "%i", hd_new_hpc);
                                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_CYL[1]"));
                                        sprintf(s, "%i", hd_new_cyl);
                                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_FN[1]"));
                                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)openfilestring);

                                        h = wx_getdlgitem(hdlg, WX_ID("IDC_TEXT_SIZE[1]"));
                                        sprintf(s, "Size : %imb", (int)(((((uint64_t)hd_new_cyl*(uint64_t)hd_new_hpc)*(uint64_t)hd_new_spt)*512)/1024)/1024);
                                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);

                                        hd_changed = 1;
                                }
                        }
                        return TRUE;
                }
                else if (ID_IS("IDC_NEW[2]"))
                {
                        if (wx_dialogbox(hdlg, "HdNewDlg", hdnew_dlgproc) == 1)
                        {
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_SPT[2]"));
                                sprintf(s, "%i", hd_new_spt);
                                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_HPC[2]"));
                                sprintf(s, "%i", hd_new_hpc);
                                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_CYL[2]"));
                                sprintf(s, "%i", hd_new_cyl);
                                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_FN[2]"));
                                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)hd_new_name);

                                h=  wx_getdlgitem(hdlg, WX_ID("IDC_TEXT_SIZE[2]"));
                                sprintf(s, "Size : %imb", (int)(((((uint64_t)hd_new_cyl*(uint64_t)hd_new_hpc)*(uint64_t)hd_new_spt)*512)/1024)/1024);
                                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);

                                hd_changed = 1;
                        }
                        return TRUE;
                }
                else if (ID_IS("IDC_FILE[2]"))
                {
                        if (!getfile(hdlg, "Hard disc image (*.img)|*.img|All files (*.*)|*.*", ""))
                        {
                                f = fopen64(openfilestring, "rb");
                                if (!f)
                                {
                                        wx_messagebox(NULL,"Can't open file for read","PCem error",WX_MB_OK);
                                        return TRUE;
                                }
                                fseeko64(f, -1, SEEK_END);
                                sz = ftello64(f) + 1;
                                fclose(f);
                                check_hd_type(sz);

                                if (wx_dialogbox(hdlg, "HdSizeDlg", hdsize_dlgproc) == 1)
                                {
                                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_SPT[2]"));
                                        sprintf(s, "%i", hd_new_spt);
                                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_HPC[2]"));
                                        sprintf(s, "%i", hd_new_hpc);
                                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_CYL[2]"));
                                        sprintf(s, "%i", hd_new_cyl);
                                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_FN[2]"));
                                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)openfilestring);

                                        h = wx_getdlgitem(hdlg, WX_ID("IDC_TEXT_SIZE[2]"));
                                        sprintf(s, "Size : %imb", (int)(((((uint64_t)hd_new_cyl*(uint64_t)hd_new_hpc)*(uint64_t)hd_new_spt)*512)/1024)/1024);
                                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);

                                        hd_changed = 1;
                                }
                        }
                        return TRUE;
                }
                else if (ID_IS("IDC_NEW[3]"))
                {
                        if (wx_dialogbox(hdlg, "HdNewDlg", hdnew_dlgproc) == 1)
                        {
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_SPT[3]"));
                                sprintf(s, "%i", hd_new_spt);
                                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_HPC[3]"));
                                sprintf(s, "%i", hd_new_hpc);
                                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_CYL[3]"));
                                sprintf(s, "%i", hd_new_cyl);
                                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_FN[3]"));
                                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)hd_new_name);

                                h=  wx_getdlgitem(hdlg, WX_ID("IDC_TEXT_SIZE[3]"));
                                sprintf(s, "Size : %imb", (int)(((((uint64_t)hd_new_cyl*(uint64_t)hd_new_hpc)*(uint64_t)hd_new_spt)*512)/1024)/1024);
                                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);

                                hd_changed = 1;
                        }
                        return TRUE;
                }
                else if (ID_IS("IDC_FILE[3]"))
                {
                        if (!getfile(hdlg, "Hard disc image (*.img)|*.img|All files (*.*)|*.*", ""))
                        {
                                f = fopen64(openfilestring, "rb");
                                if (!f)
                                {
                                        wx_messagebox(NULL,"Can't open file for read","PCem error",WX_MB_OK);
                                        return TRUE;
                                }
                                fseeko64(f, -1, SEEK_END);
                                sz = ftello64(f) + 1;
                                fclose(f);
                                check_hd_type(sz);

                                if (wx_dialogbox(hdlg, "HdSizeDlg", hdsize_dlgproc) == 1)
                                {
                                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_SPT[3]"));
                                        sprintf(s, "%i", hd_new_spt);
                                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_HPC[3]"));
                                        sprintf(s, "%i", hd_new_hpc);
                                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_CYL[3]"));
                                        sprintf(s, "%i", hd_new_cyl);
                                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_FN[3]"));
                                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)openfilestring);

                                        h = wx_getdlgitem(hdlg, WX_ID("IDC_TEXT_SIZE[3]"));
                                        sprintf(s, "Size : %imb", (int)(((((uint64_t)hd_new_cyl*(uint64_t)hd_new_hpc)*(uint64_t)hd_new_spt)*512)/1024)/1024);
                                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);

                                        hd_changed = 1;
                                }
                        }
                        return TRUE;
                }
                else if (ID_IS("IDC_EDIT_SPT[0]") || ID_IS("IDC_EDIT_HPC[0]") || ID_IS("IDC_EDIT_CYL[0]"))
                {
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_SPT[0]"));
                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                        sscanf(s, "%i", &hd[0].spt);
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_HPC[0]"));
                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                        sscanf(s, "%i", &hd[0].hpc);
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_CYL[0]"));
                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                        sscanf(s, "%i", &hd[0].tracks);

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_TEXT_SIZE[0]"));
                        sprintf(s, "Size : %imb", ((((hd[0].tracks*hd[0].hpc)*hd[0].spt)*512)/1024)/1024);
                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                        return TRUE;
                }
                else if (ID_IS("IDC_EDIT_SPT[1]") || ID_IS("IDC_EDIT_HPC[1]") || ID_IS("IDC_EDIT_CYL[1]"))
                {
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_SPT[1]"));
                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                        sscanf(s, "%i", &hd[1].spt);
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_HPC[1]"));
                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                        sscanf(s, "%i", &hd[1].hpc);
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_CYL[1]"));
                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                        sscanf(s, "%i", &hd[1].tracks);

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_TEXT_SIZE[1]"));
                        sprintf(s, "Size : %imb", ((((hd[1].tracks*hd[1].hpc)*hd[1].spt)*512)/1024)/1024);
                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                        return TRUE;
                }
                else if (ID_IS("IDC_EDIT_SPT[2]") || ID_IS("IDC_EDIT_HPC[2]") || ID_IS("IDC_EDIT_CYL[2]"))
                {
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_SPT[2]"));
                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                        sscanf(s, "%i", &hd[2].spt);
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_HPC[2]"));
                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                        sscanf(s, "%i", &hd[2].hpc);
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_CYL[2]"));
                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                        sscanf(s, "%i", &hd[2].tracks);

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_TEXT_SIZE[2]"));
                        sprintf(s, "Size : %imb", ((((hd[2].tracks*hd[2].hpc)*hd[2].spt)*512)/1024)/1024);
                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                        return TRUE;
                }
                else if (ID_IS("IDC_EDIT_SPT[3]") || ID_IS("IDC_EDIT_HPC[3]") || ID_IS("IDC_EDIT_CYL[3]"))
                {
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_SPT[3]"));
                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                        sscanf(s, "%i", &hd[3].spt);
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_HPC[3]"));
                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                        sscanf(s, "%i", &hd[3].hpc);
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_EDIT_CYL[3]"));
                        wx_sendmessage(h, WX_WM_GETTEXT, 255, (LONG_PARAM)s);
                        sscanf(s, "%i", &hd[3].tracks);

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_TEXT_SIZE[3]"));
                        sprintf(s, "Size : %imb", ((((hd[3].tracks*hd[3].hpc)*hd[3].spt)*512)/1024)/1024);
                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                        return TRUE;
                }
                else if (ID_IS("IDC_HDD[0]"))
                {
                        if (new_cdrom_channel == 0)
                                new_cdrom_channel = -1;
                        update_hdd_cdrom(hdlg);
                        return TRUE;
                }
                else if (ID_IS("IDC_HDD[1]"))
                {
                        if (new_cdrom_channel == 1)
                                new_cdrom_channel = -1;
                        update_hdd_cdrom(hdlg);
                        return TRUE;
                }
                else if (ID_IS("IDC_HDD[2]"))
                {
                        if (new_cdrom_channel == 2)
                                new_cdrom_channel = -1;
                        update_hdd_cdrom(hdlg);
                        return TRUE;
                }
                else if (ID_IS("IDC_HDD[3]"))
                {
                        if (new_cdrom_channel == 3)
                                new_cdrom_channel = -1;
                        update_hdd_cdrom(hdlg);
                        return TRUE;

                }
                else if (ID_IS("IDC_CDROM[0]"))
                {
                        new_cdrom_channel = 0;
                        update_hdd_cdrom(hdlg);
                        return TRUE;
                }
                else if (ID_IS("IDC_CDROM[1]"))
                {
                        new_cdrom_channel = 1;
                        update_hdd_cdrom(hdlg);
                        return TRUE;
                }
                else if (ID_IS("IDC_CDROM[2]"))
                {
                        new_cdrom_channel = 2;
                        update_hdd_cdrom(hdlg);
                        return TRUE;
                }
                else if (ID_IS("IDC_CDROM[3]"))
                {
                        new_cdrom_channel = 3;
                        update_hdd_cdrom(hdlg);
                        return TRUE;
                }

        }
        return FALSE;
}

void hdconf_open(void* hwnd)
{
        wx_dialogbox(hwnd, "HdConfDlg", hdconf_dlgproc);
}
