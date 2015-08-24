#include "ibm.h"
#include "disc.h"
#include "disc_img.h"

static struct
{
        FILE *f;
        uint8_t track_data[2][20*1024];
        int sectors, tracks, sides;
        int sector_size;
        int rate;
} img[2];

//static FILE *img_f[2];
//static uint8_t trackinfoa[2][2][20*1024];
//static int img_dblside[2];
//static int img_sectors[2], img_size[2], img_trackc[2];
//static int img_dblstep[2];
//static int img_density[2];

static int img_sector,   img_track,   img_side,    img_drive;
static int img_inread,   img_readpos, img_inwrite, img_inreadaddr;
static int img_notfound;
static int img_rsector=0;
static int img_informat=0;

static int img_pause = 0;
static int img_index = 6250;

void img_init()
{
        memset(img, 0, sizeof(img));
//        adl[0] = adl[1] = 0;
        img_notfound = 0;
}

void img_load(int drive, char *fn)
{
        int size;
        
        writeprot[drive] = 0;
        img[drive].f = fopen(fn, "rb+");
        if (!img[drive].f)
        {
                img[drive].f = fopen(fn, "rb");
                if (!img[drive].f)
                        return;
                writeprot[drive] = 1;
        }
        fwriteprot[drive] = writeprot[drive];
        fseek(img[drive].f, -1, SEEK_END);
        size = ftell(img[drive].f) + 1;

        img[drive].sides = 2;
        img[drive].sector_size = 512;
        if (size <= (160*1024))        { img[drive].sectors = 8;  img[drive].tracks = 40; img[drive].sides = 1; img[drive].rate = 2; }
        else if (size <= (180*1024))   { img[drive].sectors = 9;  img[drive].tracks = 40; img[drive].sides = 1; img[drive].rate = 2; }
        else if (size <= (320*1024))   { img[drive].sectors = 8;  img[drive].tracks = 40; img[drive].rate = 2; }
        else if (size <= (360*1024))   { img[drive].sectors = 9;  img[drive].tracks = 40; img[drive].rate = 2; } /*Double density*/
        else if (size < (1024*1024))   { img[drive].sectors = 9;  img[drive].tracks = 80; img[drive].rate = 2; } /*Double density*/
        else if (size <= 1228800)      { img[drive].sectors = 15; img[drive].tracks = 80; img[drive].rate = 0; } /*High density 1.2MB*/
        else if (size <= (0x1A4000-1)) { img[drive].sectors = 18; img[drive].tracks = 80; img[drive].rate = 0; } /*High density (not supported by Tandy 1000)*/
        else if (size == 1884160)      { img[drive].sectors = 23; img[drive].tracks = 80; img[drive].rate = 0; } /*XDF format - used by OS/2 Warp*/
        else if (size == 1763328)      { img[drive].sectors = 21; img[drive].tracks = 82; img[drive].rate = 0; } /*XDF format - used by OS/2 Warp*/
        else if (size < (2048*1024))   { img[drive].sectors = 21; img[drive].tracks = 80; img[drive].rate = 0; } /*DMF format - used by Windows 95*/
        else                           { img[drive].sectors = 36; img[drive].tracks = 80; img[drive].rate = 3; } /*E density*/

        drives[drive].seek        = img_seek;
        drives[drive].readsector  = img_readsector;
        drives[drive].writesector = img_writesector;
        drives[drive].readaddress = img_readaddress;
        drives[drive].poll        = img_poll;
        drives[drive].format      = img_format;
//        pclog("img_load %d %p sectors=%i tracks=%i sides=%i sector_size=%i\n", drive, drives, img[drive].sectors, img[drive].tracks, img[drive].sides, img[drive].sector_size);
}


void img_close(int drive)
{
        if (img[drive].f)
                fclose(img[drive].f);
        img[drive].f = NULL;
}

void img_seek(int drive, int track)
{
        if (!img[drive].f)
                return;
//        pclog("Seek drive=%i track=%i sectors=%i sector_size=%i sides=%i\n", drive, track, img[drive].sectors,img[drive].sector_size, img[drive].sides);

        if (drive_type[drive] && img[drive].tracks == 40)
                track /= 2;

        disc_track[drive] = track;

        if (img[drive].sides == 2)
        {
                fseek(img[drive].f, track * img[drive].sectors * img[drive].sector_size * 2, SEEK_SET);
                fread(img[drive].track_data[0], img[drive].sectors * img[drive].sector_size, 1, img[drive].f);
                fread(img[drive].track_data[1], img[drive].sectors * img[drive].sector_size, 1, img[drive].f);
        }
        else
        {
                fseek(img[drive].f, track * img[drive].sectors * img[drive].sector_size, SEEK_SET);
                fread(img[drive].track_data[0], img[drive].sectors * img[drive].sector_size, 1, img[drive].f);
        }
}
void img_writeback(int drive, int track)
{
        if (!img[drive].f)
                return;

        if (img[drive].sides == 2)
        {
                fseek(img[drive].f, track * img[drive].sectors * img[drive].sector_size * 2, SEEK_SET);
                fwrite(img[drive].track_data[0], img[drive].sectors * img[drive].sector_size, 1, img[drive].f);
                fwrite(img[drive].track_data[1], img[drive].sectors * img[drive].sector_size, 1, img[drive].f);
        }
        else
        {
                fseek(img[drive].f, track * img[drive].sectors * img[drive].sector_size, SEEK_SET);
                fwrite(img[drive].track_data[0], img[drive].sectors * img[drive].sector_size, 1, img[drive].f);
        }
}

void img_readsector(int drive, int sector, int track, int side, int rate)
{
        if (drive_type[drive] && rate == 1)
                rate = 2;

        img_sector = sector - 1;
        img_track  = track;
        img_side   = side;
        img_drive  = drive;
//        pclog("imgS Read sector drive=%i side=%i track=%i sector=%i rate=%i\n",drive,side,track,sector, rate);

        if (!img[drive].f || (side && img[drive].sides == 1) ||
            (rate != img[drive].rate) || (track != disc_track[drive]) ||
            sector > img[drive].sectors)
        {
//                pclog("Sector not found rate %i,%i track %i,%i\n", rate, img[drive].rate, track, disc_track[drive]);
                img_notfound=500;
                return;
        }
//        printf("Found\n");
        img_inread  = 1;
        img_readpos = 0;
        img_pause = 32;
}

void img_writesector(int drive, int sector, int track, int side, int rate)
{
        if (drive_type[drive] && rate == 1)
                rate = 2;

//        if (imgdblstep[drive]) track/=2;
        img_sector = sector - 1;
        img_track  = track;
        img_side   = side;
        img_drive  = drive;
//        printf("imgS Write sector %i %i %i %i\n",drive,side,track,sector);

        if (!img[drive].f || (side && img[drive].sides == 1) ||
            (rate != img[drive].rate) || (track != disc_track[drive]) ||
            sector > img[drive].sectors)
        {
                img_notfound = 500;
                return;
        }
        img_inwrite = 1;
        img_readpos = 0;
        img_pause = 32;
}

void img_readaddress(int drive, int track, int side, int rate)
{
        if (drive_type[drive] && rate == 1)
                rate = 2;

        img_drive = drive;
        img_track = track;
        img_side  = side;
//        pclog("Read address %i %i %i  %i\n",drive,side,track, rate);

        if (!img[drive].f || (side && img[drive].sides == 1) ||
            (rate != img[drive].rate) || (track != disc_track[drive]))
        {
//                pclog("Address not found rate %i,%i track %i,%i\n", rate, img[drive].rate, track, disc_track[drive]);
                img_notfound=500;
                return;
        }
        img_inreadaddr = 1;
        img_readpos    = 0;
        img_pause = 100;//500;
        img_pause = 32;
}

void img_format(int drive, int track, int side, int rate)
{
        if (drive_type[drive] && rate == 1)
                rate = 2;

        img_drive = drive;
        img_track = track;
        img_side  = side;

        if (!img[drive].f || (side && img[drive].sides == 1) ||
            (rate != img[drive].rate) || (track != disc_track[drive]))
        {
                img_notfound = 500;
                return;
        }
        img_sector  = 0;
        img_readpos = 0;
        img_informat = 1;
        img_pause = 32;
}

void img_stop()
{
        img_pause = img_notfound = img_inread = img_inwrite = img_inreadaddr = img_informat = 0;
}

void img_poll()
{
//        pclog("img_poll %i %i %p\n", img_inread, img_readpos, img[img_drive].f);
        img_index--;
        if (!img_index)
        {
                img_index = 6250;
                fdc_indexpulse();
        }
        
        if (img_pause)
        {
                img_pause--;
                if (img_pause)
                   return;
        }

        if (img_notfound)
        {
                img_notfound--;
                if (!img_notfound)
                {
//                        pclog("Not found!\n");
                        fdc_notfound();
                }
        }
        if (img_inread && img[img_drive].f)
        {
//                pclog("Read pos %i\n", img_readpos);
//                if (!imgreadpos) pclog("%i\n",imgsector*imgsize[imgdrive]);
                if (fdc_data(img[img_drive].track_data[img_side][(img_sector * img[img_drive].sector_size) + img_readpos]))
                        return;

                img_readpos++;
                if (img_readpos == img[img_drive].sector_size)
                {
//                        pclog("Read %i bytes\n",img_readpos);
                        img_inread = 0;
                        fdc_finishread();
                }
        }
        if (img_inwrite && img[img_drive].f)
        {
                int data;
                
                if (writeprot[img_drive])
                {
//                        pclog("writeprotect\n");
                        fdc_writeprotect();
                        img_inwrite = 0;
                        return;
                }
//                pclog("Write data %i\n",img_readpos);
                data = fdc_getdata(img_readpos == (img[img_drive].sector_size - 1));
                if (data == -1)
                          return;

                img[img_drive].track_data[img_side][(img_sector * img[img_drive].sector_size) + img_readpos] = data;
                img_readpos++;
                if (img_readpos == img[img_drive].sector_size)
                {
//                        pclog("write over\n");
                        img_inwrite = 0;
                        fdc_finishread();
                        img_writeback(img_drive, img_track);
                }
        }
        if (img_inreadaddr && img[img_drive].f)
        {
//                pclog("img_inreadaddr %08X\n", fdc_sectorid);
                fdc_sectorid(img_track, img_side,
                                img_rsector + ((img[img_drive].sector_size == 512) ? 1 : 0), (img[img_drive].sector_size == 256) ? 1 : ((img[img_drive].sector_size == 512) ? 2 : 3), 0, 0);
                img_inreadaddr = 0;
                img_rsector++;
                if (img_rsector == img[img_drive].sectors)
                {
                        img_rsector=0;
//                        pclog("img_rsector reset\n");
                }
        }
        if (img_informat && img[img_drive].f)
        {
                if (writeprot[img_drive])
                {
                        fdc_writeprotect();
                        img_informat = 0;
                        return;
                }
                img[img_drive].track_data[img_side][(img_sector * img[img_drive].sector_size) + img_readpos] = 0;
                img_readpos++;
                if (img_readpos == img[img_drive].sector_size)
                {
                        img_readpos = 0;
                        img_sector++;
                        if (img_sector == img[img_drive].sectors)
                        {
                                img_informat = 0;
                                fdc_finishread();
                                img_writeback(img_drive, img_track);
                        }
                }
        }
}
