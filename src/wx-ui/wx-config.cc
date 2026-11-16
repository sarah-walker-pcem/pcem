#include "wx-config.h"
#include "wx-hdnew.h"
#include "wx-hdsize.h"
#include "wx-deviceconfig.h"
#include "wx-joystickconfig.h"
#include "wx-frontend.h"

#include <wx/choicebk.h>
#include <wx/listctrl.h>
#include <wx/simplebook.h>
#include <wx/spinctrl.h>

#include <wx/progdlg.h>

#include <atomic>
#include <functional>
#include <stdio.h>
#include <thread>
#include <vector>

extern "C" {
#include "ibm.h"
#include "ide.h"
#include "config.h"
#include "cpu.h"
#include "device.h"
#include "fdd.h"
#include "gameport.h"
#include "hdd.h"
#include "lpt.h"
#include "model.h"
#include "mouse.h"
#include "mem.h"
#include "nethandler.h"
#include "nvr.h"
#include "scsi_cd.h"
#include "sound.h"
#include "video.h"
#include "vid_voodoo.h"
#include "sdl2-emu.h"
#include "minivhd/minivhd.h"
}

#if defined(__APPLE__) && defined(__MACH__)
#define fseeko64 fseeko
#define ftello64 ftello
#endif

#define PAGE_STORAGE 3
#define ARRAY_SIZE(a) (int)(sizeof(a) / sizeof((a)[0]))

int has_been_inited = 0;

int ConfigDialog::MouseValid(int type, int model_index) {
        int flags = models[model_index]->flags;
        if (((type & MOUSE_TYPE_IF_MASK) == MOUSE_TYPE_PS2) && !(flags & MODEL_PS2))
                return 0;
        if (((type & MOUSE_TYPE_IF_MASK) == MOUSE_TYPE_AMSTRAD) && !(flags & MODEL_AMSTRAD))
                return 0;
        if (((type & MOUSE_TYPE_IF_MASK) == MOUSE_TYPE_OLIM24) && !(flags & MODEL_OLIM24))
                return 0;
        return 1;
}

wxString ConfigDialog::NormalizeMacaddr(const wxString &text, bool *valid) {
        wxString s = text;
        s.Trim(true).Trim(false);
        *valid = true;
        if (s.empty())
                return s;

        wxString out;
        size_t pos = 0;
        for (int c = 0; c < 6; c++) {
                unsigned long byte;
                if (pos + 2 > s.length() || !s.Mid(pos, 2).ToULong(&byte, 16) || !wxIsxdigit(s[pos]) || !wxIsxdigit(s[pos + 1]) ||
                    (c == 0 && (byte & 1))) {
                        *valid = false;
                        return wxString();
                }
                out += wxString::Format("%02x", (unsigned)byte);
                pos += 2;
                if (c < 5) {
                        if (pos >= s.length() || (s[pos] != ':' && s[pos] != '-')) {
                                *valid = false;
                                return wxString();
                        }
                        out += ":";
                        pos++;
                }
        }
        if (pos != s.length()) {
                *valid = false;
                return wxString();
        }
        return out;
}

ConfigDialog::ConfigDialog(wxWindow *parent) {
        wxXmlResource::Get()->LoadDialog(this, parent, "ConfigureDlg");

        romstolist.assign(ROM_MAX, 0);
        listtomodel.assign(ROM_MAX, 0);
        romstomodel.assign(ROM_MAX, 0);
        modeltolist.assign(ROM_MAX, 0);
        sound_to_list.assign(SOUND_MAX, 0);
        list_to_sound.assign(SOUND_MAX, 0);
        mouse_to_list.assign(MOUSE_MAX, 0);
        list_to_mouse.assign(MOUSE_MAX, 0);
#ifdef USE_NETWORKING
        network_to_list.assign(NETWORK_CARD_MAX, 0);
        list_to_network.assign(NETWORK_CARD_MAX, 0);
#endif

        FindControls();
        InitControls();
        BindEvents();
        wx_fit_dialog(this);
}

#ifdef USE_NETWORKING
#endif
void ConfigDialog::FindControls() {
        cb_model = Ctrl<wxComboBox>("IDC_COMBO1");
        cb_cpu_manu = Ctrl<wxComboBox>("IDC_COMBOCPUM");
        cb_cpu = Ctrl<wxComboBox>("IDC_COMBO3");
        cb_fpu = Ctrl<wxComboBox>("IDC_COMBOFPU");
        cb_waitstates = Ctrl<wxComboBox>("IDC_COMBOWS");
        chk_dynarec = Ctrl<wxCheckBox>("IDC_CHECKDYNAREC");
        chk_sync = Ctrl<wxCheckBox>("IDC_CHECKSYNC");
        spin_mem = Ctrl<wxSpinCtrl>("IDC_MEMSPIN");
        text_mem_unit = Ctrl<wxStaticText>("IDC_TEXT_MB");
        cb_video = Ctrl<wxComboBox>("IDC_COMBOVID");
        cb_video_speed = Ctrl<wxComboBox>("IDC_COMBOSPD");
        chk_voodoo = Ctrl<wxCheckBox>("IDC_CHECKVOODOO");
        cb_sound = Ctrl<wxComboBox>("IDC_COMBOSND");
        chk_gameblaster = Ctrl<wxCheckBox>("IDC_CHECK3");
        chk_gus = Ctrl<wxCheckBox>("IDC_CHECKGUS");
        chk_ssi = Ctrl<wxCheckBox>("IDC_CHECKSSI");
        cb_hdd = Ctrl<wxComboBox>("IDC_COMBOHDD");
        cb_fda = Ctrl<wxComboBox>("IDC_COMBODRA");
        cb_fdb = Ctrl<wxComboBox>("IDC_COMBODRB");
        cb_cd_model = Ctrl<wxComboBox>("IDC_COMBO_CDMODEL");
        cb_cd_speed = Ctrl<wxComboBox>("IDC_COMBO_CDSPEED");
        hd_book = Ctrl<wxChoicebook>("IDC_HDCBOOK");
        cb_lpt1 = Ctrl<wxComboBox>("IDC_COMBOLPT1");
        cb_mouse = Ctrl<wxComboBox>("IDC_COMBOMOUSE");
        cb_joystick = Ctrl<wxComboBox>("IDC_COMBOJOY");
        for (int c = 0; c < 4; c++)
                btn_joystick[c] = Ctrl<wxWindow>(wxString::Format("IDC_JOY%i", c + 1));
        btn_model_cfg = Ctrl<wxWindow>("IDC_CONFIGUREMOD");
        btn_video_cfg = Ctrl<wxWindow>("IDC_CONFIGUREVID");
        btn_voodoo_cfg = Ctrl<wxWindow>("IDC_CONFIGUREVOODOO");
        btn_sound_cfg = Ctrl<wxWindow>("IDC_CONFIGURESND");
        btn_hdd_cfg = Ctrl<wxWindow>("IDC_CONFIGUREHDD");
        btn_lpt1_cfg = Ctrl<wxWindow>("IDC_CONFIGURELPT1");
        cb_netcard = Ctrl<wxComboBox>("IDC_COMBO_NETCARD");
        btn_netcard_cfg = Ctrl<wxWindow>("IDC_CONFIGURE_NETCARD");
        text_macaddr = Ctrl<wxWindow>("m_staticTextMacAddr");
        edit_macaddr = Ctrl<wxTextCtrl>("IDC_EDIT_MACADDR");
        net_panel = Ctrl<wxWindow>("m_panelnet");
        page_list = Ctrl<wxListCtrl>("IDC_PAGESELECTION");
        pages = Ctrl<wxSimplebook>("IDC_NOTEBOOK");

        for (int i = 0; i < NUM_DRIVES; i++) {
                DriveControls &d = drives[i];
                d.panel = Ctrl<wxWindow>("IDC_HDPANEL", i);
                d.type = Ctrl<wxComboBox>("IDC_COMBODRIVETYPE", i);
                d.new_button = Ctrl<wxWindow>("IDC_NEW", i);
                d.file_button = Ctrl<wxWindow>("IDC_FILE", i);
                d.eject_button = Ctrl<wxWindow>("IDC_EJECT", i);
                d.fn = Ctrl<wxTextCtrl>("IDC_EDIT_FN", i);
                d.spt = Ctrl<wxStaticText>("IDC_EDIT_SPT", i);
                d.hpc = Ctrl<wxStaticText>("IDC_EDIT_HPC", i);
                d.cyl = Ctrl<wxStaticText>("IDC_EDIT_CYL", i);
                d.size = Ctrl<wxStaticText>("IDC_TEXT_SIZE", i);
                for (int j = 0; j < 6; j++)
                        d.labels[j] = Ctrl<wxWindow>("IDC_HDD_LABEL", i * 10 + j);
        }
}

int ConfigDialog::SelectedModel() {
        int sel = cb_model->GetSelection();
        return listtomodel[sel >= 0 ? sel : 0];
}

int ConfigDialog::SelectedIndex(wxComboBox *cb) {
        int sel = cb->GetSelection();
        return sel >= 0 ? sel : 0;
}

char *ConfigDialog::SelectedHdd() {
        int sel = cb_hdd->GetSelection();
        return (sel >= 0 && sel < (int)hdd_names.size()) ? hdd_names[sel] : NULL;
}

bool ConfigDialog::SelectedHddIsMfm() { return SelectedHdd() && hdd_controller_is_mfm(SelectedHdd()); }

bool ConfigDialog::SelectedHddIsIde() { return SelectedHdd() && hdd_controller_is_ide(SelectedHdd()); }

bool ConfigDialog::SelectedHddIsScsi() { return SelectedHdd() && hdd_controller_is_scsi(SelectedHdd()); }

bool ConfigDialog::SelectedHddHasConfig() { return SelectedHdd() && hdd_controller_has_config(SelectedHdd()); }

device_t *ConfigDialog::SelectedHddDevice() { return SelectedHdd() ? hdd_controller_get_device(SelectedHdd()) : NULL; }

int ConfigDialog::SelectedCdModel() {
        int sel = cb_cd_model->GetSelection();
        return (sel >= 0 && sel < (int)cd_list_to_model.size()) ? cd_list_to_model[sel] : 0;
}

void ConfigDialog::RecalcFpuList(int model_index, int manu, int cpu_index, int fpu) {
        wx_combobox_clear(cb_fpu);
        int c;
        for (c = 0;; c++) {
                const char *name = fpu_get_name_from_index(model_index, manu, cpu_index, c);
                if (!name)
                        break;
                cb_fpu->Append(name);
                if (!c || fpu_get_type_from_index(model_index, manu, cpu_index, c) == fpu)
                        cb_fpu->SetSelection(c);
        }
        cb_fpu->Enable(c > 1);
}

void ConfigDialog::RecalcVidList(int model_index, bool force_builtin_video) {
        int found_card = 0;
        int cur_gfxcard = gfxcard;
        int model_romset = model_getromset_from_model(model_index);
        int model_flags = models[model_index]->flags;
        int d = 0;

        wx_combobox_clear(cb_video);

        if (model_has_fixed_gfx(model_index)) {
                cb_video->Append("Built-in video");
                cb_video->SetSelection(0);
                cb_video->Enable(false);
                btn_video_cfg->Enable(video_card_has_config(gfxcard, model_romset));
                return;
        }

        if (model_has_optional_gfx(model_index)) {
                cb_video->Append("Built-in video");
                if (gfxcard == GFX_BUILTIN || force_builtin_video) {
                        cb_video->SetSelection(d);
                        found_card = 1;
                        cur_gfxcard = GFX_BUILTIN;
                }
                d++;
        }

        if (cur_gfxcard == GFX_BUILTIN && !model_has_optional_gfx(model_index))
                cur_gfxcard = 0;

        for (int c = 0;; c++) {
                char *s = video_card_getname(c);
                if (!s[0])
                        break;

                if (!video_card_available(c) || !gfx_present[video_new_to_old(c)])
                        continue;
                int dev_flags = video_card_getdevice(c, model_romset)->flags;
                if (((model_flags & MODEL_PCI) || !(dev_flags & DEVICE_PCI)) &&
                    ((model_flags & MODEL_MCA) || !(dev_flags & DEVICE_MCA)) &&
                    (!(model_flags & MODEL_MCA) || (dev_flags & DEVICE_MCA))) {
                        cb_video->Append(s);
                        if (video_new_to_old(c) == gfxcard && !found_card) {
                                cb_video->SetSelection(d);
                                found_card = 1;
                        }
                        d++;
                }
        }
        if (!found_card)
                wx_combobox_select(cb_video, 0);
        cb_video->Enable(true);

        btn_video_cfg->Enable(video_card_has_config(video_old_to_new(cur_gfxcard), model_romset));
        chk_voodoo->Enable((model_flags & MODEL_PCI) != 0);
        btn_voodoo_cfg->Enable((model_flags & MODEL_PCI) != 0);
}

void ConfigDialog::RecalcSndList(int model_index) {
        wx_combobox_clear(cb_sound);

        int d = 0;
        for (int c = 0;; c++) {
                char *s = sound_card_getname(c);
                if (!s[0])
                        break;

                sound_to_list[c] = d;

                if (sound_card_available(c)) {
                        device_t *sound_dev = sound_card_getdevice(c);

                        if (!sound_dev || (sound_dev->flags & DEVICE_MCA) == (models[model_index]->flags & MODEL_MCA)) {
                                cb_sound->Append(s);
                                list_to_sound[d] = c;
                                d++;
                        }
                }
        }
        wx_combobox_select(cb_sound, sound_to_list[sound_card_current]);
}

void ConfigDialog::RecalcHddList(int model_index, bool use_selected_hdd, bool force_ide) {
        char old_name[16] = "";
        int model_flags = models[model_index]->flags;

        if (force_ide)
                strcpy(old_name, "ide");
        else if (use_selected_hdd) {
                if (SelectedHdd())
                        strncpy(old_name, SelectedHdd(), sizeof(old_name) - 1);
                else
                        strcpy(old_name, (model_flags & MODEL_HAS_IDE) ? "none" : "ide");
        } else
                strncpy(old_name, hdd_controller_name, sizeof(old_name) - 1);

        wx_combobox_clear(cb_hdd);
        hdd_names.clear();

        bool valid = false;
        for (int c = 0;; c++) {
                char *s = hdd_controller_get_name(c);
                if (!s[0])
                        break;
                int flags = hdd_controller_get_flags(c);
                if (c && (((flags & DEVICE_AT) && !(model_flags & MODEL_AT)) ||
                          (flags & DEVICE_MCA) != (model_flags & MODEL_MCA)))
                        continue;
                if (c && (((flags & DEVICE_PS1) && models[model_index]->id != ROM_IBMPS1_2011) ||
                          (!(flags & DEVICE_PS1) && models[model_index]->id == ROM_IBMPS1_2011)))
                        continue;
                if (!hdd_controller_available(c))
                        continue;

                cb_hdd->Append(s);
                hdd_names.push_back(hdd_controller_get_internal_name(c));
                if (!strcmp(old_name, hdd_names.back())) {
                        cb_hdd->SetSelection(hdd_names.size() - 1);
                        valid = true;
                }
        }

        if (!valid)
                wx_combobox_select(cb_hdd, 0);
        cb_hdd->Enable(true);
        btn_hdd_cfg->Enable(SelectedHddHasConfig());

        while (hd_book->GetPageCount())
                hd_book->RemovePage(0);
        for (int c = 0; c < NUM_DRIVES; c++) {
                drives[c].panel->Reparent(hd_book);
                hd_book->AddPage(drives[c].panel, "dummy name");
        }

        int visible;
        if (SelectedHddIsScsi()) {
                visible = NUM_DRIVES;
                for (int c = 0; c < visible; c++)
                        hd_book->SetPageText(c, wxString::Format("SCSI ID %i (%c:)", c, 'C' + c));
        } else if (SelectedHddIsIde()) {
                visible = 4;
                for (int c = 0; c < visible; c++)
                        hd_book->SetPageText(c, wxString::Format("Drive %i %s %s (%c:)", c, (c & 2) ? "Secondary" : "Primary",
                                                                 (c & 1) ? "Slave" : "Master", 'C' + c));
        } else {
                visible = 2;
                for (int c = 0; c < visible; c++)
                        hd_book->SetPageText(c, wxString::Format("Drive %i %s (%c:)", c, (c & 1) ? "Secondary" : "Primary",
                                                                 'C' + c));
        }
        for (int c = NUM_DRIVES - 1; c >= visible; c--)
                hd_book->RemovePage(c);

        hd_book->InvalidateBestSize();
        hd_book->GetParent()->Layout();
}

void ConfigDialog::RecalcCdList(int cur_speed, const char *cur_model) {
        bool has_interface = SelectedHddIsIde() || SelectedHddIsScsi();
        int temp_model = -1;

        wx_combobox_clear(cb_cd_model);
        cd_list_to_model.clear();
        cb_cd_model->Enable(has_interface);
        for (int c = 0; c < MAX_CD_MODEL; c++) {
                int interfaces = cd_get_model_interfaces(c);
                if (interfaces == CD_MODEL_INTERFACE_ALL || (interfaces == CD_MODEL_INTERFACE_IDE && SelectedHddIsIde()) ||
                    (interfaces == CD_MODEL_INTERFACE_SCSI && SelectedHddIsScsi())) {
                        char *model_name = cd_get_model(c);
                        cb_cd_model->Append(model_name);
                        cd_list_to_model.push_back(c);
                        if (cur_model && !strncmp(model_name, cur_model, 40)) {
                                cb_cd_model->SetSelection(cd_list_to_model.size() - 1);
                                temp_model = c;
                        }
                }
        }
        if (temp_model < 0)
                wx_combobox_select(cb_cd_model, 0);

        wx_combobox_clear(cb_cd_speed);
        if (temp_model >= 0 && cd_get_model_speed(temp_model) != -1) {
                cb_cd_speed->Enable(false);
                cur_speed = cd_get_model_speed(temp_model);
        } else
                cb_cd_speed->Enable(has_interface);

        bool found = false;
        for (int c = 0;; c++) {
                int speed = cd_get_speed(c);
                cb_cd_speed->Append(wxString::Format("%iX", speed));
                if (speed == cur_speed) {
                        cb_cd_speed->SetSelection(c);
                        found = true;
                }
                if (speed >= MAX_CD_SPEED)
                        break;
        }
        if (!found)
                wx_combobox_select(cb_cd_speed, 0);
}

void ConfigDialog::RecalcCdListFromSelection() {
        RecalcCdList(cd_get_speed(SelectedIndex(cb_cd_speed)), cd_get_model(SelectedCdModel()));
}

void ConfigDialog::PopulateMouseList(int model_index, int select_type) {
        wx_combobox_clear(cb_mouse);
        int d = 0;
        for (int c = 0;; c++) {
                char *s = mouse_get_name(c);
                if (!s)
                        break;
                mouse_to_list[c] = d;
                if (MouseValid(mouse_get_type(c), model_index)) {
                        cb_mouse->Append(s);
                        list_to_mouse[d] = c;
                        d++;
                }
        }
        if (MouseValid(mouse_get_type(select_type), model_index))
                wx_combobox_select(cb_mouse, mouse_to_list[select_type]);
        else
                wx_combobox_select(cb_mouse, 0);
}

void ConfigDialog::PopulateCpuManufacturers(int model_index, int selection) {
        wx_combobox_clear(cb_cpu_manu);
        int c = 0;
        while (c < 4 && models[model_index]->cpu[c].cpus != NULL) {
                cb_cpu_manu->Append(models[model_index]->cpu[c].name);
                c++;
        }
        if (selection >= c)
                selection = c - 1;
        wx_combobox_select(cb_cpu_manu, selection);
        cb_cpu_manu->Enable(c > 1);
}

void ConfigDialog::PopulateCpus(int model_index, int manu, int selection) {
        wx_combobox_clear(cb_cpu);
        int c = 0;
        while (models[model_index]->cpu[manu].cpus[c].cpu_type != -1) {
                cb_cpu->Append(models[model_index]->cpu[manu].cpus[c].name);
                c++;
        }
        if (selection >= c)
                selection = c - 1;
        wx_combobox_select(cb_cpu, selection);
        cb_cpu->Enable(true);
}

void ConfigDialog::UpdateCpuDependent(int model_index, int manu, int cpu_index, int fpu) {
        RecalcFpuList(model_index, manu, cpu_index, fpu);

        CPU *cpu_info = &models[model_index]->cpu[manu].cpus[cpu_index];
        int cpu_flags = cpu_info->cpu_flags;
        bool dynarec = chk_dynarec->GetValue();
        chk_dynarec->Enable((cpu_flags & CPU_SUPPORTS_DYNAREC) && !(cpu_flags & CPU_REQUIRES_DYNAREC));
        chk_dynarec->SetValue(((cpu_flags & CPU_SUPPORTS_DYNAREC) && dynarec) || (cpu_flags & CPU_REQUIRES_DYNAREC));

        cb_waitstates->Enable(cpu_info->cpu_type >= CPU_286 && cpu_info->cpu_type <= CPU_386DX);
}

int ConfigDialog::SelectedFpuType(int model_index, int manu, int cpu_index) {
        return fpu_get_type_from_index(model_index, manu, cpu_index, SelectedIndex(cb_fpu));
}

void ConfigDialog::UpdateMemoryUnit(int model_index) {
        bool mb = (models[model_index]->flags & MODEL_AT) && models[model_index]->ram_granularity < 128;
        text_mem_unit->SetLabel(mb ? "MB" : "KB");
}

void ConfigDialog::UpdateJoystickButtons(int type) {
        for (int c = 0; c < 4; c++)
                btn_joystick[c]->Enable(joystick_get_max_joysticks(type) >= c + 1);
}

#ifdef USE_NETWORKING
void ConfigDialog::RecalcNetList(int model_index) {
        int model_flags = models[model_index]->flags;
        bool found_card = false;
        int d = 0;

        wx_combobox_clear(cb_netcard);
        for (int c = 0;; c++) {
                char *s = network_card_getname(c);
                if (!s[0])
                        break;

                network_to_list[c] = d;

                device_t *dev = network_card_getdevice(c);
                if (network_card_available(c) && (!dev || (model_flags & MODEL_PCI) || !(dev->flags & DEVICE_PCI)) &&
                    (!dev || (dev->flags & DEVICE_MCA) == (model_flags & MODEL_MCA))) {
                        cb_netcard->Append(s);
                        list_to_network[d] = c;
                        if (c == network_card_current) {
                                cb_netcard->SetSelection(d);
                                found_card = true;
                        }
                        d++;
                }
        }
        if (!found_card)
                wx_combobox_select(cb_netcard, 0);
}

int ConfigDialog::SelectedNetworkCard() { return list_to_network[SelectedIndex(cb_netcard)]; }

bool ConfigDialog::SelectedNetworkCardHasDevice() {
        return cb_netcard->GetSelection() >= 0 && network_card_getdevice(SelectedNetworkCard()) != NULL;
}

void ConfigDialog::UpdateMacaddrVisibility() {
        bool show = SelectedNetworkCardHasDevice();
        text_macaddr->Show(show);
        edit_macaddr->Show(show);
        net_panel->Layout();
}

bool ConfigDialog::ValidateMacaddr() {
        if (!SelectedNetworkCardHasDevice())
                return true;
        bool valid;
        NormalizeMacaddr(edit_macaddr->GetValue(), &valid);
        if (!valid)
                wx_messagebox(this, "Invalid MAC address. Use the form XX:XX:XX:XX:XX:XX, or leave it blank for the default.",
                              "PCem error", wxOK);
        return valid;
}

#endif
void ConfigDialog::InitControls() {
        static const char *floppy_types[] = {"None",       "5.25\" 360k", "5.25\" 1.2M",        "5.25\" 1.2M Dual RPM",
                                             "3.5\" 720k", "3.5\" 1.44M", "3.5\" 1.44M 3-Mode", "3.5\" 2.88M"};
        static const char *video_speeds[] = {"Default",      "8-bit",        "Slow 16-bit", "Fast 16-bit",
                                             "Slow VLB/PCI", "Mid  VLB/PCI", "Fast VLB/PCI"};
        static const char *wait_states[] = {"System default", "0 W/S", "1 W/S", "2 W/S", "3 W/S",
                                            "4 W/S",          "5 W/S", "6 W/S", "7 W/S"};

        pauseEmulation = 1;

        int d = 0;
        for (int c = 0; models[c] != NULL; c++) {
                if (romspresent[models[c]->id]) {
                        cb_model->Append(models[c]->name);
                        modeltolist[c] = d;
                        listtomodel[d] = c;
                        romstolist[models[c]->id] = d;
                        romstomodel[models[c]->id] = c;
                        d++;
                }
        }
        wx_combobox_select(cb_model, modeltolist[model]);
        model = listtomodel[modeltolist[model]];
        romset = model_getromset();
        prev_model = model;

        int model_index = romstomodel[romset];

        RecalcVidList(model_index, false);

        PopulateCpuManufacturers(model_index, cpu_manufacturer);
        PopulateCpus(model_index, cpu_manufacturer, cpu);
        chk_dynarec->SetValue(cpu_use_dynarec);
        UpdateCpuDependent(model_index, cpu_manufacturer, cpu, fpu_type);

        RecalcSndList(model_index);

        chk_gameblaster->SetValue(GAMEBLASTER);
        chk_gus->SetValue(GUS);
        chk_ssi->SetValue(SSI2001);
        chk_sync->SetValue(enable_sync);
        chk_voodoo->SetValue(voodoo_enabled);

        for (int c = 0; c < ARRAY_SIZE(video_speeds); c++)
                cb_video_speed->Append(video_speeds[c]);
        wx_combobox_select(cb_video_speed, video_speed + 1);

        spin_mem->SetRange(models[model_index]->min_ram, models[model_index]->max_ram);
        spin_mem->SetIncrement(models[model]->ram_granularity);
        if ((models[model]->flags & MODEL_AT) && models[model]->ram_granularity < 128)
                spin_mem->SetValue(mem_size / 1024);
        else
                spin_mem->SetValue(mem_size);

        btn_model_cfg->Enable(model_getdevice(model) != NULL);
        btn_sound_cfg->Enable(sound_card_has_config(sound_card_current));
        btn_lpt1_cfg->Enable(lpt_device_has_config(lpt1_current));

        for (int c = 0; c < ARRAY_SIZE(floppy_types); c++) {
                cb_fda->Append(floppy_types[c]);
                cb_fdb->Append(floppy_types[c]);
        }
        wx_combobox_select(cb_fda, fdd_get_type(0));
        wx_combobox_select(cb_fdb, fdd_get_type(1));

        UpdateMemoryUnit(model);

        for (int c = 0; joystick_get_name(c); c++)
                cb_joystick->Append(joystick_get_name(c));
        cb_joystick->Enable(true);
        wx_combobox_select(cb_joystick, joystick_type);
        UpdateJoystickButtons(joystick_type);

        for (int c = 0; c < ARRAY_SIZE(wait_states); c++)
                cb_waitstates->Append(wait_states[c]);
        wx_combobox_select(cb_waitstates, cpu_waitstates);

        PopulateMouseList(model, mouse_type);

        int lpt_selection = 0;
        for (int c = 0; lpt_device_get_name(c); c++) {
                cb_lpt1->Append(lpt_device_get_name(c));
                if (!strcmp(lpt1_device_name, lpt_device_get_internal_name(c)) && lpt1_current == c)
                        lpt_selection = c;
        }
        wx_combobox_select(cb_lpt1, lpt_selection);

        RecalcHddList(model_index, false, false);

        hd_changed = false;
        new_cdrom_channel = cdrom_channel;
        new_zip_channel = zip_channel;

        for (int c = 0; c < NUM_DRIVES; c++) {
                drives[c].type->Append("Hard drive");
                drives[c].type->Append("CD-ROM");
                drives[c].type->Append("Iomega Zip");
        }
        HdConfInit();

        RecalcCdList(cd_speed, cd_model);

#ifdef USE_NETWORKING
        RecalcNetList(model_index);
        btn_netcard_cfg->Enable(network_card_has_config(network_card_current));
        edit_macaddr->ChangeValue(wx_config_get_string(CFG_MACHINE, NULL, "macaddr", ""));
        UpdateMacaddrVisibility();
#endif

        page_list->SetMaxSize(wxSize(250, -1));
        page_list->SetMinSize(wxSize(250, -1));
}

void ConfigDialog::BindEvents() {
        Bind(wxEVT_BUTTON, &ConfigDialog::OnOk, this, wxID_OK);
        Bind(wxEVT_BUTTON, &ConfigDialog::OnCancel, this, wxID_CANCEL);

        page_list->Bind(wxEVT_LIST_ITEM_SELECTED, &ConfigDialog::OnPageSelected, this);

        cb_model->Bind(wxEVT_COMBOBOX, &ConfigDialog::OnModelChanged, this);
        cb_cpu_manu->Bind(wxEVT_COMBOBOX, &ConfigDialog::OnCpuManufacturerChanged, this);
        cb_cpu->Bind(wxEVT_COMBOBOX, &ConfigDialog::OnCpuChanged, this);
        cb_video->Bind(wxEVT_COMBOBOX, &ConfigDialog::OnVideoChanged, this);
        cb_sound->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent &) {
                btn_sound_cfg->Enable(sound_card_has_config(list_to_sound[SelectedIndex(cb_sound)]));
        });
        cb_hdd->Bind(wxEVT_COMBOBOX, &ConfigDialog::OnHddChanged, this);
        cb_cd_model->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent &) { RecalcCdListFromSelection(); });
        cb_joystick->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent &) { UpdateJoystickButtons(SelectedIndex(cb_joystick)); });
        cb_lpt1->Bind(wxEVT_COMBOBOX,
                      [this](wxCommandEvent &) { btn_lpt1_cfg->Enable(lpt_device_has_config(SelectedIndex(cb_lpt1))); });

        btn_model_cfg->Bind(wxEVT_BUTTON,
                            [this](wxCommandEvent &) { DeviceConfigDialog::Open(this, model_getdevice(SelectedModel())); });
        btn_video_cfg->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
                int romset_index = model_getromset_from_model(SelectedModel());
                DeviceConfigDialog::Open(this, video_card_getdevice(SelectedVideoCard(), romset_index));
        });
        btn_voodoo_cfg->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { DeviceConfigDialog::Open(this, &voodoo_device); });
        btn_sound_cfg->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
                DeviceConfigDialog::Open(this, sound_card_getdevice(list_to_sound[SelectedIndex(cb_sound)]));
        });
        btn_hdd_cfg->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { DeviceConfigDialog::Open(this, SelectedHddDevice()); });
        btn_lpt1_cfg->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
                DeviceConfigDialog::Open(this, (device_t *)lpt_get_device(SelectedIndex(cb_lpt1)));
        });
        for (int c = 0; c < 4; c++)
                btn_joystick[c]->Bind(wxEVT_BUTTON,
                                      [this, c](wxCommandEvent &) {
                                              JoystickConfigDialog::Open(this, c, SelectedIndex(cb_joystick));
                                      });

#ifdef USE_NETWORKING
        cb_netcard->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent &) {
                btn_netcard_cfg->Enable(network_card_has_config(SelectedNetworkCard()));
                UpdateMacaddrVisibility();
        });
        btn_netcard_cfg->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
                DeviceConfigDialog::Open(this, network_card_getdevice(SelectedNetworkCard()));
        });
#endif

        for (int i = 0; i < NUM_DRIVES; i++) {
                drives[i].eject_button->Bind(wxEVT_BUTTON, [this, i](wxCommandEvent &) { HdEject(i); });
                drives[i].new_button->Bind(wxEVT_BUTTON, [this, i](wxCommandEvent &) { HdNew(i); });
                drives[i].file_button->Bind(wxEVT_BUTTON, [this, i](wxCommandEvent &) { HdFile(i); });
                drives[i].type->Bind(wxEVT_COMBOBOX, [this, i](wxCommandEvent &) { HdDriveType(i); });
        }
}

int ConfigDialog::SelectedVideoCard() { return video_card_getid((char *)(const char *)cb_video->GetStringSelection().mb_str()); }

void ConfigDialog::OnPageSelected(wxListEvent &event) {
        int index = event.GetIndex();
        if (index >= 0 && index < (int)pages->GetPageCount())
                pages->SetSelection(index);
        if (index == PAGE_STORAGE)
                UpdateHddCdrom();
}

void ConfigDialog::OnModelChanged(wxCommandEvent &event) {
        int temp_model = SelectedModel();
        if (temp_model == prev_model)
                return;

        bool force_ide = (models[temp_model]->flags & MODEL_HAS_IDE) && !(models[prev_model]->flags & MODEL_HAS_IDE);
        bool force_builtin_video = model_has_optional_gfx(temp_model);
        prev_model = temp_model;

        PopulateCpuManufacturers(temp_model, SelectedIndex(cb_cpu_manu));
        int temp_cpu_m = SelectedIndex(cb_cpu_manu);
        PopulateCpus(temp_model, temp_cpu_m, SelectedIndex(cb_cpu));
        int temp_cpu = SelectedIndex(cb_cpu);
        UpdateCpuDependent(temp_model, temp_cpu_m, temp_cpu, SelectedFpuType(temp_model, temp_cpu_m, temp_cpu));

        UpdateMemoryUnit(temp_model);

        int mem = spin_mem->GetValue();
        spin_mem->SetRange(models[temp_model]->min_ram, models[temp_model]->max_ram);
        mem &= ~(models[temp_model]->ram_granularity - 1);
        if (mem < models[temp_model]->min_ram)
                mem = models[temp_model]->min_ram;
        else if (mem > models[temp_model]->max_ram)
                mem = models[temp_model]->max_ram;
        spin_mem->SetValue(mem);
        spin_mem->SetIncrement(models[temp_model]->ram_granularity);

        btn_model_cfg->Enable(model_getdevice(temp_model) != NULL);

        PopulateMouseList(temp_model, list_to_mouse[SelectedIndex(cb_mouse)]);

        RecalcVidList(temp_model, force_builtin_video);
        RecalcHddList(temp_model, true, force_ide);
        RecalcCdListFromSelection();
        RecalcSndList(temp_model);
#ifdef USE_NETWORKING
        RecalcNetList(temp_model);
        UpdateMacaddrVisibility();
#endif
        Layout();
}

void ConfigDialog::OnCpuManufacturerChanged(wxCommandEvent &event) {
        int temp_model = SelectedModel();
        int temp_cpu_m = SelectedIndex(cb_cpu_manu);
        PopulateCpus(temp_model, temp_cpu_m, SelectedIndex(cb_cpu));
        int temp_cpu = SelectedIndex(cb_cpu);
        UpdateCpuDependent(temp_model, temp_cpu_m, temp_cpu, SelectedFpuType(temp_model, temp_cpu_m, temp_cpu));
}

void ConfigDialog::OnCpuChanged(wxCommandEvent &event) {
        int temp_model = SelectedModel();
        int temp_cpu_m = SelectedIndex(cb_cpu_manu);
        int temp_cpu = SelectedIndex(cb_cpu);
        UpdateCpuDependent(temp_model, temp_cpu_m, temp_cpu, SelectedFpuType(temp_model, temp_cpu_m, temp_cpu));
}

void ConfigDialog::OnVideoChanged(wxCommandEvent &event) {
        btn_video_cfg->Enable(video_card_has_config(SelectedVideoCard(), model_getromset_from_model(SelectedModel())));
}

void ConfigDialog::OnHddChanged(wxCommandEvent &event) {
        HdConfUpdate();
        RecalcHddList(SelectedModel(), true, false);
        RecalcCdListFromSelection();
}

void ConfigDialog::SetDriveGeometry(int drive, const HdGeometryDialog::HdGeometry &g, const wxString &fn) {
        drives[drive].spt->SetLabel(wxString::Format("%i", g.spt));
        drives[drive].hpc->SetLabel(wxString::Format("%i", g.hpc));
        drives[drive].cyl->SetLabel(wxString::Format("%i", g.cyl));
        drives[drive].fn->ChangeValue(fn);
        drives[drive].size->SetLabel(HdGeometryDialog::SizeText(g.cyl, g.hpc, g.spt, "mb"));
}

void ConfigDialog::HdConfInit() {
        for (int c = 0; c < NUM_DRIVES; c++) {
                HdGeometryDialog::HdGeometry g = {hdc[c].spt, hdc[c].hpc, hdc[c].tracks, 0};
                SetDriveGeometry(c, g, ide_fn[c]);
        }
        HdConfUpdate();
}

void ConfigDialog::HdConfUpdate() {
        bool is_mfm = SelectedHddIsMfm();

        UpdateHddCdrom();
        for (int i = 0; i < NUM_DRIVES; ++i) {
                drives[i].labels[0]->Enable(!is_mfm);
                drives[i].type->Enable(!is_mfm);
        }
        btn_hdd_cfg->Enable(SelectedHddHasConfig());
}

void ConfigDialog::UpdateHddCdrom() {
        bool is_mfm = SelectedHddIsMfm();

        for (int i = 0; i < NUM_DRIVES; ++i) {
                DriveControls &d = drives[i];
                bool removable = !is_mfm && (new_cdrom_channel == i || new_zip_channel == i);

                if (i == new_cdrom_channel)
                        wx_combobox_select(d.type, 1);
                else if (i == new_zip_channel)
                        wx_combobox_select(d.type, 2);
                else
                        wx_combobox_select(d.type, 0);

                d.fn->Enable(!removable);
                d.file_button->Enable(!removable);
                d.eject_button->Enable(!removable);
                d.new_button->Enable(!removable);
                d.spt->Enable(!removable);
                d.hpc->Enable(!removable);
                d.cyl->Enable(!removable);
                d.size->Enable(!removable);
                for (int j = 1; j < 6; ++j)
                        d.labels[j]->Enable(!removable);
        }
}

void ConfigDialog::HdEject(int drive) {
        ide_fn[drive][0] = 0;
        drives[drive].spt->SetLabel("0");
        drives[drive].hpc->SetLabel("0");
        drives[drive].cyl->SetLabel("0");
        drives[drive].fn->ChangeValue("");
        hd_changed = true;
}

void ConfigDialog::HdNew(int drive) {
        HdNewDialog dlg(this);
        if (dlg.ShowModal() == wxID_OK) {
                SetDriveGeometry(drive, dlg.geometry, dlg.name);
                hd_changed = true;
        }
}

void ConfigDialog::CheckHdType(off_t sz, HdGeometryDialog::HdGeometry &g) {
        g.type = 0;

        if (SelectedHddIsMfm()) {
                for (int c = 0; c < NUM_HD_TYPES; c++) {
                        const HdGeometryDialog::HdType &t = HdGeometryDialog::hd_types[c];
                        if ((off_t)t.cylinders * t.heads * 17 * 512 == sz) {
                                g.spt = 17;
                                g.hpc = HdGeometryDialog::hd_types[c].heads;
                                g.cyl = HdGeometryDialog::hd_types[c].cylinders;
                                g.type = c + 1;
                                return;
                        }
                }
                g.spt = 63;
                g.hpc = 16;
                g.cyl = ((sz / 512) / 16) / 63;
        } else if ((sz % 17) == 0 && sz <= 142606336) {
                g.spt = 17;

                if (sz <= 26738688)
                        g.hpc = 4;
                else if ((sz % 3072) == 0 && sz <= 53477376)
                        g.hpc = 6;
                else {
                        int c;
                        for (c = 5; c < 16; c++) {
                                if ((sz % (c * 512)) == 0 && sz <= 1024 * c * 17 * 512)
                                        break;
                                if (c == 5)
                                        c++;
                        }
                        g.hpc = c;
                }
                g.cyl = (int)((sz / 512) / g.hpc) / 17;
        } else {
                g.spt = 63;
                g.hpc = 16;
                g.cyl = ((sz / 512) / 16) / 63;
        }
}

void ConfigDialog::HdFile(int drive) {
        wxString file;
        if (!wx_getfile(this,
                        "Hard disk image (*.img;*.vhd)|*.img;*.vhd|RAM disk image (*.rdimg;*.rdvhd)|*.rdimg;*.rdvhd|All files "
                        "(*.*)|*.*",
                        "", file))
                return;

        wxCharBuffer path = file.mb_str();
        FILE *f = fopen(path, "rb");
        if (!f) {
                wx_messagebox(this, "Can't open file for read", "PCem error", wxOK);
                return;
        }

        HdGeometryDialog::HdGeometry g = {0, 0, 0, 0};
        if (mvhd_file_is_vhd(f)) {
                fclose(f);
                int vhd_error = 0;
                MVHDMeta *vhd = mvhd_open(path, false, &vhd_error);
                if (!vhd) {
                        wx_messagebox(this, mvhd_strerr((MVHDError)vhd_error), "PCem error", wxOK);
                        return;
                }
                if (vhd_error == MVHD_ERR_TIMESTAMP) {
                        const char *ts_warning =
                                "WARNING: VHD PARENT/CHILD TIMESTAMPS DO NOT MATCH!\n\n"
                                "This could indicate that the parent image was modified after this VHD was created.\n\n"
                                "This could also happen if the VHD files were moved/copied, or the differencing VHD was "
                                "created with DiskPart.\n\n"
                                "Do you wish to fix this error after a file copy or DiskPart creation?";
                        if (wx_messagebox(this, ts_warning, "PCem error", wxYES_NO | wxNO_DEFAULT) != wxYES) {
                                mvhd_close(vhd);
                                return;
                        }
                        if (mvhd_diff_update_par_timestamp(vhd, &vhd_error) != 0) {
                                wx_messagebox(this, "Can't fix VHD timestamps", "PCem error", wxOK);
                                mvhd_close(vhd);
                                return;
                        }
                }
                MVHDGeom geom = mvhd_get_geometry(vhd);
                g.cyl = geom.cyl;
                g.hpc = geom.heads;
                g.spt = geom.spt;
                HdGeometryDialog::AdjustVhdGeometryForPcem(g);
                mvhd_close(vhd);
        } else {
                fseeko64(f, -1, SEEK_END);
                off_t sz = ftello64(f) + 1;
                fclose(f);
                CheckHdType(sz, g);
        }

        HdSizeDialog dlg(this, g);
        if (dlg.ShowModal() == wxID_OK) {
                SetDriveGeometry(drive, g, file);
                hd_changed = true;
        }
}

void ConfigDialog::HdDriveType(int drive) {
        switch (drives[drive].type->GetSelection()) {
        case 0:
                if (new_cdrom_channel == drive)
                        new_cdrom_channel = -1;
                if (new_zip_channel == drive)
                        new_zip_channel = -1;
                break;
        case 1:
                new_cdrom_channel = drive;
                if (new_zip_channel == drive)
                        new_zip_channel = -1;
                break;
        case 2:
                new_zip_channel = drive;
                if (new_cdrom_channel == drive)
                        new_cdrom_channel = -1;
                break;
        }
        UpdateHddCdrom();
}

void ConfigDialog::Save() {
        int temp_model = SelectedModel();

        int mem = spin_mem->GetValue();
        mem &= ~(models[temp_model]->ram_granularity - 1);
        if (mem < models[temp_model]->min_ram)
                mem = models[temp_model]->min_ram;
        else if (mem > models[temp_model]->max_ram)
                mem = models[temp_model]->max_ram;
        if ((models[temp_model]->flags & MODEL_AT) && models[temp_model]->ram_granularity < 128)
                mem *= 1024;

        int gfx = video_new_to_old(SelectedVideoCard());
        int temp_cpu_m = SelectedIndex(cb_cpu_manu);
        int temp_cpu = SelectedIndex(cb_cpu);
        int temp_fpu = SelectedFpuType(temp_model, temp_cpu_m, temp_cpu);
        int temp_gameblaster = chk_gameblaster->GetValue();
        int temp_gus = chk_gus->GetValue();
        int temp_ssi2001 = chk_ssi->GetValue();
        enable_sync = chk_sync->GetValue();
        int temp_voodoo = chk_voodoo->GetValue();
        int temp_sound_card_current = list_to_sound[SelectedIndex(cb_sound)];
        int temp_dynarec = chk_dynarec->GetValue();
        int temp_fda_type = SelectedIndex(cb_fda);
        int temp_fdb_type = SelectedIndex(cb_fdb);
        int temp_joystick_type = SelectedIndex(cb_joystick);
        int temp_mouse_type = list_to_mouse[SelectedIndex(cb_mouse)];
        int temp_lpt1_device = SelectedIndex(cb_lpt1);
        char *temp_hdd = SelectedHdd();
        bool hdd_changed = temp_hdd && strncmp(temp_hdd, hdd_controller_name, sizeof(hdd_controller_name) - 1);

#ifdef USE_NETWORKING
        int temp_network_card = SelectedNetworkCard();
        bool mac_valid;
        wxString temp_macaddr = NormalizeMacaddr(edit_macaddr->GetValue(), &mac_valid);
        bool macaddr_changed = temp_macaddr != wxString(wx_config_get_string(CFG_MACHINE, NULL, "macaddr", ""));
#endif

        if (temp_model != model || gfx != gfxcard || mem != mem_size || temp_fpu != fpu_type || temp_gameblaster != GAMEBLASTER ||
            temp_gus != GUS || temp_ssi2001 != SSI2001 || temp_sound_card_current != sound_card_current ||
            temp_voodoo != voodoo_enabled || temp_dynarec != cpu_use_dynarec || temp_fda_type != fdd_get_type(0) ||
            temp_fdb_type != fdd_get_type(1) || temp_mouse_type != mouse_type || hdd_changed || hd_changed ||
            cdrom_channel != new_cdrom_channel || zip_channel != new_zip_channel || lpt1_current != temp_lpt1_device
#ifdef USE_NETWORKING
            || temp_network_card != network_card_current || macaddr_changed
#endif
        ) {
                if (has_been_inited && !confirm())
                        return;

                savenvr();
                model = temp_model;
                romset = model_getromset();
                gfxcard = gfx;
                mem_size = mem;
                cpu_manufacturer = temp_cpu_m;
                cpu = temp_cpu;
                fpu_type = temp_fpu;
                GAMEBLASTER = temp_gameblaster;
                GUS = temp_gus;
                SSI2001 = temp_ssi2001;
                sound_card_current = temp_sound_card_current;
                lpt1_current = temp_lpt1_device;
                voodoo_enabled = temp_voodoo;
                cpu_use_dynarec = temp_dynarec;
                mouse_type = temp_mouse_type;
                strcpy(lpt1_device_name, lpt_device_get_internal_name(temp_lpt1_device));
#ifdef USE_NETWORKING
                network_card_current = temp_network_card;
                wx_config_set_string(CFG_MACHINE, NULL, "macaddr", temp_macaddr.mb_str());
#endif

                fdd_set_type(0, temp_fda_type);
                fdd_set_type(1, temp_fdb_type);

                if (temp_hdd)
                        strncpy(hdd_controller_name, temp_hdd, sizeof(hdd_controller_name) - 1);
                else
                        strcpy(hdd_controller_name, "none");

                for (int c = 0; c < NUM_DRIVES; c++) {
                        hdc[c].spt = HdGeometryDialog::ParseInt(drives[c].spt->GetLabel());
                        hdc[c].hpc = HdGeometryDialog::ParseInt(drives[c].hpc->GetLabel());
                        hdc[c].tracks = HdGeometryDialog::ParseInt(drives[c].cyl->GetLabel());
                        wx_strlcpy(ide_fn[c], drives[c].fn->GetValue(), sizeof(ide_fn[c]));
                }

                cdrom_channel = new_cdrom_channel;
                zip_channel = new_zip_channel;

                if (has_been_inited) {
                        mem_alloc();
                        loadbios();
                        resetpchard();
                }
        }

        video_speed = SelectedIndex(cb_video_speed) - 1;

        cpu_manufacturer = temp_cpu_m;
        cpu = temp_cpu;
        cpu_set();

        cpu_waitstates = SelectedIndex(cb_waitstates);
        cpu_update_waitstates();

        cd_speed = cd_get_speed(SelectedIndex(cb_cd_speed));
        cd_set_speed(cd_speed);

        cd_model = cd_get_model(SelectedCdModel());
        cd_set_model(cd_model);

        if (has_been_inited)
                saveconfig(NULL);

        speedchanged();

        joystick_type = temp_joystick_type;
        gameport_update_joystick_type();
}

void ConfigDialog::OnOk(wxCommandEvent &event) {
#ifdef USE_NETWORKING
        if (!ValidateMacaddr())
                return;
#endif
        Save();
        pauseEmulation = 0;
        EndModal(wxID_OK);
}

void ConfigDialog::OnCancel(wxCommandEvent &event) {
        pauseEmulation = 0;
        EndModal(wxID_CANCEL);
}

bool ConfigDialog::Open(wxWindow *parent) {
        ConfigDialog dlg(parent);
        return dlg.ShowModal() == wxID_OK;
}
