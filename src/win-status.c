#define BITMAP WINDOWS_BITMAP
#include <windows.h>
#include <windowsx.h>
#undef BITMAP

#include "ibm.h"
#include "device.h"
#include "video.h"
#include "resources.h"
#include "win.h"

HWND status_hwnd;
int status_is_open = 0;

extern int sreadlnum, swritelnum, segareads, segawrites, scycles_lost;

static BOOL CALLBACK status_dlgproc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
{
        char s[256];
        char device_s[4096];
        switch (message)
        {
                case WM_INITDIALOG:
                status_is_open = 1;
                case WM_USER:
                sprintf(device_s,
                        "CPU speed : %f MIPS\n"
                        "FPU speed : %f MFLOPS\n\n"
                        "Cache misses (read) : %i/sec\n"
                        "Cache misses (write) : %i/sec\n\n"
                        "Video throughput (read) : %i bytes/sec\n\n"
                        "Video throughput (write) : %i bytes/sec\n\n"
                        "Effective clockspeed : %iHz\n\n"
                        "Timer 0 frequency : %fHz\n\n",
                        mips,
                        flops,
                        sreadlnum,
                        swritelnum,
                        segareads,
                        segawrites,
                        clockrate - (sreadlnum*memwaitstate) - (swritelnum*memwaitstate) - scycles_lost,
                        pit_timer0_freq()
                );
                
                device_add_status_info(device_s, 4096);
                SendDlgItemMessage(hdlg, IDC_STEXT_DEVICE, WM_SETTEXT, (WPARAM)NULL, (LPARAM)device_s);
                return TRUE;
                
                case WM_COMMAND:
                switch (LOWORD(wParam))
                {
                        case IDOK:
                        case IDCANCEL:
                        status_is_open = 0;
                        EndDialog(hdlg, 0);
                        return TRUE;
                }
                break;
        }
        return FALSE;
}

void status_open(HWND hwnd)
{
        status_hwnd = CreateDialog(hinstance, TEXT("StatusDlg"), hwnd, status_dlgproc);
        ShowWindow(status_hwnd, SW_SHOW);
}
