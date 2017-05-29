#include "wx-utils.h"

#include "ibm.h"
#include "cpu.h"
#include "device.h"
#include "fdd.h"
#include "gameport.h"
#include "hdd.h"
#include "model.h"
#include "mouse.h"
#include "mem.h"
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
static int settings_network_to_list[20], settings_list_to_network[20];
static char *hdd_names[16];

int has_been_inited = 0;

extern void deviceconfig_open(void* hwnd, device_t *device);

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

int config_dlgproc(void* hdlg, int message, INT_PARAM wParam, LONG_PARAM lParam)
{
        char temp_str[256];
        void* h;
        int c, d;
        int rom, gfx, mem, fpu;
        int temp_cpu, temp_cpu_m, temp_model;
        int temp_GAMEBLASTER, temp_GUS, temp_SSI2001, temp_voodoo, temp_sound_card_current;
        int temp_dynarec;
        int cpu_flags;
        int temp_fda_type, temp_fdb_type;
        int temp_joystick_type;
        int cpu_type;
        int temp_mouse_type;
        int hdd_changed;

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
                        while (models[romstomodel[romset]].cpu[cpu_manufacturer].cpus[c].cpu_type
                        != -1)
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
                        if (!(models[model].flags & MODEL_AT))
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
                        if (models[model].flags & MODEL_AT)
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

                        return TRUE;
                }
                break;
                case WX_COMMAND:
                {
                        if (wParam == wxID_OK)
                        {
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBO1"));
                                temp_model = listtomodel[wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0)];

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_MEMSPIN"));
                                int mem = wx_sendmessage(h, WX_UDM_GETPOS, 0, 0);
                                mem &= ~(models[temp_model].ram_granularity - 1);
                                if (mem < models[temp_model].min_ram)
                                        mem = models[temp_model].min_ram;
                                else if (mem > models[temp_model].max_ram)
                                        mem = models[temp_model].max_ram;
                                if (models[temp_model].flags & MODEL_AT)
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
                                temp_sound_card_current = settings_list_to_sound[wx_sendmessage(h,
                                WX_CB_GETCURSEL, 0, 0)];

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_CHECKDYNAREC"));
                                temp_dynarec = wx_sendmessage(h, WX_BM_GETCHECK, 0, 0);

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBODRA"));
                                temp_fda_type = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBODRB"));
                                temp_fdb_type = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);

                                //		h = wx_getdlgitem(hdlg, ID("IDC_COMBOJOY"));
                                //		temp_joystick_type = wx_sendmessage(h, CB_GETCURSEL, 0, 0);
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOMOUSE"));
                                temp_mouse_type = settings_list_to_mouse[wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0)];

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOHDD"));
                                c = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
                                if (hdd_names[c])
                                        hdd_changed = strncmp(hdd_names[c], hdd_controller_name, sizeof(hdd_controller_name)-1);

                                if (temp_model != model || gfx != gfxcard || mem != mem_size ||
                                    fpu != hasfpu || temp_GAMEBLASTER != GAMEBLASTER || temp_GUS != GUS ||
                                    temp_SSI2001 != SSI2001 || temp_sound_card_current != sound_card_current ||
                                    temp_voodoo != voodoo_enabled || temp_dynarec != cpu_use_dynarec ||
                                    temp_fda_type != fdd_get_type(0) || temp_fdb_type != fdd_get_type(1) ||
                                    temp_mouse_type != mouse_type || hdd_changed)
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

                                                fdd_set_type(0, temp_fda_type);
                                                fdd_set_type(1, temp_fdb_type);

                                                if (hdd_names[c])
                                                        strncpy(hdd_controller_name, hdd_names[c], sizeof(hdd_controller_name)-1);
                                                else
                                                        strcpy(hdd_controller_name, "none");


                                                if (has_been_inited)
                                                {
                                                        mem_resize();
                                                        loadbios();
                                                        resetpchard();
                                                }
                                        }
                                        else
                                        {
                                                wx_enddialog(hdlg, 0);
                                                pause = 0;
                                                return TRUE;
                                        }
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

                        }
                        if (wParam == wxID_OK || wParam == wxID_CANCEL)
                        {
                                wx_enddialog(hdlg, 0);
                                pause = 0;
                                return TRUE;
                        }
                        else if (wParam == WX_ID("IDC_COMBO1"))
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
                                if (models[temp_model].flags & MODEL_AT)
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
                                temp_sound_card_current = settings_list_to_sound[wx_sendmessage(h,
                                WX_CB_GETCURSEL, 0, 0)];

                                deviceconfig_open(hdlg,
                                (void *) sound_card_getdevice(temp_sound_card_current));
                        }
                        else if (wParam == WX_ID("IDC_COMBOSND"))
                        {
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_COMBOSND"));
                                temp_sound_card_current = settings_list_to_sound[wx_sendmessage(h,
                                WX_CB_GETCURSEL, 0, 0)];

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
                        //
                        //	case IDC_COMBOJOY:
                        //	if (HIWORD(wParam) == CBN_SELCHANGE) {
                        //		h = wx_getdlgitem(hdlg, IDC_COMBOJOY);
                        //		temp_joystick_type = wx_sendmessage(h, CB_GETCURSEL, 0, 0);
                        //
                        //		h = wx_getdlgitem(hdlg, IDC_JOY1);
                        //		wx_enablewindow(h,
                        //				(joystick_get_max_joysticks(temp_joystick_type) >= 1) ?
                        //						TRUE : FALSE);
                        //		h = wx_getdlgitem(hdlg, IDC_JOY2);
                        //		wx_enablewindow(h,
                        //				(joystick_get_max_joysticks(temp_joystick_type) >= 2) ?
                        //						TRUE : FALSE);
                        //		h = wx_getdlgitem(hdlg, IDC_JOY3);
                        //		wx_enablewindow(h,
                        //				(joystick_get_max_joysticks(temp_joystick_type) >= 3) ?
                        //						TRUE : FALSE);
                        //		h = wx_getdlgitem(hdlg, IDC_JOY4);
                        //		wx_enablewindow(h,
                        //				(joystick_get_max_joysticks(temp_joystick_type) >= 4) ?
                        //						TRUE : FALSE);
                        //	}
                        //	break;
                        //
                        //	case IDC_JOY1:
                        //	h = wx_getdlgitem(hdlg, IDC_COMBOJOY);
                        //	temp_joystick_type = wx_sendmessage(h, CB_GETCURSEL, 0, 0);
                        //	joystickconfig_open(hdlg, 0, temp_joystick_type);
                        //	break;
                        //	case IDC_JOY2:
                        //	h = wx_getdlgitem(hdlg, IDC_COMBOJOY);
                        //	temp_joystick_type = wx_sendmessage(h, CB_GETCURSEL, 0, 0);
                        //	joystickconfig_open(hdlg, 1, temp_joystick_type);
                        //	break;
                        //	case IDC_JOY3:
                        //	h = wx_getdlgitem(hdlg, IDC_COMBOJOY);
                        //	temp_joystick_type = wx_sendmessage(h, CB_GETCURSEL, 0, 0);
                        //	joystickconfig_open(hdlg, 2, temp_joystick_type);
                        //	break;
                        //	case IDC_JOY4:
                        //	h = wx_getdlgitem(hdlg, IDC_COMBOJOY);
                        //	temp_joystick_type = wx_sendmessage(h, CB_GETCURSEL, 0, 0);
                        //	joystickconfig_open(hdlg, 3, temp_joystick_type);
                        //	break;
                        return 0;
                }
        }
}

void config_open(void* hwnd)
{
        wx_dialogbox(hwnd, "ConfigureDlg", config_dlgproc);
}
