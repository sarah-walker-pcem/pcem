#ifdef _WIN32
#define BITMAP WINDOWS_BITMAP
#include <windows.h>
#include <windowsx.h>
#undef BITMAP

#include <pcap.h>
#endif

#include "wx-utils.h"
#include "wx-sdl2.h"
#include "wx-hostconfig.h"

#include "ibm.h"
#include "config.h"
#include "nethandler.h"

#ifdef _WIN32
static HINSTANCE net_hLib = 0;
static char *net_lib_name = "wpcap.dll";
static pcap_if_t *alldevs;
static char *dev_name[20];
        
int     (*_pcap_findalldevs)(pcap_if_t **, char *);
void    (*_pcap_freealldevs)(pcap_if_t *);
pcap_t *(*_pcap_open_live)(const char *, int, int, int, char *);
int     (*_pcap_datalink)(pcap_t *);
void    (*_pcap_close)(pcap_t *);
#endif

#define ETH_DEV_NAME_MAX     256                        /* maximum device name size */
#define ETH_DEV_DESC_MAX     256                        /* maximum device description size */
#define ETH_MAX_DEVICE        30                        /* maximum ethernet devices */
#define ETH_PROMISC            1                        /* promiscuous mode = true */
#define ETH_MAX_PACKET      1514                        /* maximum ethernet packet size */
#define PCAP_READ_TIMEOUT -1

#ifdef _WIN32
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
#endif
int hostconfig_dialog_proc(void *hdlg, int message, INT_PARAM wParam, LONG_PARAM lParam)
{
        void *h;
#ifdef _WIN32
        pcap_if_t *dev;
        char errbuf[PCAP_ERRBUF_SIZE];
#endif
        
        switch (message)
        {
                case WX_INITDIALOG:
                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO_NETWORK_TYPE"));
                wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"SLiRP");
                wx_sendmessage(h, WX_CB_SETCURSEL, 0, 0);
                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO_NETWORK_DEVICE"));
                wx_sendmessage(h, WX_CB_SETCURSEL, 0, 0);
                wx_enablewindow(h, FALSE);
#ifdef _WIN32
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
                                
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO_NETWORK_DEVICE"));

                                for (dev = alldevs; dev; dev = dev->next)
                                {
                                        pcap_t *conn = _pcap_open_live(dev->name, ETH_MAX_PACKET, ETH_PROMISC, PCAP_READ_TIMEOUT, errbuf);
                                        int datalink = 0;
                                        
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
                                                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)dev->name);
                                                else
                                                {
                                                        sprintf(s, "%s - %s", dev->name, desc);
                                                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)s);
                                                }
                                        }
                                }
                        }
                        if (c)
                        {
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO_NETWORK_TYPE"));
                                wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"PCAP");

                                if (config_get_int(CFG_GLOBAL, NULL, "net_type", NET_SLIRP) == NET_PCAP)
                                {
                                        wx_sendmessage(h, WX_CB_SETCURSEL, 1, 0);
                                        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO_NETWORK_DEVICE"));
                                        wx_sendmessage(h, WX_CB_SETCURSEL, match, 0);
                                        wx_enablewindow(h, TRUE);
                                }
                        }
                }
#endif
                return TRUE;

                case WX_COMMAND:
                if (wParam == wxID_OK)
                {
                        int type;
                        
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO_NETWORK_TYPE"));
                        type = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
#ifdef _WIN32
                        if (type) /*PCAP*/
                        {
                                int dev;
                                
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO_NETWORK_DEVICE"));
                                dev = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
                                
                                config_set_int(CFG_GLOBAL, NULL, "net_type", NET_PCAP);
                                config_set_string(CFG_GLOBAL, NULL, "pcap_device", dev_name[dev]);
                        }
                        else /*SLiRP*/
                        {
#endif
                                config_set_int(CFG_GLOBAL, NULL, "net_type", NET_SLIRP);
                                config_set_string(CFG_GLOBAL, NULL, "pcap_device", "nothing");
#ifdef _WIN32
                        }
#endif
                        saveconfig_global_only();

                        wx_enddialog(hdlg, 1);
#ifdef _WIN32
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
#endif
                        return TRUE;
                }
                else if (wParam == wxID_CANCEL)
                {
                        wx_enddialog(hdlg, 0);
#ifdef _WIN32
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
#endif
                        return TRUE;
                }
                else if (ID_IS("IDC_COMBO_NETWORK_TYPE"))
                {
                        int type;
                                
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO_NETWORK_TYPE"));
                        type = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO_NETWORK_DEVICE"));
                        if (!type)
                                wx_enablewindow(h, FALSE);
                        else
                                wx_enablewindow(h, TRUE);
                                
                        return TRUE;
                }
                break;
        }

        return TRUE;
}

int hostconfig_open(void *hwnd)
{
        return wx_dialogbox(hwnd, "HostConfig", hostconfig_dialog_proc);
}
