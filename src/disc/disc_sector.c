#include "ibm.h"
#include "disc.h"
#include "disc_sector.h"
#include "fdc.h"
#include "fdd.h"

/*Handling for 'sector based' image formats (like .IMG) as opposed to 'stream based' formats (eg .FDI)*/

#define MAX_SECTORS 256

typedef struct
{
        uint8_t c, h, r, n;
        int rate;
        uint8_t *data;
} sector_t;

static sector_t disc_sector_data[2][2][MAX_SECTORS];
static int disc_sector_count[2][2];
void (*disc_sector_writeback[2])(int drive, int track);

enum
{
        STATE_IDLE,
        STATE_READ_FIND_SECTOR,
        STATE_READ_SECTOR,
        STATE_READ_FIND_FIRST_SECTOR,
        STATE_READ_FIRST_SECTOR,
        STATE_READ_FIND_NEXT_SECTOR,
        STATE_READ_NEXT_SECTOR,
        STATE_WRITE_FIND_SECTOR,
        STATE_WRITE_SECTOR,
        STATE_READ_FIND_ADDRESS,
        STATE_READ_ADDRESS,
        STATE_FORMAT_FIND,
        STATE_FORMAT
};

static int disc_sector_state;
static int disc_sector_track;
static int disc_sector_side;
static int disc_sector_drive;
static int disc_sector_sector;
static int disc_sector_n;
static int disc_intersector_delay = 0;
static uint8_t disc_sector_fill;
static int cur_sector, cur_byte;
static int index_count;

static int disc_sector_status;

void disc_sector_reset(int drive, int side)
{
        disc_sector_count[drive][side] = 0;
}

void disc_sector_add(int drive, int side, uint8_t c, uint8_t h, uint8_t r, uint8_t n, int rate, uint8_t *data)
{
        sector_t *s = &disc_sector_data[drive][side][disc_sector_count[drive][side]];
//pclog("disc_sector_add: drive=%i side=%i %i r=%i\n", drive, side,         disc_sector_count[drive][side],r );
        if (disc_sector_count[drive][side] >= MAX_SECTORS)
                return;

        s->c = c;
        s->h = h;
        s->r = r;
        s->n = n;
        s->rate = rate;
        s->data = data;
        
        disc_sector_count[drive][side]++;
}

static int get_bitcell_period()
{
        return (disc_sector_data[disc_sector_drive][disc_sector_side][cur_sector].rate * 300) / fdd_getrpm(disc_sector_drive);
}

void disc_sector_readsector(int drive, int sector, int track, int side, int rate, int sector_size)
{
//        pclog("disc_sector_readsector: fdc_period=%i img_period=%i rate=%i sector=%i track=%i side=%i\n", fdc_get_bitcell_period(), get_bitcell_period(), rate, sector, track, side);

        if (sector == SECTOR_FIRST)
                disc_sector_state = STATE_READ_FIND_FIRST_SECTOR;
        else if (sector == SECTOR_NEXT)
                disc_sector_state = STATE_READ_FIND_NEXT_SECTOR;
        else
                disc_sector_state = STATE_READ_FIND_SECTOR;
        disc_sector_track = track;
        disc_sector_side  = side;
        disc_sector_drive = drive;
        disc_sector_sector = sector;
	disc_sector_n = sector_size;
        index_count = 0;
        
        disc_sector_status = FDC_STATUS_AM_NOT_FOUND;
}

void disc_sector_writesector(int drive, int sector, int track, int side, int rate, int sector_size)
{
//        pclog("disc_sector_writesector: fdc_period=%i img_period=%i rate=%i\n", fdc_get_bitcell_period(), get_bitcell_period(), rate);

        disc_sector_state = STATE_WRITE_FIND_SECTOR;
        disc_sector_track = track;
        disc_sector_side  = side;
        disc_sector_drive = drive;
        disc_sector_sector = sector;
	disc_sector_n = sector_size;
        index_count = 0;

        disc_sector_status = FDC_STATUS_AM_NOT_FOUND;
}

void disc_sector_readaddress(int drive, int track, int side, int rate)
{
//        pclog("disc_sector_readaddress: fdc_period=%i img_period=%i rate=%i track=%i side=%i\n", fdc_get_bitcell_period(), get_bitcell_period(), rate, track, side);

        disc_sector_state = STATE_READ_FIND_ADDRESS;
        disc_sector_track = track;
        disc_sector_side  = side;
        disc_sector_drive = drive;
        index_count = 0;

        disc_sector_status = FDC_STATUS_AM_NOT_FOUND;
}

void disc_sector_format(int drive, int track, int side, int rate, uint8_t fill)
{
        disc_sector_state = STATE_FORMAT_FIND;
        disc_sector_track = track;
        disc_sector_side  = side;
        disc_sector_drive = drive;
        disc_sector_fill  = fill;
        index_count = 0;
}

void disc_sector_stop()
{
        disc_sector_state = STATE_IDLE;
}

static void advance_byte()
{
        if (disc_intersector_delay)
        {
                disc_intersector_delay--;
                return;
        }
        cur_byte++;
        if (cur_byte >= (128 << disc_sector_data[disc_sector_drive][disc_sector_side][cur_sector].n))
        {
                cur_byte = 0;
                cur_sector++;
                if (cur_sector >= disc_sector_count[disc_sector_drive][disc_sector_side])
                {
                        cur_sector = 0;
                        fdc_indexpulse();
                        index_count++;
                }
                disc_intersector_delay = 40;
        }
}

void disc_sector_poll()
{
        sector_t *s;
        int data;

        if (cur_sector >= disc_sector_count[disc_sector_drive][disc_sector_side])
                cur_sector = 0;
        if (cur_byte >= (128 << disc_sector_data[disc_sector_drive][disc_sector_side][cur_sector].n))
                cur_byte = 0;

        s = &disc_sector_data[disc_sector_drive][disc_sector_side][cur_sector];
        switch (disc_sector_state)
        {
                case STATE_IDLE:
                break;
                
                case STATE_READ_FIND_SECTOR:
/*                pclog("STATE_READ_FIND_SECTOR: cur_sector=%i cur_byte=%i sector=%i,%i side=%i,%i track=%i,%i period=%i,%i\n",
                        cur_sector, cur_byte,
                        disc_sector_sector, s->r,
                        disc_sector_side, s->h,
                        disc_sector_track, s->c,
                        fdc_get_bitcell_period(), get_bitcell_period());*/
                if (index_count > 1)
                {
//                        pclog("Find sector not found\n");
                        fdc_notfound(disc_sector_status);
                        disc_sector_state = STATE_IDLE;
                        break;
                }
/*                pclog("%i %i %i %i %i\n", cur_byte, disc_sector_track != s->c,
                    disc_sector_side != s->h,
                    disc_sector_sector != s->r,
                    fdc_get_bitcell_period() != get_bitcell_period());*/
                if (!cur_byte && fdc_get_bitcell_period() == get_bitcell_period() &&
                    fdd_can_read_medium(disc_sector_drive ^ fdd_swap))
                        disc_sector_status = FDC_STATUS_NOT_FOUND; /*Disc readable, assume address marker found*/
                    
                if (cur_byte ||
                    fdc_get_bitcell_period() != get_bitcell_period() ||
		    !fdd_can_read_medium(disc_sector_drive ^ fdd_swap) ||
                    disc_intersector_delay)
                {
                        advance_byte();
                        break;
                }
                if (disc_sector_track != s->c ||
                    disc_sector_side != s->h ||
                    disc_sector_sector != s->r ||
		    disc_sector_n != s->n)
                {
                        if (disc_sector_track != s->c)
                                disc_sector_status = (disc_sector_track == 0xff) ? FDC_STATUS_BAD_CYLINDER : FDC_STATUS_WRONG_CYLINDER;
                        advance_byte();
                        break;
                }
                disc_sector_state = STATE_READ_SECTOR;
                case STATE_READ_SECTOR:
//                pclog("STATE_READ_SECTOR: cur_byte=%i %i\n", cur_byte, disc_intersector_delay);
                if (fdc_data(s->data[cur_byte]))
                {
//                        pclog("fdc_data failed\n");
                        return;
                }
                advance_byte();
                if (!cur_byte)
                {
                        disc_sector_state = STATE_IDLE;
                        fdc_finishread();
                }
                break;

                case STATE_READ_FIND_FIRST_SECTOR:
		if (!(fdd_can_read_medium(disc_sector_drive ^ fdd_swap)))
		{
//			pclog("Medium is of a density not supported by the drive\n");
                        fdc_notfound(FDC_STATUS_AM_NOT_FOUND);
                        disc_sector_state = STATE_IDLE;
                        break;
		}
                if (!cur_byte && fdc_get_bitcell_period() == get_bitcell_period() &&
                    fdd_can_read_medium(disc_sector_drive ^ fdd_swap))
                        disc_sector_status = FDC_STATUS_NOT_FOUND; /*Disc readable, assume address marker found*/

                if (cur_byte || !index_count || fdc_get_bitcell_period() != get_bitcell_period() ||
                    disc_intersector_delay)
                {
                        advance_byte();
                        break;
                }
                disc_sector_state = STATE_READ_FIRST_SECTOR;
                case STATE_READ_FIRST_SECTOR:
                if (fdc_data(s->data[cur_byte]))
                        return;
                advance_byte();
                if (!cur_byte)
                {
                        disc_sector_state = STATE_IDLE;
                        fdc_finishread();
                }
                break;

                case STATE_READ_FIND_NEXT_SECTOR:
		if (!(fdd_can_read_medium(disc_sector_drive ^ fdd_swap)))
		{
//			pclog("Medium is of a density not supported by the drive\n");
                        fdc_notfound(FDC_STATUS_AM_NOT_FOUND);
                        disc_sector_state = STATE_IDLE;
                        break;
		}
                if (index_count)
                {
//                        pclog("Find next sector hit end of track\n");
                        fdc_notfound(disc_sector_status);
                        disc_sector_state = STATE_IDLE;
                        break;
                }
                if (cur_byte || fdc_get_bitcell_period() != get_bitcell_period() ||
                    disc_intersector_delay)
                {
                        advance_byte();
                        break;
                }
                disc_sector_state = STATE_READ_NEXT_SECTOR;
                case STATE_READ_NEXT_SECTOR:
                if (fdc_data(s->data[cur_byte]))
                        break;
                advance_byte();
                if (!cur_byte)
                {
                        disc_sector_state = STATE_IDLE;
                        fdc_finishread();
                }
                break;

                case STATE_WRITE_FIND_SECTOR:
		if (!(fdd_can_read_medium(disc_sector_drive ^ fdd_swap)))
		{
//			pclog("Medium is of a density not supported by the drive\n");
                        fdc_notfound(FDC_STATUS_AM_NOT_FOUND);
                        disc_sector_state = STATE_IDLE;
                        break;
		}

                if (!cur_byte && fdc_get_bitcell_period() == get_bitcell_period() &&
                    fdd_can_read_medium(disc_sector_drive ^ fdd_swap))
                        disc_sector_status = FDC_STATUS_NOT_FOUND; /*Disc readable, assume address marker found*/

                if (writeprot[disc_sector_drive])
                {
                        fdc_writeprotect();
                        disc_sector_state = STATE_IDLE;
                        return;
                }
                if (index_count > 1)
                {
//                        pclog("Write find sector not found\n");
                        fdc_notfound(disc_sector_status);
                        disc_sector_state = STATE_IDLE;
                        break;
                }
                if (cur_byte ||
                    fdc_get_bitcell_period() != get_bitcell_period() ||
                    disc_intersector_delay)
                {
                        advance_byte();
                        break;
                }
                if (disc_sector_track != s->c ||
                    disc_sector_side != s->h ||
                    disc_sector_sector != s->r ||
		    disc_sector_n != s->n)
                {
                        if (disc_sector_track != s->c)
                                disc_sector_status = (disc_sector_track == 0xff) ? FDC_STATUS_BAD_CYLINDER : FDC_STATUS_WRONG_CYLINDER;
                        advance_byte();
                        break;
                }
                disc_sector_state = STATE_WRITE_SECTOR;
                case STATE_WRITE_SECTOR:
                data = fdc_getdata(cur_byte == ((128 << s->n) - 1));
                if (data == -1)
                        break;
                s->data[cur_byte] = data;
                advance_byte();
                if (!cur_byte)
                {
                        disc_sector_state = STATE_IDLE;
                        disc_sector_writeback[disc_sector_drive](disc_sector_drive, disc_sector_track);
                        fdc_finishread();
                }
                break;

                case STATE_READ_FIND_ADDRESS:
		if (!(fdd_can_read_medium(disc_sector_drive ^ fdd_swap)))
		{
//			pclog("Medium is of a density not supported by the drive\n");
                        fdc_notfound(FDC_STATUS_AM_NOT_FOUND);
                        disc_sector_state = STATE_IDLE;
                        break;
		}

                if (!cur_byte && fdc_get_bitcell_period() == get_bitcell_period() &&
                    fdd_can_read_medium(disc_sector_drive ^ fdd_swap))
                        disc_sector_status = FDC_STATUS_NOT_FOUND; /*Disc readable, assume address marker found*/

                if (index_count)
                {
//                        pclog("Find next sector hit end of track\n");
                        fdc_notfound(disc_sector_status);
                        disc_sector_state = STATE_IDLE;
                        break;
                }
                if (cur_byte || fdc_get_bitcell_period() != get_bitcell_period() ||
                    disc_intersector_delay)
                {
                        advance_byte();
                        break;
                }
                disc_sector_state = STATE_READ_ADDRESS;
                case STATE_READ_ADDRESS:
                fdc_sectorid(s->c, s->h, s->r, s->n, 0, 0);
                advance_byte();
                disc_sector_state = STATE_IDLE;
                break;
                
                case STATE_FORMAT_FIND:
                if (writeprot[disc_sector_drive])
                {
                        fdc_writeprotect();
                        disc_sector_state = STATE_IDLE;
                        return;
                }
                if (!index_count || fdc_get_bitcell_period() != get_bitcell_period() ||
                    disc_intersector_delay)
                {
                        advance_byte();
                        break;
                }
		if (!(fdd_can_read_medium(disc_sector_drive ^ fdd_swap)))
		{
//			pclog("Medium is of a density not supported by the drive\n");
                        fdc_notfound(FDC_STATUS_NOT_FOUND);
                        disc_sector_state = STATE_IDLE;
                        break;
		}
                if (fdc_get_bitcell_period() != get_bitcell_period())
                {
                        fdc_notfound(FDC_STATUS_NOT_FOUND);
                        disc_sector_state = STATE_IDLE;
                        break;
                }
                disc_sector_state = STATE_FORMAT;
                case STATE_FORMAT:
                if (!disc_intersector_delay && fdc_get_bitcell_period() == get_bitcell_period())
                        s->data[cur_byte] = disc_sector_fill;
                advance_byte();
                if (index_count == 2)
                {
                        disc_sector_writeback[disc_sector_drive](disc_sector_drive, disc_sector_track);
                        fdc_finishread();
                        disc_sector_state = STATE_IDLE;
                }
                break;
        }
}
