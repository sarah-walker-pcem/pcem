#include "ibm.h"
#include "allegro-main.h"
#include "allegro-gui.h"
#include "cpu.h"
#include "model.h"
#include "sound.h"
#include "video.h"

static int romstolist[ROM_MAX], listtomodel[ROM_MAX], romstomodel[ROM_MAX], modeltolist[ROM_MAX];
static int settings_sound_to_list[20], settings_list_to_sound[20];

#if 0
    DEFPUSHBUTTON   "OK",IDOK,64,232,50,14, WS_TABSTOP
    PUSHBUTTON      "Cancel",IDCANCEL,128,232,50,14, WS_TABSTOP
    COMBOBOX        IDC_COMBO1,62,16,157,120,CBS_DROPDOWN | WS_VSCROLL | WS_TABSTOP
    COMBOBOX        IDC_COMBOVID,62,36,157,120,CBS_DROPDOWN | WS_VSCROLL | WS_TABSTOP
    PUSHBUTTON      "Configure", IDC_CONFIGUREVID, 224, 36, 40, 14, WS_TABSTOP
    COMBOBOX        IDC_COMBOCPUM,62,56,157,120,CBS_DROPDOWN | WS_VSCROLL | WS_TABSTOP
    COMBOBOX        IDC_COMBO3,62,76,157,120,CBS_DROPDOWN | WS_VSCROLL | WS_TABSTOP
    COMBOBOX        IDC_COMBOCHC,62,96,157,120,CBS_DROPDOWN | WS_VSCROLL | WS_TABSTOP
    COMBOBOX        IDC_COMBOSPD,62,116,157,120,CBS_DROPDOWN | WS_VSCROLL | WS_TABSTOP
    COMBOBOX        IDC_COMBOSND,62,136,157,120,CBS_DROPDOWN | WS_VSCROLL | WS_TABSTOP
    PUSHBUTTON      "Configure", IDC_CONFIGURESND, 224, 136, 40, 14, WS_TABSTOP
    EDITTEXT        IDC_MEMTEXT, 62, 152, 36, 14, ES_AUTOHSCROLL | ES_NUMBER
    CONTROL         "", IDC_MEMSPIN, UPDOWN_CLASS, UDS_ALIGNRIGHT | UDS_ARROWKEYS | UDS_NOTHOUSANDS | UDS_SETBUDDYINT, 98, 152, 12, 14
    LTEXT           "MB", IDC_STATIC, 98, 152, 40, 10
    CONTROL         "CMS / Game Blaster",IDC_CHECK3,"Button",BS_AUTOCHECKBOX | WS_TABSTOP,14,172,118,10
    CONTROL         "Gravis Ultrasound",IDC_CHECKGUS,"Button",BS_AUTOCHECKBOX | WS_TABSTOP,14,188,118,10    
    CONTROL         "Innovation SSI-2001",IDC_CHECKSSI,"Button",BS_AUTOCHECKBOX | WS_TABSTOP,14,204,118,10
    CONTROL         "Composite CGA",IDC_CHECK4,"Button",BS_AUTOCHECKBOX | WS_TABSTOP,14,220,118,10
    LTEXT           "Machine :",IDC_STATIC,15,16,40,10
    LTEXT           "Video :",IDC_STATIC,15,36,34,10
    LTEXT           "CPU type :",IDC_STATIC,15,56,34,10
    LTEXT           "CPU :",IDC_STATIC,15,76,34,10
    LTEXT           "Cache :",IDC_STATIC,15,96,40,10
    LTEXT           "Video speed :",IDC_STATIC,15,116,40,10    
    LTEXT           "Soundcard :",IDC_STATIC,15,136,40,10
    LTEXT           "Memory :",IDC_STATIC,15,156,40,10
#endif

typedef struct allegro_list_t
{
        char name[256];
        int num;
} allegro_list_t;

static allegro_list_t model_list[ROM_MAX+1];
static allegro_list_t video_list[GFX_MAX+1];
static allegro_list_t sound_list[GFX_MAX+1];
static allegro_list_t cpumanu_list[4];
static allegro_list_t cpu_list[32];

static char mem_size_str[10];

static allegro_list_t cache_list[] =
{
        {"A little", 0},
        {"A bit", 1},
        {"Some", 2},
        {"A lot", 3},
        {"Infinite", 4},
        {"", -1}
};

static allegro_list_t vidspeed_list[] =
{
        {"8-bit", 0},
        {"Slow 16-bit", 1},
        {"Fast 16-bit", 2},
        {"Slow VLB/PCI", 3},
        {"Mid  VLB/PCI", 4},
        {"Fast VLB/PCI", 5},
        {"", -1}
};                

static void reset_list();

static char *list_proc_model(int index, int *list_size)
{
        if (index < 0)
        {
                int c = 0;
                
                while (model_list[c].name[0])
                        c++;

                *list_size = c;
                return NULL;
        }
        
        return model_list[index].name;
}

static char *list_proc_video(int index, int *list_size)
{
        if (index < 0)
        {
                int c = 0;
                
                while (video_list[c].name[0])
                        c++;

                *list_size = c;
                return NULL;
        }
        
        return video_list[index].name;
}

static char *list_proc_cache(int index, int *list_size)
{
        if (index < 0)
        {
                int c = 0;
                
                while (cache_list[c].name[0])
                        c++;

                *list_size = c;
                return NULL;
        }
        
        return cache_list[index].name;
}

static char *list_proc_vidspeed(int index, int *list_size)
{
        if (index < 0)
        {
                int c = 0;
                
                while (vidspeed_list[c].name[0])
                        c++;

                *list_size = c;
                return NULL;
        }
        
        return vidspeed_list[index].name;
}

static char *list_proc_sound(int index, int *list_size)
{
        if (index < 0)
        {
                int c = 0;
                
                while (sound_list[c].name[0])
                        c++;

                *list_size = c;
                return NULL;
        }
        
        return sound_list[index].name;
}

static char *list_proc_cpumanu(int index, int *list_size)
{
        if (index < 0)
        {
                int c = 0;
                
                while (cpumanu_list[c].name[0])
                        c++;

                *list_size = c;
                return NULL;
        }
        
        return cpumanu_list[index].name;
}

static char *list_proc_cpu(int index, int *list_size)
{
        if (index < 0)
        {
                int c = 0;
                
                while (cpu_list[c].name[0])
                        c++;

                *list_size = c;
                return NULL;
        }
        
        return cpu_list[index].name;
}

static int list_proc(int msg, DIALOG *d, int c);

static DIALOG configure_dialog[] =
{
        {d_shadow_box_proc, 0, 0, 236*2,256,0,0xffffff,0,0,     0,0,0,0,0}, // 0

        {d_button_proc, 176,  232, 50, 14, 0, 0xffffff, 0, D_EXIT, 0, 0, "OK",     0, 0}, // 1
        {d_button_proc, 246,  232, 50, 16, 0, 0xffffff, 0, D_EXIT, 0, 0, "Cancel", 0, 0}, // 2

        {list_proc,      70*2, 12,  152*2, 20, 0, 0xffffff, 0, 0,      0, 0, list_proc_model, 0, 0},

        {list_proc,      70*2, 32,  152*2, 20, 0, 0xffffff, 0, 0, 0, 0, list_proc_video, 0, 0},

        {list_proc,      70*2,  52, 152*2, 20, 0, 0xffffff, 0, 0, 0, 0, list_proc_cpumanu, 0, 0}, //5
        {d_list_proc,    70*2,  72, 152*2, 20, 0, 0xffffff, 0, 0, 0, 0, list_proc_cpu, 0, 0},
        {d_list_proc,    70*2,  92, 152*2, 20, 0, 0xffffff, 0, 0, 0, 0, list_proc_cache, 0, 0},
        {d_list_proc,    70*2, 112, 152*2, 20, 0, 0xffffff, 0, 0, 0, 0, list_proc_vidspeed, 0, 0},
        {list_proc,      70*2, 132, 152*2, 20, 0, 0xffffff, 0, 0, 0, 0, list_proc_sound, 0, 0}, //9
        
        {d_edit_proc,    70*2, 156,    32, 14, 0, 0xffffff, 0, 0, 3, 0, mem_size_str, 0, 0},
                        
        {d_text_proc,    98*2, 156,  40, 10, 0, 0xffffff, 0, 0, 0, 0, "MB", 0, 0},
        
        {d_check_proc,   14*2, 172, 118*2, 10, 0, 0xffffff, 0, 0, 0, 0, "CMS / Game Blaster", 0, 0},
        {d_check_proc,   14*2, 188, 118*2, 10, 0, 0xffffff, 0, 0, 0, 0, "Gravis Ultrasound", 0, 0},
        {d_check_proc,   14*2, 204, 118*2, 10, 0, 0xffffff, 0, 0, 0, 0, "Innovation SSI-2001", 0, 0},
        {d_check_proc,   14*2, 220, 118*2, 10, 0, 0xffffff, 0, 0, 0, 0, "Composite CGA", 0, 0},

        {d_text_proc,    16*2,  16,  40, 10, 0, 0xffffff, 0, 0, 0, 0, "Machine :", 0, 0},
        {d_text_proc,    16*2,  36,  40, 10, 0, 0xffffff, 0, 0, 0, 0, "Video :", 0, 0},
        {d_text_proc,    16*2,  56,  40, 10, 0, 0xffffff, 0, 0, 0, 0, "CPU type :", 0, 0},
        {d_text_proc,    16*2,  76,  40, 10, 0, 0xffffff, 0, 0, 0, 0, "CPU :", 0, 0},
        {d_text_proc,    16*2,  96,  40, 10, 0, 0xffffff, 0, 0, 0, 0, "Cache :", 0, 0},
        {d_text_proc,    16*2, 116,  40, 10, 0, 0xffffff, 0, 0, 0, 0, "Video speed :", 0, 0},
        {d_text_proc,    16*2, 136,  40, 10, 0, 0xffffff, 0, 0, 0, 0, "Soundcard :", 0, 0},
        {d_text_proc,    16*2, 156,  40, 10, 0, 0xffffff, 0, 0, 0, 0, "Memory :", 0, 0},
        
        {0,0,0,0,0,0,0,0,0,0,0,NULL,NULL,NULL}
};

static int list_proc(int msg, DIALOG *d, int c)
{
        int old = d->d1;
        int ret = d_list_proc(msg, d, c);
        
        if (d->d1 != old)
        {
                int new_model = model_list[configure_dialog[3].d1].num;

                reset_list();

                if (models[new_model].fixed_gfxcard)
                        configure_dialog[4].flags |= D_DISABLED;
                else
                        configure_dialog[4].flags &= ~D_DISABLED;

                return D_REDRAW;
        }

        return ret;
}

static void reset_list()
{
        int model = model_list[configure_dialog[3].d1].num;
        int cpumanu = configure_dialog[5].d1;
        int cpu = configure_dialog[6].d1;
        int c;
        
        memset(cpumanu_list, 0, sizeof(cpumanu_list));
        memset(cpu_list, 0, sizeof(cpu_list));

        c = 0;
        while (models[model].cpu[c].cpus != NULL && c < 3)
        {
                strcpy(cpumanu_list[c].name, models[model].cpu[c].name);
                cpumanu_list[c].num = c;
                c++;
        }
        
        if (cpumanu >= c)
                cpumanu = configure_dialog[6].d1 = c-1;

        c = 0;
        while (models[model].cpu[cpumanu].cpus[c].cpu_type != -1)
        {
                strcpy(cpu_list[c].name, models[model].cpu[cpumanu].cpus[c].name);
                cpu_list[c].num = c;
                c++;
        }

        if (cpu >= c)
                cpu = configure_dialog[7].d1 = c-1;
}

int settings_configure()
{
        int c, d;
        
        memset(model_list, 0, sizeof(model_list));
        memset(video_list, 0, sizeof(video_list));
        memset(sound_list, 0, sizeof(sound_list));

        for (c = 0; c < ROM_MAX; c++)
                romstolist[c] = 0;
        c = d = 0;
        while (models[c].id != -1)
        {
                pclog("INITDIALOG : %i %i %i\n",c,models[c].id,romspresent[models[c].id]);
                if (romspresent[models[c].id])
                {
                        strcpy(model_list[d].name, models[c].name);
                        model_list[d].num = c;
                        if (c == model)
                                configure_dialog[3].d1 = d;
                        d++;
                }
                c++;
        }

        if (models[model].fixed_gfxcard)
                configure_dialog[4].flags |= D_DISABLED;
        else
                configure_dialog[4].flags &= ~D_DISABLED;

        c = d = 0;
        while (1)
        {
                char *s = video_card_getname(c);

                if (!s[0])
                        break;
pclog("video_card_available : %i\n", c);
                if (video_card_available(c))
                {
                        strcpy(video_list[d].name, video_card_getname(c));
                        video_list[d].num = video_new_to_old(c);
                        if (video_new_to_old(c) == gfxcard)
                                configure_dialog[4].d1 = d;
                        d++;
                }

                c++;
        }

        c = d = 0;
        while (1)
        {
                char *s = sound_card_getname(c);

                if (!s[0])
                        break;

                if (sound_card_available(c))
                {
                        strcpy(sound_list[d].name, sound_card_getname(c));
                        sound_list[d].num = c;
                        if (c == sound_card_current)
                                configure_dialog[9].d1 = d;
                        d++;
                }

                c++;
        }

        configure_dialog[5].d1 = cpu_manufacturer;
        configure_dialog[6].d1 = cpu;
        configure_dialog[7].d1 = cache;
        configure_dialog[8].d1 = video_speed;
        reset_list();
//        strcpy(cpumanu_str, models[romstomodel[romset]].cpu[cpu_manufacturer].name);
//        strcpy(cpu_str, models[romstomodel[romset]].cpu[cpu_manufacturer].cpus[cpu].name);
//        strcpy(cache_str, cache_str_list[cache]);
//        strcpy(vidspeed_str, vidspeed_str_list[video_speed]);

//        strcpy(soundcard_str, sound_card_getname(sound_card_current));

        if (GAMEBLASTER)
                configure_dialog[12].flags |= D_SELECTED;
        else
                configure_dialog[12].flags &= ~D_SELECTED;

        if (GUS)
                configure_dialog[13].flags |= D_SELECTED;
        else
                configure_dialog[13].flags &= ~D_SELECTED;

        if (SSI2001)
                configure_dialog[14].flags |= D_SELECTED;
        else
                configure_dialog[14].flags &= ~D_SELECTED;

        if (cga_comp)
                configure_dialog[15].flags |= D_SELECTED;
        else
                configure_dialog[15].flags &= ~D_SELECTED;

        sprintf(mem_size_str, "%i", mem_size);
        
        while (1)
        {
                position_dialog(configure_dialog, SCREEN_W/2 - 272, SCREEN_H/2 - 256/2);
        
                c = popup_dialog(configure_dialog, 1);

                position_dialog(configure_dialog, -(SCREEN_W/2 - 272), -(SCREEN_H/2 - 256/2));
                
                if (c == 1)
                {
                        int new_model = model_list[configure_dialog[3].d1].num;
                        int new_gfxcard = video_list[configure_dialog[4].d1].num;
                        int new_sndcard = sound_list[configure_dialog[9].d1].num;
                        int new_cpu_m = configure_dialog[5].d1;
                        int new_cpu = configure_dialog[6].d1;
                        int new_mem_size;
                        int new_has_fpu = (models[new_model].cpu[new_cpu_m].cpus[new_cpu].cpu_type >= CPU_i486DX) ? 1 : 0;
                        int new_GAMEBLASTER = (configure_dialog[12].flags & D_SELECTED) ? 1 : 0;
                        int new_GUS = (configure_dialog[13].flags & D_SELECTED) ? 1 : 0;
                        int new_SSI2001 = (configure_dialog[14].flags & D_SELECTED) ? 1 : 0;
                        
                        sscanf(mem_size_str, "%i", &new_mem_size);
                        if (new_mem_size < 1 || new_mem_size > 256)
                        {
                                alert("Invalid memory size", "Memory must be between 1 and 256 MB", NULL, "OK", NULL, 0, 0);
                                continue;
                        }
                        
                        if (new_model != model || new_gfxcard != gfxcard || new_mem_size != mem_size || 
                            new_has_fpu != hasfpu || new_GAMEBLASTER != GAMEBLASTER || new_GUS != GUS ||
                            new_SSI2001 != SSI2001 || new_sndcard != sound_card_current)
                        {
                                if (alert("This will reset PCem!", "Okay to continue?", NULL, "OK", "Cancel", 0, 0) != 1)
                                        continue;

                                model = new_model;
                                romset = model_getromset();
                                gfxcard = new_gfxcard;
                                mem_size = new_mem_size;
                                cpu_manufacturer = new_cpu_m;
                                cpu = new_cpu;
                                GAMEBLASTER = new_GAMEBLASTER;
                                GUS = new_GUS;
                                SSI2001 = new_SSI2001;
                                sound_card_current = new_sndcard;
                                        
                                mem_resize();
                                loadbios();
                                resetpchard();
                        }

                        video_speed = configure_dialog[8].d1;

                        cga_comp = (configure_dialog[15].flags & D_SELECTED) ? 1 : 0;

                        cpu_manufacturer = new_cpu_m;
                        cpu = new_cpu;
                        cpu_set();
                        
                        cache = configure_dialog[9].d1;
                        mem_updatecache();
                        
                        saveconfig();

                        speedchanged();

                        return D_O_K;
                }
                
                if (c == 2)
                        return D_O_K;
        }
        
        return D_O_K;
}

