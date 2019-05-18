#define IDE_TIME (10 * TIMER_USEC)

#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <sys/types.h>

#include "ibm.h"
#include "hdd.h"
#include "io.h"
#include "pic.h"
#include "timer.h"
#include "hdd_file.h"
#include "scsi.h"
#include "scsi_cd.h"
#include "scsi_zip.h"
#include "ide.h"

/* ATA Commands */
#define WIN_SRST			0x08 /* ATAPI Device Reset */
#define WIN_RECAL			0x10
#define WIN_RESTORE			WIN_RECAL
#define WIN_READ			0x20 /* 28-Bit Read */
#define WIN_READ_NORETRY                0x21 /* 28-Bit Read - no retry*/
#define WIN_WRITE			0x30 /* 28-Bit Write */
#define WIN_WRITE_NORETRY		0x31 /* 28-Bit Write */
#define WIN_VERIFY			0x40 /* 28-Bit Verify */
#define WIN_VERIFY_ONCE                 0x41 /* Deprecated command - same as 0x40 */
#define WIN_FORMAT			0x50
#define WIN_SEEK			0x70
#define WIN_DRIVE_DIAGNOSTICS           0x90 /* Execute Drive Diagnostics */
#define WIN_SPECIFY			0x91 /* Initialize Drive Parameters */
#define WIN_PACKETCMD			0xA0 /* Send a packet command. */
#define WIN_PIDENTIFY			0xA1 /* Identify ATAPI device */
#define WIN_READ_MULTIPLE               0xC4
#define WIN_WRITE_MULTIPLE              0xC5
#define WIN_SET_MULTIPLE_MODE           0xC6
#define WIN_READ_DMA                    0xC8
#define WIN_WRITE_DMA                   0xCA
#define WIN_SETIDLE1			0xE3
#define WIN_CHECK_POWER_MODE            0xE5
#define WIN_IDENTIFY			0xEC /* Ask drive to identify itself */
#define WIN_SET_FEATURES                0xEF

/** Evaluate to non-zero if the currently selected drive is an ATAPI device */
#define IDE_DRIVE_IS_CDROM(ide)  (ide->type == IDE_CDROM)


enum
{
        IDE_NONE = 0,
        IDE_HDD,
        IDE_CDROM
};

struct IDE;

typedef struct IDE
{
        int type;
        int board;
        uint8_t atastat;
        uint8_t error;
        int secount,sector,cylinder,head,drive,cylprecomp;
        uint8_t command;
        uint8_t fdisk;
        int pos;
        int reset;
        uint16_t buffer[65536];
        int irqstat;
        int service;
        int lba;
        uint32_t lba_addr;
        int skip512;
        int blocksize, blockcount;
        uint8_t sector_buffer[256*512];
        int do_initial_read;
        int sector_pos;
        hdd_file_t hdd_file;
        atapi_device_t atapi;
} IDE;

int cdrom_channel = 2;
int zip_channel = -1;

IDE ide_drives[7];

IDE *ext_ide;

char ide_fn[7][512];

int (*ide_bus_master_read_data)(int channel, uint8_t *data, int size, void *p);
int (*ide_bus_master_write_data)(int channel, uint8_t *data, int size, void *p);
void (*ide_bus_master_set_irq)(int channel, void *p);
void *ide_bus_master_p;

pc_timer_t ide_timer[2];

int cur_ide[2];

uint8_t getstat(IDE *ide) { return ide->atastat; }

void ide_irq_raise(IDE *ide)
{
//        pclog("IDE_IRQ_RAISE\n");
	if (!(ide->fdisk&2)) {
//                if (ide->board && !ide->irqstat) pclog("IDE_IRQ_RAISE\n");
                picint((ide->board)?(1<<15):(1<<14));
                if (ide_bus_master_set_irq)
                        ide_bus_master_set_irq(ide->board, ide_bus_master_p);
	}
	ide->irqstat=1;
        ide->service=1;
}

static inline void ide_irq_lower(IDE *ide)
{
//        pclog("IDE_IRQ_LOWER\n");
//	if (ide.board == 0) {
                picintc((ide->board)?(1<<15):(1<<14));
//	}
	ide->irqstat=0;
}

void ide_irq_update(IDE *ide)
{
	if (ide->irqstat && !((pic2.pend|pic2.ins)&0x40) && !(ide->fdisk & 2))
            picint((ide->board)?(1<<15):(1<<14));
        else if ((pic2.pend|pic2.ins)&0x40)
            picintc((ide->board)?(1<<15):(1<<14));
}
/**
 * Copy a string into a buffer, padding with spaces, and placing characters as
 * if they were packed into 16-bit values, stored little-endian.
 *
 * @param str Destination buffer
 * @param src Source string
 * @param len Length of destination buffer to fill in. Strings shorter than
 *            this length will be padded with spaces.
 */
void ide_padstr(char *str, const char *src, int len)
{
	int i, v;

	for (i = 0; i < len; i++) {
		if (*src != '\0') {
			v = *src++;
		} else {
			v = ' ';
		}
		str[i ^ 1] = v;
	}
}

/**
 * Fill in ide->buffer with the output of the "IDENTIFY DEVICE" command
 */
static void ide_identify(IDE *ide)
{
	memset(ide->buffer, 0, 512);

	//ide->buffer[1] = 101; /* Cylinders */

        if ((hdc[cur_ide[ide->board]].tracks * hdc[cur_ide[ide->board]].hpc * hdc[cur_ide[ide->board]].spt) >= 16514064)
                ide->buffer[1] = 16383;
        else
        	ide->buffer[1] = hdc[cur_ide[ide->board]].tracks; /* Cylinders */
	ide->buffer[3] = hdc[cur_ide[ide->board]].hpc;  /* Heads */
	ide->buffer[6] = hdc[cur_ide[ide->board]].spt;  /* Sectors */

	ide_padstr((char *) (ide->buffer + 10), "", 20); /* Serial Number */
	ide_padstr((char *) (ide->buffer + 23), "v1.0", 8); /* Firmware */
	ide_padstr((char *) (ide->buffer + 27), "PCemHD", 40); /* Model */

        ide->buffer[20] = 3;   /*Buffer type*/
        ide->buffer[21] = 512; /*Buffer size*/
        ide->buffer[47] = 16;  /*Max sectors on multiple transfer command*/
        ide->buffer[48] = 1;   /*Dword transfers supported*/
	ide->buffer[49] = (1 << 9) | (1 << 8); /* LBA and DMA supported */
	ide->buffer[50] = 0x4000; /* Capabilities */
	ide->buffer[51] = 2 << 8; /*PIO timing mode*/
	ide->buffer[52] = 2 << 8; /*DMA timing mode*/
	ide->buffer[59] = ide->blocksize ? (ide->blocksize | 0x100) : 0;
	ide->buffer[60] = (hdc[cur_ide[ide->board]].tracks * hdc[cur_ide[ide->board]].hpc * hdc[cur_ide[ide->board]].spt) & 0xFFFF; /* Total addressable sectors (LBA) */
	ide->buffer[61] = (hdc[cur_ide[ide->board]].tracks * hdc[cur_ide[ide->board]].hpc * hdc[cur_ide[ide->board]].spt) >> 16;
	ide->buffer[63] = 7; /*Multiword DMA*/
	ide->buffer[80] = 0xe; /*ATA-1 to ATA-3 supported*/
}

/*
 * Return the sector offset for the current register values
 */
static off64_t ide_get_sector(IDE *ide)
{
        if (ide->lba)
        {
                return (off64_t)ide->lba_addr + ide->skip512;
        }
        else
        {
        	int heads = ide->hdd_file.hpc;
        	int sectors = ide->hdd_file.spt;

        	return ((((off64_t) ide->cylinder * heads) + ide->head) *
        	          sectors) + (ide->sector - 1) + ide->skip512;
        }
}

/**
 * Move to the next sector using CHS addressing
 */
static void ide_next_sector(IDE *ide)
{
        if (ide->lba)
        {
                ide->lba_addr++;
        }
        else
        {
        	ide->sector++;

        	if (ide->sector == (ide->hdd_file.spt + 1)) {
        		ide->sector = 1;
        		ide->head++;

        		if (ide->head == ide->hdd_file.hpc) {
        			ide->head = 0;
        			ide->cylinder++;
                        }
		}
	}
}

static void loadhd(IDE *ide, int d, const char *fn)
{
        hdd_load(&ide->hdd_file, d, fn);
        
        if (ide->hdd_file.f)
                ide->type = IDE_HDD;
        else
                ide->type = IDE_NONE;
}

void ide_set_signature(IDE *ide)
{
	ide->secount=1;
	ide->sector=1;
	ide->head=0;
	ide->cylinder=(IDE_DRIVE_IS_CDROM(ide) ? 0xEB14 : ((ide->type == IDE_HDD) ? 0 : 0xFFFF));
//	if (ide->type == IDE_HDD)  ide->drive = 0;
}

void resetide(void)
{
        int d;

        /* Close hard disk image files (if previously open) */
        for (d = 0; d < 4; d++) {
                ide_drives[d].type = IDE_NONE;
                hdd_close(&ide_drives[d].hdd_file);

                ide_drives[d].atastat = READY_STAT | DSC_STAT;
                ide_drives[d].service = 0;
                ide_drives[d].board = (d & 2) ? 1 : 0;
        }
		
        if (hdd_controller_current_is_ide())
        {
        	for (d = 0; d < 4; d++)
        	{
        	        ide_drives[d].drive = d;

        		if (cdrom_channel == d)
        		{
        			ide_drives[d].type = IDE_CDROM;
        			scsi_bus_atapi_init(&ide_drives[d].atapi.bus, &scsi_cd, d, &ide_drives[d].atapi);
        		}
        		else if (zip_channel == d)
        		{
                                ide_drives[d].type = IDE_CDROM;
                                scsi_bus_atapi_init(&ide_drives[d].atapi.bus, &scsi_zip, d, &ide_drives[d].atapi);
                        }
                        else
        		{
        			loadhd(&ide_drives[d], d, ide_fn[d]);
        		}
			
        		ide_set_signature(&ide_drives[d]);
        		
        		ide_drives[d].atapi.ide = &ide_drives[d];
                        ide_drives[d].atapi.board = (d & 2) ? 1 : 0;
                        ide_drives[d].atapi.atastat = &ide_drives[d].atastat;
                        ide_drives[d].atapi.error = &ide_drives[d].error;
                        ide_drives[d].atapi.cylinder = &ide_drives[d].cylinder;
                }
	}


        cur_ide[0] = 0;
        cur_ide[1] = 2;
}


int idetimes=0;
void writeidew(int ide_board, uint16_t val)
{
        IDE *ide = &ide_drives[cur_ide[ide_board]];
        
        if (ide->command == WIN_PACKETCMD)
        {
                atapi_data_write(&ide->atapi, val);
        }
        else
        {
                ide->buffer[ide->pos >> 1] = val;
                ide->pos += 2;

                if (ide->pos>=512)
                {
                        ide->pos=0;
                        ide->atastat = BUSY_STAT;
                        if (ide->command == WIN_WRITE_MULTIPLE)
                                callbackide(ide_board);
                        else
                      	        timer_set_delay_u64(&ide_timer[ide_board], 6 * IDE_TIME);
                }
        }
}

void writeidel(int ide_board, uint32_t val)
{
//        pclog("WriteIDEl %08X\n", val);
        writeidew(ide_board, val);
        writeidew(ide_board, val >> 16);
}

void writeide(int ide_board, uint16_t addr, uint8_t val)
{
        IDE *ide = &ide_drives[cur_ide[ide_board]];
        IDE *ide_other = &ide_drives[cur_ide[ide_board] ^ 1];

/*        if (ide_board && (cr0&1) && !(eflags&VM_FLAG))
        {
//                pclog("Failed write IDE %04X:%08X\n",CS,pc);
                return;
        }*/
//        if ((cr0&1) && !(eflags&VM_FLAG))
//         if (ide_board)
//                pclog("WriteIDE %04X %02X from %04X(%08X):%08X %i\n", addr, val, CS, cs, cpu_state.pc, ins);
//        return;
        addr|=0x80;
//        if (ide_board) pclog("Write IDEb %04X %02X %04X(%08X):%04X %i  %02X %02X\n",addr,val,CS,cs,pc,ins,ide->atastat,ide_drives[0].atastat);
        /*if (idedebug) */
//        pclog("Write IDE %08X %02X %04X:%08X\n",addr,val,CS,pc);
//        int c;
//      rpclog("Write IDE %08X %02X %08X %08X\n",addr,val,PC,armregs[12]);

        if (ide->type == IDE_NONE && (addr == 0x1f0 || addr == 0x1f7)) return;
        
        switch (addr)
        {
        case 0x1F0: /* Data */
                writeidew(ide_board, val | (val << 8));
                return;

        case 0x1F1: /* Features */
                ide->cylprecomp = val;
                ide_other->cylprecomp = val;
                return;

        case 0x1F2: /* Sector count */
                ide->secount = val;
                ide_other->secount = val;
                return;

        case 0x1F3: /* Sector */
                ide->sector = val;
                ide->lba_addr = (ide->lba_addr & 0xFFFFF00) | val;
                ide_other->sector = val;
                ide_other->lba_addr = (ide_other->lba_addr & 0xFFFFF00) | val;
                return;

        case 0x1F4: /* Cylinder low */
                ide->cylinder = (ide->cylinder & 0xFF00) | val;
                ide->lba_addr = (ide->lba_addr & 0xFFF00FF) | (val << 8);
                ide_other->cylinder = (ide_other->cylinder&0xFF00) | val;
                ide_other->lba_addr = (ide_other->lba_addr&0xFFF00FF) | (val << 8);
//                pclog("Write cylinder low %02X\n",val);
                return;

        case 0x1F5: /* Cylinder high */
                ide->cylinder = (ide->cylinder & 0xFF) | (val << 8);
                ide->lba_addr = (ide->lba_addr & 0xF00FFFF) | (val << 16);
                ide_other->cylinder = (ide_other->cylinder & 0xFF) | (val << 8);
                ide_other->lba_addr = (ide_other->lba_addr & 0xF00FFFF) | (val << 16);
                return;

        case 0x1F6: /* Drive/Head */
/*        if (val==0xB0)
        {
                dumpregs();
                exit(-1);
        }*/
        
                if (cur_ide[ide_board] != ((val>>4)&1)+(ide_board<<1))
                {
                        cur_ide[ide_board]=((val>>4)&1)+(ide_board<<1);

                        if (ide->reset || ide_other->reset)
                        {
                                ide->atastat = ide_other->atastat = READY_STAT | DSC_STAT;
                                ide->error = ide_other->error = 1;
                                ide->secount = ide_other->secount = 1;
                                ide->sector = ide_other->sector = 1;
                                ide->head = ide_other->head = 0;
                                ide->cylinder = ide_other->cylinder = 0;
                                ide->reset = ide_other->reset = 0;
                                // ide->blocksize = ide_other->blocksize = 0;
                                if (IDE_DRIVE_IS_CDROM(ide))
                                        ide->cylinder=0xEB14;
                                if (IDE_DRIVE_IS_CDROM(ide_other))
                                        ide_other->cylinder=0xEB14;

                                timer_disable(&ide_timer[ide_board]);
                                return;
                        }

                        ide = &ide_drives[cur_ide[ide_board]];
                }
                                
                ide->head = val & 0xF;
                ide->lba = val & 0x40;
                ide_other->head = val & 0xF;
                ide_other->lba = val & 0x40;
                
                ide->lba_addr = (ide->lba_addr & 0x0FFFFFF) | ((val & 0xF) << 24);
                ide_other->lba_addr = (ide_other->lba_addr & 0x0FFFFFF)|((val & 0xF) << 24);
                                
                ide_irq_update(ide);
                return;

        case 0x1F7: /* Command register */
        if (ide->type == IDE_NONE) return;
//                pclog("IDE command %02X drive %i\n",val,ide.drive);
        ide_irq_lower(ide);
                ide->command=val;
                
//                pclog("New IDE command - %02X %i %i\n",ide->command,cur_ide[ide_board],ide_board);
                ide->error=0;
                switch (val)
                {
                case WIN_SRST: /* ATAPI Device Reset */
                        if (IDE_DRIVE_IS_CDROM(ide)) ide->atastat = BUSY_STAT;
                        else                         ide->atastat = READY_STAT;
                        timer_set_delay_u64(&ide_timer[ide_board], 100*IDE_TIME);
                        return;

                case WIN_RESTORE:
                case WIN_SEEK:
//                        pclog("WIN_RESTORE start\n");
                        ide->atastat = READY_STAT;
                        timer_set_delay_u64(&ide_timer[ide_board], 100*IDE_TIME);
                        return;

                case WIN_READ_MULTIPLE:
			if (!ide->blocksize && (ide->type != IDE_CDROM))
                           fatal("READ_MULTIPLE - blocksize = 0\n");
#if 0
                        if (ide->lba) pclog("Read Multiple %i sectors from LBA addr %07X\n",ide->secount,ide->lba_addr);
                        else          pclog("Read Multiple %i sectors from sector %i cylinder %i head %i  %i\n",ide->secount,ide->sector,ide->cylinder,ide->head,ins);
#endif
                        ide->blockcount = 0;
                        
                case WIN_READ:
                case WIN_READ_NORETRY:
                case WIN_READ_DMA:
/*                        if (ide.secount>1)
                        {
                                fatal("Read %i sectors from sector %i cylinder %i head %i\n",ide.secount,ide.sector,ide.cylinder,ide.head);
                        }*/
#if 0
                        if (ide->lba) pclog("Read %i sectors from LBA addr %07X\n",ide->secount,ide->lba_addr);
                        else          pclog("Read %i sectors from sector %i cylinder %i head %i  %i\n",ide->secount,ide->sector,ide->cylinder,ide->head,ins);
#endif
                        ide->atastat = BUSY_STAT;
                        timer_set_delay_u64(&ide_timer[ide_board], 200*IDE_TIME);
                        ide->do_initial_read = 1;
                        return;
                        
                case WIN_WRITE_MULTIPLE:
                        if (!ide->blocksize && (ide->type != IDE_CDROM))
                           fatal("Write_MULTIPLE - blocksize = 0\n");
#if 0
                        if (ide->lba) pclog("Write Multiple %i sectors from LBA addr %07X\n",ide->secount,ide->lba_addr);
                        else          pclog("Write Multiple %i sectors to sector %i cylinder %i head %i\n",ide->secount,ide->sector,ide->cylinder,ide->head);
#endif
                        ide->blockcount = 0;
                        
                case WIN_WRITE:
                case WIN_WRITE_NORETRY:
                /*                        if (ide.secount>1)
                        {
                                fatal("Write %i sectors to sector %i cylinder %i head %i\n",ide.secount,ide.sector,ide.cylinder,ide.head);
                        }*/
#if 0
                        if (ide->lba) pclog("Write %i sectors from LBA addr %07X\n",ide->secount,ide->lba_addr);
                        else          pclog("Write %i sectors to sector %i cylinder %i head %i\n",ide->secount,ide->sector,ide->cylinder,ide->head);
#endif
                        ide->atastat = DRQ_STAT | DSC_STAT | READY_STAT;
                        ide->pos=0;
                        return;

                case WIN_WRITE_DMA:
#if 0
                        if (ide->lba) pclog("Write %i sectors from LBA addr %07X\n",ide->secount,ide->lba_addr);
                        else          pclog("Write %i sectors to sector %i cylinder %i head %i\n",ide->secount,ide->sector,ide->cylinder,ide->head);
#endif
                        ide->atastat = BUSY_STAT;
                        timer_set_delay_u64(&ide_timer[ide_board], 200*IDE_TIME);
                        return;

                case WIN_VERIFY:
                case WIN_VERIFY_ONCE:
#if 0
                        if (ide->lba) pclog("Read verify %i sectors from LBA addr %07X\n",ide->secount,ide->lba_addr);
                        else          pclog("Read verify %i sectors from sector %i cylinder %i head %i\n",ide->secount,ide->sector,ide->cylinder,ide->head);
#endif
                        ide->atastat = BUSY_STAT;
                        timer_set_delay_u64(&ide_timer[ide_board], 200*IDE_TIME);
                        return;

                case WIN_FORMAT:
//                        pclog("Format track %i head %i\n",ide.cylinder,ide.head);
                        ide->atastat = DRQ_STAT;
//                        idecallback[ide_board]=200;
                        ide->pos=0;
                        return;

                case WIN_SPECIFY: /* Initialize Drive Parameters */
                        ide->atastat = BUSY_STAT;
                        timer_set_delay_u64(&ide_timer[ide_board], 30*IDE_TIME);
//                        pclog("SPECIFY\n");
//                        output=1;
                        return;

                case WIN_DRIVE_DIAGNOSTICS: /* Execute Drive Diagnostics */
                        ide->atastat = BUSY_STAT;
                        timer_set_delay_u64(&ide_timer[ide_board], 200*IDE_TIME);
                        return;

                case WIN_PIDENTIFY: /* Identify Packet Device */
                case WIN_SET_MULTIPLE_MODE: /*Set Multiple Mode*/
//                output=1;
                case WIN_SETIDLE1: /* Idle */
                case WIN_CHECK_POWER_MODE:
                        ide->atastat = BUSY_STAT;
                        callbackide(ide_board);
//                        idecallback[ide_board]=200*IDE_TIME;
                        return;

                case WIN_IDENTIFY: /* Identify Device */
                case WIN_SET_FEATURES:
//                        output=3;
//                        timetolive=500;
                        ide->atastat = BUSY_STAT;
                        timer_set_delay_u64(&ide_timer[ide_board], 200*IDE_TIME);
                        return;

                case WIN_PACKETCMD: /* ATAPI Packet */
                        if (ide->type == IDE_CDROM)
                                atapi_command_start(&ide->atapi, ide->cylprecomp);

                        ide->atastat = BUSY_STAT;
                        timer_set_delay_u64(&ide_timer[ide_board], IDE_TIME);
                        
                        ide->atapi.bus_state = 0;
                        return;
                        
                case 0xF0:
                        default:
                	ide->atastat = READY_STAT | ERR_STAT | DSC_STAT;
                	ide->error = ABRT_ERR;
                        ide_irq_raise(ide);
/*                        fatal("Bad IDE command %02X\n", val);*/
                        return;
                }
                
                return;

        case 0x3F6: /* Device control */
                if ((ide->fdisk&4) && !(val&4) && (ide->type != IDE_NONE || ide_other->type != IDE_NONE))
                {
                        timer_set_delay_u64(&ide_timer[ide_board], 500*IDE_TIME);
                        ide->reset = ide_other->reset = 1;
                        ide->atastat = ide_other->atastat = BUSY_STAT;
//                        pclog("IDE Reset %i\n", ide_board);
                }
                if (val & 4)
                {
                        /*Drive held in reset*/
			timer_disable(&ide_timer[ide_board]);
                        ide->atastat = ide_other->atastat = BUSY_STAT;
                }
                ide->fdisk = ide_other->fdisk = val;
                ide_irq_update(ide);
                return;
        }
//        fatal("Bad IDE write %04X %02X\n", addr, val);
}

uint8_t readide(int ide_board, uint16_t addr)
{
        IDE *ide = &ide_drives[cur_ide[ide_board]];
        uint8_t temp = 0xff;
        uint16_t tempw;

        addr|=0x80;

/*        if (ide_board && (cr0&1) && !(eflags&VM_FLAG))
        {
//                pclog("Failed read IDE %04X:%08X\n",CS,pc);
                return 0xFF;
        }*/
//        if ((cr0&1) && !(eflags&VM_FLAG))
//         pclog("ReadIDE %04X  from %04X(%08X):%08X\n", addr, CS, cs, pc);
//        return 0xFF;

        if (ide->type == IDE_NONE && (addr == 0x1f0 || addr == 0x1f7)) return 0;
//        /*if (addr!=0x1F7 && addr!=0x3F6) */pclog("Read IDEb %04X %02X %02X %i %04X:%04X %i  %04X\n",addr,ide->atastat,(ide->atastat & ~DSC_STAT) | (ide->service ? SERVICE_STAT : 0),cur_ide[ide_board],CS,pc,ide_board, BX);
//rpclog("Read IDE %08X %08X %02X\n",addr,PC,iomd.irqb.mask);
        switch (addr)
        {
        case 0x1F0: /* Data */
                tempw = readidew(ide_board);
//                pclog("Read IDEW %04X\n", tempw);                
                temp = tempw & 0xff;
                break;
                
        case 0x1F1: /* Error */
//        pclog("Read error %02X\n",ide.error);
                temp = ide->error;
                break;

        case 0x1F2: /* Sector count */
//        pclog("Read sector count %02X\n",ide->secount);
                if (ide->type == IDE_CDROM && ide->command == WIN_PACKETCMD)
                        temp = atapi_read_iir(&ide->atapi);
                else
                        temp = (uint8_t)ide->secount;
                break;

        case 0x1F3: /* Sector */
                temp = (uint8_t)ide->sector;
                break;

        case 0x1F4: /* Cylinder low */
//        pclog("Read cyl low %02X\n",ide.cylinder&0xFF);
                temp = (uint8_t)(ide->cylinder&0xFF);
                break;

        case 0x1F5: /* Cylinder high */
//        pclog("Read cyl low %02X\n",ide.cylinder>>8);
                temp = (uint8_t)(ide->cylinder>>8);
                break;

        case 0x1F6: /* Drive/Head */
                temp = (uint8_t)(ide->head | ((cur_ide[ide_board] & 1) ? 0x10 : 0) | (ide->lba ? 0x40 : 0) | 0xa0);
                break;

        case 0x1F7: /* Status */
                if (ide->type == IDE_NONE)
                {
//                        pclog("Return status 00\n");
                        temp = 0;
                        break;
                }
                ide_irq_lower(ide);
                if (ide->type == IDE_CDROM)
                {
//                        pclog("Read CDROM status %02X\n",(ide->atastat & ~DSC_STAT) | (ide->service ? SERVICE_STAT : 0));
                        temp = (ide->atastat & ~DSC_STAT) | (ide->service ? SERVICE_STAT : 0);
                        if (ide->command == WIN_PACKETCMD)
                        {
                                if (atapi_read_drq(&ide->atapi))
                                        temp |= DRQ_STAT;
                                else
                                        temp &= ~DRQ_STAT;
                        }
                }
                else
                {
//                 && ide->service) return ide.atastat[ide.board]|SERVICE_STAT;
//                pclog("Return status %02X %04X:%04X %02X %02X\n",ide->atastat, CS ,pc, AH, BH);
                        temp = ide->atastat;
                }
                break;

        case 0x3F6: /* Alternate Status */
//        pclog("3F6 read %02X\n",ide.atastat[ide.board]);
//        if (output) output=0;
                if (ide->type == IDE_NONE)
                {
//                        pclog("Return status 00\n");
                        temp = 0;
                        break;
                }
                if (ide->type == IDE_CDROM)
                {
//                        pclog("Read CDROM status %02X\n",(ide->atastat & ~DSC_STAT) | (ide->service ? SERVICE_STAT : 0));
                        temp = (ide->atastat & ~DSC_STAT) | (ide->service ? SERVICE_STAT : 0);
                        if (ide->command == WIN_PACKETCMD)
                        {
                                if (atapi_read_drq(&ide->atapi))
                                        temp |= DRQ_STAT;
                                else
                                        temp &= ~DRQ_STAT;
                        }
                }
                else
                {
//                 && ide->service) return ide.atastat[ide.board]|SERVICE_STAT;
//                pclog("Return status %02X\n",ide->atastat);
                        temp = ide->atastat;
                }
                break;
        }
//        if (ide_board) pclog("Read IDEb %04X %02X   %02X %02X %i %04X:%04X %i\n", addr, temp, ide->atastat,(ide->atastat & ~DSC_STAT) | (ide->service ? SERVICE_STAT : 0),cur_ide[ide_board],CS,cpu_state.pc,ide_board);
        return temp;
//        fatal("Bad IDE read %04X\n", addr);
}

uint16_t readidew(int ide_board)
{
        IDE *ide = &ide_drives[cur_ide[ide_board]];
        uint16_t temp;

/*        if (ide_board && (cr0&1) && !(eflags&VM_FLAG))
        {
//                pclog("Failed read IDEw %04X:%08X\n",CS,pc);
                return 0xFFFF;
        }*/
//        return 0xFFFF;
//        pclog("Read IDEw %04X %04X:%04X %02X %i %i\n",ide->buffer[ide->pos >> 1],CS,pc,opcode,ins, ide->pos);
        
        if (ide->command == WIN_PACKETCMD)
        {
                temp = atapi_data_read(&ide->atapi);
        }
        else
        {
                temp = ide->buffer[ide->pos >> 1];
                ide->pos += 2;
                if (ide->pos >= 512 && ide->command)
                {
//                        pclog("Over! packlen %i %i\n",ide->packlen,ide->pos);
                        ide->pos = 0;
                        ide->atastat = READY_STAT | DSC_STAT;
                        if (ide->command == WIN_READ || ide->command == WIN_READ_NORETRY || ide->command == WIN_READ_MULTIPLE)
                        {
                                ide->secount = (ide->secount - 1) & 0xff;
                                if (ide->secount)
                                {
                                        ide_next_sector(ide);
                                        ide->atastat = BUSY_STAT;
                                        if (ide->command == WIN_READ_MULTIPLE)
                                                callbackide(ide_board);
                                        else
                                                timer_set_delay_u64(&ide_timer[ide_board], 6 * IDE_TIME);
//                                        pclog("set idecallback\n");
//                                        callbackide(ide_board);
                                }
//                                else
//                                   pclog("readidew done %02X\n", ide->atastat);
                        }
                }
        }
//        pclog("Read IDEw %04X\n",temp);
        return temp;
}

uint32_t readidel(int ide_board)
{
        uint16_t temp;
//        pclog("Read IDEl %i\n", ide_board);
        temp = readidew(ide_board);
        return temp | (readidew(ide_board) << 16);
}

int times30=0;
void callbackide(int ide_board)
{
        IDE *ide = &ide_drives[cur_ide[ide_board]];
        IDE *ide_other = &ide_drives[cur_ide[ide_board] ^ 1];
        atapi_device_t *atapi_dev = &ide->atapi;

        ext_ide = ide;
//        return;
        if (ide->command==0x30) times30++;
//        if (times30==2240) output=1;
        //if (times30==2471 && ide->command==0xA0) output=1;
///*if (ide_board) */pclog("CALLBACK %02X %i %i  %i\n",ide->command,times30,ide->reset,cur_ide[ide_board]);
//        if (times30==1294)
//                output=1;
        if (ide->reset)
        {
                ide->atastat = ide_other->atastat = READY_STAT | DSC_STAT;
                ide->error = ide_other->error = 1;
                ide->secount = ide_other->secount = 1;
                ide->sector = ide_other->sector = 1;
                ide->head = ide_other->head = 0;
                ide->cylinder = ide_other->cylinder = 0;
                ide->reset = ide_other->reset = 0;
                if (IDE_DRIVE_IS_CDROM(ide))
                {
                        ide->cylinder=0xEB14;
                        atapi->stop();
                        atapi_reset(&ide->atapi);
                }
                if (ide->type == IDE_NONE)
                {
                        ide->cylinder=0xFFFF;
                        ide->error = 0xff;
                        atapi->stop();
                }
                if (IDE_DRIVE_IS_CDROM(ide_other))
                {
                        ide_other->cylinder=0xEB14;
                        atapi->stop();
                        atapi_reset(&ide_other->atapi);
                }
                if (ide_other->type == IDE_NONE)
                {
                        ide_other->cylinder=0xFFFF;
                        ide_other->error = 0xff;
                        atapi->stop();
                }
//                pclog("Reset callback\n");
                return;
        }
        switch (ide->command)
        {
                //Initialize the Task File Registers as follows: Status = 00h, Error = 01h, Sector Count = 01h, Sector Number = 01h, Cylinder Low = 14h, Cylinder High =EBh and Drive/Head = 00h.
        case WIN_SRST: /*ATAPI Device Reset */
                ide->atastat = READY_STAT | DSC_STAT;
                ide->error=1; /*Device passed*/
                ide->secount = ide->sector = 1;
		ide_set_signature(ide);
		if (IDE_DRIVE_IS_CDROM(ide))
			ide->atastat = 0;
                ide_irq_raise(ide);
                if (IDE_DRIVE_IS_CDROM(ide))
                   ide->service = 0;
                return;

        case WIN_RESTORE:
        case WIN_SEEK:
                if (IDE_DRIVE_IS_CDROM(ide)) {
                        pclog("WIN_RESTORE callback on CD-ROM\n");
                        goto abort_cmd;
                }
//                pclog("WIN_RESTORE callback\n");
                ide->atastat = READY_STAT | DSC_STAT;
                ide_irq_raise(ide);
                return;

        case WIN_READ:
        case WIN_READ_NORETRY:
                if (IDE_DRIVE_IS_CDROM(ide)) {
			ide_set_signature(ide);
                        goto abort_cmd;
                }
                if (ide->do_initial_read)
                {
                        ide->do_initial_read = 0;
                        ide->sector_pos = 0;
                        hdd_read_sectors(&ide->hdd_file, ide_get_sector(ide), ide->secount ? ide->secount : 256, ide->sector_buffer);
                }                        
                memcpy(ide->buffer, &ide->sector_buffer[ide->sector_pos*512], 512);
                ide->sector_pos++;
//                pclog("Read %i %i %i %08X\n",ide.cylinder,ide.head,ide.sector,addr);
                /*                if (ide.cylinder || ide.head)
                {
                        fatal("Read from other cylinder/head");
                }*/
                ide->pos=0;
                ide->atastat = DRQ_STAT | READY_STAT | DSC_STAT;
//                pclog("Read sector callback %i %i %i offset %08X %i left %i %02X\n",ide.sector,ide.cylinder,ide.head,addr,ide.secount,ide.spt,ide.atastat[ide.board]);
//                if (addr) output=3;
                ide_irq_raise(ide);

                readflash_set(READFLASH_HDC, ide->drive);
                return;

        case WIN_READ_DMA:
                if (IDE_DRIVE_IS_CDROM(ide)) {
                        goto abort_cmd;
                }
                if (ide->do_initial_read)
                {
                        ide->do_initial_read = 0;
                        ide->sector_pos = 0;
                        hdd_read_sectors(&ide->hdd_file, ide_get_sector(ide), ide->secount ? ide->secount : 256, ide->sector_buffer);
                }                        
                ide->pos=0;
                
                if (ide_bus_master_read_data)
                {
                        if (ide_bus_master_read_data(ide_board, &ide->sector_buffer[ide->sector_pos*512], 512, ide_bus_master_p))
                                timer_set_delay_u64(&ide_timer[ide_board], 6*IDE_TIME);           /*DMA not performed, try again later*/
                        else
                        {
                                /*DMA successful*/
                                ide->sector_pos++;                                
                                ide->atastat = DRQ_STAT | READY_STAT | DSC_STAT;

                                ide->secount = (ide->secount - 1) & 0xff;
                                if (ide->secount)
                                {
                                        ide_next_sector(ide);
                                        ide->atastat = BUSY_STAT;
                                        timer_set_delay_u64(&ide_timer[ide_board], 6*IDE_TIME);
                                }
                                else
                                {
                                        ide_irq_raise(ide);
                                }
                        }
                }

                readflash_set(READFLASH_HDC, ide->drive);
                return;

        case WIN_READ_MULTIPLE:
                if (IDE_DRIVE_IS_CDROM(ide)) {
                        goto abort_cmd;
                }
                if (ide->do_initial_read)
                {
                        ide->do_initial_read = 0;
                        ide->sector_pos = 0;
                        hdd_read_sectors(&ide->hdd_file, ide_get_sector(ide), ide->secount ? ide->secount : 256, ide->sector_buffer);
                }                        
                memcpy(ide->buffer, &ide->sector_buffer[ide->sector_pos*512], 512);
                ide->sector_pos++;
                ide->pos=0;
                ide->atastat = DRQ_STAT | READY_STAT | DSC_STAT;
                if (!ide->blockcount)// || ide->secount == 1)
                {
//                        pclog("Read multiple int\n");
                        ide_irq_raise(ide);
                }                        
                ide->blockcount++;
                if (ide->blockcount >= ide->blocksize)
                   ide->blockcount = 0;

                readflash_set(READFLASH_HDC, ide->drive);
                return;

        case WIN_WRITE:
        case WIN_WRITE_NORETRY:
                if (IDE_DRIVE_IS_CDROM(ide)) {
                        goto abort_cmd;
                }
                hdd_write_sectors(&ide->hdd_file, ide_get_sector(ide), 1, ide->buffer);
                ide_irq_raise(ide);
                ide->secount = (ide->secount - 1) & 0xff;
                if (ide->secount)
                {
                        ide->atastat = DRQ_STAT | READY_STAT | DSC_STAT;
                        ide->pos=0;
                        ide_next_sector(ide);
                }
                else
                   ide->atastat = READY_STAT | DSC_STAT;

                readflash_set(READFLASH_HDC, ide->drive);
                return;
                
        case WIN_WRITE_DMA:
                if (IDE_DRIVE_IS_CDROM(ide)) {
                        goto abort_cmd;
                }

                if (ide_bus_master_write_data)
                {
                        if (ide_bus_master_write_data(ide_board, (uint8_t *)ide->buffer, 512, ide_bus_master_p))
                        	timer_set_delay_u64(&ide_timer[ide_board], 6*IDE_TIME);           /*DMA not performed, try again later*/
                        else
                        {
                                /*DMA successful*/
                                hdd_write_sectors(&ide->hdd_file, ide_get_sector(ide), 1, ide->buffer);
                                
                                ide->atastat = DRQ_STAT | READY_STAT | DSC_STAT;

                                ide->secount = (ide->secount - 1) & 0xff;
                                if (ide->secount)
                                {
                                        ide_next_sector(ide);
                                        ide->atastat = BUSY_STAT;
                                        timer_set_delay_u64(&ide_timer[ide_board], 6*IDE_TIME);
                                }
                                else
                                {
                                        ide_irq_raise(ide);
                                }
                        }
                }

                readflash_set(READFLASH_HDC, ide->drive);
                return;

        case WIN_WRITE_MULTIPLE:
                if (IDE_DRIVE_IS_CDROM(ide)) {
                        goto abort_cmd;
                }
                hdd_write_sectors(&ide->hdd_file, ide_get_sector(ide), 1, ide->buffer);
                ide->blockcount++;
                if (ide->blockcount >= ide->blocksize || ide->secount == 1)
                {
                        ide->blockcount = 0;
                        ide_irq_raise(ide);
                }
                ide->secount = (ide->secount - 1) & 0xff;
                if (ide->secount)
                {
                        ide->atastat = DRQ_STAT | READY_STAT | DSC_STAT;
                        ide->pos=0;
                        ide_next_sector(ide);
                }
                else
                   ide->atastat = READY_STAT | DSC_STAT;

                readflash_set(READFLASH_HDC, ide->drive);
                return;

        case WIN_VERIFY:
        case WIN_VERIFY_ONCE:
                if (IDE_DRIVE_IS_CDROM(ide)) {
                        goto abort_cmd;
                }
                ide->pos=0;
                ide->atastat = READY_STAT | DSC_STAT;
//                pclog("Read verify callback %i %i %i offset %08X %i left\n",ide.sector,ide.cylinder,ide.head,addr,ide.secount);
                ide_irq_raise(ide);

                readflash_set(READFLASH_HDC, ide->drive);
                return;

        case WIN_FORMAT:
                if (IDE_DRIVE_IS_CDROM(ide)) {
                        goto abort_cmd;
                }
                hdd_format_sectors(&ide->hdd_file, ide_get_sector(ide), ide->secount);
                ide->atastat = READY_STAT | DSC_STAT;
                ide_irq_raise(ide);

                readflash_set(READFLASH_HDC, ide->drive);
                return;

        case WIN_DRIVE_DIAGNOSTICS:
		ide_set_signature(ide);
                ide->error=1; /*No error detected*/
		if (IDE_DRIVE_IS_CDROM(ide))
		{
			ide->atastat = 0;
		}
		else
		{
			ide->atastat = READY_STAT | DSC_STAT;
			ide_irq_raise(ide);
		}
                return;

        case WIN_SPECIFY: /* Initialize Drive Parameters */
                if (IDE_DRIVE_IS_CDROM(ide)) {
                        pclog("IS CDROM - ABORT\n");
                        goto abort_cmd;
                }
                ide->hdd_file.spt = ide->secount;
                ide->hdd_file.hpc = ide->head+1;
                ide->atastat = READY_STAT | DSC_STAT;
//                pclog("SPECIFY - %i sectors per track, %i heads per cylinder  %i %i\n",ide->spt,ide->hpc,cur_ide[ide_board],ide_board);
                ide_irq_raise(ide);
                return;

        case WIN_PIDENTIFY: /* Identify Packet Device */
                if (IDE_DRIVE_IS_CDROM(ide)) {
//                        pclog("ATAPI identify\n");
                        atapi_dev->bus.devices[0]->atapi_identify(ide->buffer, atapi_dev->bus.device_data[0]);
                        
                        ide->pos=0;
                        ide->error=0;
                        ide->atastat = DRQ_STAT | READY_STAT | DSC_STAT;
                        ide_irq_raise(ide);
                        return;
                }
//                pclog("Not ATAPI\n");
                goto abort_cmd;

        case WIN_SET_MULTIPLE_MODE:
                if (IDE_DRIVE_IS_CDROM(ide)) {
                        pclog("IS CDROM - ABORT\n");
                        goto abort_cmd;
                }
                ide->blocksize = ide->secount;
                ide->atastat = READY_STAT | DSC_STAT;
                pclog("Set multiple mode - %i\n", ide->blocksize);
                ide_irq_raise(ide);
                return;
                
        case WIN_SETIDLE1: /* Idle */
                goto abort_cmd;

        case WIN_IDENTIFY: /* Identify Device */
		if (ide->type == IDE_NONE)
		{
			goto abort_cmd;
		}
                if (IDE_DRIVE_IS_CDROM(ide))
		{
			ide_set_signature(ide);
			goto abort_cmd;
		}
		else
		{
                        ide_identify(ide);
                        ide->pos=0;
                        ide->atastat = DRQ_STAT | READY_STAT | DSC_STAT;
//                pclog("ID callback\n");
                        ide_irq_raise(ide);
                }
                return;

        case WIN_SET_FEATURES:
		if (ide->type == IDE_NONE)
			goto abort_cmd;
                if (!IDE_DRIVE_IS_CDROM(ide))
			goto abort_cmd;

                if (!atapi_dev->bus.devices[0]->atapi_set_feature(ide->cylprecomp, ide->secount, atapi_dev->bus.device_data[0]))
                        goto abort_cmd;
                ide->atastat = READY_STAT | DSC_STAT;
                ide_irq_raise(ide);
                return;
                
        case WIN_CHECK_POWER_MODE:
		if (ide->type == IDE_NONE)
		{
			goto abort_cmd;
		}
                ide->secount = 0xff;
                ide->atastat = READY_STAT | DSC_STAT;
                ide_irq_raise(ide);
                return;

        case WIN_PACKETCMD: /* ATAPI Packet */
                if (!IDE_DRIVE_IS_CDROM(ide)) goto abort_cmd;
                
                atapi_process_packet(atapi_dev);
                
                return;
        }

abort_cmd:
	ide->command = 0;
	ide->atastat = READY_STAT | ERR_STAT | DSC_STAT;
	ide->error = ABRT_ERR;
	ide->pos = 0;
	ide_irq_raise(ide);
}

void ide_callback_pri()
{
	callbackide(0);
}

void ide_callback_sec()
{
	callbackide(1);
}



void ide_write_pri(uint16_t addr, uint8_t val, void *priv)
{
        writeide(0, addr, val);
}
void ide_write_pri_w(uint16_t addr, uint16_t val, void *priv)
{
        writeidew(0, val);
}
void ide_write_pri_l(uint16_t addr, uint32_t val, void *priv)
{
        writeidel(0, val);
}
uint8_t ide_read_pri(uint16_t addr, void *priv)
{
        return readide(0, addr);
}
uint16_t ide_read_pri_w(uint16_t addr, void *priv)
{
        return readidew(0);
}
uint32_t ide_read_pri_l(uint16_t addr, void *priv)
{
        return readidel(0);
}

void ide_write_sec(uint16_t addr, uint8_t val, void *priv)
{
        writeide(1, addr, val);
}
void ide_write_sec_w(uint16_t addr, uint16_t val, void *priv)
{
        writeidew(1, val);
}
void ide_write_sec_l(uint16_t addr, uint32_t val, void *priv)
{
        writeidel(1, val);
}
uint8_t ide_read_sec(uint16_t addr, void *priv)
{
        return readide(1, addr);
}
uint16_t ide_read_sec_w(uint16_t addr, void *priv)
{
        return readidew(1);
}
uint32_t ide_read_sec_l(uint16_t addr, void *priv)
{
        return readidel(1);
}

void ide_pri_enable()
{
        io_sethandler(0x01f0, 0x0008, ide_read_pri, ide_read_pri_w, ide_read_pri_l, ide_write_pri, ide_write_pri_w, ide_write_pri_l, NULL);
        io_sethandler(0x03f6, 0x0001, ide_read_pri, NULL,           NULL,           ide_write_pri, NULL,            NULL           , NULL);
}

void ide_pri_disable()
{
        io_removehandler(0x01f0, 0x0008, ide_read_pri, ide_read_pri_w, ide_read_pri_l, ide_write_pri, ide_write_pri_w, ide_write_pri_l, NULL);
        io_removehandler(0x03f6, 0x0001, ide_read_pri, NULL,           NULL,           ide_write_pri, NULL,            NULL           , NULL);
}

void ide_sec_enable()
{
        io_sethandler(0x0170, 0x0008, ide_read_sec, ide_read_sec_w, ide_read_sec_l, ide_write_sec, ide_write_sec_w, ide_write_sec_l, NULL);
        io_sethandler(0x0376, 0x0001, ide_read_sec, NULL,           NULL,           ide_write_sec, NULL,            NULL           , NULL);
}

void ide_sec_disable()
{
        io_removehandler(0x0170, 0x0008, ide_read_sec, ide_read_sec_w, ide_read_sec_l, ide_write_sec, ide_write_sec_w, ide_write_sec_l, NULL);
        io_removehandler(0x0376, 0x0001, ide_read_sec, NULL,           NULL,           ide_write_sec, NULL,            NULL           , NULL);
}

static void *ide_init()
{
        ide_pri_enable();
        ide_sec_enable();
        
        timer_add(&ide_timer[0], ide_callback_pri, NULL, 0);
        timer_add(&ide_timer[1], ide_callback_sec, NULL, 0);
        
        return (void *)-1;
}

static void ide_close(void *p)
{
}

void ide_set_bus_master(int (*read_data)(int channel, uint8_t *data, int size, void *p), int (*write_data)(int channel, uint8_t *data, int size, void *p), void (*set_irq)(int channel, void *p), void *p)
{
        ide_bus_master_read_data = read_data;
        ide_bus_master_write_data = write_data;
        ide_bus_master_set_irq = set_irq;
        ide_bus_master_p = p;
}

void ide_reset_devices()
{
        if (ide_drives[0].type != IDE_NONE || ide_drives[1].type != IDE_NONE)
        {
                timer_set_delay_u64(&ide_timer[0], 500*IDE_TIME);
                ide_drives[0].reset = ide_drives[1].reset = 1;
                ide_drives[0].atastat = ide_drives[1].atastat = BUSY_STAT;
                if (ide_drives[0].type != IDE_NONE)
                        cur_ide[0] = 0;
                else
                        cur_ide[0] = 1;
        }
        if (ide_drives[2].type != IDE_NONE || ide_drives[3].type != IDE_NONE)
        {
                timer_set_delay_u64(&ide_timer[1], 500*IDE_TIME);
                ide_drives[2].reset = ide_drives[3].reset = 1;
                ide_drives[2].atastat = ide_drives[3].atastat = BUSY_STAT;
                if (ide_drives[2].type != IDE_NONE)
                        cur_ide[1] = 2;
                else
                        cur_ide[1] = 3;
        }
}

device_t ide_device =
{
        "Standard IDE",
        DEVICE_AT,
        ide_init,
        ide_close,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
};
