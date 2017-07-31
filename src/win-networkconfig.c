#define BITMAP WINDOWS_BITMAP
#include <windows.h>
#include <windowsx.h>
#undef BITMAP

#include <commctrl.h>
#include <pcap.h>

#include "ibm.h"
#include "config.h"
#include "nethandler.h"
#include "resources.h"
#include "win.h"

static HINSTANCE net_hLib = 0;
static char *net_lib_name = "wpcap.dll";
static pcap_if_t *alldevs;
static char *dev_name[20];
        
int     (*_pcap_findalldevs)(pcap_if_t **, char *);
void    (*_pcap_freealldevs)(pcap_if_t *);
pcap_t *(*_pcap_open_live)(const char *, int, int, int, char *);
int     (*_pcap_datalink)(pcap_t *);
void    (*_pcap_close)(pcap_t *);

#define ETH_DEV_NAME_MAX     256                        /* maximum device name size */
#define ETH_DEV_DESC_MAX     256                        /* maximum device description size */
#define ETH_MAX_DEVICE        30                        /* maximum ethernet devices */
#define ETH_PROMISC            1                        /* promiscuous mode = true */
#define ETH_MAX_PACKET      1514                        /* maximum ethernet packet size */
#define PCAP_READ_TIMEOUT -1

static int get_network_name(char *dev_name, char *regval)
{
        if (dev_name[strlen( "\\Device\\NPF_" )] == '{')
        {
                char regkey[2048];
                HKEY reghnd;
                                                
                sprintf(regkey, "SYSTEM\\CurrentControlSet\\Control\\Network\\"
                        "{4D36E972-E325-11CE-BFC1-08002BE10318}\\%s\\Connection", dev_name+
                        strlen("\\Device\\NPF_"));

                if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, regkey, 0, KEY_QUERY_VALUE, &reghnd) == ERROR_SUCCESS)
                {
                        DWORD reglen = 2048;
                        DWORD regtype;

                        if (RegQueryValueExA(reghnd, "Name", NULL, &regtype, (LPBYTE)regval, &reglen) == ERROR_SUCCESS)
                        {
                                RegCloseKey (reghnd);
                                
                                if ((regtype != REG_SZ) || (reglen > 2048))
                                        return -1;

                                /*Name now in regval*/
                                return 0;
                        }
                        RegCloseKey (reghnd);
                }
        }
        return -1;
}
                        
static BOOL CALLBACK networkconfig_dlgproc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
{
        HWND h;
        pcap_if_t *dev;
        char errbuf[PCAP_ERRBUF_SIZE];
        int type;

        switch (message)
        {
                case WM_INITDIALOG:
                h = GetDlgItem(hdlg, IDC_COMBONETTYPE);
                SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)"SLiRP");
                SendMessage(h, CB_SETCURSEL, 0, 0);
                h = GetDlgItem(hdlg, IDC_COMBONETDEVICE);
                SendMessage(h, CB_SETCURSEL, 0, 0);
                EnableWindow(h, FALSE);
                
                net_hLib = LoadLibraryA(net_lib_name);
                
                if (net_hLib)
                {
                        int c = 0;
                        int match = 0;
                        
                        _pcap_findalldevs = (void *)GetProcAddress(net_hLib, "pcap_findalldevs");
                        _pcap_freealldevs = (void *)GetProcAddress(net_hLib, "pcap_freealldevs");
                        _pcap_open_live   = (void *)GetProcAddress(net_hLib, "pcap_open_live");
                        _pcap_datalink    = (void *)GetProcAddress(net_hLib, "pcap_datalink");
                        _pcap_close       = (void *)GetProcAddress(net_hLib, "pcap_close");

                        if (_pcap_findalldevs(&alldevs, errbuf) != -1)
                        {
                                char *pcap_device = config_get_string(CFG_GLOBAL, NULL, "pcap_device", "nothing");
                                
                                h = GetDlgItem(hdlg, IDC_COMBONETDEVICE);

                                for (dev = alldevs; dev; dev = dev->next)
                                {
                                        pcap_t *conn = _pcap_open_live(dev->name, ETH_MAX_PACKET, ETH_PROMISC, PCAP_READ_TIMEOUT, errbuf);
                                        int datalink;
                                        
                                        if (conn)
                                        {
                                                datalink = _pcap_datalink(conn);
                                                _pcap_close(conn);
                                        }
                                        
                                        if (conn && datalink == DLT_EN10MB)
                                        {
                                                char desc[2048];
                                                char s[2048];
                                                
                                                if ((dev->flags & PCAP_IF_LOOPBACK) || (!strcmp("any", dev->name)))
                                                        continue;
                                                
                                                if (pcap_device)
                                                {
                                                        if (!strcmp(pcap_device, dev->name))
                                                                match = c;
                                                }
                                                
                                                dev_name[c++] = dev->name;
                                                if (get_network_name(dev->name, desc))
                                                        SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)dev->name);
                                                else
                                                {
                                                        sprintf(s, "%s - %s", dev->name, desc);
                                                        SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)s);
                                                }
                                        }
                                }
                        }
                        if (c)
                        {
                                h = GetDlgItem(hdlg, IDC_COMBONETTYPE);
                                SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)"PCAP");

                                if (config_get_int(CFG_GLOBAL, NULL, "net_type", NET_SLIRP) == NET_PCAP)
                                {
                                        SendMessage(h, CB_SETCURSEL, 1, 0);
                                        h = GetDlgItem(hdlg, IDC_COMBONETDEVICE);
                                        SendMessage(h, CB_SETCURSEL, match, 0);
                                        EnableWindow(h, TRUE);
                                }
                        }
                }
                return TRUE;
                
                case WM_COMMAND:
                switch (LOWORD(wParam))
                {
                        case IDOK:
                        h = GetDlgItem(hdlg, IDC_COMBONETTYPE);
                        type = SendMessage(h, CB_GETCURSEL, 0, 0);
                        if (type) /*PCAP*/
                        {
                                int dev;
                                
                                h = GetDlgItem(hdlg, IDC_COMBONETDEVICE);
                                dev = SendMessage(h, CB_GETCURSEL, 0, 0);
                                
                                config_set_int(CFG_GLOBAL, NULL, "net_type", NET_PCAP);
                                config_set_string(CFG_GLOBAL, NULL, "pcap_device", dev_name[dev]);
                        }
                        else /*SLiRP*/
                        {
                                config_set_int(CFG_GLOBAL, NULL, "net_type", NET_SLIRP);
                                config_set_string(CFG_GLOBAL, NULL, "pcap_device", "nothing");
                        }
                        saveconfig_global_only();
                        case IDCANCEL:                        
                        EndDialog(hdlg, 0);
                        if (net_hLib)
                        {
                                _pcap_freealldevs(alldevs);
                                _pcap_findalldevs = NULL;
                                _pcap_freealldevs = NULL;
                                _pcap_open_live   = NULL;
                                _pcap_datalink    = NULL;
                                _pcap_close       = NULL;
                                FreeLibrary(net_hLib);
                        }
                        return TRUE;

                        case IDC_COMBONETTYPE:
                        if (HIWORD(wParam) == CBN_SELCHANGE)
                        {
                                int type;
                                
                                h = GetDlgItem(hdlg, IDC_COMBONETTYPE);
                                type = SendMessage(h, CB_GETCURSEL, 0, 0);
                                h = GetDlgItem(hdlg, IDC_COMBONETDEVICE);
                                if (!type)
                                        EnableWindow(h, FALSE);
                                else
                                        EnableWindow(h, TRUE);
                                
                                return TRUE;
                        }
                }
                break;
        }
        return FALSE;
}

void networkconfig_open(HWND hwnd)
{
        DialogBox(hinstance, TEXT("NetworkConfDlg"), hwnd, (DLGPROC)networkconfig_dlgproc);
}
