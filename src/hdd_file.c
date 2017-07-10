#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _GNU_SOURCE
#include <errno.h>

#include "ibm.h"
#include "hdd_file.h"

void hdd_load(hdd_file_t *hdd, int d, const char *fn)
{
	if (hdd->f == NULL)
        {
		/* Try to open existing hard disk image */
		hdd->f = fopen64(fn, "rb+");
		if (hdd->f == NULL)
                {
			/* Failed to open existing hard disk image */
			if (errno == ENOENT)
                        {
				/* Failed because it does not exist,
				   so try to create new file */
				hdd->f = fopen64(fn, "wb+");
				if (hdd->f == NULL)
                                {
					pclog("Cannot create file '%s': %s",
					      fn, strerror(errno));
					return;
				}
			}
                        else
                        {
				/* Failed for another reason */
				pclog("Cannot open file '%s': %s",
				      fn, strerror(errno));
				return;
			}
		}
	}

        hdd->spt = hdc[d].spt;
        hdd->hpc = hdc[d].hpc;
        hdd->tracks = hdc[d].tracks;
        hdd->sectors = hdc[d].spt * hdc[d].hpc * hdc[d].tracks;
}

void hdd_close(hdd_file_t *hdd)
{
        if (hdd->f)
        {
                fclose(hdd->f);
                hdd->f = NULL;
        }
}

int hdd_read_sectors(hdd_file_t *hdd, int offset, int nr_sectors, void *buffer)
{
        off64_t addr;
        int transfer_sectors = nr_sectors;

        if ((hdd->sectors - offset) < transfer_sectors)
                transfer_sectors = hdd->sectors - offset;
        addr = (uint64_t)offset * 512;

        fseeko64(hdd->f, addr, SEEK_SET);
        fread(buffer, transfer_sectors*512, 1, hdd->f);
        
        if (nr_sectors != transfer_sectors)
                return 1;
        return 0;
}

int hdd_write_sectors(hdd_file_t *hdd, int offset, int nr_sectors, void *buffer)
{
        off64_t addr;
        int transfer_sectors = nr_sectors;

        if ((hdd->sectors - offset) < transfer_sectors)
                transfer_sectors = hdd->sectors - offset;
        addr = (uint64_t)offset * 512;

        fseeko64(hdd->f, addr, SEEK_SET);
        fwrite(buffer, transfer_sectors*512, 1, hdd->f);
        
        if (nr_sectors != transfer_sectors)
                return 1;
        return 0;
}

int hdd_format_sectors(hdd_file_t *hdd, int offset, int nr_sectors)
{
        off64_t addr;
        int c;
        uint8_t zero_buffer[512];
        int transfer_sectors = nr_sectors;
                
        memset(zero_buffer, 0, 512);
        
        if ((hdd->sectors - offset) < transfer_sectors)
                transfer_sectors = hdd->sectors - offset;
        addr = (uint64_t)offset * 512;
        fseeko64(hdd->f, addr, SEEK_SET);
        for (c = 0; c < transfer_sectors; c++)
                fwrite(zero_buffer, 512, 1, hdd->f);
        
        if (nr_sectors != transfer_sectors)
                return 1;
        return 0;
}
