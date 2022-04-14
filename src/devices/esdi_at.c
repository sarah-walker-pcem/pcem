#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _GNU_SOURCE

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>

#include "ibm.h"
#include "device.h"
#include "hdd_file.h"
#include "io.h"
#include "mem.h"
#include "pic.h"
#include "rom.h"
#include "timer.h"

#include "esdi_at.h"


#define IDE_TIME (TIMER_USEC*10)//(5 * 100 * (1 << TIMER_SHIFT))

#define STAT_ERR		0x01
#define STAT_INDEX		0x02
#define STAT_CORRECTED_DATA	0x04
#define STAT_DRQ		0x08 /* Data request */
#define STAT_DSC                0x10
#define STAT_SEEK_COMPLETE      0x20
#define STAT_READY		0x40
#define STAT_BUSY		0x80

#define ERR_DAM_NOT_FOUND       0x01 /*Data Address Mark not found*/
#define ERR_TR000               0x02 /*Track 0 not found*/
#define ERR_ABRT		0x04 /*Command aborted*/
#define ERR_ID_NOT_FOUND	0x10 /*ID not found*/
#define ERR_DATA_CRC	        0x40 /*Data CRC error*/
#define ERR_BAD_BLOCK	        0x80 /*Bad Block detected*/

#define CMD_NOP                         0x00
#define CMD_RESTORE			0x10
#define CMD_READ			0x20
#define CMD_WRITE			0x30
#define CMD_VERIFY			0x40
#define CMD_FORMAT			0x50
#define CMD_SEEK   			0x70
#define CMD_DIAGNOSE                    0x90
#define CMD_SET_PARAMETERS              0x91
#define CMD_READ_PARAMETERS             0xec

extern char ide_fn[7][512];

typedef struct esdi_drive_t
{
        int cfg_spt;
        int cfg_hpc;
        int current_cylinder;
        hdd_file_t hdd_file;
} esdi_drive_t;

typedef struct esdi_t
{
        uint8_t status;
        uint8_t error;
        int secount,sector,cylinder,head,cylprecomp;
        uint8_t command;
        uint8_t fdisk;
        int pos;
        
        int drive_sel;
        int reset;
        uint16_t buffer[256];
        int irqstat;
        
        pc_timer_t callback_timer;
        
        esdi_drive_t drives[2];
        
        rom_t bios_rom;
} esdi_t;

uint16_t esdi_readw(uint16_t port, void *p);
void esdi_writew(uint16_t port, uint16_t val, void *p);

static inline void esdi_irq_raise(esdi_t *esdi)
{
//        pclog("IDE_IRQ_RAISE\n");
	if (!(esdi->fdisk&2))
                picint(1 << 14);

	esdi->irqstat=1;
}

static inline void esdi_irq_lower(esdi_t *esdi)
{
        picintc(1 << 14);
}

void esdi_irq_update(esdi_t *esdi)
{
	if (esdi->irqstat && !((pic2.pend|pic2.ins)&0x40) && !(esdi->fdisk & 2))
                picint(1 << 14);
}

/*
 * Return the sector offset for the current register values
 */
int esdi_get_sector(esdi_t *esdi, off64_t *addr)
{
        esdi_drive_t *drive = &esdi->drives[esdi->drive_sel];
        int heads = drive->cfg_hpc;
        int sectors = drive->cfg_spt;

        if (esdi->head > heads)
        {
                pclog("esdi_get_sector: past end of configured heads\n");
                return 1;
        }
        if (esdi->sector >= sectors+1)
        {
                pclog("esdi_get_sector: past end of configured sectors\n");
                return 1;
        }

        if (drive->cfg_spt == drive->hdd_file.spt && drive->cfg_hpc == drive->hdd_file.hpc)
        {
                *addr = ((((off64_t) esdi->cylinder * heads) + esdi->head) *
                	          sectors) + (esdi->sector - 1);
        }
        else
        {
                /*When performing translation, the firmware seems to leave 1
                  sector per track inaccessible (spare sector)*/
                int c, h, s;
                *addr = ((((off64_t) esdi->cylinder * heads) + esdi->head) *
                	          sectors) + (esdi->sector - 1);
                
                s = *addr % (drive->hdd_file.spt - 1);
                h = (*addr / (drive->hdd_file.spt - 1)) % drive->hdd_file.hpc;
                c = (*addr / (drive->hdd_file.spt - 1)) / drive->hdd_file.hpc;

                *addr = ((((off64_t) c * drive->hdd_file.hpc) + h) *
                	          drive->hdd_file.spt) + s;
        }
        
        return 0;
}

/**
 * Move to the next sector using CHS addressing
 */
void esdi_next_sector(esdi_t *esdi)
{
        esdi_drive_t *drive = &esdi->drives[esdi->drive_sel];
        
        esdi->sector++;
        if (esdi->sector == (drive->cfg_spt + 1))
        {
        	esdi->sector = 1;
        	esdi->head++;
        	if (esdi->head == drive->cfg_hpc)
                {
        		esdi->head = 0;
        		esdi->cylinder++;
        		if (drive->current_cylinder < drive->hdd_file.tracks)
                		drive->current_cylinder++;
		}
	}
}

void esdi_write(uint16_t port, uint8_t val, void *p)
{
        esdi_t *esdi = (esdi_t *)p;

//        pclog("esdi_write: addr=%04x val=%02x\n", port, val);
        switch (port)
        {
                case 0x1F0: /* Data */
                esdi_writew(port, val | (val << 8), p);
                return;

                case 0x1F1: /* Write precompenstation */
                esdi->cylprecomp = val;
                return;

                case 0x1F2: /* Sector count */
                esdi->secount = val;
                return;

                case 0x1F3: /* Sector */
                esdi->sector = val;
                return;

                case 0x1F4: /* Cylinder low */
                esdi->cylinder = (esdi->cylinder & 0xFF00) | val;
                return;

                case 0x1F5: /* Cylinder high */
                esdi->cylinder = (esdi->cylinder & 0xFF) | (val << 8);
                return;

                case 0x1F6: /* Drive/Head */
                esdi->head = val & 0xF;
                esdi->drive_sel = (val & 0x10) ? 1 : 0;
                if (esdi->drives[esdi->drive_sel].hdd_file.f == NULL)
                        esdi->status = 0;
                else
                        esdi->status = STAT_READY | STAT_DSC;
                return;

                case 0x1F7: /* Command register */
//                if (esdi->drives[esdi->drive_sel].hdd_file.f == NULL && val != CMD_SET_PARAMETERS && val != CMD_NOP && val != 0xe0 && (val & ~3) != 0x20 && (val & ~3) != 0x30 && (val & ~3) != 0x40 && val != 0x90 && val != CMD_READ_PARAMETERS)
//                        fatal("Command %02x on non-present drive\n", val);

                esdi_irq_lower(esdi);
                esdi->command = val;
                esdi->error = 0;
                
                switch (val & 0xf0)
                {
                        case CMD_RESTORE:
//                        pclog("Restore\n");
                        esdi->command &= ~0x0f; /*Mask off step rate*/
                        esdi->status = STAT_BUSY;
                        timer_set_delay_u64(&esdi->callback_timer, 200*IDE_TIME);
                        break;

                        case CMD_SEEK:
//                        pclog("Seek to cylinder %i\n", esdi->cylinder);
                        esdi->command &= ~0x0f; /*Mask off step rate*/
                        esdi->status = STAT_BUSY;
                        timer_set_delay_u64(&esdi->callback_timer, 200*IDE_TIME);
                        break;

                        default:
                        switch (val)
                        {
                                case CMD_NOP:
                                esdi->status = STAT_BUSY;
                                timer_set_delay_u64(&esdi->callback_timer, 200*IDE_TIME);
                                break;
                                
                                case CMD_READ: case CMD_READ+1:
                                case CMD_READ+2: case CMD_READ+3:
//                                pclog("Read %i sectors from sector %i cylinder %i head %i  %i\n",esdi->secount,esdi->sector,esdi->cylinder,esdi->head,ins);
                                esdi->command &= ~3;
                                if (val & 2)
                                        fatal("Read with ECC\n");
                                case 0xa0:
                                esdi->status = STAT_BUSY;
                                timer_set_delay_u64(&esdi->callback_timer, 200*IDE_TIME);
                                break;

                                case CMD_WRITE: case CMD_WRITE+1:
                                case CMD_WRITE+2: case CMD_WRITE+3:
//                                pclog("Write %i sectors to sector %i cylinder %i head %i\n",esdi->secount,esdi->sector,esdi->cylinder,esdi->head);
                                esdi->command &= ~3;
                                if (val & 2)
                                        fatal("Write with ECC\n");
                                esdi->status = STAT_DRQ | STAT_DSC;// | STAT_BUSY;
                                esdi->pos=0;
                                break;

                                case CMD_VERIFY: case CMD_VERIFY+1:
//                                pclog("Read verify %i sectors from sector %i cylinder %i head %i\n",esdi->secount,esdi->sector,esdi->cylinder,esdi->head);
                                esdi->command &= ~1;
                                esdi->status = STAT_BUSY;
                                timer_set_delay_u64(&esdi->callback_timer, 200 * IDE_TIME);
                                break;

                                case CMD_FORMAT:
//                                pclog("Format track %i head %i\n", esdi->cylinder, esdi->head);
                                esdi->status = STAT_DRQ;// | STAT_BUSY;
                                esdi->pos=0;
                                break;

                                case CMD_SET_PARAMETERS: /* Initialize Drive Parameters */
                                esdi->status = STAT_BUSY;
                                timer_set_delay_u64(&esdi->callback_timer, 30*IDE_TIME);
                                break;

                                case CMD_DIAGNOSE: /* Execute Drive Diagnostics */
                                esdi->status = STAT_BUSY;
                                timer_set_delay_u64(&esdi->callback_timer, 200*IDE_TIME);
                                break;

                                case 0xe0: /*???*/
                                case CMD_READ_PARAMETERS:
                                esdi->status = STAT_BUSY;
                                timer_set_delay_u64(&esdi->callback_timer, 200*IDE_TIME);
                                break;

                                default:
                                pclog("Bad esdi command %02X\n", val);
                                case 0xe8: /*???*/
                                esdi->status = STAT_BUSY;
                                timer_set_delay_u64(&esdi->callback_timer, 200*IDE_TIME);
                                break;
                        }
                }                
                break;

                case 0x3F6: /* Device control */
                if ((esdi->fdisk & 4) && !(val & 4))
                {
                        timer_set_delay_u64(&esdi->callback_timer, 500*IDE_TIME);
                        esdi->reset = 1;
                        esdi->status = STAT_BUSY;
//                        pclog("esdi Reset\n");
                }
                if (val & 4)
                {
                        /*Drive held in reset*/
                        timer_disable(&esdi->callback_timer);
                        esdi->status = STAT_BUSY;
                }
                esdi->fdisk = val;
                esdi_irq_update(esdi);
                return;
        }
//        fatal("Bad ESDI write %04X %02X\n", port, val);
}

void esdi_writew(uint16_t port, uint16_t val, void *p)
{
        esdi_t *esdi = (esdi_t *)p;
        
//        pclog("Write ESDIw %04X\n",val);
        esdi->buffer[esdi->pos >> 1] = val;
        esdi->pos += 2;

        if (esdi->pos >= 512)
        {
                esdi->pos = 0;
                esdi->status = STAT_BUSY;
              	timer_set_delay_u64(&esdi->callback_timer, 6*IDE_TIME);
        }
}

uint8_t esdi_read(uint16_t port, void *p)
{
        esdi_t *esdi = (esdi_t *)p;
        uint8_t temp = 0xff;

        switch (port)
        {
                case 0x1F0: /* Data */
                temp = esdi_readw(port, esdi) & 0xff;
                break;
                
                case 0x1F1: /* Error */
                temp = esdi->error;
                break;

                case 0x1F2: /* Sector count */
                temp = (uint8_t)esdi->secount;
                break;

                case 0x1F3: /* Sector */
                temp = (uint8_t)esdi->sector;
                break;

                case 0x1F4: /* Cylinder low */
                temp = (uint8_t)(esdi->cylinder&0xFF);
                break;

                case 0x1F5: /* Cylinder high */
                temp = (uint8_t)(esdi->cylinder>>8);
                break;

                case 0x1F6: /* Drive/Head */
                temp = (uint8_t)(esdi->head | (esdi->drive_sel ? 0x10 : 0) | 0xa0);
                break;

                case 0x1F7: /* Status */
                esdi_irq_lower(esdi);
                temp = esdi->status;
                break;
        }

//        if (port != 0x1f7) pclog("esdi_read: addr=%04x val=%02x %04X:%04x\n", port, temp, CS, cpu_state.pc);
        return temp;
}

uint16_t esdi_readw(uint16_t port, void *p)
{
        esdi_t *esdi = (esdi_t *)p;
        uint16_t temp;

        temp = esdi->buffer[esdi->pos >> 1];
        esdi->pos += 2;

        if (esdi->pos >= 512)
        {
//                pclog("Over! packlen %i %i\n",ide->packlen,ide->pos);
                esdi->pos=0;
                esdi->status = STAT_READY | STAT_DSC;
                if (esdi->command == CMD_READ || esdi->command == 0xa0)
                {
                        esdi->secount = (esdi->secount - 1) & 0xff;
                        if (esdi->secount)
                        {
                                esdi_next_sector(esdi);
                                esdi->status = STAT_BUSY;
                                timer_set_delay_u64(&esdi->callback_timer, 6*IDE_TIME);
                        }
                }
        }
        
//        pclog("esdi_readw: temp=%04x %i\n", temp, esdi->pos);
        return temp;
}

void esdi_callback(void *p)
{
        esdi_t *esdi = (esdi_t *)p;
        esdi_drive_t *drive = &esdi->drives[esdi->drive_sel];
        off64_t addr;
        
//        pclog("esdi_callback: command=%02x reset=%i\n", esdi->command, esdi->reset);
        
        if (esdi->reset)
        {
                esdi->status = STAT_READY | STAT_DSC;
                esdi->error = 1;
                esdi->secount = 1;
                esdi->sector = 1;
                esdi->head = 0;
                esdi->cylinder = 0;
                esdi->reset = 0;
//                pclog("Reset callback\n");
                return;
        }
        switch (esdi->command)
        {
                case CMD_RESTORE:
                if (!drive->hdd_file.f)
                {
                	esdi->status = STAT_READY | STAT_ERR | STAT_DSC;
                	esdi->error = ERR_ABRT;
                	esdi_irq_raise(esdi);
                }
                else
                {
                        drive->current_cylinder = 0;
                        esdi->status = STAT_READY | STAT_DSC;
                        esdi_irq_raise(esdi);
                }
                break;

                case CMD_SEEK:
                if (!drive->hdd_file.f)
                {
                	esdi->status = STAT_READY | STAT_ERR | STAT_DSC;
                	esdi->error = ERR_ABRT;
                	esdi_irq_raise(esdi);
                }
                else
                {
                        esdi->status = STAT_READY | STAT_DSC;
                        esdi_irq_raise(esdi);
                }
                break;

                case CMD_READ:
                if (!drive->hdd_file.f)
                {
                	esdi->status = STAT_READY | STAT_ERR | STAT_DSC;
                	esdi->error = ERR_ABRT;
                	esdi_irq_raise(esdi);
                }
                else
                {
                        if (esdi_get_sector(esdi, &addr))
                        {
                                esdi->error = ERR_ID_NOT_FOUND;
                                esdi->status = STAT_READY | STAT_DSC | STAT_ERR;
                                esdi_irq_raise(esdi);
                                break;
                        }
                        if (hdd_read_sectors(&drive->hdd_file, addr, 1, esdi->buffer))
                        {
                                esdi->error = ERR_ID_NOT_FOUND;
                                esdi->status = STAT_READY | STAT_DSC | STAT_ERR;
                                esdi_irq_raise(esdi);
                                break;
                        }                        
                        esdi->pos = 0;
                        esdi->status = STAT_DRQ | STAT_READY | STAT_DSC;
//                pclog("Read sector callback %i %i %i offset %08X %i left %i %02X\n",ide.sector,ide.cylinder,ide.head,addr,ide.secount,ide.spt,ide.atastat[ide.board]);
//                if (addr) output=3;
                        esdi_irq_raise(esdi);
                        readflash_set(READFLASH_HDC, esdi->drive_sel);
                }
                break;

                case CMD_WRITE:
                if (!drive->hdd_file.f)
                {
                	esdi->status = STAT_READY | STAT_ERR | STAT_DSC;
                	esdi->error = ERR_ABRT;
                	esdi_irq_raise(esdi);
                }
                else
                {
                        if (esdi_get_sector(esdi, &addr))
                        {
                                esdi->error = ERR_ID_NOT_FOUND;
                                esdi->status = STAT_READY | STAT_DSC | STAT_ERR;
                                esdi_irq_raise(esdi);
                                break;
                        }
                        if (hdd_write_sectors(&drive->hdd_file, addr, 1, esdi->buffer))
                        {
                                esdi->error = ERR_ID_NOT_FOUND;
                                esdi->status = STAT_READY | STAT_DSC | STAT_ERR;
                                esdi_irq_raise(esdi);
                                break;
                        }                        
                        esdi_irq_raise(esdi);
                        esdi->secount = (esdi->secount - 1) & 0xff;
                        if (esdi->secount)
                        {
                                esdi->status = STAT_DRQ | STAT_READY | STAT_DSC;
                                esdi->pos = 0;
                                esdi_next_sector(esdi);
                        }
                        else
                                esdi->status = STAT_READY | STAT_DSC;
                        readflash_set(READFLASH_HDC, esdi->drive_sel);
                }
                break;
                
                case CMD_VERIFY:
                if (!drive->hdd_file.f)
                {
                	esdi->status = STAT_READY | STAT_ERR | STAT_DSC;
                	esdi->error = ERR_ABRT;
                	esdi_irq_raise(esdi);
                }
                else
                {
                        if (esdi_get_sector(esdi, &addr))
                        {
                                esdi->error = ERR_ID_NOT_FOUND;
                                esdi->status = STAT_READY | STAT_DSC | STAT_ERR;
                                esdi_irq_raise(esdi);
                                break;
                        }
                        if (hdd_read_sectors(&drive->hdd_file, addr, 1, esdi->buffer))
                        {
                                esdi->error = ERR_ID_NOT_FOUND;
                                esdi->status = STAT_READY | STAT_DSC | STAT_ERR;
                                esdi_irq_raise(esdi);
                                break;
                        }
                        readflash_set(READFLASH_HDC, esdi->drive_sel);
                        esdi_next_sector(esdi);
                        esdi->secount = (esdi->secount - 1) & 0xff;
                        if (esdi->secount)
                                timer_set_delay_u64(&esdi->callback_timer, 6*IDE_TIME);
                        else
                        {
                                esdi->pos = 0;
                                esdi->status = STAT_READY | STAT_DSC;
//                pclog("Read verify callback %i %i %i offset %08X %i left\n",ide.sector,ide.cylinder,ide.head,addr,ide.secount);
                                esdi_irq_raise(esdi);
                        }
                }
                break;

                case CMD_FORMAT:
                if (!drive->hdd_file.f)
                {
                	esdi->status = STAT_READY | STAT_ERR | STAT_DSC;
                	esdi->error = ERR_ABRT;
                	esdi_irq_raise(esdi);
                }
                else
                {
                        if (esdi_get_sector(esdi, &addr))
                        {
                                esdi->error = ERR_ID_NOT_FOUND;
                                esdi->status = STAT_READY | STAT_DSC | STAT_ERR;
                                esdi_irq_raise(esdi);
                                break;
                        }
                        if (hdd_format_sectors(&drive->hdd_file, addr, esdi->secount))
                        {
                                esdi->error = ERR_ID_NOT_FOUND;
                                esdi->status = STAT_READY | STAT_DSC | STAT_ERR;
                                esdi_irq_raise(esdi);
                                break;
                        }                        
                        esdi->status = STAT_READY | STAT_DSC;
                        esdi_irq_raise(esdi);
                        readflash_set(READFLASH_HDC, esdi->drive_sel);
                }
                break;

                case CMD_DIAGNOSE:
                if (!drive->hdd_file.f)
                {
                	esdi->status = STAT_READY | STAT_ERR | STAT_DSC;
                	esdi->error = ERR_ABRT;
                	esdi_irq_raise(esdi);
                }
                else
                {
                        esdi->error = 1; /*No error detected*/
        		esdi->status = STAT_READY | STAT_DSC;
        		esdi_irq_raise(esdi);
                }
                break;

                case CMD_SET_PARAMETERS: /* Initialize Drive Parameters */
                if (!drive->hdd_file.f)
                {
                	esdi->status = STAT_READY | STAT_ERR | STAT_DSC;
                	esdi->error = ERR_ABRT;
                	esdi_irq_raise(esdi);
                }
                else
                {
                        drive->cfg_spt = esdi->secount;
                        drive->cfg_hpc = esdi->head+1;
                        pclog("Parameters: spt=%i hpc=%i\n", drive->cfg_spt,drive->cfg_hpc);
                        if (!esdi->secount)
                                fatal("secount=0\n");
                        esdi->status = STAT_READY | STAT_DSC;
                        esdi_irq_raise(esdi);
                }
                break;
                
                case CMD_NOP:
                esdi->status = STAT_READY | STAT_ERR | STAT_DSC;
                esdi->error = ERR_ABRT;
                esdi_irq_raise(esdi);
                break;
                
                case 0xe0:
                if (!drive->hdd_file.f)
                {
                	esdi->status = STAT_READY | STAT_ERR | STAT_DSC;
                	esdi->error = ERR_ABRT;
                	esdi_irq_raise(esdi);
                }
                else
                {
                        switch (esdi->cylinder >> 8)
                        {
                                case 0x31:
                                esdi->cylinder = drive->hdd_file.tracks;
                                break;
                                case 0x33:
                                esdi->cylinder = drive->hdd_file.hpc;
                                break;
                                case 0x35:
                                esdi->cylinder = 0x200;
                                break;
                                case 0x36:
                                esdi->cylinder = drive->hdd_file.spt;
                                break;
                                default:
                                pclog("EDSI Bad read config %02x\n", esdi->cylinder >> 8);
                        }
        		esdi->status = STAT_READY | STAT_DSC;
        		esdi_irq_raise(esdi);
                }
                break;

                case 0xa0:
                if (!drive->hdd_file.f)
                {
                	esdi->status = STAT_READY | STAT_ERR | STAT_DSC;
                	esdi->error = ERR_ABRT;
                	esdi_irq_raise(esdi);
                }
                else
                {
                        memset(esdi->buffer, 0, 512);
                        memset(&esdi->buffer[3], 0xff, 512-6);
                        esdi->pos = 0;
                        esdi->status = STAT_DRQ | STAT_READY | STAT_DSC;
//                pclog("Read sector callback %i %i %i offset %08X %i left %i %02X\n",ide.sector,ide.cylinder,ide.head,addr,ide.secount,ide.spt,ide.atastat[ide.board]);
//                if (addr) output=3;
                        esdi_irq_raise(esdi);
                }
                break;
                
                case CMD_READ_PARAMETERS:
                if (!drive->hdd_file.f)
                {
                	esdi->status = STAT_READY | STAT_ERR | STAT_DSC;
                	esdi->error = ERR_ABRT;
                	esdi_irq_raise(esdi);
                }
                else
                {
                        memset(esdi->buffer, 0, 512);

                        esdi->buffer[0] = 0x44;                      /* general configuration */
                        esdi->buffer[1] = drive->hdd_file.tracks; /* number of non-removable cylinders */
                        esdi->buffer[2] = 0;                      /* number of removable cylinders */
                        esdi->buffer[3] = drive->hdd_file.hpc;    /* number of heads */
	                esdi->buffer[5] = esdi->buffer[4] * drive->hdd_file.spt; /* number of unformatted bytes/sector */
                        esdi->buffer[4] = 600; /* number of unformatted bytes/track */
                	esdi->buffer[6] = drive->hdd_file.spt; /* number of sectors */
                        esdi->buffer[7] = 0; /*minimum bytes in inter-sector gap*/
                        esdi->buffer[8] = 0; /* minimum bytes in postamble */
                        esdi->buffer[9] = 0; /* number of words of vendor status */
	/* controller info */
//	char	wdp_cnsn[20];		/* controller serial number */
                        esdi->buffer[20] = 2; 	/* controller type */
                	esdi->buffer[21] = 1; /* sector buffer size, in sectors */
                	esdi->buffer[22] = 0; /* ecc bytes appended */
//	char	wdp_rev[8];		/* firmware revision */
	//27
	                esdi->buffer[27] = 'W' | ('D' << 8);
	                esdi->buffer[28] = '1' | ('0' << 8);
	                esdi->buffer[29] = '0' | ('7' << 8);
	                esdi->buffer[30] = 'V' | ('-' << 8);
	                esdi->buffer[31] = 'S' | ('E' << 8);
	                esdi->buffer[32] = '1';
                	esdi->buffer[47] = 0; /* sectors per interrupt */
                	esdi->buffer[48] = 0;/* can use double word read/write? */
                        esdi->pos = 0;
                        esdi->status = STAT_DRQ | STAT_READY | STAT_DSC;
                        esdi_irq_raise(esdi);
                }
                break;
        
                default:
                pclog("ESDI Callback on unknown command %02x\n", esdi->command);
                case 0xe8:
        	esdi->status = STAT_READY | STAT_ERR | STAT_DSC;
        	esdi->error = ERR_ABRT;
        	esdi_irq_raise(esdi);
        	break;
        }
}

static void esdi_rom_write(uint32_t addr, uint8_t val, void *p)
{
        rom_t *rom = (rom_t *)p;
        
        addr &= rom->mask;
        
        if (addr >= 0x1f00 && addr < 0x2000)
                rom->rom[addr] = val;
}

void *wd1007vse1_init()
{
        esdi_t *esdi = malloc(sizeof(esdi_t));
        memset(esdi, 0, sizeof(esdi_t));

        hdd_load(&esdi->drives[0].hdd_file, 0, ide_fn[0]);
        hdd_load(&esdi->drives[1].hdd_file, 1, ide_fn[1]);

        esdi->drives[0].cfg_spt = esdi->drives[0].hdd_file.spt;
        esdi->drives[0].cfg_hpc = esdi->drives[0].hdd_file.hpc;
        esdi->drives[1].cfg_spt = esdi->drives[1].hdd_file.spt;
        esdi->drives[1].cfg_hpc = esdi->drives[1].hdd_file.hpc;

        esdi->status = STAT_READY | STAT_DSC;
        esdi->error = 1; /*No errors*/

        rom_init(&esdi->bios_rom, "62-000279-061.bin", 0xc8000, 0x4000, 0x3fff, 0, MEM_MAPPING_EXTERNAL);

        mem_mapping_set_handler(&esdi->bios_rom.mapping,
                    rom_read, rom_readw, rom_readl,
                    esdi_rom_write, NULL, NULL);
                            
        io_sethandler(0x01f0, 0x0001, esdi_read, esdi_readw, NULL, esdi_write, esdi_writew, NULL, esdi);
        io_sethandler(0x01f1, 0x0007, esdi_read, NULL,      NULL, esdi_write, NULL,       NULL, esdi);
        io_sethandler(0x03f6, 0x0001, NULL,     NULL,      NULL, esdi_write, NULL,       NULL, esdi);

        timer_add(&esdi->callback_timer, esdi_callback, esdi, 0);
        
	return esdi;
}

void wd1007vse1_close(void *p)
{
        esdi_t *esdi = (esdi_t *)p;
        
        hdd_close(&esdi->drives[0].hdd_file);
        hdd_close(&esdi->drives[1].hdd_file);

        free(esdi);
}

static int wd1007vse1_available()
{
        return rom_present("62-000279-061.bin");
}

device_t wd1007vse1_device =
{
        "Western Digital WD1007V-SE1 (ESDI)",
        DEVICE_AT,
        wd1007vse1_init,
        wd1007vse1_close,
        wd1007vse1_available,
        NULL,
        NULL,
        NULL,
        NULL
};
