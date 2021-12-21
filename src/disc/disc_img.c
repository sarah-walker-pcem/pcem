#include "ibm.h"
#include "fdd.h"
#include "disc.h"
#include "disc_img.h"
#include "disc_sector.h"

static struct
{
        FILE *f;
        uint8_t track_data[2][20*1024];
        int sectors, tracks, sides;
        int sector_size;
        int rate;
	int xdf_type;	/* 0 = not XDF, 1-5 = one of the five XDF types */
	int hole;
        double bitcell_period_300rpm;
} img[2];

static uint8_t xdf_track0[5][3];
static uint8_t xdf_spt[5];
static uint8_t xdf_map[5][24][3];

int bpb_disable;

void img_writeback(int drive, int track);

static int img_sector_size_code(int drive)
{
	switch(img[drive].sector_size)
	{
		case 128:
			return 0;
		case 256:
			return 1;
		default:
		case 512:
			return 2;
		case 1024:
			return 3;
		case 2048:
			return 4;
		case 4096:
			return 5;
		case 8192:
			return 6;
		case 16384:
			return 7;
	}
}

void img_init()
{
        memset(img, 0, sizeof(img));
//        adl[0] = adl[1] = 0;
}

static void add_to_map(uint8_t *arr, uint8_t p1, uint8_t p2, uint8_t p3)
{
	arr[0] = p1;
	arr[1] = p2;
	arr[2] = p3;
}

static int xdf_maps_initialized = 0;

static void initialize_xdf_maps()
{
	// XDF 5.25" 2HD
	/* Adds, in this order: sectors per FAT, sectors per each side of track 0, difference between that and virtual sector number specified in BPB. */
	add_to_map(xdf_track0[0], 9, 17, 2);
	xdf_spt[0] = 3;
	/* Adds, in this order: side, sequential order (not used in PCem), sector size. */
	add_to_map(xdf_map[0][0], 0, 0, 3);
	add_to_map(xdf_map[0][1], 0, 2, 6);
	add_to_map(xdf_map[0][2], 1, 0, 2);
	add_to_map(xdf_map[0][3], 0, 1, 2);
	add_to_map(xdf_map[0][4], 1, 2, 6);
	add_to_map(xdf_map[0][5], 1, 1, 3);

	// XDF 3.5" 2HD
	add_to_map(xdf_track0[1], 11, 19, 4);
	xdf_spt[1] = 4;
	add_to_map(xdf_map[1][0], 0, 0, 3);
	add_to_map(xdf_map[1][1], 0, 2, 4);
	add_to_map(xdf_map[1][2], 1, 3, 6);
	add_to_map(xdf_map[1][3], 0, 1, 2);
	add_to_map(xdf_map[1][4], 1, 1, 2);
	add_to_map(xdf_map[1][5], 0, 3, 6);
	add_to_map(xdf_map[1][6], 1, 0, 4);
	add_to_map(xdf_map[1][7], 1, 2, 3);

	// XDF 3.5" 2ED
	add_to_map(xdf_track0[2], 22, 37, 9);
	xdf_spt[2] = 4;
	add_to_map(xdf_map[2][0], 0, 0, 3);
	add_to_map(xdf_map[2][1], 0, 1, 4);
	add_to_map(xdf_map[2][2], 0, 2, 5);
	add_to_map(xdf_map[2][3], 0, 3, 7);
	add_to_map(xdf_map[2][4], 1, 0, 3);
	add_to_map(xdf_map[2][5], 1, 1, 4);
	add_to_map(xdf_map[2][6], 1, 2, 5);
	add_to_map(xdf_map[2][7], 1, 3, 7);

	// XXDF 3.5" 2HD
	add_to_map(xdf_track0[3], 12, 20, 4);
	xdf_spt[3] = 2;
	add_to_map(xdf_map[3][0], 0, 0, 5);
	add_to_map(xdf_map[3][1], 1, 1, 6);
	add_to_map(xdf_map[3][2], 0, 1, 6);
	add_to_map(xdf_map[3][3], 1, 0, 5);

	// XXDF 3.5" 2ED
	add_to_map(xdf_track0[4], 21, 39, 9);
	xdf_spt[4] = 2;
	add_to_map(xdf_map[4][0], 0, 0, 6);
	add_to_map(xdf_map[4][1], 1, 1, 7);
	add_to_map(xdf_map[4][2], 0, 1, 7);
	add_to_map(xdf_map[4][3], 1, 0, 6);

	xdf_maps_initialized = 1;
}

void img_load(int drive, char *fn)
{
        int size;
        double bit_rate_300;
	uint16_t bpb_bps;
	uint16_t bpb_total;
	uint8_t bpb_mid;	/* Media type ID. */
	uint8_t bpb_sectors;
	uint8_t bpb_sides;
	uint32_t bpt;
	uint8_t max_spt = 0;	/* Used for XDF detection. */

	if (!xdf_maps_initialized)  initialize_xdf_maps();	/* Initialize XDF maps, will need them to properly register sectors in tracks. */
        
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

	/* Read the BPB */
	fseek(img[drive].f, 0x0B, SEEK_SET);
	fread(&bpb_bps, 1, 2, img[drive].f);
	fseek(img[drive].f, 0x13, SEEK_SET);
	fread(&bpb_total, 1, 2, img[drive].f);
	fseek(img[drive].f, 0x15, SEEK_SET);
	bpb_mid = fgetc(img[drive].f);
	fseek(img[drive].f, 0x18, SEEK_SET);
	bpb_sectors = fgetc(img[drive].f);
	fseek(img[drive].f, 0x1A, SEEK_SET);
	bpb_sides = fgetc(img[drive].f);

        fseek(img[drive].f, -1, SEEK_END);
        size = ftell(img[drive].f) + 1;

        img[drive].sides = 2;
        img[drive].sector_size = 512;

	pclog("BPB reports %i sides and %i bytes per sector\n", bpb_sides, bpb_bps);

	if (bpb_disable || (bpb_sides < 1) || (bpb_sides > 2) || (bpb_bps < 128) || (bpb_bps > 2048))
	{
		/* The BPB is giving us a wacky number of sides and/or bytes per sector, therefore it is most probably
		   not a BPB at all, so we have to guess the parameters from file size. */

		if (size <= (160*1024))        { img[drive].sectors = 8;  img[drive].tracks = 40; img[drive].sides = 1; bit_rate_300 = 250; }
	        else if (size <= (180*1024))   { img[drive].sectors = 9;  img[drive].tracks = 40; img[drive].sides = 1; bit_rate_300 = 250; }
	        else if (size <= (320*1024))   { img[drive].sectors = 8;  img[drive].tracks = 40; bit_rate_300 = 250; }
	        else if (size <= (360*1024))   { img[drive].sectors = 9;  img[drive].tracks = 40; bit_rate_300 = 250; } /*Double density*/
	        else if (size < (1024*1024))   { img[drive].sectors = 9;  img[drive].tracks = 80; bit_rate_300 = 250; } /*Double density*/
	        else if (size <= 1228800)      { img[drive].sectors = 15; img[drive].tracks = 80; bit_rate_300 = (500.0 * 300.0) / 360.0; } /*High density 1.2MB*/
	        else if (size <= (0x1A4000-1)) { img[drive].sectors = 18; img[drive].tracks = 80; bit_rate_300 = 500; } /*High density (not supported by Tandy 1000)*/
	        // else if (size == 1884160)      { img[drive].sectors = 23; img[drive].tracks = 80; bit_rate_300 = 500; } /*XDF format - used by OS/2 Warp*/
	        // else if (size == 1763328)      { img[drive].sectors = 21; img[drive].tracks = 82; bit_rate_300 = 500; } /*XDF format - used by OS/2 Warp*/
	        else if (size <= 2000000)   { img[drive].sectors = 21; img[drive].tracks = 80; bit_rate_300 = 500; } /*DMF format - used by Windows 95 - changed by OBattler to 2000000, ie. the real unformatted capacity @ 500 kbps and 300 rpm */
	        else                           { img[drive].sectors = 36; img[drive].tracks = 80; bit_rate_300 = 1000; } /*E density*/

		img[drive].xdf_type = 0;
	}
	else
	{
		/* The BPB readings appear to be valid, so let's set the values. */
		/* Number of tracks = number of total sectors divided by sides times sectors per track. */
		img[drive].tracks = ((uint32_t) bpb_total) / (((uint32_t) bpb_sides) * ((uint32_t) bpb_sectors));
		/* The rest we just set directly from the BPB. */
		img[drive].sectors = bpb_sectors;
		img[drive].sides = bpb_sides;
		/* Now we calculate bytes per track, which is bpb_sectors * bpb_bps. */
		bpt = (uint32_t) bpb_sectors * (uint32_t) bpb_bps;
		/* Now we should be able to calculate the bit rate. */
		pclog("The image has %i bytes per track\n", bpt);
		if (bpt <= 6250)
			bit_rate_300 = 250;		/* Double-density */
		else if (bpt <= 7500)
			bit_rate_300 = 300;		/* Double-density, 300 kbps @ 300 rpm */
		else if (bpt <= 10416)
		{
			bit_rate_300 = (bpb_mid == 0xF0) ? 500 : ((500.0 * 300.0) / 360.0);		/* High-density @ 300 or 360 rpm, depending on media type ID */
			max_spt = (bpb_mid == 0xF0) ? 22 : 18;
		}
		else if (bpt <= 12500)			/* High-density @ 300 rpm */
		{
			bit_rate_300 = 500;
			max_spt = 22;
		}
		else if (bpt <= 25000)			/* Extended density @ 300 rpm */
		{
			bit_rate_300 = 1000;
			max_spt = 45;
		}
		else					/* Image too big, eject */
		{
			pclog("Image has more than 25000 bytes per track, ejecting...\n");
			fclose(img[drive].f);
			return;
		}

		if (bpb_bps == 512)			/* BPB reports 512 bytes per sector, let's see if it's XDF or not */
		{
			if (bit_rate_300 <= 300)	/* Double-density disk, not XDF */
			{
				img[drive].xdf_type = 0;
			}
			else
			{
				if (bpb_sectors > max_spt)
				{
					switch(bpb_sectors)
					{
						case 19:	/* High density XDF @ 360 rpm */
							img[drive].xdf_type = 1;
							break;
						case 23:	/* High density XDF @ 300 rpm */
							img[drive].xdf_type = 2;
							break;
						case 24:	/* High density XXDF @ 300 rpm */
							img[drive].xdf_type = 4;
							break;
						case 46:	/* Extended density XDF */
							img[drive].xdf_type = 3;
							break;
						case 48:	/* Extended density XXDF */
							img[drive].xdf_type = 5;
							break;
						default:	/* Unknown, as we're beyond maximum sectors, get out */
							fclose(img[drive].f);
							return;
					}
				}
				else			/* Amount of sectors per track that fits into a track, therefore not XDF */
				{
					img[drive].xdf_type = 0;
				}
			}
		}
		else					/* BPB reports sector size other than 512, can't possibly be XDF */
		{
			img[drive].xdf_type = 0;
		}
	}

	if ((bit_rate_300 == 250) || (bit_rate_300 == 300))
	{
		img[drive].hole = 0;
	}
	else if (bit_rate_300 == 1000)
	{
		img[drive].hole = 2;
	}
	else
	{
		img[drive].hole = 1;
	}

	if (img[drive].xdf_type)			/* In case of XDF-formatted image, write-protect */
	{
                writeprot[drive] = 1;
	        fwriteprot[drive] = writeprot[drive];
	}

        drives[drive].seek        = img_seek;
        drives[drive].readsector  = disc_sector_readsector;
        drives[drive].writesector = disc_sector_writesector;
        drives[drive].readaddress = disc_sector_readaddress;
        drives[drive].hole        = img_hole;
        drives[drive].poll        = disc_sector_poll;
        drives[drive].format      = disc_sector_format;
        disc_sector_writeback[drive] = img_writeback;
        
        img[drive].bitcell_period_300rpm = 1000000.0 / bit_rate_300*2.0;
        pclog("bit_rate_300=%g\n", bit_rate_300);
        pclog("bitcell_period_300=%g\n", img[drive].bitcell_period_300rpm);
//        img[drive].bitcell_period_300rpm = disc_get_bitcell_period(img[drive].rate);
        pclog("img_load %d %p sectors=%i tracks=%i sides=%i sector_size=%i hole=%i\n", drive, drives, img[drive].sectors, img[drive].tracks, img[drive].sides, img[drive].sector_size, img[drive].hole);
}

int img_hole(int drive)
{
	return img[drive].hole;
}

void img_close(int drive)
{
        if (img[drive].f)
                fclose(img[drive].f);
        img[drive].f = NULL;
}

void img_seek(int drive, int track)
{
        int side;
	int current_xdft = img[drive].xdf_type - 1;

	uint8_t sectors_fat, effective_sectors;		/* Needed for XDF */
        
        if (!img[drive].f)
                return;
        pclog("Seek drive=%i track=%i sectors=%i sector_size=%i sides=%i\n", drive, track, img[drive].sectors,img[drive].sector_size, img[drive].sides);
//        pclog("  %i %i\n", drive_type[drive], img[drive].tracks);
        if (img[drive].tracks <= 41 && fdd_doublestep_40(drive))
                track /= 2;

	pclog("Disk seeked to track %i\n", track);
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
        
        disc_sector_reset(drive, 0);
        disc_sector_reset(drive, 1);
        
	int sector, current_pos;

	if (img[drive].xdf_type)
	{
		sectors_fat = xdf_track0[current_xdft][0];
		effective_sectors = xdf_track0[current_xdft][1];
//		sector_gap = xdf_track0[current_xdft][2];

		if (!track)
		{
			/* Track 0, register sectors according to track 0 map. */
			/* First, the "Side 0" buffer, will also contain one sector from side 1. */
			current_pos = 0;
	                for (sector = 0; sector < sectors_fat; sector++)
			{
	                        disc_sector_add(drive, 0, track, 0, sector+0x81, 2,
       		                                img[drive].bitcell_period_300rpm, 
               		                        &img[drive].track_data[0][current_pos]);
				current_pos += 512;
			}
                        disc_sector_add(drive, 1, track, 1, 0x81, 2,
				img[drive].bitcell_period_300rpm, 
				&img[drive].track_data[0][current_pos]);
			current_pos += 512;
	                for (sector = 0; sector < 8; sector++)
			{
	                        disc_sector_add(drive, 0, track, 0, sector+1, 2,
       		                                img[drive].bitcell_period_300rpm, 
               		                        &img[drive].track_data[0][current_pos]);
				current_pos += 512;
			}
			/* Now the "Side 1" buffer, will also contain one sector from side 0. */
			current_pos = 0;
			for (sector = 0; sector < 14; sector++)
			{
	                        disc_sector_add(drive, 1, track, 1, sector+0x82, 2,
       		                                img[drive].bitcell_period_300rpm, 
               		                        &img[drive].track_data[1][current_pos]);
				current_pos += 512;
			}
			current_pos += (5 * 512);
			for (; sector < effective_sectors-1; sector++)
			{
	                        disc_sector_add(drive, 1, track, 1, sector+0x82, 2,
       		                                img[drive].bitcell_period_300rpm, 
               		                        &img[drive].track_data[1][current_pos]);
				current_pos += 512;
			}
			current_pos += 512;
		}
		else
		{
			/* Non-zero track, this will have sectors of various sizes. */
			/* First, the "Side 0" buffer. */
			current_pos = 0;
	                for (sector = 0; sector < xdf_spt[current_xdft]; sector++)
			{
	                        disc_sector_add(drive, xdf_map[current_xdft][sector][0], track, xdf_map[current_xdft][sector][0],
						xdf_map[current_xdft][sector][2] + 0x80, xdf_map[current_xdft][sector][2],
       		                                img[drive].bitcell_period_300rpm, 
               		                        &img[drive].track_data[0][current_pos]);
				current_pos += (128 << xdf_map[current_xdft][sector][2]);
			}
			/* Then, the "Side 1" buffer. */
			current_pos = 0;
	                for (sector = xdf_spt[current_xdft]; sector < (xdf_spt[current_xdft] << 1); sector++)
			{
	                        disc_sector_add(drive, xdf_map[current_xdft][sector][0], track, xdf_map[current_xdft][sector][0],
						xdf_map[current_xdft][sector][2] + 0x80, xdf_map[current_xdft][sector][2],
       		                                img[drive].bitcell_period_300rpm, 
               		                        &img[drive].track_data[1][current_pos]);
				current_pos += (128 << xdf_map[current_xdft][sector][2]);
			}
		}
	}
	else
	{                
	        for (side = 0; side < img[drive].sides; side++)
       		{
	                for (sector = 0; sector < img[drive].sectors; sector++)
        	                disc_sector_add(drive, side, track, side, sector+1, img_sector_size_code(drive),
       	        	                        img[drive].bitcell_period_300rpm, 
               	        	                &img[drive].track_data[side][sector * img[drive].sector_size]);
		}
	}
}
void img_writeback(int drive, int track)
{
        if (!img[drive].f)
                return;
                
        if (img[drive].xdf_type)
                return; /*Should never happen*/

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

