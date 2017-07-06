#include "ibm.h"
#include "device.h"
#include "allegro-main.h"
#include "allegro-gui.h"
#include "config.h"
#include "paths.h"
#undef printf
typedef struct config_t
{
        char name[512];
        struct config_t *next;
} config_t;

static config_t *config_head;

static char *get_config(int nr);
static int get_config_nr();
static void free_configs();
static void populate_configs();

static char *list_proc_joystick(int index, int *list_size)
{
        int c = 0;
        config_t *config = config_head;
        
        if (index < 0)
        {
                while (config)
                {
                        c++;
                        config = config->next;
                }

                *list_size = c;
                return NULL;
        }
        
        for (; c < index; c++)
        {
                if (!config)
                        return NULL;
                config = config->next;
        }
        
        if (!config)
                return NULL;
                
        return config->name;
//        return joystick_list[index].name;
}

static char config_name[256];

static DIALOG new_config_dialog[] =
{
        {d_shadow_box_proc, 0, 0, 480,80,0,0xffffff,0,0,     0,0,0,0,0}, // 0

        {d_button_proc, 240-148,  56, 144, 16, 0, 0xffffff, 0, D_EXIT, 0, 0, "OK",     0, 0}, // 1
        {d_button_proc, 240+4,    56, 144, 16, 0, 0xffffff, 0, D_EXIT, 0, 0, "Cancel", 0, 0}, // 2

        {d_edit_proc,       8,    32, 464, 16, 0, 0xffffff, 0, 0,      255, 0, config_name, 0, 0}, // 3

        {d_text_proc,   240-29*4, 16, 29*8, 16, 0, 0xffffff, 0, 0,      255, 0, "Please type new config name :", 0, 0},
        
        {0,0,0,0,0,0,0,0,0,0,0,NULL,NULL,NULL}
};

static int new_proc(int msg, DIALOG *d, int c)
{
        int ret = d_button_proc(msg, d, c);

        if (ret == D_CLOSE)
        {
                int c;
                
                config_name[0] = 0;
                new_config_dialog[3].d2 = 0;
                
                position_dialog(new_config_dialog, SCREEN_W/2 - new_config_dialog[0].w/2, SCREEN_H/2 - new_config_dialog[0].h/2);
        
                c = popup_dialog(new_config_dialog, 1);

                position_dialog(new_config_dialog, -(SCREEN_W/2 - new_config_dialog[0].w/2), -(SCREEN_H/2 - new_config_dialog[0].h/2));
                
                if (c == 1)
                {
                        sprintf(config_file_default, "%s%s.cfg", configs_path, config_name);
                        c = settings_configure();
			free_configs();
			populate_configs();
			return c;
                }
                
                return D_O_K;
        }
        
        return ret;
}

static int config_proc(int msg, DIALOG *d, int c)
{
        int ret = d_button_proc(msg, d, c);

        if (ret == D_CLOSE)
        {
                char *name = get_config(get_config_nr());

                if (name)
                { 
                        sprintf(config_file_default, "%s%s.cfg", configs_path, name);
                        loadconfig(NULL);
                        return settings_configure();
                }
                
                return D_O_K;
        }
        
        return ret;
}

static int hdd_proc(int msg, DIALOG *d, int c)
{
        int ret = d_button_proc(msg, d, c);

        if (ret == D_CLOSE)
        {
                char *name = get_config(get_config_nr());

                if (name)
                { 
                        sprintf(config_file_default, "%s%s.cfg", configs_path, name);
                        loadconfig(NULL);
                        return disc_hdconf();
                }
                
                return D_O_K;
        }
        
        return ret;
}

static DIALOG config_sel_dialog[] =
{
        {d_shadow_box_proc, 0, 0, 568,372,0,0xffffff,0,0,     0,0,0,0,0}, // 0

        {d_button_proc, 416,   8, 144, 16, 0, 0xffffff, 0, D_EXIT, 0, 0, "Load",     0, 0}, // 1
        {d_button_proc, 416, 348, 144, 16, 0, 0xffffff, 0, D_EXIT, 0, 0, "Quit", 0, 0}, // 2

        {new_proc,      416,  56, 144, 16, 0, 0xffffff, 0, D_EXIT, 0, 0, "New...",     0, 0}, // 3
        {config_proc,   416,  80, 144, 16, 0, 0xffffff, 0, D_EXIT, 0, 0, "Configure...",     0, 0}, // 4
        {hdd_proc,      416, 104, 144, 16, 0, 0xffffff, 0, D_EXIT, 0, 0, "HDD...", 0, 0}, // 5

        {d_list_proc,    8, 8,  400, 356, 0, 0xffffff, 0, 0,      0, 0, list_proc_joystick, 0, 0}, // 6

        {0,0,0,0,0,0,0,0,0,0,0,NULL,NULL,NULL}
};

static int get_config_nr()
{
        return config_sel_dialog[6].d1;
}

static void populate_configs()
{
        struct al_ffblk info;
        char cfg[512];
        config_t *config = NULL;

        strcpy(cfg, configs_path);
        strcat(cfg, "*.cfg");
		printf("cfg %s\n", cfg);
        if (al_findfirst(cfg, &info, FA_ALL))
                return;
        
        do
        {
                config_t *old_config = config;
                char *ext;
                
                config = malloc(sizeof(config_t));
                if (old_config)
                        old_config->next = config;
                else
                        config_head = config;
                config->next = 0;
                strcpy(config->name, info.name);

                ext = get_extension(config->name);
                if (ext && ext[0])
                {
                        ext--;
                        *ext = 0;
                }
                
                printf("Found %s\n", info.name);
        } while (al_findnext(&info) == 0);
        
        al_findclose(&info);
}

static char *get_config(int nr)
{
        config_t *config = config_head;
        int c;
        
        for (c = 0; c < nr; c++)
        {
                if (!config)
                        return NULL;
                config = config->next;
        }
        if (!config)
                return NULL;
                
        return config->name;
}

static void free_configs()
{
        config_t *config = config_head;
        
        while (config)
        {
                config_t *old_config = config;
                
                config = config->next;
                free(old_config);
        }
}

int config_sel()
{
        populate_configs();
        set_gfx_mode(GFX_AUTODETECT_WINDOWED, 640, 480, 0, 0);
        while (1)
        {
                int c;
                
                position_dialog(config_sel_dialog, SCREEN_W/2 - config_sel_dialog[0].w/2, SCREEN_H/2 - config_sel_dialog[0].h/2);
        
                c = popup_dialog(config_sel_dialog, 1);

                position_dialog(config_sel_dialog, -(SCREEN_W/2 - config_sel_dialog[0].w/2), -(SCREEN_H/2 - config_sel_dialog[0].h/2));
                
                if (c == 1)
                {
                        char *name = get_config(config_sel_dialog[6].d1);

                        if (name)
                        {                        
                                sprintf(config_file_default, "%s%s.cfg", configs_path, name);
                                pclog("config_file_default=%s\n", config_file_default);
                                free_configs();
                                return 0;
                        }
                }
                
                if (c == 2)
                        return 1;
        }
        
        free_configs();
        
        return 1;
}

