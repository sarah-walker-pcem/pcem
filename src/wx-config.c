#include "wx-utils.h"
#include "wx-sdl2.h"

#include "ibm.h"
#include "ide.h"
#include "cpu.h"
#include "device.h"
#include "fdd.h"
#include "gameport.h"
#include "hdd.h"
#include "model.h"
#include "mouse.h"
#include "mem.h"
#include "nethandler.h"
#include "nvr.h"
#include "sound.h"
#include "video.h"
#include "vid_voodoo.h"

extern int pause;

extern int is486;
static int romstolist[ROM_MAX], listtomodel[ROM_MAX], romstomodel[ROM_MAX],
                modeltolist[ROM_MAX];
static int settings_sound_to_list[20], settings_list_to_sound[20];
static int settings_mouse_to_list[20], settings_list_to_mouse[20];
#ifdef USE_NETWORKING
static int settings_network_to_list[20], settings_list_to_network[20];
#endif
static char *hdd_names[16];

int has_been_inited = 0;

static int hd_changed = 0;

static char hd_new_name[512];
static int hd_new_spt, hd_new_hpc, hd_new_cyl;
static int hd_new_type;
static int new_cdrom_channel;

extern int pause;

extern void deviceconfig_open(void* hwnd, device_t *device);
extern int hdconf_init(void* hdlg);
extern int hdconf_update(void* hdlg);

static int mouse_valid(int type, int model)
{
        if (((type & MOUSE_TYPE_IF_MASK) == MOUSE_TYPE_PS2)
                        && !(models[model].flags & MODEL_PS2))
                return 0;
        if (((type & MOUSE_TYPE_IF_MASK) == MOUSE_TYPE_AMSTRAD)
                        && !(models[model].flags & MODEL_AMSTRAD))
                return 0;
        if (((type & MOUSE_TYPE_IF_MASK) == MOUSE_TYPE_OLIM24)
                        && !(models[model].flags & MODEL_OLIM24))
                return 0;
        return 1;
}

/*static int mpu401_available(int sound_card)
{
        char* name = sound_card_get_internal_name(sound_card);
        if (name && (!strcmp(name, "sb16") || !strcmp(name, "sbawe32")))
                return TRUE;

        return FALSE;
}*/

static void recalc_vid_list(void* hdlg, int model)
{
        void* h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOVID"));
        int c = 0, d = 0;
        int found_card = 0;

        wx_sendmessage(h, WX_CB_RESETCONTENT, 0, 0);
        wx_sendmessage(h, WX_CB_SETCURSEL, 0, 0);

        while (1)
        {
                char *s = video_card_getname(c);

                if (!s[0])
                        break;

                if (video_card_available(c) && gfx_present[video_new_to_old(c)] &&
                    ((models[model].flags & MODEL_PCI) || !(video_card_getdevice(c)->flags & DEVICE_PCI)))
                {
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)s);
                        if (video_new_to_old(c) == gfxcard)
                        {
                                wx_sendmessage(h, WX_CB_SETCURSEL, d, 0);
                                found_card = 1;
                        }

                        d++;
                }

                c++;
        }
        if (!found_card)
                wx_sendmessage(h, WX_CB_SETCURSEL, 0, 0);
        wx_enablewindow(h, models[model].fixed_gfxcard ? FALSE : TRUE);

        h = wx_getdlgitem(hdlg, WX_ID("IDC_CHECKVOODOO"));
        wx_enablewindow(h, (models[model].flags & MODEL_PCI) ? TRUE : FALSE);
        h = wx_getdlgitem(hdlg, WX_ID("IDC_CONFIGUREVOODOO"));
        wx_enablewindow(h, (models[model].flags & MODEL_PCI) ? TRUE : FALSE);
}

static void recalc_snd_list(void* hdlg, int model)
{
        void* h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOSND"));
        int c = 0, d = 0;

        wx_sendmessage(h, WX_CB_RESETCONTENT, 0, 0);

        while (1)
        {
                char *s = sound_card_getname(c);

                if (!s[0])
                        break;

                settings_sound_to_list[c] = d;

                if (sound_card_available(c))
                {
                        device_t *sound_dev = sound_card_getdevice(c);

                        if (!sound_dev || (sound_dev->flags & DEVICE_MCA) == (models[model].flags & MODEL_MCA))
                        {
                                wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM) s);
                                settings_list_to_sound[d] = c;
                                d++;
                        }
                }

                c++;
        }
        wx_sendmessage(h, WX_CB_SETCURSEL, settings_sound_to_list[sound_card_current], 0);
}

static void recalc_hdd_list(void* hdlg, int model, int use_selected_hdd)
{
        void* h;

        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOHDD"));

        if (models[model].flags & MODEL_HAS_IDE)
        {
                wx_sendmessage(h, WX_CB_RESETCONTENT, 0, 0);
                wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"Internal IDE");
                wx_enablewindow(h, FALSE);
                wx_sendmessage(h, WX_CB_SETCURSEL, 0, 0);
        }
        else
        {
                char *s;
                int valid = 0;
                char old_name[16];
                int c, d;

                if (use_selected_hdd)
                {
                        c = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);

                        if (c != -1 && hdd_names[c])
                        strncpy(old_name, hdd_names[c], sizeof(old_name)-1);
                        else
                        strcpy(old_name, "none");
                }
                else
                strncpy(old_name, hdd_controller_name, sizeof(old_name)-1);

                wx_sendmessage(h, WX_CB_RESETCONTENT, 0, 0);
                c = d = 0;
                while (1)
                {
                        s = hdd_controller_get_name(c);
                        if (s[0] == 0)
                        break;
                        if ((((hdd_controller_get_flags(c) & DEVICE_AT) && !(models[model].flags & MODEL_AT)) ||
                             (hdd_controller_get_flags(c) & DEVICE_MCA) != (models[model].flags & MODEL_MCA)) && c)
                        {
                                c++;
                                continue;
                        }
                        if (!hdd_controller_available(c))
                        {
                                c++;
                                continue;
                        }
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)s);
                        hdd_names[d] = hdd_controller_get_internal_name(c);
                        if (!strcmp(old_name, hdd_names[d]))
                        {
                                wx_sendmessage(h, WX_CB_SETCURSEL, d, 0);
                                valid = 1;
                        }
                        c++;
                        d++;
                }

                if (!valid)
                wx_sendmessage(h, WX_CB_SETCURSEL, 0, 0);

                wx_enablewindow(h, TRUE);
        }
}

#ifdef USE_NETWORKING
static void recalc_net_list(void *hdlg, int model)
{
        void *h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO_NETCARD"));
        int c = 0, d = 0;
        int set_zero = 0;

        wx_sendmessage(h, WX_CB_RESETCONTENT, 0, 0);

        while (1)
        {
                char *s = network_card_getname(c);

                if (!s[0])
                        break;

                settings_network_to_list[c] = d;
                        
                if (network_card_available(c))
                {
                        device_t *network_dev = network_card_getdevice(c);

                        if (!network_dev || (network_dev->flags & DEVICE_MCA) == (models[model].flags & MODEL_MCA))
                        {
                                wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)s);
                                settings_list_to_network[d] = c;
                                d++;
                        }
                        else if (c == network_card_current)
                                set_zero = 1;
                }

                c++;
        }
        if (set_zero)
                wx_sendmessage(h, WX_CB_SETCURSEL, 0, 0);
        else
                wx_sendmessage(h, WX_CB_SETCURSEL, settings_network_to_list[network_card_current], 0);
}
#endif

int config_dlgsave(void* hdlg)
{
        char temp_str[256];
        void* h;
        int c;
        int gfx, fpu;
        int temp_cpu, temp_cpu_m, temp_model;
        int temp_GAMEBLASTER, temp_GUS, temp_SSI2001, temp_voodoo, temp_sound_card_current;
        int temp_dynarec;
        int temp_fda_type, temp_fdb_type;
        int temp_mouse_type;
#ifdef USE_NETWORKING
        int temp_network_card;
#endif
        int hdd_changed = 0;
        char s[260];
        PcemHDC hd[4];

        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO1"));
        temp_model = listtomodel[wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0)];

        h = wx_getdlgitem(hdlg, WX_ID("IDC_MEMSPIN"));
        int mem = wx_sendmessage(h, WX_UDM_GETPOS, 0, 0);
        mem &= ~(models[temp_model].ram_granularity - 1);
        if (mem < models[temp_model].min_ram)
                mem = models[temp_model].min_ram;
        else if (mem > models[temp_model].max_ram)
                mem = models[temp_model].max_ram;
        if ((models[temp_model].flags & MODEL_AT) && models[temp_model].ram_granularity < 128)
                mem *= 1024;

        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOVID"));
        wx_sendmessage(h, WX_CB_GETLBTEXT, wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0),
        (LONG_PARAM) temp_str);
        gfx = video_new_to_old(video_card_getid(temp_str));

        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOCPUM"));
        temp_cpu_m = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO3"));
        temp_cpu = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
        fpu = (models[temp_model].cpu[temp_cpu_m].cpus[temp_cpu].cpu_type
        >= CPU_i486DX) ? 1 : 0;

        h = wx_getdlgitem(hdlg, WX_ID("IDC_CHECK3"));
        temp_GAMEBLASTER = wx_sendmessage(h, WX_BM_GETCHECK, 0, 0);

        h = wx_getdlgitem(hdlg, WX_ID("IDC_CHECKGUS"));
        temp_GUS = wx_sendmessage(h, WX_BM_GETCHECK, 0, 0);

        h = wx_getdlgitem(hdlg, WX_ID("IDC_CHECKSSI"));
        temp_SSI2001 = wx_sendmessage(h, WX_BM_GETCHECK, 0, 0);

        h = wx_getdlgitem(hdlg, WX_ID("IDC_CHECKSYNC"));
        enable_sync = wx_sendmessage(h, WX_BM_GETCHECK, 0, 0);

        h = wx_getdlgitem(hdlg, WX_ID("IDC_CHECKVOODOO"));
        temp_voodoo = wx_sendmessage(h, WX_BM_GETCHECK, 0, 0);

        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOSND"));
        temp_sound_card_current = settings_list_to_sound[wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0)];

        h = wx_getdlgitem(hdlg, WX_ID("IDC_CHECKDYNAREC"));
        temp_dynarec = wx_sendmessage(h, WX_BM_GETCHECK, 0, 0);

        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBODRA"));
        temp_fda_type = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBODRB"));
        temp_fdb_type = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);

        //              h = wx_getdlgitem(hdlg, ID("IDC_COMBOJOY"));
        //              temp_joystick_type = wx_sendmessage(h, CB_GETCURSEL, 0, 0);
        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOMOUSE"));
        temp_mouse_type = settings_list_to_mouse[wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0)];

        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOHDD"));
        c = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
        if (hdd_names[c])
                hdd_changed = strncmp(hdd_names[c], hdd_controller_name, sizeof(hdd_controller_name)-1);

#ifdef USE_NETWORKING
        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO_NETCARD"));
        temp_network_card = settings_list_to_network[wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0)];
#endif                        

        if (temp_model != model || gfx != gfxcard || mem != mem_size ||
            fpu != hasfpu || temp_GAMEBLASTER != GAMEBLASTER || temp_GUS != GUS ||
            temp_SSI2001 != SSI2001 || temp_sound_card_current != sound_card_current ||
            temp_voodoo != voodoo_enabled || temp_dynarec != cpu_use_dynarec ||
            temp_fda_type != fdd_get_type(0) || temp_fdb_type != fdd_get_type(1) ||
            temp_mouse_type != mouse_type || hdd_changed || hd_changed || cdrom_channel != new_cdrom_channel
#ifdef USE_NETWORKING
                            || temp_network_card != network_card_current
#endif
            )
        {
                if (!has_been_inited || confirm())
                {
                        savenvr();
                        model = temp_model;
                        romset = model_getromset();
                        gfxcard = gfx;
                        mem_size = mem;
                        cpu_manufacturer = temp_cpu_m;
                        cpu = temp_cpu;
                        GAMEBLASTER = temp_GAMEBLASTER;
                        GUS = temp_GUS;
                        SSI2001 = temp_SSI2001;
                        sound_card_current = temp_sound_card_current;
                        voodoo_enabled = temp_voodoo;
                        cpu_use_dynarec = temp_dynarec;
                        mouse_type = temp_mouse_type;
#ifdef USE_NETWORKING
                        network_card_current = temp_network_card;
#endif

                        fdd_set_type(0, temp_fda_type);
                        fdd_set_type(1, temp_fdb_type);

                        if (hdd_names[c])
                                strncpy(hdd_controller_name, hdd_names[c], sizeof(hdd_controller_name)-1);
                        else
                                strcpy(hdd_controller_name, "none");

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
                                mem_resize();
                                loadbios();
                                resetpchard();
                        }
                }
                else
                        return TRUE;
        }

        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOSPD"));
        video_speed = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);

        cpu_manufacturer = temp_cpu_m;
        cpu = temp_cpu;
        cpu_set();

        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOWS"));
        cpu_waitstates = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
        cpu_update_waitstates();

        if (has_been_inited)
                saveconfig(NULL);

        speedchanged();

//                                joystick_type = temp_joystick_type;
//                                gameport_update_joystick_type();


        return TRUE;
}

int config_dlgproc(void* hdlg, int message, INT_PARAM wParam, LONG_PARAM lParam)
{
        char temp_str[256];
        void* h;
        int c, d;
        int gfx, mem;
        int temp_cpu, temp_cpu_m, temp_model;
        int temp_sound_card_current;
        int temp_dynarec;
        int cpu_flags;
        int cpu_type;
        int temp_mouse_type;
#ifdef USE_NETWORKING
        int temp_network_card;
#endif

        switch (message)
        {
                case WX_INITDIALOG:
                {
                        pause = 1;
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO1"));
                        for (c = 0; c < ROM_MAX; c++)
                        romstolist[c] = 0;
                        c = d = 0;
                        while (models[c].id != -1)
                        {
                                pclog("INITDIALOG : %i %i %i\n", c, models[c].id,
                                romspresent[models[c].id]);
                                if (romspresent[models[c].id])
                                {
                                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM) models[c].name);
                                        modeltolist[c] = d;
                                        listtomodel[d] = c;
                                        romstolist[models[c].id] = d;
                                        romstomodel[models[c].id] = c;
                                        d++;
                                }
                                c++;
                        }
                        wx_sendmessage(h, WX_CB_SETCURSEL, modeltolist[model], 0);

                        recalc_vid_list(hdlg, romstomodel[romset]);

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOCPUM"));
                        c = 0;
                        while (models[romstomodel[romset]].cpu[c].cpus != NULL && c < 4)
                        {
                                wx_sendmessage(h, WX_CB_ADDSTRING, 0,
                                (LONG_PARAM) models[romstomodel[romset]].cpu[c].name);
                                c++;
                        }
                        wx_enablewindow(h, TRUE);
                        wx_sendmessage(h, WX_CB_SETCURSEL, cpu_manufacturer, 0);
                        if (c == 1)
                                wx_enablewindow(h, FALSE);
                        else
                                wx_enablewindow(h, TRUE);

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO3"));
                        c = 0;
                        while (models[romstomodel[romset]].cpu[cpu_manufacturer].cpus[c].cpu_type != -1)
                        {
                                wx_sendmessage(h, WX_CB_ADDSTRING, 0,
                                (LONG_PARAM) models[romstomodel[romset]].cpu[cpu_manufacturer].cpus[c].name);
                                c++;
                        }
                        wx_enablewindow(h, TRUE);
                        wx_sendmessage(h, WX_CB_SETCURSEL, cpu, 0);

                        recalc_snd_list(hdlg, romstomodel[romset]);

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_CHECK3"));
                        wx_sendmessage(h, WX_BM_SETCHECK, GAMEBLASTER, 0);

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_CHECKGUS"));
                        wx_sendmessage(h, WX_BM_SETCHECK, GUS, 0);

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_CHECKSSI"));
                        wx_sendmessage(h, WX_BM_SETCHECK, SSI2001, 0);

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_CHECKSYNC"));
                        wx_sendmessage(h, WX_BM_SETCHECK, enable_sync, 0);

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_CHECKVOODOO"));
                        wx_sendmessage(h, WX_BM_SETCHECK, voodoo_enabled, 0);

                        cpu_flags =
                        models[romstomodel[romset]].cpu[cpu_manufacturer].cpus[cpu].cpu_flags;
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_CHECKDYNAREC"));
                        if (!(cpu_flags & CPU_SUPPORTS_DYNAREC) || (cpu_flags & CPU_REQUIRES_DYNAREC))
                                wx_enablewindow(h, FALSE);
                        else
                                wx_enablewindow(h, TRUE);
                        wx_sendmessage(h, WX_BM_SETCHECK,
                        ((cpu_flags & CPU_SUPPORTS_DYNAREC) && cpu_use_dynarec)
                        || (cpu_flags & CPU_REQUIRES_DYNAREC), 0);

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOSPD"));
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM) "8-bit");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM) "Slow 16-bit");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM) "Fast 16-bit");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM) "Slow VLB/PCI");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM) "Mid  VLB/PCI");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM) "Fast VLB/PCI");
                        wx_sendmessage(h, WX_CB_SETCURSEL, video_speed, 0);

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_MEMSPIN"));
                        printf("%d\n", models[model].ram_granularity);
                        wx_sendmessage(h, WX_UDM_SETRANGE, 0,
                        (models[romstomodel[romset]].min_ram << 16)
                        | models[romstomodel[romset]].max_ram);
                        wx_sendmessage(h, WX_UDM_SETINCR, 0, models[model].ram_granularity);
                        if (!((models[model].flags & MODEL_AT) && models[model].ram_granularity < 128))
                                wx_sendmessage(h, WX_UDM_SETPOS, 0, mem_size);
                        else
                                wx_sendmessage(h, WX_UDM_SETPOS, 0, mem_size / 1024);

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_CONFIGUREMOD"));
                        if (model_getdevice(model))
                                wx_enablewindow(h, TRUE);
                        else
                                wx_enablewindow(h, FALSE);

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_CONFIGUREVID"));
                        if (video_card_has_config(video_old_to_new(gfxcard)))
                                wx_enablewindow(h, TRUE);
                        else
                                wx_enablewindow(h, FALSE);

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_CONFIGURESND"));
                        if (sound_card_has_config(sound_card_current))
                                wx_enablewindow(h, TRUE);
                        else
                                wx_enablewindow(h, FALSE);

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBODRA"));
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"None");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"5.25\" 360k");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"5.25\" 1.2M");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"5.25\" 1.2M Dual RPM");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"3.5\" 720k");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"3.5\" 1.44M");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"3.5\" 1.44M 3-Mode");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"3.5\" 2.88M");
                        wx_sendmessage(h, WX_CB_SETCURSEL, fdd_get_type(0), 0);
                        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBODRB"));
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"None");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"5.25\" 360k");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"5.25\" 1.2M");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"5.25\" 1.2M Dual RPM");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"3.5\" 720k");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"3.5\" 1.44M");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"3.5\" 1.44M 3-Mode");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM)"3.5\" 2.88M");
                        wx_sendmessage(h, WX_CB_SETCURSEL, fdd_get_type(1), 0);

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_TEXT_MB"));
                        if ((models[model].flags & MODEL_AT) && (models[model].ram_granularity < 128))
                                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM) "MB");
                        else
                                wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM) "KB");

                        //    h = wx_getdlgitem(hdlg, ID("IDC_COMBOJOY"));
                        //    c = 0;
                        //    while (joystick_get_name(c))
                        //    {
                        //            wx_sendmessage(h, CB_ADDSTRING, 0, joystick_get_name(c));
                        //            c++;
                        //    }
                        //    wx_enablewindow(h, TRUE);
                        //    wx_sendmessage(h, CB_SETCURSEL, joystick_type, 0);
                        //
                        //    h = wx_getdlgitem(hdlg, ID("IDC_JOY1"));
                        //    wx_enablewindow(h, (joystick_get_max_joysticks(joystick_type) >= 1) ? TRUE : FALSE);
                        //    h = wx_getdlgitem(hdlg, ID("IDC_JOY2"));
                        //    wx_enablewindow(h, (joystick_get_max_joysticks(joystick_type) >= 2) ? TRUE : FALSE);
                        //    h = wx_getdlgitem(hdlg, ID("IDC_JOY3"));
                        //    wx_enablewindow(h, (joystick_get_max_joysticks(joystick_type) >= 3) ? TRUE : FALSE);
                        //    h = wx_getdlgitem(hdlg, ID("IDC_JOY4"));
                        //    wx_enablewindow(h, (joystick_get_max_joysticks(joystick_type) >= 4) ? TRUE : FALSE);

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOWS"));
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM) "System default");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM) "0 W/S");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM) "1 W/S");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM) "2 W/S");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM) "3 W/S");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM) "4 W/S");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM) "5 W/S");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM) "6 W/S");
                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM) "7 W/S");
                        wx_sendmessage(h, WX_CB_SETCURSEL, cpu_waitstates, 0);
                        cpu_type =
                        models[romstomodel[romset]].cpu[cpu_manufacturer].cpus[cpu].cpu_type;
                        if ((cpu_type >= CPU_286) && (cpu_type <= CPU_386DX))
                                wx_enablewindow(h, TRUE);
                        else
                                wx_enablewindow(h, FALSE);

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOMOUSE"));
                        c = d = 0;
                        while (1)
                        {
                                char *s = mouse_get_name(c);
                                int type;

                                if (!s)
                                break;

                                type = mouse_get_type(c);

                                settings_mouse_to_list[c] = d;

                                if (mouse_valid(type, model))
                                {
                                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM) s);

                                        settings_list_to_mouse[d] = c;
                                        d++;
                                }
                                c++;
                        }
                        wx_sendmessage(h, WX_CB_SETCURSEL, settings_mouse_to_list[mouse_type], 0);

                        recalc_hdd_list(hdlg, romstomodel[romset], 0);

                        hd_changed = 0;
                        new_cdrom_channel = cdrom_channel;

                        hdconf_init(hdlg);

#ifdef USE_NETWORKING
                        recalc_net_list(hdlg, romstomodel[romset]);

                        h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO_NETCARD"));
                        if (network_card_has_config(network_card_current))
                                wx_enablewindow(h, TRUE);
                        else
                                wx_enablewindow(h, FALSE);
#endif

                        return TRUE;
                }
                break;
                case WX_COMMAND:
                {
                        if (wParam == WX_ID("IDC_COMBO1"))
                        {
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO1"));
                                temp_model = listtomodel[wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0)];

                                /*Rebuild manufacturer list*/
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOCPUM"));
                                temp_cpu_m = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
                                wx_sendmessage(h, WX_CB_RESETCONTENT, 0, 0);
                                c = 0;
                                while (models[temp_model].cpu[c].cpus != NULL && c < 4)
                                {
                                        wx_sendmessage(h, WX_CB_ADDSTRING, 0,
                                        (LONG_PARAM) models[temp_model].cpu[c].name);
                                        c++;
                                }
                                if (temp_cpu_m >= c)
                                temp_cpu_m = c - 1;
                                wx_sendmessage(h, WX_CB_SETCURSEL, temp_cpu_m, 0);
                                if (c == 1)
                                        wx_enablewindow(h, FALSE);
                                else
                                        wx_enablewindow(h, TRUE);

                                /*Rebuild CPU list*/
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO3"));
                                temp_cpu = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
                                wx_sendmessage(h, WX_CB_RESETCONTENT, 0, 0);
                                c = 0;
                                while (models[temp_model].cpu[temp_cpu_m].cpus[c].cpu_type != -1)
                                {
                                        wx_sendmessage(h, WX_CB_ADDSTRING, 0,
                                        (LONG_PARAM) models[temp_model].cpu[temp_cpu_m].cpus[c].name);
                                        c++;
                                }
                                if (temp_cpu >= c)
                                        temp_cpu = c - 1;
                                wx_sendmessage(h, WX_CB_SETCURSEL, temp_cpu, 0);

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_CHECKDYNAREC"));
                                temp_dynarec = wx_sendmessage(h, WX_BM_GETCHECK, 0, 0);

                                cpu_flags =
                                models[temp_model].cpu[temp_cpu_m].cpus[temp_cpu].cpu_flags;
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_CHECKDYNAREC"));
                                if (!(cpu_flags & CPU_SUPPORTS_DYNAREC) || (cpu_flags & CPU_REQUIRES_DYNAREC))
                                        wx_enablewindow(h, FALSE);
                                else
                                        wx_enablewindow(h, TRUE);
                                wx_sendmessage(h, WX_BM_SETCHECK,
                                ((cpu_flags & CPU_SUPPORTS_DYNAREC) && temp_dynarec)
                                || (cpu_flags & CPU_REQUIRES_DYNAREC), 0);

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_TEXT_MB"));
                                if ((models[temp_model].flags & MODEL_AT) && models[temp_model].ram_granularity < 128)
                                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM) "MB");
                                else
                                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM) "KB");

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_MEMSPIN"));
                                mem = wx_sendmessage(h, WX_UDM_GETPOS, 0, 0);
                                wx_sendmessage(h, WX_UDM_SETRANGE, 0,
                                (models[temp_model].min_ram << 16)
                                | models[temp_model].max_ram);
                                mem &= ~(models[temp_model].ram_granularity - 1);
                                if (mem < models[temp_model].min_ram)
                                        mem = models[temp_model].min_ram;
                                else if (mem > models[temp_model].max_ram)
                                        mem = models[temp_model].max_ram;
                                wx_sendmessage(h, WX_UDM_SETPOS, 0, mem);
                                wx_sendmessage(h, WX_UDM_SETINCR, 0, models[temp_model].ram_granularity);

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOWS"));
                                cpu_type =
                                models[temp_model].cpu[temp_cpu_m].cpus[temp_cpu].cpu_type;
                                if (cpu_type >= CPU_286 && cpu_type <= CPU_386DX)
                                        wx_enablewindow(h, TRUE);
                                else
                                        wx_enablewindow(h, FALSE);

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_CONFIGUREMOD"));
                                if (model_getdevice(temp_model))
                                        wx_enablewindow(h, TRUE);
                                else
                                        wx_enablewindow(h, FALSE);

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOMOUSE"));
                                temp_mouse_type = settings_list_to_mouse[wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0)];
                                wx_sendmessage(h, WX_CB_RESETCONTENT, 0, 0);
                                c = d = 0;
                                while (1)
                                {
                                        char *s = mouse_get_name(c);
                                        int type;

                                        if (!s)
                                        break;

                                        type = mouse_get_type(c);
                                        settings_mouse_to_list[c] = d;

                                        if (mouse_valid(type, temp_model))
                                        {
                                                wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM) s);

                                                settings_list_to_mouse[d] = c;
                                                d++;
                                        }

                                        c++;
                                }
                                if (mouse_valid(temp_mouse_type, temp_model))
                                        wx_sendmessage(h, WX_CB_SETCURSEL, settings_mouse_to_list[temp_mouse_type], 0);
                                else
                                        wx_sendmessage(h, WX_CB_SETCURSEL, 0, 0);

                                recalc_vid_list(hdlg, temp_model);

                                recalc_hdd_list(hdlg, temp_model, 1);

                                recalc_snd_list(hdlg, temp_model);
#ifdef USE_NETWORKING
                                recalc_net_list(hdlg, temp_model);
#endif
                        }
                        else if (wParam == WX_ID("IDC_COMBOCPUM"))
                        {
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO1"));
                                temp_model = listtomodel[wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0)];
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOCPUM"));
                                temp_cpu_m = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);

                                /*Rebuild CPU list*/
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO3"));
                                temp_cpu = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
                                wx_sendmessage(h, WX_CB_RESETCONTENT, 0, 0);
                                c = 0;
                                while (models[temp_model].cpu[temp_cpu_m].cpus[c].cpu_type != -1)
                                {
                                        wx_sendmessage(h, WX_CB_ADDSTRING, 0,
                                        (LONG_PARAM) models[temp_model].cpu[temp_cpu_m].cpus[c].name);
                                        c++;
                                }
                                if (temp_cpu >= c)
                                temp_cpu = c - 1;
                                wx_sendmessage(h, WX_CB_SETCURSEL, temp_cpu, 0);

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_CHECKDYNAREC"));
                                temp_dynarec = wx_sendmessage(h, WX_BM_GETCHECK, 0, 0);

                                cpu_flags =
                                models[temp_model].cpu[temp_cpu_m].cpus[temp_cpu].cpu_flags;
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_CHECKDYNAREC"));
                                if (!(cpu_flags & CPU_SUPPORTS_DYNAREC) || (cpu_flags & CPU_REQUIRES_DYNAREC))
                                        wx_enablewindow(h, FALSE);
                                else
                                        wx_enablewindow(h, TRUE);
                                wx_sendmessage(h, WX_BM_SETCHECK,
                                ((cpu_flags & CPU_SUPPORTS_DYNAREC) && temp_dynarec)
                                || (cpu_flags & CPU_REQUIRES_DYNAREC), 0);

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOWS"));
                                cpu_type =
                                models[temp_model].cpu[temp_cpu_m].cpus[temp_cpu].cpu_type;
                                if (cpu_type >= CPU_286 && cpu_type <= CPU_386DX)
                                        wx_enablewindow(h, TRUE);
                                else
                                        wx_enablewindow(h, FALSE);

                        }
                        else if (wParam == WX_ID("IDC_COMBO3"))
                        {
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO1"));
                                temp_model = listtomodel[wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0)];
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOCPUM"));
                                temp_cpu_m = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO3"));
                                temp_cpu = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_CHECKDYNAREC"));
                                temp_dynarec = wx_sendmessage(h, WX_BM_GETCHECK, 0, 0);

                                cpu_flags =
                                models[temp_model].cpu[temp_cpu_m].cpus[temp_cpu].cpu_flags;
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_CHECKDYNAREC"));
                                if (!(cpu_flags & CPU_SUPPORTS_DYNAREC) || (cpu_flags & CPU_REQUIRES_DYNAREC))
                                        wx_enablewindow(h, FALSE);
                                else
                                        wx_enablewindow(h, TRUE);
                                wx_sendmessage(h, WX_BM_SETCHECK,
                                ((cpu_flags & CPU_SUPPORTS_DYNAREC) && temp_dynarec)
                                || (cpu_flags & CPU_REQUIRES_DYNAREC), 0);

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOWS"));
                                cpu_type =
                                models[temp_model].cpu[temp_cpu_m].cpus[temp_cpu].cpu_type;
                                if (cpu_type >= CPU_286 && cpu_type <= CPU_386DX)
                                        wx_enablewindow(h, TRUE);
                                else
                                        wx_enablewindow(h, FALSE);
                        }
                        else if (wParam == WX_ID("IDC_CONFIGUREMOD"))
                        {
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO1"));
                                temp_model = listtomodel[wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0)];

                                deviceconfig_open(hdlg, (void *) model_getdevice(temp_model));
                        }
                        else if (wParam == WX_ID("IDC_CONFIGUREVID"))
                        {
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOVID"));
                                wx_sendmessage(h, WX_CB_GETLBTEXT, wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0),
                                (LONG_PARAM) temp_str);

                                deviceconfig_open(hdlg,
                                (void *) video_card_getdevice(video_card_getid(temp_str)));
                        }
                        else if (wParam == WX_ID("IDC_COMBOVID"))
                        {
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOVID"));
                                wx_sendmessage(h, WX_CB_GETLBTEXT, wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0),
                                (LONG_PARAM) temp_str);
                                gfx = video_card_getid(temp_str);

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_CONFIGUREVID"));
                                if (video_card_has_config(gfx))
                                        wx_enablewindow(h, TRUE);
                                else
                                        wx_enablewindow(h, FALSE);
                        }
                        else if (wParam == WX_ID("IDC_CONFIGURESND"))
                        {
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOSND"));
                                temp_sound_card_current = settings_list_to_sound[wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0)];

                                deviceconfig_open(hdlg, (void *) sound_card_getdevice(temp_sound_card_current));
                        }
                        else if (wParam == WX_ID("IDC_COMBOSND"))
                        {
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOSND"));
                                temp_sound_card_current = settings_list_to_sound[wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0)];

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_CONFIGURESND"));
                                if (sound_card_has_config(temp_sound_card_current))
                                        wx_enablewindow(h, TRUE);
                                else
                                        wx_enablewindow(h, FALSE);
                        }
                        else if (wParam == WX_ID("IDC_CONFIGUREVOODOO"))
                        {
                                deviceconfig_open(hdlg, (void *) &voodoo_device);
                        }
                        else if (wParam == WX_ID("IDC_COMBOHDD"))
                        {
                                hdconf_update(hdlg);
                        }
#ifdef USE_NETWORKING
                        else if (wParam == WX_ID("IDC_COMBO_NETCARD"))
                        {
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO_NETCARD"));
                                temp_network_card = settings_list_to_network[wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0)];
                                
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO_NETCARD"));
                                if (network_card_has_config(temp_network_card))
                                        wx_enablewindow(h, TRUE);
                                else
                                        wx_enablewindow(h, FALSE);
                        }
#endif
                        //
                        //      case IDC_COMBOJOY:
                        //      if (HIWORD(wParam) == CBN_SELCHANGE) {
                        //              h = wx_getdlgitem(hdlg, IDC_COMBOJOY);
                        //              temp_joystick_type = wx_sendmessage(h, CB_GETCURSEL, 0, 0);
                        //
                        //              h = wx_getdlgitem(hdlg, IDC_JOY1);
                        //              wx_enablewindow(h,
                        //                              (joystick_get_max_joysticks(temp_joystick_type) >= 1) ?
                        //                                              TRUE : FALSE);
                        //              h = wx_getdlgitem(hdlg, IDC_JOY2);
                        //              wx_enablewindow(h,
                        //                              (joystick_get_max_joysticks(temp_joystick_type) >= 2) ?
                        //                                              TRUE : FALSE);
                        //              h = wx_getdlgitem(hdlg, IDC_JOY3);
                        //              wx_enablewindow(h,
                        //                              (joystick_get_max_joysticks(temp_joystick_type) >= 3) ?
                        //                                              TRUE : FALSE);
                        //              h = wx_getdlgitem(hdlg, IDC_JOY4);
                        //              wx_enablewindow(h,
                        //                              (joystick_get_max_joysticks(temp_joystick_type) >= 4) ?
                        //                                              TRUE : FALSE);
                        //      }
                        //      break;
                        //
                        //      case IDC_JOY1:
                        //      h = wx_getdlgitem(hdlg, IDC_COMBOJOY);
                        //      temp_joystick_type = wx_sendmessage(h, CB_GETCURSEL, 0, 0);
                        //      joystickconfig_open(hdlg, 0, temp_joystick_type);
                        //      break;
                        //      case IDC_JOY2:
                        //      h = wx_getdlgitem(hdlg, IDC_COMBOJOY);
                        //      temp_joystick_type = wx_sendmessage(h, CB_GETCURSEL, 0, 0);
                        //      joystickconfig_open(hdlg, 1, temp_joystick_type);
                        //      break;
                        //      case IDC_JOY3:
                        //      h = wx_getdlgitem(hdlg, IDC_COMBOJOY);
                        //      temp_joystick_type = wx_sendmessage(h, CB_GETCURSEL, 0, 0);
                        //      joystickconfig_open(hdlg, 2, temp_joystick_type);
                        //      break;
                        //      case IDC_JOY4:
                        //      h = wx_getdlgitem(hdlg, IDC_COMBOJOY);
                        //      temp_joystick_type = wx_sendmessage(h, CB_GETCURSEL, 0, 0);
                        //      joystickconfig_open(hdlg, 3, temp_joystick_type);
                        //      break;
                        return 0;
                }
        }
        return 0;
}

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

static int hdd_controller_selected_is_mfm(void* hdlg)
{
        void* h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOHDD"));
        int c = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
        if (hdd_names[c])
                return hdd_controller_is_mfm(hdd_names[c]);
        return 0;
}

static void check_hd_type(void* hdlg, off64_t sz)
{
        int c;

        hd_new_type = 0;

        if (hdd_controller_selected_is_mfm(hdlg))
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

static void update_hdd_cdrom(void* hdlg)
{
        void* h;
        int i, j, b;
        char s[25];

        int is_mfm = hdd_controller_selected_is_mfm(hdlg);

        for (i = 0; i < 4; ++i)
        {
                b = !is_mfm && (new_cdrom_channel == i);
                sprintf(s, "IDC_HDD[%d]", i);
                h = wx_getdlgitem(hdlg, WX_ID(s));
                wx_sendmessage(h, WX_BM_SETCHECK, b ? 0 : 1, 0);
                sprintf(s, "IDC_CDROM[%d]", i);
                h = wx_getdlgitem(hdlg, WX_ID(s));
                wx_sendmessage(h, WX_BM_SETCHECK, b ? 1 : 0, 0);
                sprintf(s, "IDC_EDIT_FN[%d]", i);
                wx_enablewindow(wx_getdlgitem(hdlg, WX_ID(s)), !b);
                sprintf(s, "IDC_FILE[%d]", i);
                wx_enablewindow(wx_getdlgitem(hdlg, WX_ID(s)), !b);
                sprintf(s, "IDC_EJECT[%d]", i);
                wx_enablewindow(wx_getdlgitem(hdlg, WX_ID(s)), !b);
                sprintf(s, "IDC_NEW[%d]", i);
                wx_enablewindow(wx_getdlgitem(hdlg, WX_ID(s)), !b);
                sprintf(s, "IDC_EDIT_SPT[%d]", i);
                wx_enablewindow(wx_getdlgitem(hdlg, WX_ID(s)), !b);
                sprintf(s, "IDC_EDIT_HPC[%d]", i);
                wx_enablewindow(wx_getdlgitem(hdlg, WX_ID(s)), !b);
                sprintf(s, "IDC_EDIT_CYL[%d]", i);
                wx_enablewindow(wx_getdlgitem(hdlg, WX_ID(s)), !b);
                sprintf(s, "IDC_TEXT_SIZE[%d]", i);
                wx_enablewindow(wx_getdlgitem(hdlg, WX_ID(s)), !b);
                for (j = 1; j < 6; ++j)
                {
                        sprintf(s, "IDC_HDD_LABEL[%d]", i*10+j);
                        wx_enablewindow(wx_getdlgitem(hdlg, WX_ID(s)), !b);
                }
        }
}

static int create_drive(void* data)
{
        int c;
        uint8_t buf[512];
        FILE* f = (FILE*)data;
        memset(buf, 0, 512);
        for (c = 0; c < (hd_new_cyl * hd_new_hpc * hd_new_spt); c++)
            fwrite(buf, 512, 1, f);

        fclose(f);

        return 1;
}

static int hdnew_dlgproc(void* hdlg, int message, INT_PARAM wParam, LONG_PARAM lParam)
{
        char s[260];
        void* h;
        int c;
        PcemHDC hd[4];
        FILE *f;
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
                sprintf(s, "%imb", (((511*16*63)*512)/1024)/1024);
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
                                wx_messagebox(hdlg,"Please enter a valid filename","PCem error",WX_MB_OK);
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
                                wx_messagebox(hdlg, "Drive has too many sectors (maximum is 63)", "PCem error", WX_MB_OK);
                                return TRUE;
                        }
                        if (hd_new_hpc > 16)
                        {
                                wx_messagebox(hdlg, "Drive has too many heads (maximum is 16)", "PCem error", WX_MB_OK);
                                return TRUE;
                        }
                        if (hd_new_cyl > 16383)
                        {
                                wx_messagebox(hdlg, "Drive has too many cylinders (maximum is 16383)", "PCem error", WX_MB_OK);
                                return TRUE;
                        }

                        f = fopen64(hd_new_name, "wb");
                        if (!f)
                        {
                                wx_messagebox(hdlg, "Can't open file for write", "PCem error", WX_MB_OK);
                                return TRUE;
                        }
                        wx_progressdialogpulse(hdlg, "PCem", "Creating drive, please wait...", create_drive, f);

                        wx_messagebox(hdlg, "Drive created, remember to partition and format the new drive.", "PCem", WX_MB_OK);

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
                        sprintf(s, "%imb", (int)(((((uint64_t)hd[0].tracks*(uint64_t)hd[0].hpc)*(uint64_t)hd[0].spt)*512)/1024)/1024);
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
                                sprintf(s, "%imb", (int)(((uint64_t)hd_types[hd_type-1].cylinders*hd_types[hd_type-1].heads*17*512)/1024)/1024);
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
                sprintf(s, "%imb", (int)((((uint64_t)hd_new_spt*(uint64_t)hd_new_hpc*(uint64_t)hd_new_cyl)*512)/1024)/1024);
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
                                wx_messagebox(hdlg,"Drive has too many sectors (maximum is 63)","PCem error",WX_MB_OK);
                                return TRUE;
                        }
                        if (hd_new_hpc > 16)
                        {
                                wx_messagebox(hdlg,"Drive has too many heads (maximum is 16)","PCem error",WX_MB_OK);
                                return TRUE;
                        }
                        if (hd_new_cyl > 16383)
                        {
                                wx_messagebox(hdlg,"Drive has too many cylinders (maximum is 16383)","PCem error",WX_MB_OK);
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
                        sprintf(s, "%imb", (int)(((((uint64_t)hd[0].tracks*(uint64_t)hd[0].hpc)*(uint64_t)hd[0].spt)*512)/1024)/1024);
                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                        return TRUE;
                }

        }
        return FALSE;
}

int hdconf_init(void* hdlg)
{
        char s[260];
        void* h;
        PcemHDC hd[4];

        hd[0] = hdc[0];
        hd[1] = hdc[1];
        hd[2] = hdc[2];
        hd[3] = hdc[3];

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
        sprintf(s, "%imb", (int)(((((uint64_t)hd[0].tracks*(uint64_t)hd[0].hpc)*(uint64_t)hd[0].spt)*512)/1024)/1024);
        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);

        h = wx_getdlgitem(hdlg, WX_ID("IDC_TEXT_SIZE[1]"));
        sprintf(s, "%imb", (int)(((((uint64_t)hd[1].tracks*(uint64_t)hd[1].hpc)*(uint64_t)hd[1].spt)*512)/1024)/1024);
        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);

        h = wx_getdlgitem(hdlg, WX_ID("IDC_TEXT_SIZE[2]"));
        sprintf(s, "%imb", (int)(((((uint64_t)hd[2].tracks*(uint64_t)hd[2].hpc)*(uint64_t)hd[2].spt)*512)/1024)/1024);
        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);

        h = wx_getdlgitem(hdlg, WX_ID("IDC_TEXT_SIZE[3]"));
        sprintf(s, "%imb", (int)(((((uint64_t)hd[3].tracks*(uint64_t)hd[3].hpc)*(uint64_t)hd[3].spt)*512)/1024)/1024);
        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);

        return hdconf_update(hdlg);
}

int hdconf_update(void* hdlg)
{
        char s[260];

        int is_mfm = hdd_controller_selected_is_mfm(hdlg);

        update_hdd_cdrom(hdlg);

        int i, j;
        for (i = 0; i < 4; ++i)
        {
                sprintf(s, "IDC_HDD_LABEL[%d]", i*10);
                wx_enablewindow(wx_getdlgitem(hdlg, WX_ID(s)), !is_mfm);
                sprintf(s, "IDC_HDD[%d]", i);
                wx_enablewindow(wx_getdlgitem(hdlg, WX_ID(s)), !is_mfm);
                sprintf(s, "IDC_CDROM[%d]", i);
                wx_enablewindow(wx_getdlgitem(hdlg, WX_ID(s)), !is_mfm);
                if (i >= 2)
                {
                        sprintf(s, "IDC_EDIT_FN[%d]", i);
                        wx_enablewindow(wx_getdlgitem(hdlg, WX_ID(s)), !is_mfm);
                        sprintf(s, "IDC_FILE[%d]", i);
                        wx_enablewindow(wx_getdlgitem(hdlg, WX_ID(s)), !is_mfm);
                        sprintf(s, "IDC_EJECT[%d]", i);
                        wx_enablewindow(wx_getdlgitem(hdlg, WX_ID(s)), !is_mfm);
                        sprintf(s, "IDC_NEW[%d]", i);
                        wx_enablewindow(wx_getdlgitem(hdlg, WX_ID(s)), !is_mfm);
                        sprintf(s, "IDC_EDIT_SPT[%d]", i);
                        wx_enablewindow(wx_getdlgitem(hdlg, WX_ID(s)), !is_mfm);
                        sprintf(s, "IDC_EDIT_HPC[%d]", i);
                        wx_enablewindow(wx_getdlgitem(hdlg, WX_ID(s)), !is_mfm);
                        sprintf(s, "IDC_EDIT_CYL[%d]", i);
                        wx_enablewindow(wx_getdlgitem(hdlg, WX_ID(s)), !is_mfm);
                        sprintf(s, "IDC_TEXT_SIZE[%d]", i);
                        wx_enablewindow(wx_getdlgitem(hdlg, WX_ID(s)), !is_mfm);
                        for (j = 1; j < 6; ++j)
                        {
                                sprintf(s, "IDC_HDD_LABEL[%d]", i*10+j);
                                wx_enablewindow(wx_getdlgitem(hdlg, WX_ID(s)), !is_mfm);
                        }
                }
        }

        return TRUE;
}

int hdconf_dlgproc(void* hdlg, int message, INT_PARAM wParam, LONG_PARAM lParam)
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
                return hdconf_init(hdlg);

                case WX_COMMAND:
                if (ID_IS("IDC_EJECT[0]"))
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
                                sprintf(s, "%imb", (int)(((((uint64_t)hd_new_cyl*(uint64_t)hd_new_hpc)*(uint64_t)hd_new_spt)*512)/1024)/1024);
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
                                        wx_messagebox(hdlg,"Can't open file for read","PCem error",WX_MB_OK);
                                        return TRUE;
                                }
                                fseeko64(f, -1, SEEK_END);
                                sz = ftello64(f) + 1;
                                fclose(f);
                                check_hd_type(hdlg, sz);

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
                                        sprintf(s, "%imb", (int)(((((uint64_t)hd_new_cyl*(uint64_t)hd_new_hpc)*(uint64_t)hd_new_spt)*512)/1024)/1024);
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
                                sprintf(s, "%imb", (int)(((((uint64_t)hd_new_cyl*(uint64_t)hd_new_hpc)*(uint64_t)hd_new_spt)*512)/1024)/1024);
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
                                        wx_messagebox(hdlg,"Can't open file for read","PCem error",WX_MB_OK);
                                        return TRUE;
                                }
                                fseeko64(f, -1, SEEK_END);
                                sz = ftello64(f) + 1;
                                fclose(f);
                                check_hd_type(hdlg, sz);

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
                                        sprintf(s, "%imb", (int)(((((uint64_t)hd_new_cyl*(uint64_t)hd_new_hpc)*(uint64_t)hd_new_spt)*512)/1024)/1024);
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
                                sprintf(s, "%imb", (int)(((((uint64_t)hd_new_cyl*(uint64_t)hd_new_hpc)*(uint64_t)hd_new_spt)*512)/1024)/1024);
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
                                        wx_messagebox(hdlg,"Can't open file for read","PCem error",WX_MB_OK);
                                        return TRUE;
                                }
                                fseeko64(f, -1, SEEK_END);
                                sz = ftello64(f) + 1;
                                fclose(f);
                                check_hd_type(hdlg, sz);

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
                                        sprintf(s, "%imb", (int)(((((uint64_t)hd_new_cyl*(uint64_t)hd_new_hpc)*(uint64_t)hd_new_spt)*512)/1024)/1024);
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
                                sprintf(s, "%imb", (int)(((((uint64_t)hd_new_cyl*(uint64_t)hd_new_hpc)*(uint64_t)hd_new_spt)*512)/1024)/1024);
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
                                        wx_messagebox(hdlg,"Can't open file for read","PCem error",WX_MB_OK);
                                        return TRUE;
                                }
                                fseeko64(f, -1, SEEK_END);
                                sz = ftello64(f) + 1;
                                fclose(f);
                                check_hd_type(hdlg, sz);

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
                                        sprintf(s, "%imb", (int)(((((uint64_t)hd_new_cyl*(uint64_t)hd_new_hpc)*(uint64_t)hd_new_spt)*512)/1024)/1024);
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
                        sprintf(s, "%imb", ((((hd[0].tracks*hd[0].hpc)*hd[0].spt)*512)/1024)/1024);
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
                        sprintf(s, "%imb", ((((hd[1].tracks*hd[1].hpc)*hd[1].spt)*512)/1024)/1024);
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
                        sprintf(s, "%imb", ((((hd[2].tracks*hd[2].hpc)*hd[2].spt)*512)/1024)/1024);
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
                        sprintf(s, "%imb", ((((hd[3].tracks*hd[3].hpc)*hd[3].spt)*512)/1024)/1024);
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

int config_dialog_proc(void* hdlg, int message, INT_PARAM wParam, LONG_PARAM lParam)
{
        switch (message)
        {
        case WX_INITDIALOG:
                pause = 1;
                return config_dlgproc(hdlg, message, wParam, lParam);
        case WX_COMMAND:
        {
                if (wParam == wxID_OK)
                {
                        config_dlgsave(hdlg);
                        pause = 0;
                        wx_enddialog(hdlg, 1);
                        return TRUE;
                }
                else if (wParam == wxID_CANCEL)
                {
                        pause = 0;
                        wx_enddialog(hdlg, 0);
                        return TRUE;
                }
                else if (wParam == WX_ID("IDC_NOTEBOOK"))
                {
                        if (lParam == 3)
                                hdconf_update(hdlg);
                }
                break;
        }
        }
        config_dlgproc(hdlg, message, wParam, lParam);
        hdconf_dlgproc(hdlg, message, wParam, lParam);
        return TRUE;
}


int config_open(void* hwnd)
{
        return wx_dialogbox(hwnd, "ConfigureDlg", config_dialog_proc);
}
