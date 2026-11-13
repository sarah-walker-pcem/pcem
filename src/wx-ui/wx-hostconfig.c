#ifdef _WIN32
#define BITMAP WINDOWS_BITMAP
#include <windows.h>
#include <windowsx.h>
#undef BITMAP
#endif

#ifdef USE_PCAP_NETWORKING
#include <pcap.h>
#include <stdlib.h>
#include <string.h>
#endif

#include "wx-utils.h"
#include "wx-sdl2.h"
#include "wx-hostconfig.h"

#include "ibm.h"
#include "config.h"
#include "nethandler.h"

#ifdef USE_PCAP_NETWORKING
#define MAX_PCAP_DEVICES 64
static char *dev_names[MAX_PCAP_DEVICES];
static int dev_count = 0;

static void free_dev_names() {
        int i;

        for (i = 0; i < dev_count; i++) {
                free(dev_names[i]);
                dev_names[i] = NULL;
        }
        dev_count = 0;
}
#endif

#define ETH_DEV_NAME_MAX 256 /* maximum device name size */
#define ETH_DEV_DESC_MAX 256 /* maximum device description size */
#define ETH_MAX_DEVICE 30    /* maximum ethernet devices */
#define ETH_PROMISC 1        /* promiscuous mode = true */
#define ETH_MAX_PACKET 1514  /* maximum ethernet packet size */
#define PCAP_READ_TIMEOUT 15

#ifdef USE_PCAP_NETWORKING
static int get_network_name(char *dev_name, char *regval) {
#if _WIN32
        if (dev_name[strlen("\\Device\\NPF_")] == '{') {
                char regkey[2048];
                HKEY reghnd;

                sprintf(regkey,
                        "SYSTEM\\CurrentControlSet\\Control\\Network\\"
                        "{4D36E972-E325-11CE-BFC1-08002BE10318}\\%s\\Connection",
                        dev_name + strlen("\\Device\\NPF_"));

                if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, regkey, 0, KEY_QUERY_VALUE, &reghnd) == ERROR_SUCCESS) {
                        DWORD reglen = 2048;
                        DWORD regtype;

                        if (RegQueryValueExA(reghnd, "Name", NULL, &regtype, (LPBYTE)regval, &reglen) == ERROR_SUCCESS) {
                                RegCloseKey(reghnd);

                                if ((regtype != REG_SZ) || (reglen > 2048))
                                        return -1;

                                /*Name now in regval*/
                                return 0;
                        }
                        RegCloseKey(reghnd);
                }
        }
#endif
        return -1;
}
#endif
int hostconfig_dialog_proc(void *hdlg, int message, INT_PARAM wParam, LONG_PARAM lParam) {
        void *h;
#ifdef USE_PCAP_NETWORKING
        pcap_if_t *alldevs = NULL;
        pcap_if_t *dev;
        char errbuf[PCAP_ERRBUF_SIZE];
#endif

        switch (message) {
        case WX_INITDIALOG:
                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO_NETWORK_TYPE"));
                wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM) "SLiRP");
                wx_sendmessage(h, WX_CB_SETCURSEL, 0, 0);
                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO_NETWORK_DEVICE"));
                wx_sendmessage(h, WX_CB_SETCURSEL, 0, 0);
                wx_enablewindow(h, FALSE);
#ifdef USE_PCAP_NETWORKING

                int c = 0;
                int match = 0;
                char open_err[PCAP_ERRBUF_SIZE] = "";
                const char *pcap_version = pcap_lib_version();
                pclog("PCAP library version: %s\n", pcap_version);

                if (pcap_findalldevs(&alldevs, errbuf) == 0) {

                        char *pcap_device = config_get_string(CFG_GLOBAL, NULL, "pcap_device", "nothing");

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO_NETWORK_DEVICE"));
                        free_dev_names();

                        for (dev = alldevs; dev && c < MAX_PCAP_DEVICES; dev = dev->next) {
                                pcap_t *conn = pcap_open_live(dev->name, ETH_MAX_PACKET, ETH_PROMISC, PCAP_READ_TIMEOUT, errbuf);
                                int datalink = 0;

                                if (conn) {
                                        datalink = pcap_datalink(conn);
                                        pcap_close(conn);
                                } else if (!open_err[0]) {
                                        errbuf[PCAP_ERRBUF_SIZE - 1] = '\0';
                                        strcpy(open_err, errbuf);
                                }

                                if (conn && datalink == DLT_EN10MB) {
                                        char desc[2048];
                                        char s[2048];

                                        if ((dev->flags & PCAP_IF_LOOPBACK) || (!strcmp("any", dev->name)))
                                                continue;

                                        if (pcap_device) {
                                                if (!strcmp(pcap_device, dev->name))
                                                        match = c;
                                        }

                                        dev_names[c] = strdup(dev->name);
                                        if (!dev_names[c])
                                                continue;
                                        c++;
                                        dev_count = c;
                                        if (get_network_name(dev->name, desc))
                                                wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)dev->name);
                                        else {
                                                sprintf(s, "%s - %s", dev->name, desc);
                                                wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)s);
                                        }
                                }
                        }
                } else {
                        errbuf[PCAP_ERRBUF_SIZE - 1] = '\0';
                        error("Error in pcap_findalldevs: %s\n", errbuf);
                }
                if (alldevs != NULL) {
                        pcap_freealldevs(alldevs);
                }
                if (!c && open_err[0]) {
                        pclog("PCAP: no usable devices: %s\n", open_err);
                }
                if (c) {
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO_NETWORK_TYPE"));
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM) "PCAP");

                        if (config_get_int(CFG_GLOBAL, NULL, "net_type", NET_SLIRP) == NET_PCAP) {
                                wx_sendmessage(h, WX_CB_SETCURSEL, 1, 0);
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO_NETWORK_DEVICE"));
                                wx_sendmessage(h, WX_CB_SETCURSEL, match, 0);
                                wx_enablewindow(h, TRUE);
                        }
                }

#endif
                return TRUE;

        case WX_COMMAND:
                if (wParam == wxID_OK) {
                        int type;

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO_NETWORK_TYPE"));
                        type = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
#ifdef USE_PCAP_NETWORKING
                        if (type) /*PCAP*/
                        {
                                int idev;

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO_NETWORK_DEVICE"));
                                idev = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);

                                if (idev >= 0 && idev < dev_count && dev_names[idev]) {
                                        config_set_int(CFG_GLOBAL, NULL, "net_type", NET_PCAP);
                                        config_set_string(CFG_GLOBAL, NULL, "pcap_device", dev_names[idev]);
                                } else {
                                        config_set_int(CFG_GLOBAL, NULL, "net_type", NET_SLIRP);
                                        config_set_string(CFG_GLOBAL, NULL, "pcap_device", "nothing");
                                }
                        } else /*SLiRP*/
                        {
#endif
                                config_set_int(CFG_GLOBAL, NULL, "net_type", NET_SLIRP);
                                config_set_string(CFG_GLOBAL, NULL, "pcap_device", "nothing");
#ifdef USE_PCAP_NETWORKING
                        }
#endif
                        saveconfig_global_only();

                        wx_enddialog(hdlg, 1);

                        return TRUE;
                } else if (wParam == wxID_CANCEL) {
                        wx_enddialog(hdlg, 0);

                        return TRUE;
                } else if (ID_IS("IDC_COMBO_NETWORK_TYPE")) {
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

int hostconfig_open(void *hwnd) { return wx_dialogbox(hwnd, "HostConfig", hostconfig_dialog_proc); }
