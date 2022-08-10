#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _GNU_SOURCE
#include <errno.h>

#include "ibm.h"
#include "hdd_file.h"
#include "minivhd/minivhd.h"
#include "minivhd/minivhd_util.h"

void hdd_load_ext(hdd_file_t *hdd, const char *fn, int spt, int hpc, int tracks, int read_only) {
        if (hdd->f == NULL) {
                /* Try to open existing hard disk image */
                if (read_only)
                        hdd->f = (void *)fopen64(fn, "rb");
                else
                        hdd->f = (void *)fopen64(fn, "rb+");
                if (hdd->f != NULL) {
                        hdd->img_type = HDD_IMG_RAW;
                        /* Check if the file we opened is a VHD */
                        if (mvhd_file_is_vhd((FILE *)hdd->f)) {
                                int err;
                                fclose((FILE *)hdd->f);
                                MVHDMeta *vhdm = mvhd_open(fn, (bool)read_only, &err);
                                if (vhdm == NULL) {
                                        hdd->f = NULL;
                                        if (err == MVHD_ERR_FILE) {
                                                pclog("Cannot open VHD file '%s' : %s", fn, strerror(mvhd_errno));
                                        } else {
                                                pclog("Cannot open VHD file '%s' : %s", fn, mvhd_strerr(err));
                                        }
                                        return;
                                }
                                /*else if (err == MVHD_ERR_TIMESTAMP)
                                {
                                        pclog("VHD: Parent/child timestamp mismatch");
                                        mvhd_close(vhdm);
                                        hdd->f = NULL;
                                        return;
                                }*/ // FIX: This is to see if this resolves Issue #59
                                hdd->f = (void *)vhdm;
                                hdd->img_type = HDD_IMG_VHD;
                        }
                } else {
                        /* Failed to open existing hard disk image */
                        if (errno == ENOENT && !read_only) {
                                /* Failed because it does not exist,
                                   so try to create new file */
                                hdd->f = (void *)fopen64(fn, "wb+");
                                if (hdd->f == NULL) {
                                        pclog("Cannot create file '%s': %s", fn, strerror(errno));
                                        return;
                                }
                        } else {
                                /* Failed for another reason */
                                pclog("Cannot open file '%s': %s", fn, strerror(errno));
                                return;
                        }
                        hdd->img_type = HDD_IMG_RAW;
                }
        }
        if (hdd->img_type == HDD_IMG_VHD) {
                MVHDMeta *vhdm = (MVHDMeta *)hdd->f;
                MVHDGeom geom = mvhd_get_geometry(vhdm);
                hdd->spt = geom.spt;
                hdd->hpc = geom.heads;
                hdd->tracks = geom.cyl;
        } else if (hdd->img_type == HDD_IMG_RAW) {
                hdd->spt = spt;
                hdd->hpc = hpc;
                hdd->tracks = tracks;
        }
        hdd->sectors = hdd->spt * hdd->hpc * hdd->tracks;
        hdd->read_only = read_only;
}

void hdd_load(hdd_file_t *hdd, int d, const char *fn) { hdd_load_ext(hdd, fn, hdc[d].spt, hdc[d].hpc, hdc[d].tracks, 0); }

void hdd_close(hdd_file_t *hdd) {
        if (hdd->f) {
                if (hdd->img_type == HDD_IMG_VHD)
                        mvhd_close((MVHDMeta *)hdd->f);
                else if (hdd->img_type == HDD_IMG_RAW)
                        fclose((FILE *)hdd->f);
        }
        hdd->img_type = HDD_IMG_RAW;
        hdd->f = NULL;
}

int hdd_read_sectors(hdd_file_t *hdd, int offset, int nr_sectors, void *buffer) {
        if (hdd->img_type == HDD_IMG_VHD) {
                return mvhd_read_sectors((MVHDMeta *)hdd->f, offset, nr_sectors, buffer);
        } else if (hdd->img_type == HDD_IMG_RAW) {
                off64_t addr;
                int transfer_sectors = nr_sectors;

                if ((hdd->sectors - offset) < transfer_sectors)
                        transfer_sectors = hdd->sectors - offset;
                addr = (uint64_t)offset * 512;

                fseeko64((FILE *)hdd->f, addr, SEEK_SET);
                fread(buffer, transfer_sectors * 512, 1, (FILE *)hdd->f);

                if (nr_sectors != transfer_sectors)
                        return 1;
                return 0;
        }
        /* Keep the compiler happy */
        return 1;
}

int hdd_write_sectors(hdd_file_t *hdd, int offset, int nr_sectors, void *buffer) {
        if (hdd->img_type == HDD_IMG_VHD) {
                return mvhd_write_sectors((MVHDMeta *)hdd->f, offset, nr_sectors, buffer);
        } else if (hdd->img_type == HDD_IMG_RAW) {
                off64_t addr;
                int transfer_sectors = nr_sectors;

                if (hdd->read_only)
                        return 1;

                if ((hdd->sectors - offset) < transfer_sectors)
                        transfer_sectors = hdd->sectors - offset;
                addr = (uint64_t)offset * 512;

                fseeko64((FILE *)hdd->f, addr, SEEK_SET);
                fwrite(buffer, transfer_sectors * 512, 1, (FILE *)hdd->f);

                if (nr_sectors != transfer_sectors)
                        return 1;
                return 0;
        }
        /* Keep the compiler happy */
        return 1;
}

int hdd_format_sectors(hdd_file_t *hdd, int offset, int nr_sectors) {
        if (hdd->img_type == HDD_IMG_VHD) {
                return mvhd_format_sectors((MVHDMeta *)hdd->f, offset, nr_sectors);
        } else if (hdd->img_type == HDD_IMG_RAW) {
                off64_t addr;
                int c;
                uint8_t zero_buffer[512];
                int transfer_sectors = nr_sectors;

                if (hdd->read_only)
                        return 1;

                memset(zero_buffer, 0, 512);

                if ((hdd->sectors - offset) < transfer_sectors)
                        transfer_sectors = hdd->sectors - offset;
                addr = (uint64_t)offset * 512;
                fseeko64((FILE *)hdd->f, addr, SEEK_SET);
                for (c = 0; c < transfer_sectors; c++)
                        fwrite(zero_buffer, 512, 1, (FILE *)hdd->f);

                if (nr_sectors != transfer_sectors)
                        return 1;
                return 0;
        }
        /* Keep the compiler happy */
        return 1;
}
