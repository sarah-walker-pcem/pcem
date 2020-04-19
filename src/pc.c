#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "ibm.h"
#include "device.h"

#include "ali1429.h"
#include "cdrom-ioctl.h"
#include "cdrom-image.h"
#include "cpu.h"
#include "disc.h"
#include "disc_img.h"
#include "mem.h"
#include "x86_ops.h"
#include "codegen.h"
#include "cdrom-null.h"
#include "config.h"
#include "cpu.h"
#include "disc_fdi.h"
#include "dma.h"
#include "fdc.h"
#include "fdd.h"
#include "gameport.h"
#include "sound_gus.h"
#include "ide.h"
#include "io.h"
#include "keyboard.h"
#include "keyboard_at.h"
#include "lpt.h"
#include "model.h"
#include "mouse.h"
#include "nvr.h"
#include "pic.h"
#include "pit.h"
#include "plat-joystick.h"
#include "plat-keyboard.h"
#include "plat-midi.h"
#include "plat-mouse.h"
#include "scsi_cd.h"
#include "scsi_zip.h"
#include "serial.h"
#include "sound.h"
#include "sound_cms.h"
#include "sound_dbopl.h"
#include "sound_opl.h"
#include "sound_sb.h"
#include "sound_speaker.h"
#include "sound_ssi2001.h"
#include "timer.h"
#include "vid_voodoo.h"
#include "video.h"
#include "amstrad.h"
#include "hdd.h"
#include "x86.h"
#include "paths.h"

#ifdef USE_NETWORKING
#include "nethandler.h"
#define NE2000      1
uint8_t ethif;
int inum;
#endif

int window_w, window_h, window_x, window_y, window_remember;

int start_in_fullscreen = 0;

static int override_drive_a = 0, override_drive_b = 0;

int vid_resize, vid_api;

int cycles_lost = 0;

int config_override = 0;

int insc=0;
float mips,flops;
extern int mmuflush;
extern int readlnum,writelnum;
void fullspeed();

int framecount,fps;

int atfullspeed;

void saveconfig(char *fn);
int infocus;
int mousecapture;
FILE *pclogf;
void pclog(const char *format, ...)
{
#ifndef RELEASE_BUILD
        char buf[1024];
        //return;
        if (!pclogf)
        {
                strcpy(buf, logs_path);
                put_backslash(buf);
                strcat(buf, "pcem.log");
                pclogf=fopen(buf, "wt");
        }
        //return;
        va_list ap;
        va_start(ap, format);
        vsprintf(buf, format, ap);
        va_end(ap);
        fputs(buf,pclogf);
//        fflush(pclogf);
#endif
}

void fatal(const char *format, ...)
{
        char buf[256];
        //   return;
        if (!pclogf)
        {
                strcpy(buf, logs_path);
                put_backslash(buf);
                strcat(buf, "pcem.log");
                pclogf=fopen(buf, "wt");
        }
        //return;
        va_list ap;
        va_start(ap, format);
        vsprintf(buf, format, ap);
        va_end(ap);
        fputs(buf,pclogf);
        fflush(pclogf);
        savenvr();
        dumppic();
        dumpregs();
        exit(-1);
}

uint8_t cgastat;

int pollmouse_delay = 2;
void pollmouse()
{
        int x, y, z;
//        return;
        pollmouse_delay--;
        if (pollmouse_delay) return;
        pollmouse_delay = 2;
        mouse_poll_host();
        mouse_get_mickeys(&x, &y, &z);
        mouse_poll(x, y, z, mouse_buttons);
//        if (mousecapture) position_mouse(64,64);
}

/*PC1512 languages -
  7=English
  6=German
  5=French
  4=Spanish
  3=Danish
  2=Swedish
  1=Italian
        3,2,1 all cause the self test to fail for some reason
  */

int cpuspeed2;

int clocks[3][12][4]=
{
        {
                {4772728,13920,59660,5965},  /*4.77MHz*/
                {8000000,23333,110000,0}, /*8MHz*/
                {10000000,29166,137500,0}, /*10MHz*/
                {12000000,35000,165000,0}, /*12MHz*/
                {16000000,46666,220000,0}, /*16MHz*/
        },
        {
                {8000000,23333,110000,0}, /*8MHz*/
                {12000000,35000,165000,0}, /*12MHz*/
                {16000000,46666,220000,0}, /*16MHz*/
                {20000000,58333,275000,0}, /*20MHz*/
                {25000000,72916,343751,0}, /*25MHz*/
        },
        {
                {16000000, 46666,220000,0}, /*16MHz*/
                {20000000, 58333,275000,0}, /*20MHz*/
                {25000000, 72916,343751,0}, /*25MHz*/
                {33000000, 96000,454000,0}, /*33MHz*/
                {40000000,116666,550000,0}, /*40MHz*/
                {50000000, 72916*2,343751*2,0}, /*50MHz*/
                {33000000*2, 96000*2,454000*2,0}, /*66MHz*/
                {75000000, 72916*3,343751*3,0}, /*75MHz*/
                {80000000,116666*2,550000*2,0}, /*80MHz*/
                {100000000, 72916*4,343751*4,0}, /*100MHz*/
                {120000000,116666*3,550000*3,0}, /*120MHz*/
                {133000000, 96000*4,454000*4,0}, /*133MHz*/
        }
};

int updatestatus;
int win_title_update=0;


void onesec()
{
        fps=framecount;
        framecount=0;
        video_refresh_rate = video_frames;
        video_frames = 0;
        win_title_update=1;
}

void pc_reset()
{
        resetx86();
        //timer_reset();
        dma_reset();
        fdc_reset();
        pic_reset();
        serial_reset();

        if (AT)
                setpitclock(models[model].cpu[cpu_manufacturer].cpus[cpu].rspeed);
        else
                setpitclock(14318184.0);
        
//        sb_reset();

        ali1429_reset();
//        video_init();
}
#undef printf


void initpc(int argc, char *argv[])
{
        //char *p;
//        char *config_file = NULL;
        int c;

        for (c = 1; c < argc; c++)
        {
                if (!strcasecmp(argv[c], "--help"))
                {
                        printf("PCem command line options :\n\n");
                        printf("--config file.cfg - use given config file as initial configuration\n");
                        printf("--fullscreen      - start in fullscreen mode\n");
                        printf("--load_drive_a file.img - load drive A: with the given disc image\n");
                        printf("--load_drive_b file.img - load drive B: with the given disc image\n");
                        exit(-1);
                }
                else if (!strcasecmp(argv[c], "--fullscreen"))
                {
                        start_in_fullscreen = 1;
                }
                else if (!strcasecmp(argv[c], "--config"))
                {
                        char *ext;
                        
                        if ((c+1) == argc)
                                break;
                        strncpy(config_file_default, argv[c+1], 256);
                        strcpy(config_name, get_filename(config_file_default));
                        
                        ext = get_extension(config_name);
                        if (ext && ext[0])
                        {
                                ext--;
                                *ext = 0;
                        }
                        
                        config_override = 1;
                        c++;
                }
                else if (!strcasecmp(argv[c], "--load_drive_a"))
                {
                        if ((c+1) == argc)
                                break;

                        strncpy(discfns[0], argv[c+1], 256);
                        c++;
                        override_drive_a = 1;
                }
                else if (!strcasecmp(argv[c], "--load_drive_b"))
                {
                        if ((c+1) == argc)
                                break;

                        strncpy(discfns[1], argv[c+1], 256);
                        c++;
                        override_drive_b = 1;
                }
        }

//        append_filename(config_file_default, pcempath, "pcem.cfg", 511);
        
        loadconfig(NULL);
        pclog("Config loaded\n");
//        if (config_file)
//                saveconfig();

        cpuspeed2=(AT)?2:1;
//        cpuspeed2=cpuspeed;
        atfullspeed=0;

        device_init();        
        
        initvideo();
        mem_init();
        loadbios();
        mem_add_bios();
                        
        codegen_init();
        
        timer_reset();
        sound_reset();
        io_init();
	fdc_init();
	disc_init();
        fdi_init();
        img_init();
#ifdef USE_NETWORKING
	vlan_reset();	//NETWORK
	network_card_init(network_card_current);
#endif

        //loadfont();
        loadnvr();
        resetide();
#if __unix
	if (cdrom_drive == -1)
	        cdrom_null_open(cdrom_drive);	
	else
#endif
	{
		if (cdrom_drive == CDROM_IMAGE)
		{
			FILE *ff = fopen(image_path, "rb");
			if (ff)
			{
				fclose(ff);
				image_open(image_path);
			}
			else
			{
#if __unix
				cdrom_drive = -1;
				cdrom_null_open(cdrom_drive);
#else
				cdrom_drive = 0;
				ioctl_set_drive(cdrom_drive);
#endif
			}
		}
		else
		{
			ioctl_set_drive(cdrom_drive);
		}
	}
        
/*        if (romset==ROM_AMI386 || romset==ROM_AMI486) */fullspeed();
        ali1429_reset();
//        CPUID=(is486 && (cpuspeed==7 || cpuspeed>=9));
//        pclog("Init - CPUID %i %i\n",CPUID,cpuspeed);

#if __unix
	if (cdrom_drive == -1)
	        cdrom_null_reset();	
	else
#endif
	{
		if (cdrom_drive == CDROM_IMAGE)
		{
			image_reset();
		}
		else
		{
			ioctl_reset();
		}
	}
}

void resetpc()
{
        cpu_set();
        pc_reset();
        cpu_set_turbo(1);
//        cpuspeed2=(AT)?2:1;
//        atfullspeed=0;
///*        if (romset==ROM_AMI386 || romset==ROM_AMI486) */fullspeed();
}

void resetpc_cad()
{
	keyboard_send_scancode(29, 0);	/* Ctrl key pressed */
	keyboard_send_scancode(56, 0);	/* Alt key pressed */
	keyboard_send_scancode(83, 0);	/* Delete key pressed */
	keyboard_send_scancode(29, 1);	/* Ctrl key released */
	keyboard_send_scancode(56, 1);	/* Alt key released */
	keyboard_send_scancode(83, 1);	/* Delete key released */
}

void resetpchard()
{
        device_close_all();
        mouse_emu_close();
        device_init();
        
        timer_reset();
        sound_reset();
        io_init();
        cpu_set();
        mem_alloc();
        fdc_init();
	disc_reset();
        disc_load(0, discfns[0]);
        disc_load(1, discfns[1]);

        if (!AT && models[model].max_ram > 640 && models[model].max_ram <= 768 && !video_is_ega_vga())
                mem_set_704kb();
        model_init();
        mouse_emu_init();
        video_init();
        speaker_init();
        lpt1_device_init();

#ifdef USE_NETWORKING
	vlan_reset();	//NETWORK
	network_card_init(network_card_current);      
#endif

        sound_card_init(sound_card_current);
        if (GUS)
                device_add(&gus_device);
        if (GAMEBLASTER)
                device_add(&cms_device);
        if (SSI2001)
                device_add(&ssi2001_device);
        if (voodoo_enabled)
                device_add(&voodoo_device);
        hdd_controller_init(hdd_controller_name);
        pc_reset();
        
        resetide();
        
        loadnvr();

//        cpuspeed2 = (AT)?2:1;
//        atfullspeed = 0;
//        setpitclock(models[model].cpu[cpu_manufacturer].cpus[cpu].rspeed);

        ali1429_reset();
        
        keyboard_at_reset();
        
        cpu_cache_int_enabled = cpu_cache_ext_enabled = 0;
        
//        output=3;

        image_close();
#if __unix
	if (cdrom_drive == -1)
	        cdrom_null_reset();	
	else
#endif
	{
		if (cdrom_drive == CDROM_IMAGE)
		{
			FILE *ff = fopen(image_path, "rb");
			if (ff)
			{
				fclose(ff);
				image_open(image_path);
			}
			else
			{
#if __unix
				cdrom_drive = -1;
				cdrom_null_open(cdrom_drive);
#else
				cdrom_drive = 0;
				ioctl_set_drive(cdrom_drive);
#endif
			}
		}
		else
		{
                        ioctl_set_drive(cdrom_drive);
			ioctl_reset();
		}
	}

        sound_update_buf_length();
        cpu_set_turbo(1);
}

char romsets[17][40]={"IBM PC","IBM XT","Generic Turbo XT","Euro PC","Tandy 1000","Amstrad PC1512","Sinclair PC200","Amstrad PC1640","IBM AT","AMI 286 clone","Dell System 200","Misc 286","IBM AT 386","Misc 386","386 clone","486 clone","486 clone 2"};
char clockspeeds[3][12][16]=
{
        {"4.77MHz","8MHz","10MHz","12MHz","16MHz"},
        {"8MHz","12MHz","16MHz","20MHz","25MHz"},
        {"16MHz","20MHz","25MHz","33MHz","40MHz","50MHz","66MHz","75MHz","80MHz","100MHz","120MHz","133MHz"},
};
int framecountx=0;
int sndcount=0;
int oldat70hz;

int sreadlnum,swritelnum,segareads,segawrites, scycles_lost;

int serial_fifo_read, serial_fifo_write;

int emu_fps = 0;

void runpc()
{
        char s[200];
        int done=0;
        int cycles_to_run = cpu_get_speed() / 100;
        
        override_drive_a = override_drive_b = 0;

        startblit();
        
        if (is386)   
        {
                if (cpu_use_dynarec)
                        exec386_dynarec(cycles_to_run);
                else
                        exec386(cycles_to_run);
        }
        else if (AT)
                exec386(cycles_to_run);
        else
                execx86(cycles_to_run);
        
        keyboard_poll_host();
        keyboard_process();
//        checkkeys();
        pollmouse();
        joystick_poll();
        endblit();

        framecountx++;
        framecount++;
        if (framecountx>=100)
        {
                pclog("onesec\n");
                framecountx=0;
                mips=(float)insc/1000000.0f;
                insc=0;
                flops=(float)fpucount/1000000.0f;
                fpucount=0;
                sreadlnum=readlnum;
                swritelnum=writelnum;
                segareads=egareads;
                segawrites=egawrites;
                scycles_lost = cycles_lost;

                cpu_recomp_blocks_latched = cpu_recomp_blocks;
                cpu_recomp_ins_latched = cpu_state.cpu_recomp_ins;
                cpu_recomp_full_ins_latched = cpu_recomp_full_ins;
                cpu_new_blocks_latched = cpu_new_blocks;
                cpu_recomp_flushes_latched = cpu_recomp_flushes;
                cpu_recomp_evicted_latched = cpu_recomp_evicted;
                cpu_recomp_reuse_latched = cpu_recomp_reuse;
                cpu_recomp_removed_latched = cpu_recomp_removed;

                cpu_recomp_blocks = 0;
                cpu_state.cpu_recomp_ins = 0;
                cpu_recomp_full_ins = 0;
                cpu_new_blocks = 0;
                cpu_recomp_flushes = 0;
                cpu_recomp_evicted = 0;
                cpu_recomp_reuse = 0;
                cpu_recomp_removed = 0;

                updatestatus=1;
                readlnum=writelnum=0;
                egareads=egawrites=0;
                cycles_lost = 0;
                mmuflush=0;
                emu_fps = frames;
                frames = 0;
        }
        if (win_title_update)
        {
                win_title_update=0;
                sprintf(s, "PCem v16 - %i%% - %s - %s - %s", fps, model_getname(), models[model].cpu[cpu_manufacturer].cpus[cpu].name, (!mousecapture) ? "Click to capture mouse" : ((mouse_get_type(mouse_type) & MOUSE_TYPE_3BUTTON) ? "Press CTRL-END to release mouse" : "Press CTRL-END or middle button to release mouse"));
                set_window_title(s);
        }
        done++;
}

void fullspeed()
{
        cpuspeed2=cpuspeed;
        if (!atfullspeed)
        {
                printf("Set fullspeed - %i %i %i\n",is386,AT,cpuspeed2);
                if (AT)
                        setpitclock(models[model].cpu[cpu_manufacturer].cpus[cpu].rspeed);
                else
                        setpitclock(14318184.0);
//                if (is386) setpitclock(clocks[2][cpuspeed2][0]);
//                else       setpitclock(clocks[AT?1:0][cpuspeed2][0]);
        }
        atfullspeed=1;
}

void speedchanged()
{
        if (AT)
                setpitclock(models[model].cpu[cpu_manufacturer].cpus[cpu].rspeed);
        else
                setpitclock(14318184.0);
}

void closepc()
{
        codegen_close();
        atapi->exit();
//        ioctl_close();
        dumppic();
//        output=7;
//        setpitclock(clocks[0][0][0]);
//        while (1) runpc();
        disc_close(0);
        disc_close(1);
        dumpregs();
        closevideo();
        lpt1_device_close();
        mouse_emu_close();
        device_close_all();
        zip_eject();
}

/*int main()
{
        initpc();
        while (!key[KEY_F11])
        {
                runpc();
        }
        closepc();
        return 0;
}

END_OF_MAIN();*/

typedef struct config_callback_t
{
        void(*loadconfig)();
        void(*saveconfig)();
        void(*onloaded)();
} config_callback_t;
config_callback_t config_callbacks[10];
int num_config_callbacks = 0;

void add_config_callback(void(*loadconfig)(), void(*saveconfig)(), void(*onloaded)())
{
        config_callbacks[num_config_callbacks].loadconfig = loadconfig;
        config_callbacks[num_config_callbacks].saveconfig = saveconfig;
        config_callbacks[num_config_callbacks].onloaded = onloaded;
        num_config_callbacks++;
}

void loadconfig(char *fn)
{
        int c, d;
        char s[512];
        char global_config_file[512];
        char *p;

        append_filename(global_config_file, pcem_path, "pcem.cfg", 511);

        config_load(CFG_GLOBAL, global_config_file);
        
        if (!fn)
                config_load(CFG_MACHINE, config_file_default);
        else
                config_load(CFG_MACHINE, fn);

        vid_resize = config_get_int(CFG_GLOBAL, NULL, "vid_resize", 0);
        video_force_aspect_ration = config_get_int(CFG_GLOBAL, NULL, "vid_force_aspect_ratio", 0);
        vid_disc_indicator = config_get_int(CFG_GLOBAL, NULL, "vid_disc_indicator", 1);
        vid_api = config_get_int(CFG_GLOBAL, NULL, "vid_api", 0);
        video_fullscreen_scale = config_get_int(CFG_GLOBAL, NULL, "video_fullscreen_scale", 0);
        video_fullscreen_first = config_get_int(CFG_GLOBAL, NULL, "video_fullscreen_first", 1);

        window_w = config_get_int(CFG_GLOBAL, NULL, "window_w", 0);
        window_h = config_get_int(CFG_GLOBAL, NULL, "window_h", 0);
        window_x = config_get_int(CFG_GLOBAL, NULL, "window_x", 0);
        window_y = config_get_int(CFG_GLOBAL, NULL, "window_y", 0);
        window_remember = config_get_int(CFG_GLOBAL, NULL, "window_remember", 0);

        sound_buf_len = config_get_int(CFG_GLOBAL, NULL, "sound_buf_len", 200);
        sound_gain = config_get_int(CFG_GLOBAL, NULL, "sound_gain", 0);
        
        GAMEBLASTER = config_get_int(CFG_MACHINE, NULL, "gameblaster", 0);
        GUS = config_get_int(CFG_MACHINE, NULL, "gus", 0);
        SSI2001 = config_get_int(CFG_MACHINE, NULL, "ssi2001", 0);
        voodoo_enabled = config_get_int(CFG_MACHINE, NULL, "voodoo", 0);
        
        p = (char *)config_get_string(CFG_MACHINE, NULL, "model", "");
        if (p)
                model = model_get_model_from_internal_name(p);
        else
                model = 0;

        if (model >= model_count())
                model = model_count() - 1;

        romset = model_getromset();
        cpu_manufacturer = config_get_int(CFG_MACHINE, NULL, "cpu_manufacturer", 0);
        cpu = config_get_int(CFG_MACHINE, NULL, "cpu", 0);
        p = (char *)config_get_string(CFG_MACHINE, NULL, "fpu", "none");
        fpu_type = fpu_get_type(model, cpu_manufacturer, cpu, p);
        cpu_use_dynarec = config_get_int(CFG_MACHINE, NULL, "cpu_use_dynarec", 0);
        cpu_waitstates = config_get_int(CFG_MACHINE, NULL, "cpu_waitstates", 0);
                
        p = (char *)config_get_string(CFG_MACHINE, NULL, "gfxcard", "");
        if (p)
                gfxcard = video_get_video_from_internal_name(p);
        else
                gfxcard = 0;
        video_speed = config_get_int(CFG_MACHINE, NULL, "video_speed", -1);
        p = (char *)config_get_string(CFG_MACHINE, NULL, "sndcard", "");
        if (p)
                sound_card_current = sound_card_get_from_internal_name(p);
        else
                sound_card_current = 0;

        if (!override_drive_a)
        {
                p = (char *)config_get_string(CFG_MACHINE, NULL, "disc_a", "");
                if (p) strcpy(discfns[0], p);
                else   strcpy(discfns[0], "");
        }

        if (!override_drive_b)
        {
                p = (char *)config_get_string(CFG_MACHINE, NULL, "disc_b", "");
                if (p) strcpy(discfns[1], p);
                else   strcpy(discfns[1], "");
        }

        p = (char *)config_get_string(CFG_MACHINE, NULL, "hdd_controller", "");
        if (p)
                strncpy(hdd_controller_name, p, sizeof(hdd_controller_name)-1);
        else
                strncpy(hdd_controller_name, "none", sizeof(hdd_controller_name)-1);        

        mem_size = config_get_int(CFG_MACHINE, NULL, "mem_size", 4096);
        if (mem_size < (((models[model].flags & MODEL_AT) && models[model].ram_granularity < 128) ? models[model].min_ram*1024 : models[model].min_ram))
                mem_size = (((models[model].flags & MODEL_AT) && models[model].ram_granularity < 128) ? models[model].min_ram*1024 : models[model].min_ram);

        cdrom_drive = config_get_int(CFG_MACHINE, NULL, "cdrom_drive", 0);
        cdrom_channel = config_get_int(CFG_MACHINE, NULL, "cdrom_channel", 2);
        
        zip_channel = config_get_int(CFG_MACHINE, NULL, "zip_channel", -1);
        
        p = (char *)config_get_string(CFG_MACHINE, NULL, "cdrom_path", "");
        if (p) strcpy(image_path, p);
        else   strcpy(image_path, "");
        
        hdc[0].spt = config_get_int(CFG_MACHINE, NULL, "hdc_sectors", 0);
        hdc[0].hpc = config_get_int(CFG_MACHINE, NULL, "hdc_heads", 0);
        hdc[0].tracks = config_get_int(CFG_MACHINE, NULL, "hdc_cylinders", 0);
        p = (char *)config_get_string(CFG_MACHINE, NULL, "hdc_fn", "");
        if (p) strcpy(ide_fn[0], p);
        else   strcpy(ide_fn[0], "");
        hdc[1].spt = config_get_int(CFG_MACHINE, NULL, "hdd_sectors", 0);
        hdc[1].hpc = config_get_int(CFG_MACHINE, NULL, "hdd_heads", 0);
        hdc[1].tracks = config_get_int(CFG_MACHINE, NULL, "hdd_cylinders", 0);
        p = (char *)config_get_string(CFG_MACHINE, NULL, "hdd_fn", "");
        if (p) strcpy(ide_fn[1], p);
        else   strcpy(ide_fn[1], "");
        hdc[2].spt = config_get_int(CFG_MACHINE, NULL, "hde_sectors", 0);
        hdc[2].hpc = config_get_int(CFG_MACHINE, NULL, "hde_heads", 0);
        hdc[2].tracks = config_get_int(CFG_MACHINE, NULL, "hde_cylinders", 0);
        p = (char *)config_get_string(CFG_MACHINE, NULL, "hde_fn", "");
        if (p) strcpy(ide_fn[2], p);
        else   strcpy(ide_fn[2], "");
        hdc[3].spt = config_get_int(CFG_MACHINE, NULL, "hdf_sectors", 0);
        hdc[3].hpc = config_get_int(CFG_MACHINE, NULL, "hdf_heads", 0);
        hdc[3].tracks = config_get_int(CFG_MACHINE, NULL, "hdf_cylinders", 0);
        p = (char *)config_get_string(CFG_MACHINE, NULL, "hdf_fn", "");
        if (p) strcpy(ide_fn[3], p);
        else   strcpy(ide_fn[3], "");
        hdc[4].spt = config_get_int(CFG_MACHINE, NULL, "hdg_sectors", 0);
        hdc[4].hpc = config_get_int(CFG_MACHINE, NULL, "hdg_heads", 0);
        hdc[4].tracks = config_get_int(CFG_MACHINE, NULL, "hdg_cylinders", 0);
        p = (char *)config_get_string(CFG_MACHINE, NULL, "hdg_fn", "");
        if (p) strcpy(ide_fn[4], p);
        else   strcpy(ide_fn[4], "");
        hdc[5].spt = config_get_int(CFG_MACHINE, NULL, "hdh_sectors", 0);
        hdc[5].hpc = config_get_int(CFG_MACHINE, NULL, "hdh_heads", 0);
        hdc[5].tracks = config_get_int(CFG_MACHINE, NULL, "hdh_cylinders", 0);
        p = (char *)config_get_string(CFG_MACHINE, NULL, "hdh_fn", "");
        if (p) strcpy(ide_fn[5], p);
        else   strcpy(ide_fn[5], "");
        hdc[6].spt = config_get_int(CFG_MACHINE, NULL, "hdi_sectors", 0);
        hdc[6].hpc = config_get_int(CFG_MACHINE, NULL, "hdi_heads", 0);
        hdc[6].tracks = config_get_int(CFG_MACHINE, NULL, "hdi_cylinders", 0);
        p = (char *)config_get_string(CFG_MACHINE, NULL, "hdi_fn", "");
        if (p) strcpy(ide_fn[6], p);
        else   strcpy(ide_fn[6], "");

        fdd_set_type(0, config_get_int(CFG_MACHINE, NULL, "drive_a_type", 7));
        fdd_set_type(1, config_get_int(CFG_MACHINE, NULL, "drive_b_type", 7));
        bpb_disable = config_get_int(CFG_MACHINE, NULL, "bpb_disable", 0);

        cd_speed = config_get_int(CFG_MACHINE, NULL, "cd_speed", 24);
        cd_model = cd_model_from_config((char *)config_get_string(CFG_MACHINE, NULL, "cd_model", cd_get_config_model(0)));
        
        joystick_type = config_get_int(CFG_MACHINE, NULL, "joystick_type", 0);
        mouse_type = config_get_int(CFG_MACHINE, NULL, "mouse_type", 0);
        
        for (c = 0; c < joystick_get_max_joysticks(joystick_type); c++)
        {
                sprintf(s, "joystick_%i_nr", c);
                joystick_state[c].plat_joystick_nr = config_get_int(CFG_MACHINE, "Joysticks", s, 0);
                
                if (joystick_state[c].plat_joystick_nr)
                {
                        for (d = 0; d < joystick_get_axis_count(joystick_type); d++)
                        {                        
                                sprintf(s, "joystick_%i_axis_%i", c, d);
                                joystick_state[c].axis_mapping[d] = config_get_int(CFG_MACHINE, "Joysticks", s, d);
                        }
                        for (d = 0; d < joystick_get_button_count(joystick_type); d++)
                        {                        
                                sprintf(s, "joystick_%i_button_%i", c, d);
                                joystick_state[c].button_mapping[d] = config_get_int(CFG_MACHINE, "Joysticks", s, d);
                        }
                        for (d = 0; d < joystick_get_pov_count(joystick_type); d++)
                        {                        
                                sprintf(s, "joystick_%i_pov_%i_x", c, d);
                                joystick_state[c].pov_mapping[d][0] = config_get_int(CFG_MACHINE, "Joysticks", s, d);
                                sprintf(s, "joystick_%i_pov_%i_y", c, d);
                                joystick_state[c].pov_mapping[d][1] = config_get_int(CFG_MACHINE, "Joysticks", s, d);
                        }
                }
        }

        enable_sync = config_get_int(CFG_MACHINE, NULL, "enable_sync", 1);

        p = (char *)config_get_string(CFG_MACHINE, NULL, "lpt1_device", "");
        if (p) strcpy(lpt1_device_name, p);
        else   strcpy(lpt1_device_name, "");
        
#ifdef USE_NETWORKING
	//network
	ethif = config_get_int(CFG_GLOBAL, NULL, "netinterface", 1);
        if (ethif >= inum)
                inum = ethif + 1;

        p = (char *)config_get_string(CFG_MACHINE, NULL, "netcard", "");
        if (p)
                network_card_current = network_card_get_from_internal_name(p);
        else
                network_card_current = 0;
#endif

        for (d = 0; d < num_config_callbacks; ++d)
                if (config_callbacks[d].loadconfig)
                        config_callbacks[d].loadconfig();


        for (d = 0; d < num_config_callbacks; ++d)
                if (config_callbacks[d].onloaded)
                        config_callbacks[d].onloaded();

        config_dump(CFG_GLOBAL);
        config_dump(CFG_MACHINE);
}

void saveconfig(char *fn)
{
        int c, d;
        char global_config_file[512];

        append_filename(global_config_file, pcem_path, "pcem.cfg", 511);

        config_set_int(CFG_GLOBAL, NULL, "vid_resize", vid_resize);
        config_set_int(CFG_GLOBAL, NULL, "vid_force_aspect_ratio", video_force_aspect_ration);
        config_set_int(CFG_GLOBAL, NULL, "vid_disc_indicator", vid_disc_indicator);
        config_set_int(CFG_GLOBAL, NULL, "vid_api", vid_api);
        config_set_int(CFG_GLOBAL, NULL, "video_fullscreen_scale", video_fullscreen_scale);
        config_set_int(CFG_GLOBAL, NULL, "video_fullscreen_first", video_fullscreen_first);

        config_set_int(CFG_GLOBAL, NULL, "window_w", window_w);
        config_set_int(CFG_GLOBAL, NULL, "window_h", window_h);
        config_set_int(CFG_GLOBAL, NULL, "window_x", window_x);
        config_set_int(CFG_GLOBAL, NULL, "window_y", window_y);
        config_set_int(CFG_GLOBAL, NULL, "window_remember", window_remember);

        config_set_int(CFG_GLOBAL, NULL, "sound_buf_len", sound_buf_len);
        config_set_int(CFG_GLOBAL, NULL, "sound_gain", sound_gain);
        
        config_set_int(CFG_MACHINE, NULL, "gameblaster", GAMEBLASTER);
        config_set_int(CFG_MACHINE, NULL, "gus", GUS);
        config_set_int(CFG_MACHINE, NULL, "ssi2001", SSI2001);
        config_set_int(CFG_MACHINE, NULL, "voodoo", voodoo_enabled);
        
        config_set_string(CFG_MACHINE, NULL, "model", model_get_internal_name());
        config_set_int(CFG_MACHINE, NULL, "cpu_manufacturer", cpu_manufacturer);
        config_set_int(CFG_MACHINE, NULL, "cpu", cpu);
        config_set_string(CFG_MACHINE, NULL, "fpu", (char *)fpu_get_internal_name(model, cpu_manufacturer, cpu, fpu_type));
        config_set_int(CFG_MACHINE, NULL, "cpu_use_dynarec", cpu_use_dynarec);
        config_set_int(CFG_MACHINE, NULL, "cpu_waitstates", cpu_waitstates);
        
        config_set_string(CFG_MACHINE, NULL, "gfxcard", video_get_internal_name(video_old_to_new(gfxcard)));
        config_set_int(CFG_MACHINE, NULL, "video_speed", video_speed);
        config_set_string(CFG_MACHINE, NULL, "sndcard", sound_card_get_internal_name(sound_card_current));
        config_set_int(CFG_MACHINE, NULL, "cpu_speed", cpuspeed);
        config_set_string(CFG_MACHINE, NULL, "disc_a", discfns[0]);
        config_set_string(CFG_MACHINE, NULL, "disc_b", discfns[1]);
        config_set_string(CFG_MACHINE, NULL, "hdd_controller", hdd_controller_name);

        config_set_int(CFG_MACHINE, NULL, "mem_size", mem_size);
        config_set_int(CFG_MACHINE, NULL, "cdrom_drive", cdrom_drive);
        config_set_int(CFG_MACHINE, NULL, "cdrom_channel", cdrom_channel);
        config_set_string(CFG_MACHINE, NULL, "cdrom_path", image_path);

        config_set_int(CFG_MACHINE, NULL, "zip_channel", zip_channel);
                
        config_set_int(CFG_MACHINE, NULL, "hdc_sectors", hdc[0].spt);
        config_set_int(CFG_MACHINE, NULL, "hdc_heads", hdc[0].hpc);
        config_set_int(CFG_MACHINE, NULL, "hdc_cylinders", hdc[0].tracks);
        config_set_string(CFG_MACHINE, NULL, "hdc_fn", ide_fn[0]);
        config_set_int(CFG_MACHINE, NULL, "hdd_sectors", hdc[1].spt);
        config_set_int(CFG_MACHINE, NULL, "hdd_heads", hdc[1].hpc);
        config_set_int(CFG_MACHINE, NULL, "hdd_cylinders", hdc[1].tracks);
        config_set_string(CFG_MACHINE, NULL, "hdd_fn", ide_fn[1]);
        config_set_int(CFG_MACHINE, NULL, "hde_sectors", hdc[2].spt);
        config_set_int(CFG_MACHINE, NULL, "hde_heads", hdc[2].hpc);
        config_set_int(CFG_MACHINE, NULL, "hde_cylinders", hdc[2].tracks);
        config_set_string(CFG_MACHINE, NULL, "hde_fn", ide_fn[2]);
        config_set_int(CFG_MACHINE, NULL, "hdf_sectors", hdc[3].spt);
        config_set_int(CFG_MACHINE, NULL, "hdf_heads", hdc[3].hpc);
        config_set_int(CFG_MACHINE, NULL, "hdf_cylinders", hdc[3].tracks);
        config_set_string(CFG_MACHINE, NULL, "hdf_fn", ide_fn[3]);
        config_set_int(CFG_MACHINE, NULL, "hdg_sectors", hdc[4].spt);
        config_set_int(CFG_MACHINE, NULL, "hdg_heads", hdc[4].hpc);
        config_set_int(CFG_MACHINE, NULL, "hdg_cylinders", hdc[4].tracks);
        config_set_string(CFG_MACHINE, NULL, "hdg_fn", ide_fn[4]);
        config_set_int(CFG_MACHINE, NULL, "hdh_sectors", hdc[5].spt);
        config_set_int(CFG_MACHINE, NULL, "hdh_heads", hdc[5].hpc);
        config_set_int(CFG_MACHINE, NULL, "hdh_cylinders", hdc[5].tracks);
        config_set_string(CFG_MACHINE, NULL, "hdh_fn", ide_fn[5]);
        config_set_int(CFG_MACHINE, NULL, "hdi_sectors", hdc[6].spt);
        config_set_int(CFG_MACHINE, NULL, "hdi_heads", hdc[6].hpc);
        config_set_int(CFG_MACHINE, NULL, "hdi_cylinders", hdc[6].tracks);
        config_set_string(CFG_MACHINE, NULL, "hdi_fn", ide_fn[6]);

        config_set_int(CFG_MACHINE, NULL, "drive_a_type", fdd_get_type(0));
        config_set_int(CFG_MACHINE, NULL, "drive_b_type", fdd_get_type(1));
        config_set_int(CFG_MACHINE, NULL, "bpb_disable", bpb_disable);

        config_set_int(CFG_MACHINE, NULL, "cd_speed", cd_speed);
        config_set_string(CFG_MACHINE, NULL, "cd_model", cd_model_to_config(cd_model));
        
        config_set_int(CFG_MACHINE, NULL, "joystick_type", joystick_type);
        config_set_int(CFG_MACHINE, NULL, "mouse_type", mouse_type);
                
        for (c = 0; c < joystick_get_max_joysticks(joystick_type); c++)
        {
                char s[80];

                sprintf(s, "joystick_%i_nr", c);
                config_set_int(CFG_MACHINE, "Joysticks", s, joystick_state[c].plat_joystick_nr);
                
                if (joystick_state[c].plat_joystick_nr)
                {
                        for (d = 0; d < joystick_get_axis_count(joystick_type); d++)
                        {                        
                                sprintf(s, "joystick_%i_axis_%i", c, d);
                                config_set_int(CFG_MACHINE, "Joysticks", s, joystick_state[c].axis_mapping[d]);
                        }
                        for (d = 0; d < joystick_get_button_count(joystick_type); d++)
                        {                        
                                sprintf(s, "joystick_%i_button_%i", c, d);
                                config_set_int(CFG_MACHINE, "Joysticks", s, joystick_state[c].button_mapping[d]);
                        }
                        for (d = 0; d < joystick_get_pov_count(joystick_type); d++)
                        {                        
                                sprintf(s, "joystick_%i_pov_%i_x", c, d);
                                config_set_int(CFG_MACHINE, "Joysticks", s, joystick_state[c].pov_mapping[d][0]);
                                sprintf(s, "joystick_%i_pov_%i_y", c, d);
                                config_set_int(CFG_MACHINE, "Joysticks", s, joystick_state[c].pov_mapping[d][1]);
                        }
                }
        }
        
        config_set_int(CFG_MACHINE, NULL, "enable_sync", enable_sync);

#ifdef USE_NETWORKING
	config_set_int(CFG_GLOBAL, NULL, "netinterface", ethif);
	config_set_string(CFG_MACHINE, NULL, "netcard", network_card_get_internal_name(network_card_current));
#endif

        config_set_string(CFG_MACHINE, NULL, "lpt1_device", lpt1_device_name);
        
        for (d = 0; d < num_config_callbacks; ++d)
                if (config_callbacks[d].saveconfig)
                        config_callbacks[d].saveconfig();

        pclog("config_save(%s)\n", config_file_default);
        if (fn)
                config_save(CFG_MACHINE, fn);
        else if (strlen(config_file_default))
                config_save(CFG_MACHINE, config_file_default);

        config_save(CFG_GLOBAL, global_config_file);
}

void saveconfig_global_only()
{
        char global_config_file[512];

        append_filename(global_config_file, pcem_path, "pcem.cfg", 511);

        config_save(CFG_GLOBAL, global_config_file);
}
