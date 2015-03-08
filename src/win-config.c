#define BITMAP WINDOWS_BITMAP
#include <windows.h>
#include <windowsx.h>
#undef BITMAP

#include <commctrl.h>

#include "ibm.h"
#include "cpu.h"
#include "device.h"
#include "model.h"
#include "resources.h"
#include "sound.h"
#include "video.h"
#include "vid_voodoo.h"
#include "win.h"

extern int is486;
static int romstolist[ROM_MAX], listtomodel[ROM_MAX], romstomodel[ROM_MAX], modeltolist[ROM_MAX];
static int settings_sound_to_list[20], settings_list_to_sound[20];

static BOOL CALLBACK config_dlgproc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
{
        char temp_str[256];
        HWND h;
        int c, d;
        int rom, gfx, mem, fpu;
        int temp_cpu, temp_cpu_m, temp_model;
        int temp_GAMEBLASTER, temp_GUS, temp_SSI2001, temp_voodoo, temp_sound_card_current;
//        pclog("Dialog msg %i %08X\n",message,message);
        switch (message)
        {
                case WM_INITDIALOG:
                pause = 1;
                h = GetDlgItem(hdlg, IDC_COMBO1);
                for (c = 0; c < ROM_MAX; c++)
                        romstolist[c] = 0;
                c = d = 0;
                while (models[c].id != -1)
                {
                        pclog("INITDIALOG : %i %i %i\n",c,models[c].id,romspresent[models[c].id]);
                        if (romspresent[models[c].id])
                        {
                                SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)models[c].name);
                                modeltolist[c] = d;
                                listtomodel[d] = c;
                                romstolist[models[c].id] = d;
                                romstomodel[models[c].id] = c;
                                d++;
                        }
                        c++;
                }
                SendMessage(h, CB_SETCURSEL, modeltolist[model], 0);

                h = GetDlgItem(hdlg, IDC_COMBOVID);
                c = d = 0;
                while (1)
                {
                        char *s = video_card_getname(c);

                        if (!s[0])
                                break;

                        if (video_card_available(c) && gfx_present[video_new_to_old(c)])
                        {
                                SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)s);
                                if (video_new_to_old(c) == gfxcard)
                                        SendMessage(h, CB_SETCURSEL, d, 0);                                

                                d++;
                        }

                        c++;
                }
                if (models[model].fixed_gfxcard)
                        EnableWindow(h, FALSE);

                h = GetDlgItem(hdlg, IDC_COMBOCPUM);
                c = 0;
                while (models[romstomodel[romset]].cpu[c].cpus != NULL && c < 3)
                {
                        SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)models[romstomodel[romset]].cpu[c].name);
                        c++;
                }
                EnableWindow(h, TRUE);
                SendMessage(h, CB_SETCURSEL, cpu_manufacturer, 0);
                if (c == 1) EnableWindow(h, FALSE);
                else        EnableWindow(h, TRUE);

                h = GetDlgItem(hdlg, IDC_COMBO3);
                c = 0;
                while (models[romstomodel[romset]].cpu[cpu_manufacturer].cpus[c].cpu_type != -1)
                {
                        SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)models[romstomodel[romset]].cpu[cpu_manufacturer].cpus[c].name);
                        c++;
                }
                EnableWindow(h, TRUE);
                SendMessage(h, CB_SETCURSEL, cpu, 0);

                h = GetDlgItem(hdlg, IDC_COMBOSND);
                c = d = 0;
                while (1)
                {
                        char *s = sound_card_getname(c);

                        if (!s[0])
                                break;

                        settings_sound_to_list[c] = d;
                        
                        if (sound_card_available(c))
                        {
                                SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)s);
                                settings_list_to_sound[d] = c;
                                d++;
                        }

                        c++;
                }
                SendMessage(h, CB_SETCURSEL, settings_sound_to_list[sound_card_current], 0);

                h=GetDlgItem(hdlg, IDC_CHECK3);
                SendMessage(h, BM_SETCHECK, GAMEBLASTER, 0);

                h=GetDlgItem(hdlg, IDC_CHECKGUS);
                SendMessage(h, BM_SETCHECK, GUS, 0);

                h=GetDlgItem(hdlg, IDC_CHECKSSI);
                SendMessage(h, BM_SETCHECK, SSI2001, 0);
                
                h=GetDlgItem(hdlg, IDC_CHECK2);
                SendMessage(h, BM_SETCHECK, slowega, 0);

                h=GetDlgItem(hdlg, IDC_CHECK4);
                SendMessage(h, BM_SETCHECK, cga_comp, 0);

                h=GetDlgItem(hdlg, IDC_CHECKVOODOO);
                SendMessage(h, BM_SETCHECK, voodoo_enabled, 0);

                h = GetDlgItem(hdlg, IDC_COMBOCHC);
                SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)"A little");
                SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)"A bit");
                SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)"Some");
                SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)"A lot");
                SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)"Infinite");
                SendMessage(h, CB_SETCURSEL, cache, 0);

                h = GetDlgItem(hdlg, IDC_COMBOSPD);
                SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)"8-bit");
                SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)"Slow 16-bit");
                SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)"Fast 16-bit");
                SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)"Slow VLB/PCI");
                SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)"Mid  VLB/PCI");
                SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)"Fast VLB/PCI");                
                SendMessage(h, CB_SETCURSEL, video_speed, 0);

                h = GetDlgItem(hdlg, IDC_MEMSPIN);
                SendMessage(h, UDM_SETBUDDY, (WPARAM)GetDlgItem(hdlg, IDC_MEMTEXT), 0);
                SendMessage(h, UDM_SETRANGE, 0, (1 << 16) | 256);
                SendMessage(h, UDM_SETPOS, 0, mem_size);

                h = GetDlgItem(hdlg, IDC_CONFIGUREVID);
                if (video_card_has_config(video_old_to_new(gfxcard)))
                        EnableWindow(h, TRUE);
                else
                        EnableWindow(h, FALSE);
                
                h = GetDlgItem(hdlg, IDC_CONFIGURESND);
                if (sound_card_has_config(sound_card_current))
                        EnableWindow(h, TRUE);
                else
                        EnableWindow(h, FALSE);
                        
                return TRUE;
                
                case WM_COMMAND:
                switch (LOWORD(wParam))
                {
                        case IDOK:
			h = GetDlgItem(hdlg, IDC_MEMTEXT);
			SendMessage(h, WM_GETTEXT, 255, (LPARAM)temp_str);
			sscanf(temp_str, "%i", &mem);
			if (mem < 1 || mem > 256)
			{
				MessageBox(NULL, "Invalid memory size\nMemory must be between 1 and 256 MB", "PCem", MB_OK);
				break;
			}
			
			
                        h = GetDlgItem(hdlg, IDC_COMBO1);
                        temp_model = listtomodel[SendMessage(h, CB_GETCURSEL, 0, 0)];

                        h = GetDlgItem(hdlg, IDC_COMBOVID);
                        SendMessage(h, CB_GETLBTEXT, SendMessage(h, CB_GETCURSEL, 0, 0), (LPARAM)temp_str);
                        gfx = video_new_to_old(video_card_getid(temp_str));

                        h = GetDlgItem(hdlg, IDC_COMBOCPUM);
                        temp_cpu_m = SendMessage(h, CB_GETCURSEL, 0, 0);
                        h = GetDlgItem(hdlg, IDC_COMBO3);
                        temp_cpu = SendMessage(h, CB_GETCURSEL, 0, 0);
                        fpu = (models[temp_model].cpu[temp_cpu_m].cpus[temp_cpu].cpu_type >= CPU_i486DX) ? 1 : 0;

                        h = GetDlgItem(hdlg, IDC_CHECK3);
                        temp_GAMEBLASTER = SendMessage(h, BM_GETCHECK, 0, 0);

                        h = GetDlgItem(hdlg, IDC_CHECKGUS);
                        temp_GUS = SendMessage(h, BM_GETCHECK, 0, 0);

                        h = GetDlgItem(hdlg, IDC_CHECKSSI);
                        temp_SSI2001 = SendMessage(h, BM_GETCHECK, 0, 0);

                        h = GetDlgItem(hdlg, IDC_CHECKVOODOO);
                        temp_voodoo = SendMessage(h, BM_GETCHECK, 0, 0);

                        h = GetDlgItem(hdlg, IDC_COMBOSND);
                        temp_sound_card_current = settings_list_to_sound[SendMessage(h, CB_GETCURSEL, 0, 0)];

                        if (temp_model != model || gfx != gfxcard || mem != mem_size || fpu != hasfpu || temp_GAMEBLASTER != GAMEBLASTER || temp_GUS != GUS || temp_SSI2001 != SSI2001 || temp_sound_card_current != sound_card_current || temp_voodoo != voodoo_enabled)
                        {
                                if (MessageBox(NULL,"This will reset PCem!\nOkay to continue?","PCem",MB_OKCANCEL)==IDOK)
                                {
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
                                        
                                        mem_resize();
                                        loadbios();
                                        resetpchard();
                                }
                                else
                                {
                                        EndDialog(hdlg, 0);
                                        pause = 0;
                                        return TRUE;
                                }
                        }

                        h = GetDlgItem(hdlg, IDC_COMBOSPD);
                        video_speed = SendMessage(h, CB_GETCURSEL, 0, 0);

                        h = GetDlgItem(hdlg, IDC_CHECK4);
                        cga_comp=SendMessage(h, BM_GETCHECK, 0, 0);

                        cpu_manufacturer = temp_cpu_m;
                        cpu = temp_cpu;
                        cpu_set();
                        
                        h = GetDlgItem(hdlg, IDC_COMBOCHC);
                        cache=SendMessage(h, CB_GETCURSEL, 0, 0);
                        mem_updatecache();
                        
                        saveconfig();

                        speedchanged();

                        case IDCANCEL:
                        EndDialog(hdlg, 0);
                        pause=0;
                        return TRUE;
                        case IDC_COMBO1:
                        if (HIWORD(wParam) == CBN_SELCHANGE)
                        {
                                h = GetDlgItem(hdlg,IDC_COMBO1);
                                temp_model = listtomodel[SendMessage(h,CB_GETCURSEL,0,0)];
                                
                                /*Enable/disable gfxcard list*/
                                h = GetDlgItem(hdlg, IDC_COMBOVID);
                                if (!models[temp_model].fixed_gfxcard)
                                {
                                        char *s = video_card_getname(video_old_to_new(gfxcard));
                                        
                                        EnableWindow(h, TRUE);
                                        
                                        c = 0;
                                        while (1)
                                        {
                                                SendMessage(h, CB_GETLBTEXT, c, (LPARAM)temp_str);
                                                if (!strcmp(temp_str, s))
                                                        break;
                                                c++;
                                        }
                                        SendMessage(h, CB_SETCURSEL, c, 0);
                                }
                                else
                                        EnableWindow(h, FALSE);
                                
                                /*Rebuild manufacturer list*/
                                h = GetDlgItem(hdlg, IDC_COMBOCPUM);
                                temp_cpu_m = SendMessage(h, CB_GETCURSEL, 0, 0);
                                SendMessage(h, CB_RESETCONTENT, 0, 0);
                                c = 0;
                                while (models[temp_model].cpu[c].cpus != NULL && c < 3)
                                {
                                        SendMessage(h,CB_ADDSTRING,0,(LPARAM)(LPCSTR)models[temp_model].cpu[c].name);
                                        c++;
                                }
                                if (temp_cpu_m >= c) temp_cpu_m = c - 1;
                                SendMessage(h, CB_SETCURSEL, temp_cpu_m, 0);
                                if (c == 1) EnableWindow(h, FALSE);
                                else        EnableWindow(h, TRUE);

                                /*Rebuild CPU list*/
                                h = GetDlgItem(hdlg, IDC_COMBO3);
                                temp_cpu = SendMessage(h, CB_GETCURSEL, 0, 0);
                                SendMessage(h, CB_RESETCONTENT, 0, 0);
                                c = 0;
                                while (models[temp_model].cpu[temp_cpu_m].cpus[c].cpu_type != -1)
                                {
                                        SendMessage(h,CB_ADDSTRING,0,(LPARAM)(LPCSTR)models[temp_model].cpu[temp_cpu_m].cpus[c].name);
                                        c++;
                                }
                                if (temp_cpu >= c) temp_cpu = c - 1;
                                SendMessage(h, CB_SETCURSEL, temp_cpu, 0);
                        }
                        break;
                        case IDC_COMBOCPUM:
                        if (HIWORD(wParam) == CBN_SELCHANGE)
                        {
                                h = GetDlgItem(hdlg, IDC_COMBO1);
                                temp_model = listtomodel[SendMessage(h, CB_GETCURSEL, 0, 0)];
                                h = GetDlgItem(hdlg, IDC_COMBOCPUM);
                                temp_cpu_m = SendMessage(h, CB_GETCURSEL, 0, 0);
                                
                                /*Rebuild CPU list*/
                                h=GetDlgItem(hdlg, IDC_COMBO3);
                                temp_cpu = SendMessage(h, CB_GETCURSEL, 0, 0);
                                SendMessage(h, CB_RESETCONTENT, 0, 0);
                                c = 0;
                                while (models[temp_model].cpu[temp_cpu_m].cpus[c].cpu_type != -1)
                                {
                                        SendMessage(h,CB_ADDSTRING,0,(LPARAM)(LPCSTR)models[temp_model].cpu[temp_cpu_m].cpus[c].name);
                                        c++;
                                }
                                if (temp_cpu >= c) temp_cpu = c - 1;
                                SendMessage(h, CB_SETCURSEL, temp_cpu, 0);
                        }
                        break;
                        
                        case IDC_CONFIGUREVID:
                        h = GetDlgItem(hdlg, IDC_COMBOVID);
                        SendMessage(h, CB_GETLBTEXT, SendMessage(h, CB_GETCURSEL, 0, 0), (LPARAM)temp_str);
                        
                        deviceconfig_open(hdlg, (void *)video_card_getdevice(video_card_getid(temp_str)));
                        break;
                        
                        case IDC_COMBOVID:
                        h = GetDlgItem(hdlg, IDC_COMBOVID);
                        SendMessage(h, CB_GETLBTEXT, SendMessage(h, CB_GETCURSEL, 0, 0), (LPARAM)temp_str);
                        gfx = video_card_getid(temp_str);
                        
                        h = GetDlgItem(hdlg, IDC_CONFIGUREVID);
                        if (video_card_has_config(gfx))
                                EnableWindow(h, TRUE);
                        else
                                EnableWindow(h, FALSE);
                        break;                                

                        case IDC_CONFIGURESND:
                        h = GetDlgItem(hdlg, IDC_COMBOSND);
                        temp_sound_card_current = settings_list_to_sound[SendMessage(h, CB_GETCURSEL, 0, 0)];
                        
                        deviceconfig_open(hdlg, (void *)sound_card_getdevice(temp_sound_card_current));
                        break;
                        
                        case IDC_COMBOSND:
                        h = GetDlgItem(hdlg, IDC_COMBOSND);
                        temp_sound_card_current = settings_list_to_sound[SendMessage(h, CB_GETCURSEL, 0, 0)];
                        
                        h = GetDlgItem(hdlg, IDC_CONFIGURESND);
                        if (sound_card_has_config(temp_sound_card_current))
                                EnableWindow(h, TRUE);
                        else
                                EnableWindow(h, FALSE);
                        break;                                

                        case IDC_CONFIGUREVOODOO:
                        deviceconfig_open(hdlg, (void *)&voodoo_device);
                        break;
                }
                break;
        }
        return FALSE;
}

void config_open(HWND hwnd)
{
        DialogBox(hinstance, TEXT("ConfigureDlg"), hwnd, config_dlgproc);
}
