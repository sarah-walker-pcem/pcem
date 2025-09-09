#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>

#include "ibm.h"
#include "device.h"
#include "hdd_file.h"
#include "io.h"
#include "pic.h"
#include "timer.h"

#include "mfm_at.h"

#define IDE_TIME (TIMER_USEC * 10) //(5 * 100 * (1 << TIMER_SHIFT))

/*Rough estimate - MFM drives spin at 3600 RPM, with 17 sectors per track,
  meaning (3600/60)*17 = 1020 sectors per second, or 980us per sector.

  This is required for OS/2 on slow 286 systems, as the hard drive formatter
  will crash with 'internal processing error' if write sector interrupts are too
  close in time*/
#define SECTOR_TIME (TIMER_USEC * 980)

#define STAT_ERR 0x01
#define STAT_INDEX 0x02
#define STAT_CORRECTED_DATA 0x04
#define STAT_DRQ 0x08 /* Data request */
#define STAT_DSC 0x10
#define STAT_SEEK_COMPLETE 0x20
#define STAT_READY 0x40
#define STAT_BUSY 0x80

#define ERR_DAM_NOT_FOUND 0x01 /*Data Address Mark not found*/
#define ERR_TR000 0x02         /*Track 0 not found*/
#define ERR_ABRT 0x04          /*Command aborted*/
#define ERR_ID_NOT_FOUND 0x10  /*ID not found*/
#define ERR_DATA_CRC 0x40      /*Data CRC error*/
#define ERR_BAD_BLOCK 0x80     /*Bad Block detected*/

#define CMD_RESTORE 0x10
#define CMD_READ 0x20
#define CMD_WRITE 0x30
#define CMD_VERIFY 0x40
#define CMD_FORMAT 0x50
#define CMD_SEEK 0x70
#define CMD_DIAGNOSE 0x90
#define CMD_SET_PARAMETERS 0x91

extern char ide_fn[7][512];

typedef struct mfm_drive_t {
        int cfg_spt;
        int cfg_hpc;
        int current_cylinder;
        hdd_file_t hdd_file;
} mfm_drive_t;

typedef struct mfm_t {
        uint8_t status;
        uint8_t error;
        int secount, sector, cylinder, head, cylprecomp;
        uint8_t command;
        uint8_t fdisk;
        int pos;

        int drive_sel;
        int reset;
        uint16_t buffer[256];
        int irqstat;

        pc_timer_t callback_timer;

        mfm_drive_t drives[2];
} mfm_t;

uint16_t mfm_readw(uint16_t port, void *p);
void mfm_writew(uint16_t port, uint16_t val, void *p);

static inline void mfm_irq_raise(mfm_t *mfm) {
        //        pclog("IDE_IRQ_RAISE\n");
        if (!(mfm->fdisk & 2))
                picint(1 << 14);

        mfm->irqstat = 1;
}

static inline void mfm_irq_lower(mfm_t *mfm) { picintc(1 << 14); }

void mfm_irq_update(mfm_t *mfm) {
        if (mfm->irqstat && !((pic2.pend | pic2.ins) & 0x40) && !(mfm->fdisk & 2))
                picint(1 << 14);
}

/*
 * Return the sector offset for the current register values
 */
static int mfm_get_sector(mfm_t *mfm, off_t *addr) {
        mfm_drive_t *drive = &mfm->drives[mfm->drive_sel];
        int heads = drive->cfg_hpc;
        int sectors = drive->cfg_spt;

        if (drive->current_cylinder != mfm->cylinder) {
                pclog("mfm_get_sector: wrong cylinder\n");
                return 1;
        }
        if (mfm->head > heads) {
                pclog("mfm_get_sector: past end of configured heads\n");
                return 1;
        }
        if (mfm->sector >= sectors + 1) {
                pclog("mfm_get_sector: past end of configured sectors\n");
                return 1;
        }
        if (mfm->head > drive->hdd_file.hpc) {
                pclog("mfm_get_sector: past end of heads\n");
                return 1;
        }
        if (mfm->sector >= drive->hdd_file.spt + 1) {
                pclog("mfm_get_sector: past end of sectors\n");
                return 1;
        }

        *addr = ((((off_t)mfm->cylinder * heads) + mfm->head) * sectors) + (mfm->sector - 1);

        return 0;
}

/**
 * Move to the next sector using CHS addressing
 */
static void mfm_next_sector(mfm_t *mfm) {
        mfm_drive_t *drive = &mfm->drives[mfm->drive_sel];

        mfm->sector++;
        if (mfm->sector == (drive->cfg_spt + 1)) {
                mfm->sector = 1;
                mfm->head++;
                if (mfm->head == drive->cfg_hpc) {
                        mfm->head = 0;
                        mfm->cylinder++;
                        if (drive->current_cylinder < drive->hdd_file.tracks)
                                drive->current_cylinder++;
                }
        }
}

void mfm_write(uint16_t port, uint8_t val, void *p) {
        mfm_t *mfm = (mfm_t *)p;

        //        pclog("mfm_write: addr=%04x val=%02x\n", port, val);
        switch (port) {
        case 0x1F0: /* Data */
                mfm_writew(port, val | (val << 8), p);
                return;

        case 0x1F1: /* Write precompenstation */
                mfm->cylprecomp = val;
                return;

        case 0x1F2: /* Sector count */
                mfm->secount = val;
                return;

        case 0x1F3: /* Sector */
                mfm->sector = val;
                return;

        case 0x1F4: /* Cylinder low */
                mfm->cylinder = (mfm->cylinder & 0xFF00) | val;
                return;

        case 0x1F5: /* Cylinder high */
                mfm->cylinder = (mfm->cylinder & 0xFF) | (val << 8);
                return;

        case 0x1F6: /* Drive/Head */
                mfm->head = val & 0xF;
                mfm->drive_sel = (val & 0x10) ? 1 : 0;
                if (mfm->drives[mfm->drive_sel].hdd_file.f == NULL)
                        mfm->status = 0;
                else
                        mfm->status = STAT_READY | STAT_DSC;
                return;

        case 0x1F7: /* Command register */
                if (mfm->drives[mfm->drive_sel].hdd_file.f == NULL)
                        fatal("Command on non-present drive\n");

                mfm_irq_lower(mfm);
                mfm->command = val;
                mfm->error = 0;

                switch (val & 0xf0) {
                case CMD_RESTORE:
                        //                        pclog("Restore\n");
                        mfm->command &= ~0x0f; /*Mask off step rate*/
                        mfm->status = STAT_BUSY;
                        timer_set_delay_u64(&mfm->callback_timer, 200 * IDE_TIME);
                        break;

                case CMD_SEEK:
                        //                        pclog("Seek to cylinder %i\n", mfm->cylinder);
                        mfm->command &= ~0x0f; /*Mask off step rate*/
                        mfm->status = STAT_BUSY;
                        timer_set_delay_u64(&mfm->callback_timer, 200 * IDE_TIME);
                        break;

                default:
                        switch (val) {
                        case CMD_READ:
                        case CMD_READ + 1:
                        case CMD_READ + 2:
                        case CMD_READ + 3:
                                //                                pclog("Read %i sectors from sector %i cylinder %i head %i
                                //                                %i\n",mfm->secount,mfm->sector,mfm->cylinder,mfm->head,ins);
                                mfm->command &= ~3;
                                if (val & 2)
                                        fatal("Read with ECC\n");
                                mfm->status = STAT_BUSY;
                                timer_set_delay_u64(&mfm->callback_timer, 200 * IDE_TIME);
                                break;

                        case CMD_WRITE:
                        case CMD_WRITE + 1:
                        case CMD_WRITE + 2:
                        case CMD_WRITE + 3:
                                //                                pclog("Write %i sectors to sector %i cylinder %i head
                                //                                %i\n",mfm->secount,mfm->sector,mfm->cylinder,mfm->head);
                                mfm->command &= ~3;
                                if (val & 2)
                                        fatal("Write with ECC\n");
                                mfm->status = STAT_DRQ | STAT_DSC; // | STAT_BUSY;
                                mfm->pos = 0;
                                break;

                        case CMD_VERIFY:
                        case CMD_VERIFY + 1:
                                //                                pclog("Read verify %i sectors from sector %i cylinder %i head
                                //                                %i\n",mfm->secount,mfm->sector,mfm->cylinder,mfm->head);
                                mfm->command &= ~1;
                                mfm->status = STAT_BUSY;
                                timer_set_delay_u64(&mfm->callback_timer, 200 * IDE_TIME);
                                break;

                        case CMD_FORMAT:
                                //                                pclog("Format track %i head %i\n", mfm->cylinder, mfm->head);
                                mfm->status = STAT_DRQ | STAT_BUSY;
                                mfm->pos = 0;
                                break;

                        case CMD_SET_PARAMETERS: /* Initialize Drive Parameters */
                                mfm->status = STAT_BUSY;
                                timer_set_delay_u64(&mfm->callback_timer, 30 * IDE_TIME);
                                break;

                        case CMD_DIAGNOSE: /* Execute Drive Diagnostics */
                                mfm->status = STAT_BUSY;
                                timer_set_delay_u64(&mfm->callback_timer, 200 * IDE_TIME);
                                break;

                        default:
                                pclog("Bad MFM command %02X\n", val);
                                mfm->status = STAT_BUSY;
                                timer_set_delay_u64(&mfm->callback_timer, 200 * IDE_TIME);
                                break;
                        }
                }
                break;

        case 0x3F6: /* Device control */
                if ((mfm->fdisk & 4) && !(val & 4)) {
                        timer_set_delay_u64(&mfm->callback_timer, 500 * IDE_TIME);
                        mfm->reset = 1;
                        mfm->status = STAT_BUSY;
                        //                        pclog("MFM Reset\n");
                }
                if (val & 4) {
                        /*Drive held in reset*/
                        timer_disable(&mfm->callback_timer);
                        mfm->status = STAT_BUSY;
                }
                mfm->fdisk = val;
                mfm_irq_update(mfm);
                return;
        }
        //        fatal("Bad IDE write %04X %02X\n", addr, val);
}

void mfm_writew(uint16_t port, uint16_t val, void *p) {
        mfm_t *mfm = (mfm_t *)p;

        //        pclog("Write IDEw %04X\n",val);
        mfm->buffer[mfm->pos >> 1] = val;
        mfm->pos += 2;

        if (mfm->pos >= 512) {
                mfm->pos = 0;
                mfm->status = STAT_BUSY;
                timer_set_delay_u64(&mfm->callback_timer, SECTOR_TIME);
        }
}

uint8_t mfm_read(uint16_t port, void *p) {
        mfm_t *mfm = (mfm_t *)p;
        uint8_t temp = 0xff;

        switch (port) {
        case 0x1F0: /* Data */
                temp = mfm_readw(port, mfm) & 0xff;
                break;

        case 0x1F1: /* Error */
                temp = mfm->error;
                break;

        case 0x1F2: /* Sector count */
                temp = (uint8_t)mfm->secount;
                break;

        case 0x1F3: /* Sector */
                temp = (uint8_t)mfm->sector;
                break;

        case 0x1F4: /* Cylinder low */
                temp = (uint8_t)(mfm->cylinder & 0xFF);
                break;

        case 0x1F5: /* Cylinder high */
                temp = (uint8_t)(mfm->cylinder >> 8);
                break;

        case 0x1F6: /* Drive/Head */
                temp = (uint8_t)(mfm->head | (mfm->drive_sel ? 0x10 : 0) | 0xa0);
                break;

        case 0x1F7: /* Status */
                mfm_irq_lower(mfm);
                temp = mfm->status;
                break;
        }

        //        pclog("mfm_read: addr=%04x val=%02x %04X:%04x\n", port, temp, CS, cpu_state.pc);
        return temp;
}

uint16_t mfm_readw(uint16_t port, void *p) {
        mfm_t *mfm = (mfm_t *)p;
        uint16_t temp;

        temp = mfm->buffer[mfm->pos >> 1];
        mfm->pos += 2;

        if (mfm->pos >= 512) {
                //                pclog("Over! packlen %i %i\n",ide->packlen,ide->pos);
                mfm->pos = 0;
                mfm->status = STAT_READY | STAT_DSC;
                if (mfm->command == CMD_READ) {
                        mfm->secount = (mfm->secount - 1) & 0xff;
                        if (mfm->secount) {
                                mfm_next_sector(mfm);
                                mfm->status = STAT_BUSY | STAT_READY | STAT_DSC;
                                timer_set_delay_u64(&mfm->callback_timer, SECTOR_TIME);
                        }
                }
        }

        //        pclog("mem_readw: temp=%04x %i\n", temp, mfm->pos);
        return temp;
}

static void do_seek(mfm_t *mfm) {
        mfm_drive_t *drive = &mfm->drives[mfm->drive_sel];

        if (mfm->cylinder < drive->hdd_file.tracks)
                drive->current_cylinder = mfm->cylinder;
        else
                drive->current_cylinder = drive->hdd_file.tracks - 1;
}

void mfm_callback(void *p) {
        mfm_t *mfm = (mfm_t *)p;
        mfm_drive_t *drive = &mfm->drives[mfm->drive_sel];
        off_t addr;

        //        pclog("mfm_callback: command=%02x reset=%i\n", mfm->command, mfm->reset);

        if (mfm->reset) {
                mfm->status = STAT_READY | STAT_DSC;
                mfm->error = 1;
                mfm->secount = 1;
                mfm->sector = 1;
                mfm->head = 0;
                mfm->cylinder = 0;
                mfm->reset = 0;
                //                pclog("Reset callback\n");
                return;
        }
        switch (mfm->command) {
        case CMD_RESTORE:
                drive->current_cylinder = 0;
                mfm->status = STAT_READY | STAT_DSC;
                mfm_irq_raise(mfm);
                break;

        case CMD_SEEK:
                do_seek(mfm);
                mfm->status = STAT_READY | STAT_DSC;
                mfm_irq_raise(mfm);
                break;

        case CMD_READ:
                do_seek(mfm);
                if (mfm_get_sector(mfm, &addr)) {
                        mfm->error = ERR_ID_NOT_FOUND;
                        mfm->status = STAT_READY | STAT_DSC | STAT_ERR;
                        mfm_irq_raise(mfm);
                        break;
                }

                //                pclog("Read %i %i %i %08X\n",ide.cylinder,ide.head,ide.sector,addr);
                hdd_read_sectors(&drive->hdd_file, addr, 1, mfm->buffer);
                mfm->pos = 0;
                mfm->status = STAT_DRQ | STAT_READY | STAT_DSC;
                //                pclog("Read sector callback %i %i %i offset %08X %i left %i
                //                %02X\n",ide.sector,ide.cylinder,ide.head,addr,ide.secount,ide.spt,ide.atastat[ide.board]); if
                //                (addr) output=3;
                mfm_irq_raise(mfm);
                readflash_set(READFLASH_HDC, mfm->drive_sel);
                break;

        case CMD_WRITE:
                do_seek(mfm);
                if (mfm_get_sector(mfm, &addr)) {
                        mfm->error = ERR_ID_NOT_FOUND;
                        mfm->status = STAT_READY | STAT_DSC | STAT_ERR;
                        mfm_irq_raise(mfm);
                        break;
                }
                hdd_write_sectors(&drive->hdd_file, addr, 1, mfm->buffer);
                mfm_irq_raise(mfm);
                mfm->secount = (mfm->secount - 1) & 0xff;
                if (mfm->secount) {
                        mfm->status = STAT_DRQ | STAT_READY | STAT_DSC;
                        mfm->pos = 0;
                        mfm_next_sector(mfm);
                } else
                        mfm->status = STAT_READY | STAT_DSC;
                readflash_set(READFLASH_HDC, mfm->drive_sel);
                break;

        case CMD_VERIFY:
                do_seek(mfm);
                mfm->pos = 0;
                mfm->status = STAT_READY | STAT_DSC;
                //                pclog("Read verify callback %i %i %i offset %08X %i
                //                left\n",ide.sector,ide.cylinder,ide.head,addr,ide.secount);
                mfm_irq_raise(mfm);
                readflash_set(READFLASH_HDC, mfm->drive_sel);
                break;

        case CMD_FORMAT:
                do_seek(mfm);
                if (mfm_get_sector(mfm, &addr)) {
                        mfm->error = ERR_ID_NOT_FOUND;
                        mfm->status = STAT_READY | STAT_DSC | STAT_ERR;
                        mfm_irq_raise(mfm);
                        break;
                }
                hdd_format_sectors(&drive->hdd_file, addr, mfm->secount);
                mfm->status = STAT_READY | STAT_DSC;
                mfm_irq_raise(mfm);
                readflash_set(READFLASH_HDC, mfm->drive_sel);
                break;

        case CMD_DIAGNOSE:
                mfm->error = 1; /*No error detected*/
                mfm->status = STAT_READY | STAT_DSC;
                mfm_irq_raise(mfm);
                break;

        case CMD_SET_PARAMETERS: /* Initialize Drive Parameters */
                drive->cfg_spt = mfm->secount;
                drive->cfg_hpc = mfm->head + 1;
                pclog("Parameters: spt=%i hpc=%i\n", drive->cfg_spt, drive->cfg_hpc);
                mfm->status = STAT_READY | STAT_DSC;
                mfm_irq_raise(mfm);
                break;

        default:
                pclog("Callback on unknown command %02x\n", mfm->command);
                mfm->status = STAT_READY | STAT_ERR | STAT_DSC;
                mfm->error = ERR_ABRT;
                mfm_irq_raise(mfm);
                break;
        }
}

void *mfm_init() {
        mfm_t *mfm = malloc(sizeof(mfm_t));
        memset(mfm, 0, sizeof(mfm_t));

        hdd_load(&mfm->drives[0].hdd_file, 0, ide_fn[0]);
        hdd_load(&mfm->drives[1].hdd_file, 1, ide_fn[1]);

        mfm->status = STAT_READY | STAT_DSC;
        mfm->error = 1; /*No errors*/

        io_sethandler(0x01f0, 0x0001, mfm_read, mfm_readw, NULL, mfm_write, mfm_writew, NULL, mfm);
        io_sethandler(0x01f1, 0x0007, mfm_read, NULL, NULL, mfm_write, NULL, NULL, mfm);
        io_sethandler(0x03f6, 0x0001, NULL, NULL, NULL, mfm_write, NULL, NULL, mfm);

        timer_add(&mfm->callback_timer, mfm_callback, mfm, 0);

        return mfm;
}

void mfm_close(void *p) {
        mfm_t *mfm = (mfm_t *)p;

        hdd_close(&mfm->drives[0].hdd_file);
        hdd_close(&mfm->drives[1].hdd_file);

        free(mfm);
}

device_t mfm_at_device = {"IBM PC AT Fixed Disk Adapter", DEVICE_AT, mfm_init, mfm_close, NULL, NULL, NULL, NULL, NULL};
