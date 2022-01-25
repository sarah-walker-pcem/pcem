#ifndef _DISC_H_
#define _DISC_H_
typedef struct
{
        void (*seek)(int drive, int track);
        void (*readsector)(int drive, int sector, int track, int side, int density, int sector_size);
        void (*writesector)(int drive, int sector, int track, int side, int density, int sector_size);
        void (*readaddress)(int drive, int track, int side, int density);
        void (*format)(int drive, int track, int side, int density, uint8_t fill);
        int (*hole)(int drive);
        void (*stop)();
        void (*poll)();
} DRIVE;

extern DRIVE drives[2];

extern int curdrive;

void disc_load(int drive, char *fn);
void disc_new(int drive, char *fn);
void disc_close(int drive);
void disc_init();
void disc_reset();
void disc_poll();
void disc_seek(int drive, int track);
void disc_readsector(int drive, int sector, int track, int side, int density, int sector_size);
void disc_writesector(int drive, int sector, int track, int side, int density, int sector_size);
void disc_readaddress(int drive, int track, int side, int density);
void disc_format(int drive, int track, int side, int density, uint8_t fill);
int disc_hole(int drive);
void disc_stop(int drive);
int disc_empty(int drive);
void disc_set_rate(int drive, int drvden, int rate);
void disc_set_drivesel(int drive);
void disc_set_motor_enable(int motor_enable);
extern int disc_drivesel;

void fdc_callback();
int  fdc_data(uint8_t dat);
void fdc_spindown();
void fdc_finishread();
void fdc_notfound(int reason);
void fdc_datacrcerror();
void fdc_headercrcerror();
void fdc_writeprotect();
int  fdc_getdata(int last);
void fdc_sectorid(uint8_t track, uint8_t side, uint8_t sector, uint8_t size, uint8_t crc1, uint8_t crc2);
void fdc_indexpulse();
/*extern int fdc_ready;
extern int fdc_indexcount;*/

extern int motorspin;
extern int motoron;

extern int defaultwriteprot;
//extern char discfns[4][260];

extern int writeprot[2], fwriteprot[2];
extern int disc_track[2];
extern int disc_changed[2];
extern int drive_empty[2];
extern int drive_type[2];

/*Used in the Read A Track command. Only valid for disc_readsector(). */
#define SECTOR_FIRST -2
#define SECTOR_NEXT  -1


#endif /* _DISC_H_ */
