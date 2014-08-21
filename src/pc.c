#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "ibm.h"
#include "device.h"

#include "ali1429.h"
#include "amstrad.h"
#include "cdrom-ioctl.h"
#include "config.h"
#include "cpu.h"
#include "dma.h"
#include "fdc.h"
#include "sound_gus.h"
#include "ide.h"
#include "keyboard.h"
#include "mem.h"
#include "model.h"
#include "mouse.h"
#include "nvr.h"
#include "pic.h"
#include "pit.h"
#include "plat-joystick.h"
#include "plat-mouse.h"
#include "serial.h"
#include "sound.h"
#include "sound_cms.h"
#include "sound_opl.h"
#include "sound_sb.h"
#include "sound_ssi2001.h"
#include "timer.h"
#include "vid_voodoo.h"
#include "video.h"

int frame = 0;

int cdrom_enabled;
int CPUID;
int vid_resize, vid_api;

int cycles_lost = 0;

int clockrate;
int insc=0;
float mips,flops;
extern int mmuflush;
extern int readlnum,writelnum;
void fullspeed();

int framecount,fps;
int intcount;

int output;
int atfullspeed;
void loadconfig();
void saveconfig();
int infocus;
int mousecapture;
FILE *pclogf;
void pclog(const char *format, ...)
{
#ifndef RELEASE_BUILD
   char buf[1024];
   //return;
   if (!pclogf)
      pclogf=fopen("pclog.txt","wt");
//return;
   va_list ap;
   va_start(ap, format);
   vsprintf(buf, format, ap);
   va_end(ap);
   fputs(buf,pclogf);
fflush(pclogf);
#endif
}

void fatal(const char *format, ...)
{
   char buf[256];
//   return;
   if (!pclogf)
      pclogf=fopen("pclog.txt","wt");
//return;
   va_list ap;
   va_start(ap, format);
   vsprintf(buf, format, ap);
   va_end(ap);
   fputs(buf,pclogf);
   fflush(pclogf);
   dumppic();
   dumpregs();
   exit(-1);
}

uint8_t cgastat;

int pollmouse_delay = 2;
void pollmouse()
{
        int x,y;
//        return;
        pollmouse_delay--;
        if (pollmouse_delay) return;
        pollmouse_delay = 2;
        poll_mouse();
        get_mouse_mickeys(&x,&y);
        if (mouse_poll)
           mouse_poll(x, y, mouse_b);
        if (mousecapture) position_mouse(64,64);
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
        win_title_update=1;
}

void pc_reset()
{
        resetx86();
        cpu_set();
        //timer_reset();
        dma_reset();
        fdc_reset();
        pic_reset();
        pit_reset();
        serial_reset();

        setpitclock(models[model].cpu[cpu_manufacturer].cpus[cpu].rspeed);
        
//        sb_reset();

        ali1429_reset();
//        video_init();
}

void initpc()
{
        char *p;
//        allegro_init();
        get_executable_name(pcempath,511);
        pclog("executable_name = %s\n", pcempath);
        p=get_filename(pcempath);
        *p=0;
        pclog("path = %s\n", pcempath);        

        keyboard_init();
        mouse_init();
        joystick_init();
        
        loadconfig();
        pclog("Config loaded\n");
        
        cpuspeed2=(AT)?2:1;
//        cpuspeed2=cpuspeed;
        atfullspeed=0;

        device_init();        
        
        initvideo();
        mem_init();
        loadbios();
        mem_add_bios();
                
        loaddisc(0,discfns[0]);
        loaddisc(1,discfns[1]);
        
        timer_reset();
        sound_reset();
	fdc_init();
        
        //loadfont();
        loadnvr();
        sound_init();
        resetide();
        ioctl_open(cdrom_drive);
        model_init();        
        video_init();
        speaker_init();        
        sound_card_init(sound_card_current);
        if (GUS)
                device_add(&gus_device);
        if (GAMEBLASTER)
                device_add(&cms_device);
        if (SSI2001)
                device_add(&ssi2001_device);
               
        pc_reset();
        
        pit_reset();        
/*        if (romset==ROM_AMI386 || romset==ROM_AMI486) */fullspeed();
        mem_updatecache();
        ali1429_reset();
//        CPUID=(is486 && (cpuspeed==7 || cpuspeed>=9));
//        pclog("Init - CPUID %i %i\n",CPUID,cpuspeed);
        shadowbios=0;
        voodoo_init();
        
        ioctl_reset();
}

void resetpc()
{
        pc_reset();
//        cpuspeed2=(AT)?2:1;
//        atfullspeed=0;
///*        if (romset==ROM_AMI386 || romset==ROM_AMI486) */fullspeed();
        shadowbios=0;
}

void resetpchard()
{
        device_close_all();
        device_init();
        
        timer_reset();
        sound_reset();
        mem_resize();
        fdc_init();
        model_init();
        video_init();
        speaker_init();        
        sound_card_init(sound_card_current);
        if (GUS)
                device_add(&gus_device);
        if (GAMEBLASTER)
                device_add(&cms_device);
        if (SSI2001)
                device_add(&ssi2001_device);
        
        pc_reset();
        
        resetide();
        
        loadnvr();

//        cpuspeed2 = (AT)?2:1;
//        atfullspeed = 0;
//        setpitclock(models[model].cpu[cpu_manufacturer].cpus[cpu].rspeed);

        shadowbios = 0;
        ali1429_reset();
        
        keyboard_at_reset();
        
//        output=3;

        ioctl_reset();
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

        startblit();
        clockrate = models[model].cpu[cpu_manufacturer].cpus[cpu].rspeed;
                if (is386)   exec386(models[model].cpu[cpu_manufacturer].cpus[cpu].rspeed / 100);
                else if (AT) exec386(models[model].cpu[cpu_manufacturer].cpus[cpu].rspeed / 100);
                else         execx86(models[model].cpu[cpu_manufacturer].cpus[cpu].rspeed / 100);
                keyboard_poll_host();
                keyboard_process();
//                checkkeys();
                pollmouse();
                poll_joystick();
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
                        updatestatus=1;
                        readlnum=writelnum=0;
                        egareads=egawrites=0;
                        cycles_lost = 0;
                        mmuflush=0;
                        intcount=0;
                        intcount=pitcount=0;
                        emu_fps = frames;
                        frames = 0;
                }
                if (win_title_update)
                {
                        win_title_update=0;
                        sprintf(s, "PCem v8.1 - %s - %s - %s - %i%%", model_getname(), models[model].cpu[cpu_manufacturer].cpus[cpu].name, (!mousecapture) ? "Click to capture mouse" : "Press CTRL-END or middle button to release mouse", fps);
                        set_window_title(s);
                }
                done++;
                frame++;
}

void fullspeed()
{
        cpuspeed2=cpuspeed;
        if (!atfullspeed)
        {
                printf("Set fullspeed - %i %i %i\n",is386,AT,cpuspeed2);
                setpitclock(models[model].cpu[cpu_manufacturer].cpus[cpu].rspeed);
//                if (is386) setpitclock(clocks[2][cpuspeed2][0]);
//                else       setpitclock(clocks[AT?1:0][cpuspeed2][0]);
        }
        atfullspeed=1;
        nvr_recalc();
}

void speedchanged()
{
        if (atfullspeed)
        {
                cpuspeed2=cpuspeed;
                setpitclock(models[model].cpu[cpu_manufacturer].cpus[cpu].rspeed);
//                if (is386) setpitclock(clocks[2][cpuspeed2][0]);
//                else       setpitclock(clocks[AT?1:0][cpuspeed2][0]);
        }
        mem_updatecache();
        nvr_recalc();
}

void closepc()
{
        atapi->exit();
//        ioctl_close();
        dumppic();
//        output=7;
//        setpitclock(clocks[0][0][0]);
//        while (1) runpc();
        savedisc(0);
        savedisc(1);
        dumpregs();
        closevideo();
        device_close_all();
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

int cga_comp=0;

void loadconfig()
{
        char s[512];
        char *p;
        append_filename(s, pcempath, "pcem.cfg", 511);
        set_config_file(s);
        
        config_load();
        
        GAMEBLASTER = get_config_int(NULL, "gameblaster", 0);
        GUS = get_config_int(NULL, "gus", 0);
        SSI2001 = get_config_int(NULL, "ssi2001", 0);
        
        model = get_config_int(NULL, "model", 14);

        if (model >= model_count())
                model = model_count() - 1;

        romset = model_getromset();
        cpu_manufacturer = get_config_int(NULL, "cpu_manufacturer", 0);
        cpu = get_config_int(NULL, "cpu", 0);
        
        gfxcard = get_config_int(NULL, "gfxcard", 0);
        video_speed = get_config_int(NULL, "video_speed", 3);
        sound_card_current = get_config_int(NULL, "sndcard", SB2);

        p = (char *)get_config_string(NULL, "disc_a", "");
        if (p) strcpy(discfns[0], p);
        else   strcpy(discfns[0], "");

        p = (char *)get_config_string(NULL, "disc_b", "");
        if (p) strcpy(discfns[1], p);
        else   strcpy(discfns[1], "");

        mem_size = get_config_int(NULL, "mem_size", 4);
        cdrom_drive = get_config_int(NULL, "cdrom_drive", 0);
        cdrom_enabled = get_config_int(NULL, "cdrom_enabled", 0);
        
        slowega = get_config_int(NULL, "slow_video", 1);
        cache = get_config_int(NULL, "cache", 3);
        cga_comp = get_config_int(NULL, "cga_composite", 0);
        
        vid_resize = get_config_int(NULL, "vid_resize", 0);
        vid_api = get_config_int(NULL, "vid_api", 0);
        video_fullscreen_scale = get_config_int(NULL, "video_fullscreen_scale", 0);
        video_fullscreen_first = get_config_int(NULL, "video_fullscreen_first", 1);

        hdc[0].spt = get_config_int(NULL, "hdc_sectors", 0);
        hdc[0].hpc = get_config_int(NULL, "hdc_heads", 0);
        hdc[0].tracks = get_config_int(NULL, "hdc_cylinders", 0);
        p = (char *)get_config_string(NULL, "hdc_fn", "");
        if (p) strcpy(ide_fn[0], p);
        else   strcpy(ide_fn[0], "");
        hdc[1].spt = get_config_int(NULL, "hdd_sectors", 0);
        hdc[1].hpc = get_config_int(NULL, "hdd_heads", 0);
        hdc[1].tracks = get_config_int(NULL, "hdd_cylinders", 0);
        p = (char *)get_config_string(NULL, "hdd_fn", "");
        if (p) strcpy(ide_fn[1], p);
        else   strcpy(ide_fn[1], "");
}

void saveconfig()
{
        set_config_int(NULL, "gameblaster", GAMEBLASTER);
        set_config_int(NULL, "gus", GUS);
        set_config_int(NULL, "ssi2001", SSI2001);
        
        set_config_int(NULL, "model", model);
        set_config_int(NULL, "cpu_manufacturer", cpu_manufacturer);
        set_config_int(NULL, "cpu", cpu);
        
        set_config_int(NULL, "gfxcard", gfxcard);
        set_config_int(NULL, "video_speed", video_speed);
        set_config_int(NULL, "sndcard", sound_card_current);
        set_config_int(NULL, "cpu_speed", cpuspeed);
        set_config_int(NULL, "has_fpu", hasfpu);
        set_config_int(NULL, "slow_video", slowega);
        set_config_int(NULL, "cache", cache);
        set_config_int(NULL, "cga_composite", cga_comp);
        set_config_string(NULL, "disc_a", discfns[0]);
        set_config_string(NULL, "disc_b", discfns[1]);
        set_config_int(NULL, "mem_size", mem_size);
        set_config_int(NULL, "cdrom_drive", cdrom_drive);
        set_config_int(NULL, "cdrom_enabled", cdrom_enabled);
        set_config_int(NULL, "vid_resize", vid_resize);
        set_config_int(NULL, "vid_api", vid_api);
        set_config_int(NULL, "video_fullscreen_scale", video_fullscreen_scale);
        set_config_int(NULL, "video_fullscreen_first", video_fullscreen_first);
        
        set_config_int(NULL, "hdc_sectors", hdc[0].spt);
        set_config_int(NULL, "hdc_heads", hdc[0].hpc);
        set_config_int(NULL, "hdc_cylinders", hdc[0].tracks);
        set_config_string(NULL, "hdc_fn", ide_fn[0]);
        set_config_int(NULL, "hdd_sectors", hdc[1].spt);
        set_config_int(NULL, "hdd_heads", hdc[1].hpc);
        set_config_int(NULL, "hdd_cylinders", hdc[1].tracks);
        set_config_string(NULL, "hdd_fn", ide_fn[1]);
        
        config_save();
}
