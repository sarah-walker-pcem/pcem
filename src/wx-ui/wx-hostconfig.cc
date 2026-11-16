#include "wx-hostconfig.h"

#include <string>
#include <vector>

#ifdef _WIN32
#define BITMAP WINDOWS_BITMAP
#include <windows.h>
#undef BITMAP
#endif

#ifdef USE_PCAP_NETWORKING
#include <pcap.h>
#endif

extern "C" {
#include "ibm.h"
#include "config.h"
#include "nethandler.h"
}

#define ETH_PROMISC 1
#define ETH_MAX_PACKET 1514
#define PCAP_READ_TIMEOUT 15
#define MAX_PCAP_DEVICES 64

HostConfigDialog::HostConfigDialog(wxWindow *parent) {
        wxXmlResource::Get()->LoadDialog(this, parent, "HostConfig");
        type = wx_ctrl<wxComboBox>(this, "IDC_COMBO_NETWORK_TYPE");
        device = wx_ctrl<wxComboBox>(this, "IDC_COMBO_NETWORK_DEVICE");

        type->Append("SLiRP");
        type->SetSelection(0);
        device->Enable(false);
#ifdef USE_PCAP_NETWORKING
        PopulatePcapDevices();
#endif

        Bind(wxEVT_BUTTON, &HostConfigDialog::OnOk, this, wxID_OK);
        Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_CANCEL); }, wxID_CANCEL);
        type->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent &) { device->Enable(type->GetSelection() > 0); });
        wx_fit_dialog(this);
}

#ifdef USE_PCAP_NETWORKING
bool HostConfigDialog::GetNetworkName(const char *dev_name, wxString &name) {
#ifdef _WIN32
        const char *prefix = "\\Device\\NPF_";
        if (strncmp(dev_name, prefix, strlen(prefix)) || dev_name[strlen(prefix)] != '{')
                return false;

        char regkey[2048];
        snprintf(regkey, sizeof(regkey),
                 "SYSTEM\\CurrentControlSet\\Control\\Network\\"
                 "{4D36E972-E325-11CE-BFC1-08002BE10318}\\%s\\Connection",
                 dev_name + strlen(prefix));

        HKEY reghnd;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, regkey, 0, KEY_QUERY_VALUE, &reghnd) != ERROR_SUCCESS)
                return false;

        char regval[2048];
        DWORD reglen = sizeof(regval);
        DWORD regtype;
        bool ok = RegQueryValueExA(reghnd, "Name", NULL, &regtype, (LPBYTE)regval, &reglen) == ERROR_SUCCESS &&
                  regtype == REG_SZ && reglen <= sizeof(regval);
        RegCloseKey(reghnd);
        if (ok) {
                regval[sizeof(regval) - 1] = 0;
                name = regval;
        }
        return ok;
#else
        return false;
#endif
}

void HostConfigDialog::PopulatePcapDevices() {
        pcap_if_t *alldevs = NULL;
        char errbuf[PCAP_ERRBUF_SIZE];
        char open_err[PCAP_ERRBUF_SIZE] = "";
        int match = 0;

        pclog("PCAP library version: %s\n", pcap_lib_version());

        if (pcap_findalldevs(&alldevs, errbuf) == 0) {
                const char *pcap_device = wx_config_get_string(CFG_GLOBAL, NULL, "pcap_device", "nothing");

                for (pcap_if_t *dev = alldevs; dev && dev_names.size() < MAX_PCAP_DEVICES; dev = dev->next) {
                        pcap_t *conn = pcap_open_live(dev->name, ETH_MAX_PACKET, ETH_PROMISC, PCAP_READ_TIMEOUT, errbuf);
                        int datalink = 0;

                        if (conn) {
                                datalink = pcap_datalink(conn);
                                pcap_close(conn);
                        } else if (!open_err[0]) {
                                errbuf[PCAP_ERRBUF_SIZE - 1] = '\0';
                                strcpy(open_err, errbuf);
                        }

                        if (!conn || datalink != DLT_EN10MB)
                                continue;
                        if ((dev->flags & PCAP_IF_LOOPBACK) || !strcmp("any", dev->name))
                                continue;

                        if (pcap_device && !strcmp(pcap_device, dev->name))
                                match = dev_names.size();

                        dev_names.push_back(dev->name);
                        wxString desc;
                        if (GetNetworkName(dev->name, desc))
                                device->Append(wxString::Format("%s - %s", dev->name, desc));
                        else
                                device->Append(dev->name);
                }
        } else {
                errbuf[PCAP_ERRBUF_SIZE - 1] = '\0';
                error("Error in pcap_findalldevs: %s\n", errbuf);
        }
        if (alldevs)
                pcap_freealldevs(alldevs);
        if (dev_names.empty() && open_err[0])
                pclog("PCAP: no usable devices: %s\n", open_err);

        if (!dev_names.empty()) {
                type->Append("PCAP");
                if (wx_config_get_int(CFG_GLOBAL, NULL, "net_type", NET_SLIRP) == NET_PCAP) {
                        type->SetSelection(1);
                        wx_combobox_select(device, match);
                        device->Enable(true);
                }
        }
}

#endif
void HostConfigDialog::OnOk(wxCommandEvent &event) {
        int idev = device->GetSelection();
        if (type->GetSelection() > 0 && idev >= 0 && idev < (int)dev_names.size()) {
                wx_config_set_int(CFG_GLOBAL, NULL, "net_type", NET_PCAP);
                wx_config_set_string(CFG_GLOBAL, NULL, "pcap_device", dev_names[idev].c_str());
        } else {
                wx_config_set_int(CFG_GLOBAL, NULL, "net_type", NET_SLIRP);
                wx_config_set_string(CFG_GLOBAL, NULL, "pcap_device", "nothing");
        }
        saveconfig_global_only();
        EndModal(wxID_OK);
}

bool HostConfigDialog::Open(wxWindow *parent) {
        HostConfigDialog dlg(parent);
        return dlg.ShowModal() == wxID_OK;
}
