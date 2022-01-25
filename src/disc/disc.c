#include "ibm.h"

#include "config.h"
#include "disc.h"
#include "disc_fdi.h"
#include "disc_img.h"
#include "fdc.h"
#include "fdd.h"
#include "timer.h"

int disc_drivesel = 0;
pc_timer_t disc_poll_timer;

int disc_track[2];
int writeprot[2], fwriteprot[2];

DRIVE drives[2];
int drive_type[2];

int curdrive = 0;

//char discfns[2][260] = {"", ""};
int defaultwriteprot = 0;

int fdc_ready;

int drive_empty[2] = { 1, 1 };
int disc_changed[2];

int motorspin;
int motoron;

int fdc_indexcount = 52;

/*void (*fdc_callback)();
void (*fdc_data)(uint8_t dat);
void (*fdc_spindown)();
void (*fdc_finishread)();
void (*fdc_notfound)();
void (*fdc_datacrcerror)();
void (*fdc_headercrcerror)();
void (*fdc_writeprotect)();
int  (*fdc_getdata)(int last);
void (*fdc_sectorid)(uint8_t track, uint8_t side, uint8_t sector, uint8_t size, uint8_t crc1, uint8_t crc2);
void (*fdc_indexpulse)();*/

static struct
{
        char* ext;
        void (* load)(int drive, char* fn);
        void (* close)(int drive);
        int size;
}
        loaders[] =
        {
                { "IMG", img_load, img_close, -1 },
                { "IMA", img_load, img_close, -1 },
                { "360", img_load, img_close, -1 },
                { "XDF", img_load, img_close, -1 },
                { "FDI", fdi_load, fdi_close, -1 },
                { 0, 0, 0 }
        };

static int driveloaders[4];

void disc_load(int drive, char* fn)
{
        int c = 0, size;
        char* p;
        FILE* f;
//        pclog("disc_load %i %s\n", drive, fn);
//        setejecttext(drive, "");
        if (!fn) return;
        p = get_extension(fn);
        if (!p) return;
//        setejecttext(drive, fn);
        pclog("Loading :%i %s %s\n", drive, fn, p);
        f = fopen(fn, "rb");
        if (!f) return;
        fseek(f, -1, SEEK_END);
        size = ftell(f) + 1;
        fclose(f);
        while (loaders[c].ext)
        {
                if (!strcasecmp(p, loaders[c].ext) && (size == loaders[c].size || loaders[c].size == -1))
                {
                        pclog("Loading as %s\n", p);
                        driveloaders[drive] = c;
                        loaders[c].load(drive, fn);
                        drive_empty[drive] = 0;
                        disc_changed[drive] = 1;
                        strcpy(discfns[drive], fn);
                        fdd_disc_changed(drive);
                        return;
                }
                c++;
        }
        pclog("Couldn't load %s %s\n", fn, p);
        drive_empty[drive] = 1;
        discfns[drive][0] = 0;
}

void disc_close(int drive)
{
//        pclog("disc_close %i\n", drive);
        if (loaders[driveloaders[drive]].close) loaders[driveloaders[drive]].close(drive);
        drive_empty[drive] = 1;
        discfns[drive][0] = 0;
        drives[drive].hole = NULL;
        drives[drive].poll = NULL;
        drives[drive].seek = NULL;
        drives[drive].readsector = NULL;
        drives[drive].writesector = NULL;
        drives[drive].readaddress = NULL;
        drives[drive].format = NULL;
}

int disc_notfound = 0;
static int disc_period = 32;

int disc_hole(int drive)
{
        drive ^= fdd_swap;

        if (drive < 2 && drives[drive].hole)
        {
                return drives[drive].hole(drive);
        }
        else
        {
                return 0;
        }
}

void disc_poll()
{
        timer_advance_u64(&disc_poll_timer, disc_period * TIMER_USEC);

        if (disc_drivesel < 2 && drives[disc_drivesel].poll)
                drives[disc_drivesel].poll();

        if (disc_notfound)
        {
                disc_notfound--;
                if (!disc_notfound)
                        fdc_notfound(FDC_STATUS_AM_NOT_FOUND);
        }
}

int disc_get_bitcell_period(int rate)
{
        int bit_rate = 0;

        switch (rate)
        {
        case 0: /*High density*/
                bit_rate = 500;
                break;
        case 1: /*Double density (360 rpm)*/
                bit_rate = 300;
                break;
        case 2: /*Double density*/
                bit_rate = 250;
                break;
        case 3: /*Extended density*/
                bit_rate = 1000;
                break;
        }

        return 1000000 / bit_rate * 2; /*Bitcell period in ns*/
}

void disc_set_rate(int drive, int drvden, int rate)
{
        switch (rate)
        {
        case 0: /*High density*/
                disc_period = 16;
                break;
        case 1:
                switch (drvden)
                {
                case 0: /*Double density (360 rpm)*/
                        disc_period = 26;
                        break;
                case 1: /*High density (360 rpm)*/
                        disc_period = 16;
                        break;
                case 2:
                        disc_period = 4;
                        break;
                }
        case 2: /*Double density*/
                disc_period = 32;
                break;
        case 3: /*Extended density*/
                disc_period = 8;
                break;
        }
}

void disc_reset()
{
        int drive;

        curdrive = 0;
        disc_period = 32;
        timer_add(&disc_poll_timer, disc_poll, NULL, 0);

        for (drive = 0; drive < 2; drive++)
        {
                if (loaders[driveloaders[drive]].close)
                        loaders[driveloaders[drive]].close(drive);
                drive_empty[drive] = 1;
                drives[drive].hole = NULL;
                drives[drive].poll = NULL;
                drives[drive].seek = NULL;
                drives[drive].readsector = NULL;
                drives[drive].writesector = NULL;
                drives[drive].readaddress = NULL;
                drives[drive].format = NULL;
        }
}

void disc_init()
{
        disc_reset();
}

int oldtrack[2] = { 0, 0 };
void disc_seek(int drive, int track)
{
//        pclog("disc_seek: drive=%i track=%i\n", drive, track);
        if (drive < 2 && drives[drive].seek)
                drives[drive].seek(drive, track);
//        if (track != oldtrack[drive])
//                fdc_discchange_clear(drive);
//        ddnoise_seek(track - oldtrack[drive]);
//        oldtrack[drive] = track;
}

void disc_readsector(int drive, int sector, int track, int side, int density, int sector_size)
{
        drive ^= fdd_swap;

        if (drive < 2 && drives[drive].readsector)
                drives[drive].readsector(drive, sector, track, side, density, sector_size);
        else
                disc_notfound = 1000;
}

void disc_writesector(int drive, int sector, int track, int side, int density, int sector_size)
{
        drive ^= fdd_swap;

        if (drive < 2 && drives[drive].writesector)
                drives[drive].writesector(drive, sector, track, side, density, sector_size);
        else
                disc_notfound = 1000;
}

void disc_readaddress(int drive, int track, int side, int density)
{
        drive ^= fdd_swap;

        if (drive < 2 && drives[drive].readaddress)
                drives[drive].readaddress(drive, track, side, density);
}

void disc_format(int drive, int track, int side, int density, uint8_t fill)
{
        drive ^= fdd_swap;

        if (drive < 2 && drives[drive].format)
                drives[drive].format(drive, track, side, density, fill);
        else
                disc_notfound = 1000;
}

void disc_stop(int drive)
{
        drive ^= fdd_swap;

        if (drive < 2 && drives[drive].stop)
                drives[drive].stop(drive);
}

void disc_set_drivesel(int drive)
{
        drive ^= fdd_swap;

        disc_drivesel = drive;
}

void disc_set_motor_enable(int motor_enable)
{
        if (motor_enable && !motoron)
                timer_set_delay_u64(&disc_poll_timer, disc_period * TIMER_USEC);
        else if (!motor_enable)
                timer_disable(&disc_poll_timer);
        motoron = motor_enable;
}
