#include "ibm.h"
#include "device.h"
#include "allegro-gui.h"
#include "allegro-main.h"
#include "allegro-video.h"
#include "config.h"
#include "cpu.h"
#include "mem.h"
#include "model.h"
#include "nvr.h"
#include "plat-joystick.h"
#include "plat-keyboard.h"
#include "plat-midi.h"
#include "plat-mouse.h"
#include "sound.h"
#include "video.h"
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>
#include "paths.h"

 #undef printf
 
int has_been_inited = 0;

int mousecapture = 0;
int quited = 0;
int winsizex = -1, winsizey = -1;

int romspresent[ROM_MAX];
int gfx_present[GFX_MAX];

void updatewindowsize(int x, int y)
{
        if (x < 128)
                x = 128;
        if (y < 128)
                y = 128;
        if (winsizex != x || winsizey != y)
        {
                winsizex = x;
                winsizey = y;
                allegro_video_update_size(x, y);
        }
}

void startblit()
{
}

void endblit()
{
}

static int ticks = 0;
static void timer_rout()
{
        ticks++;
}

uint64_t timer_freq;
uint64_t timer_read()
{
        return 0;
}

int dir_exists(char *path)
{
        DIR *dir = opendir(path);
        if (dir)
        {
                closedir(dir);
                return 1;
        }
        return 0;
}

int create_dir(char *path)
{
        if (!mkdir(path, 0755))
                return 1;
        return 0;
}

void get_pcem_path(char *s, int size)
{
        struct passwd *pw = getpwuid(getuid());

        strncpy(s, pw->pw_dir, size-10);
        append_slash(s, size);
        strcat(s, ".pcem/");
        //get_executable_name(s, size);
}

int main(int argc, char *argv[])
{
        char s[512];
        int frames = 0;
        int c;
        FILE *f;
        
        allegro_init();
        allegro_video_init();
        install_timer();
        install_int_ex(timer_rout, BPS_TO_TIMER(100));
	install_int_ex(onesec, BPS_TO_TIMER(1));
	midi_init();
        
        paths_init();
        
        /* create directories if they don't exist */
        if (!dir_exists(pcem_path)) {
                if (!create_dir(pcem_path))
                        return 0;
                append_filename(s, pcem_path, "configs", 511);
                if (!dir_exists(s) && !create_dir(s))
                        return 0;

                append_filename(s, pcem_path, "roms", 511);
                if (!dir_exists(s) && !create_dir(s))
                        return 0;

                append_filename(s, pcem_path, "nvr", 511);
                if (!dir_exists(s) && !create_dir(s))
                        return 0;

                append_filename(s, pcem_path, "logs", 511);
                if (!dir_exists(s) && !create_dir(s))
                        return 0;
        }

        /* set up default paths */
        sprintf(s, "%s%s%c%s", pcem_path, "roms/", get_path_separator(), "/usr/share/pcem/roms/");
        set_default_roms_paths(s);
        append_filename(s, pcem_path, "nvr/", 511);
        set_default_nvr_path(s);
        append_filename(s, pcem_path, "configs/", 511);
        set_default_configs_path(s);
        append_filename(s, pcem_path, "logs/", 511);
        set_default_logs_path(s);

        sound_init();

        initpc(argc, argv);

        /* check if cfg exists, and if not create it */
        append_filename(s, pcem_path, "pcem.cfg", 511);
        f = fopen(s, "r");
        if (f)
                fclose(f);
        else
                saveconfig(NULL);

        keyboard_init();
        mouse_init();
        joystick_init();

        for (c = 0; c < ROM_MAX; c++)
        {
                romset = c;
                romspresent[c] = loadbios();
                pclog("romset %i - %i\n", c, romspresent[c]);
        }
        
        for (c = 0; c < ROM_MAX; c++)
        {
                if (romspresent[c])
                   break;
        }
        if (c == ROM_MAX)
        {
                printf("No ROMs present!\nYou must have at least one romset to use PCem.");
                return 0;
        }

        for (c = 0; c < GFX_MAX; c++)
                gfx_present[c] = video_card_available(video_old_to_new(c));

        while (1)
        {
                has_been_inited = 0;
                if (config_sel())
                        return 0;        
                has_been_inited = 1;
                
                quited = 0;
        
                loadconfig(NULL);

                c=loadbios();

                if (!c)
                 {
                        if (romset != -1)
        			printf("Configured romset not available.\nDefaulting to available romset.");
                        for (c = 0; c < ROM_MAX; c++)
                         {
                                if (romspresent[c])
                                {
                                        romset = c;
                                        model = model_getmodel(romset);
                                        saveconfig(NULL);
                                        resetpchard();
                                        break;
                                }
                         }
                 }
 
                if (!video_card_available(video_old_to_new(gfxcard)))
                 {
                        if (gfxcard) printf("Configured video BIOS not available.\nDefaulting to available romset.");
                        for (c = GFX_MAX-1; c >= 0; c--)
                         {
                                if (gfx_present[c])
                                {
                                        gfxcard = c;
                                        saveconfig(NULL);
                                        resetpchard();
                                        break;
                                }
                         }
                 }
 
        	resetpchard();

                ticks = 0;
                while (!quited)
                {
                        if (ticks)
                        {
                                ticks--;
                                runpc();
                                frames++;
                                if (frames >= 200 && nvr_dosave)
                                {
                                        frames = 0;
                                        nvr_dosave = 0;
                                        savenvr();
                                }
                        }
                        else
                                rest(1);

        		if (ticks > 10)
        			ticks = 0;
                 
        		if ((mouse_b & 1) && !mousecapture)
        			mousecapture = 1;
 
        		if (((key[KEY_LCONTROL] || key[KEY_RCONTROL]) && key[KEY_END]) || (mouse_b & 4))
        			mousecapture = 0;
 
                        if ((key[KEY_LCONTROL] || key[KEY_RCONTROL]) && key[KEY_ALT] && key[KEY_PGDN])
                        {
        			int old_winsizex = winsizex, old_winsizey = winsizey;
        			if (winsizex < 512 || winsizey < 350)
        				updatewindowsize(512, 350);
                                gui_enter();
        			if (old_winsizex < 512 || old_winsizey < 350)
        				updatewindowsize(old_winsizex, old_winsizey);
                                ticks = 0;
                        }
                 }
                
                savenvr();
         }
         
         closepc();
 
        joystick_close();
        mouse_close();
        keyboard_close();
 	midi_close();
 
         return 0;
}

END_OF_MAIN();
