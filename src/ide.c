/*RPCemu v0.6 by Tom Walker
  IDE emulation*/
//#define RPCEMU_IDE

#define IDE_TIME 5

#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <sys/types.h>

#ifdef RPCEMU_IDE
        #include "rpcemu.h"
        #include "iomd.h"
        #include "arm.h"
#else
        #include "ibm.h"
#endif
#include "ide.h"

/* Bits of 'atastat' */
#define ERR_STAT		0x01
#define DRQ_STAT		0x08 /* Data request */
#define DSC_STAT                0x10
#define SERVICE_STAT            0x10
#define READY_STAT		0x40
#define BUSY_STAT		0x80

/* Bits of 'error' */
#define ABRT_ERR		0x04 /* Command aborted */
#define MCR_ERR			0x08 /* Media change request */

/* ATA Commands */
#define WIN_SRST			0x08 /* ATAPI Device Reset */
#define WIN_RECAL			0x10
#define WIN_RESTORE			WIN_RECAL
#define WIN_READ			0x20 /* 28-Bit Read */
#define WIN_READ_NORETRY                0x21 /* 28-Bit Read - no retry*/
#define WIN_WRITE			0x30 /* 28-Bit Write */
#define WIN_WRITE_NORETRY		0x31 /* 28-Bit Write */
#define WIN_VERIFY			0x40 /* 28-Bit Verify */
#define WIN_FORMAT			0x50
#define WIN_SEEK			0x70
#define WIN_DRIVE_DIAGNOSTICS           0x90 /* Execute Drive Diagnostics */
#define WIN_SPECIFY			0x91 /* Initialize Drive Parameters */
#define WIN_PACKETCMD			0xA0 /* Send a packet command. */
#define WIN_PIDENTIFY			0xA1 /* Identify ATAPI device */
#define WIN_SETIDLE1			0xE3
#define WIN_IDENTIFY			0xEC /* Ask drive to identify itself */

/* ATAPI Commands */
#define GPCMD_INQUIRY			0x12
#define GPCMD_MODE_SELECT_10		0x55
#define GPCMD_MODE_SENSE_10		0x5a
#define GPCMD_PAUSE_RESUME		0x4b
#define GPCMD_PLAY_AUDIO_12		0xa5
#define GPCMD_PLAY_AUDIO_MSF	        0x47
#define GPCMD_PREVENT_REMOVAL           0x1e
#define GPCMD_READ_10                   0x28
#define GPCMD_READ_CD			0xbe
#define GPCMD_READ_HEADER		0x44
#define GPCMD_READ_SUBCHANNEL		0x42
#define GPCMD_READ_TOC_PMA_ATIP		0x43
#define GPCMD_REQUEST_SENSE		0x03
#define GPCMD_SEEK			0x2b
#define GPCMD_SEND_DVD_STRUCTURE	0xad
#define GPCMD_SET_SPEED			0xbb
#define GPCMD_START_STOP_UNIT		0x1b
#define GPCMD_TEST_UNIT_READY		0x00

/* Mode page codes for mode sense/set */
#define GPMODE_R_W_ERROR_PAGE		0x01
#define GPMODE_CDROM_PAGE		0x0d
#define GPMODE_CDROM_AUDIO_PAGE		0x0e
#define GPMODE_CAPABILITIES_PAGE	0x2a
#define GPMODE_ALL_PAGES		0x3f

/* ATAPI Sense Keys */
#define SENSE_NONE		0
#define SENSE_NOT_READY		2
#define SENSE_ILLEGAL_REQUEST	5
#define SENSE_UNIT_ATTENTION	6

/* ATAPI Additional Sense Codes */
#define ASC_ILLEGAL_OPCODE		0x20
#define ASC_MEDIUM_NOT_PRESENT		0x3a

/* Tell RISC OS that we have a 4x CD-ROM drive (600kb/sec data, 706kb/sec raw).
   Not that it means anything */
#define CDROM_SPEED	706

/** Evaluate to non-zero if the currently selected drive is an ATAPI device */
#define IDE_DRIVE_IS_CDROM(ide)  (ide->type == IDE_CDROM)
/*
\
	(!ide.drive)*/

ATAPI *atapi;

enum
{
        IDE_NONE = 0,
        IDE_HDD,
        IDE_CDROM
};

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
        int packlen;
        int spt,hpc;
        int tracks;
        int packetstatus;
        int cdpos,cdlen;
        uint8_t asc;
        int discchanged;
        int reset;
        FILE *hdfile;
        uint16_t buffer[65536];
        int irqstat;
        int service;
        int lba;
        uint32_t lba_addr;
        int skip512;
} IDE;

IDE ide_drives[4];

IDE *ext_ide;

char ide_fn[2][512];

static void callreadcd(IDE *ide);
static void atapicommand(int ide_board);

int idecallback[2] = {0, 0};

int cur_ide[2];

uint8_t getstat(IDE *ide) { return ide->atastat; }

static inline void ide_irq_raise(IDE *ide)
{
//        pclog("IDE_IRQ_RAISE\n");
	if (!(ide->fdisk&2)) {
#ifdef RPCEMU_IDE
		iomd.irqb.status |= IOMD_IRQB_IDE;
		updateirqs();
#else
//                if (ide->board && !ide->irqstat) pclog("IDE_IRQ_RAISE\n");
                picint((ide->board)?(1<<15):(1<<14));
#endif
	}
	ide->irqstat=1;
        ide->service=1;
}

static inline void ide_irq_lower(IDE *ide)
{
//        pclog("IDE_IRQ_LOWER\n");
//	if (ide.board == 0) {
#ifdef RPCEMU_IDE
		iomd.irqb.status &= ~IOMD_IRQB_IDE;
		updateirqs();
#else
                picintc((ide->board)?(1<<15):(1<<14));
#endif
//	}
	ide->irqstat=0;
}

void ide_irq_update(IDE *ide)
{
#ifdef RPCEMU_IDE
	if (ide->irqstat && !(iomd.irqb.status & IOMD_IRQB_IDE) && !(ide->fdisk & 2)) {
		iomd.irqb.status |= IOMD_IRQB_IDE;
		updateirqs();
        }
        else if (iomd.irqb.status & IOMD_IRQB_IDE)
        {
		iomd.irqb.status &= ~IOMD_IRQB_IDE;
		updateirqs();
        }
#else
	if (ide->irqstat && !((pic2.pend|pic2.ins)&0x40) && !(ide->fdisk & 2))
            picint((ide->board)?(1<<15):(1<<14));
        else if ((pic2.pend|pic2.ins)&0x40)
            picintc((ide->board)?(1<<15):(1<<14));
#endif
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
static void
ide_padstr(char *str, const char *src, int len)
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
 * Copy a string into a buffer, padding with spaces. Does not add string
 * terminator.
 *
 * @param buf      Destination buffer
 * @param buf_size Size of destination buffer to fill in. Strings shorter than
 *                 this length will be padded with spaces.
 * @param src      Source string
 */
static void
ide_padstr8(uint8_t *buf, int buf_size, const char *src)
{
	int i;

	for (i = 0; i < buf_size; i++) {
		if (*src != '\0') {
			buf[i] = *src++;
		} else {
			buf[i] = ' ';
		}
	}
}

/**
 * Fill in ide->buffer with the output of the "IDENTIFY DEVICE" command
 */
static void ide_identify(IDE *ide)
{
	memset(ide->buffer, 0, 512);

	//ide->buffer[1] = 101; /* Cylinders */

#ifdef RPCEMU_IDE
	ide->buffer[1] = 65535; /* Cylinders */
	ide->buffer[3] = 16;  /* Heads */
	ide->buffer[6] = 63;  /* Sectors */
#else
	ide->buffer[1] = hdc[cur_ide[0]].tracks; /* Cylinders */
	ide->buffer[3] = hdc[cur_ide[0]].hpc;  /* Heads */
	ide->buffer[6] = hdc[cur_ide[0]].spt;  /* Sectors */
#endif
	ide_padstr((char *) (ide->buffer + 10), "", 20); /* Serial Number */
	ide_padstr((char *) (ide->buffer + 23), "v1.0", 8); /* Firmware */
#ifdef RPCEMU_IDE
	ide_padstr((char *) (ide->buffer + 27), "RPCemuHD", 40); /* Model */
#else
	ide_padstr((char *) (ide->buffer + 27), "PCemHD", 40); /* Model */
#endif
	ide->buffer[49] = (1<<9); /* LBA supported */
	ide->buffer[50] = 0x4000; /* Capabilities */
#ifdef RPCEMU_IDE
	ide->buffer[60] = (65535 * 16 * 63) & 0xFFFF; /* Total addressable sectors (LBA) */
	ide->buffer[61] = (65535 * 16 * 63) >> 16;
#else
	ide->buffer[60] = (hdc[cur_ide[0]].tracks * hdc[cur_ide[0]].hpc * hdc[cur_ide[0]].spt) & 0xFFFF; /* Total addressable sectors (LBA) */
	ide->buffer[61] = (hdc[cur_ide[0]].tracks * hdc[cur_ide[0]].hpc * hdc[cur_ide[0]].spt) >> 16;
#endif
}

/**
 * Fill in ide->buffer with the output of the "IDENTIFY PACKET DEVICE" command
 */
static void ide_atapi_identify(IDE *ide)
{
	memset(ide->buffer, 0, 512);

	ide->buffer[0] = 0x8000 | (5<<8) | 0x80 | (2<<5); /* ATAPI device, CD-ROM drive, removable media, accelerated DRQ */
	ide_padstr((char *) (ide->buffer + 10), "", 20); /* Serial Number */
	ide_padstr((char *) (ide->buffer + 23), "v1.0", 8); /* Firmware */
#ifdef RPCEMU_IDE
	ide_padstr((char *) (ide->buffer + 27), "RPCemuCD", 40); /* Model */
#else
	ide_padstr((char *) (ide->buffer + 27), "PCemCD", 40); /* Model */
#endif
	ide->buffer[49] = 0x200; /* LBA supported */
}

/**
 * Fill in ide->buffer with the output of the ATAPI "MODE SENSE" command
 *
 * @param pos Offset within the buffer to start filling in data
 *
 * @return Offset within the buffer after the end of the data
 */
static uint32_t ide_atapi_mode_sense(IDE *ide, uint32_t pos, uint8_t type)
{
	uint8_t *buf = (uint8_t *) ide->buffer;
//        pclog("ide_atapi_mode_sense %02X\n",type);
        if (type==GPMODE_ALL_PAGES || type==GPMODE_R_W_ERROR_PAGE)
        {
        	/* &01 - Read error recovery */
        	buf[pos++] = GPMODE_R_W_ERROR_PAGE;
        	buf[pos++] = 6; /* Page length */
        	buf[pos++] = 0; /* Error recovery parameters */
        	buf[pos++] = 3; /* Read retry count */
        	buf[pos++] = 0; /* Reserved */
        	buf[pos++] = 0; /* Reserved */
        	buf[pos++] = 0; /* Reserved */
        	buf[pos++] = 0; /* Reserved */
        }

        if (type==GPMODE_ALL_PAGES || type==GPMODE_CDROM_PAGE)
        {
        	/* &0D - CD-ROM Parameters */
        	buf[pos++] = GPMODE_CDROM_PAGE;
        	buf[pos++] = 6; /* Page length */
        	buf[pos++] = 0; /* Reserved */
        	buf[pos++] = 1; /* Inactivity time multiplier *NEEDED BY RISCOS* value is a guess */
        	buf[pos++] = 0; buf[pos++] = 60; /* MSF settings */
        	buf[pos++] = 0; buf[pos++] = 75; /* MSF settings */
        }

        if (type==GPMODE_ALL_PAGES || type==GPMODE_CDROM_AUDIO_PAGE)
        {
        	/* &0e - CD-ROM Audio Control Parameters */
        	buf[pos++] = GPMODE_CDROM_AUDIO_PAGE;
        	buf[pos++] = 0xE; /* Page length */
        	buf[pos++] = 4; /* Reserved */
        	buf[pos++] = 0; /* Reserved */
        	buf[pos++] = 0; /* Reserved */
        	buf[pos++] = 0; /* Reserved */
        	buf[pos++] = 0; buf[pos++] = 75; /* Logical audio block per second */
        	buf[pos++] = 1;    /* CDDA Output Port 0 Channel Selection */
        	buf[pos++] = 0xFF; /* CDDA Output Port 0 Volume */
        	buf[pos++] = 2;    /* CDDA Output Port 1 Channel Selection */
        	buf[pos++] = 0xFF; /* CDDA Output Port 1 Volume */
        	buf[pos++] = 0;    /* CDDA Output Port 2 Channel Selection */
        	buf[pos++] = 0;    /* CDDA Output Port 2 Volume */
        	buf[pos++] = 0;    /* CDDA Output Port 3 Channel Selection */
        	buf[pos++] = 0;    /* CDDA Output Port 3 Volume */
        }

        if (type==GPMODE_ALL_PAGES || type==GPMODE_CAPABILITIES_PAGE)
        {
//                pclog("Capabilities page\n");
               	/* &2A - CD-ROM capabilities and mechanical status */
        	buf[pos++] = GPMODE_CAPABILITIES_PAGE;
        	buf[pos++] = 0x12; /* Page length */
        	buf[pos++] = 0; buf[pos++] = 0; /* CD-R methods */
        	buf[pos++] = 1; /* Supports audio play, not multisession */
        	buf[pos++] = 0; /* Some other stuff not supported */
        	buf[pos++] = 0; /* Some other stuff not supported (lock state + eject) */
        	buf[pos++] = 0; /* Some other stuff not supported */
        	buf[pos++] = (uint8_t) (CDROM_SPEED >> 8);
        	buf[pos++] = (uint8_t) CDROM_SPEED; /* Maximum speed */
        	buf[pos++] = 0; buf[pos++] = 2; /* Number of audio levels - on and off only */
        	buf[pos++] = 0; buf[pos++] = 0; /* Buffer size - none */
        	buf[pos++] = (uint8_t) (CDROM_SPEED >> 8);
        	buf[pos++] = (uint8_t) CDROM_SPEED; /* Current speed */
        	buf[pos++] = 0; /* Reserved */
        	buf[pos++] = 0; /* Drive digital format */
        	buf[pos++] = 0; /* Reserved */
        	buf[pos++] = 0; /* Reserved */
        }

	return pos;
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
        	int heads = ide->hpc;
        	int sectors = ide->spt;

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
        	if (ide->sector == (ide->spt + 1)) {
        		ide->sector = 1;
        		ide->head++;
        		if (ide->head == ide->hpc) {
        			ide->head = 0;
        			ide->cylinder++;
                        }
		}
	}
}

#ifdef RPCEMU_IDE
static void loadhd(IDE *ide, int d, const char *fn)
{
	char pathname[512];

	append_filename(pathname, rpcemu_get_datadir(), fn, 512);

        rpclog("Loading %s\n",pathname);
	if (ide->hdfile == NULL) {
		/* Try to open existing hard disk image */
		ide->hdfile = fopen64(pathname, "rb+");
		if (ide->hdfile == NULL) {
			/* Failed to open existing hard disk image */
			if (errno == ENOENT) {
				/* Failed because it does not exist,
				   so try to create new file */
				ide->hdfile = fopen64(pathname, "wb+");
				if (ide->hdfile == NULL) {
					fatal("Cannot create file '%s': %s",
					      pathname, strerror(errno));
				}
			} else {
				/* Failed for another reason */
				fatal("Cannot open file '%s': %s",
				      pathname, strerror(errno));
			}
		}
	}

        fseek(ide->hdfile, 0xfc1, SEEK_SET);
        ide->spt = getc(ide->hdfile);
        ide->hpc = getc(ide->hdfile);
        ide->skip512 = 1;
//        rpclog("First check - spt %i hpc %i\n",ide.spt[0],ide.hpc[0]);
        if (!ide->spt || !ide->hpc)
        {
                fseek(ide->hdfile, 0xdc1, SEEK_SET);
                ide->spt = getc(ide->hdfile);
                ide->hpc = getc(ide->hdfile);
//                rpclog("Second check - spt %i hpc %i\n",ide.spt[0],ide.hpc[0]);
                ide->skip512 = 0;
                if (!ide->spt || !ide->hpc)
                {
                        ide->spt=63;
                        ide->hpc=16;
                        ide->skip512 = 1;
//        rpclog("Final check - spt %i hpc %i\n",ide.spt[0],ide.hpc[0]);
                }
        }

        ide->type = IDE_HDD;
        
        rpclog("%i %i %i\n",ide->spt,ide->hpc,ide->skip512);
}
#else
static void loadhd(IDE *ide, int d, const char *fn)
{
	if (ide->hdfile == NULL) {
		/* Try to open existing hard disk image */
		ide->hdfile = fopen64(fn, "rb+");
		if (ide->hdfile == NULL) {
			/* Failed to open existing hard disk image */
			if (errno == ENOENT) {
				/* Failed because it does not exist,
				   so try to create new file */
				ide->hdfile = fopen64(fn, "wb+");
				if (ide->hdfile == NULL) {
                                        ide->type = IDE_NONE;
/*					fatal("Cannot create file '%s': %s",
					      fn, strerror(errno));*/
				}
			} else {
				/* Failed for another reason */
                                ide->type = IDE_NONE;
/*				fatal("Cannot open file '%s': %s",
				      fn, strerror(errno));*/
			}
		}
	}

        ide->spt = hdc[d].spt;
        ide->hpc = hdc[d].hpc;
        ide->tracks = hdc[d].tracks;
        ide->type = IDE_HDD;
}
#endif

void resetide(void)
{
        int d;

        /* Close hard disk image files (if previously open) */
        for (d = 0; d < 4; d++) {
                ide_drives[d].type = IDE_NONE;
                if (ide_drives[d].hdfile != NULL) {
                        fclose(ide_drives[d].hdfile);
                        ide_drives[d].hdfile = NULL;
                }
                ide_drives[d].atastat = READY_STAT | DSC_STAT;
                ide_drives[d].service = 0;
                ide_drives[d].board = (d & 2) ? 1 : 0;
        }

        idecallback[0]=idecallback[1]=0;
#ifdef RPCEMU_IDE
	loadhd(&ide_drives[0], 0, "hd4.hdf");
	if (!config.cdromenabled) {
		loadhd(&ide_drives[1], 1, "hd5.hdf");
	}
	else
           ide_drives[1].type = IDE_CDROM;
#else
        loadhd(&ide_drives[0], 0, ide_fn[0]);
        loadhd(&ide_drives[1], 1, ide_fn[1]);
        if (cdrom_enabled)
           ide_drives[2].type = IDE_CDROM;
#endif

        cur_ide[0] = 0;
        cur_ide[1] = 2;
        
//        ide_drives[1].type = IDE_CDROM;
}

int idetimes=0;
void writeidew(int ide_board, uint16_t val)
{
        IDE *ide = &ide_drives[cur_ide[ide_board]];
#ifndef RPCEMU_IDE
/*        if (ide_board && (cr0&1) && !(eflags&VM_FLAG))
        {
//                pclog("Failed write IDE %04X:%08X\n",CS,pc);
                return;
        }*/
#endif
#ifdef _RPCEMU_BIG_ENDIAN
		val=(val>>8)|(val<<8);
#endif
//        pclog("Write IDEw %04X\n",val);
        ide->buffer[ide->pos >> 1] = val;
        ide->pos+=2;

        if (ide->packetstatus==4)
        {
                if (ide->pos>=(ide->packlen+2))
                {
                        ide->packetstatus=5;
                        idecallback[ide_board]=6*IDE_TIME;
//                        pclog("Packet over!\n");
                        ide_irq_lower(ide);
                }
                return;
        }
        else if (ide->packetstatus==5) return;
        else if (ide->command == WIN_PACKETCMD && ide->pos>=0xC)
        {
                ide->pos=0;
                ide->atastat = BUSY_STAT;
                ide->packetstatus=1;
                idecallback[ide_board]=6*IDE_TIME;
                callbackide(ide_board);
//                idecallback[ide_board]=60*IDE_TIME;
//                if ((ide->buffer[0]&0xFF)==0x43) idecallback[ide_board]=1*IDE_TIME;
//                pclog("Packet now waiting!\n");
/*                if (ide->buffer[0]==0x243)
                {
                        idetimes++;
                        output=3;
                }*/
        }
        else if (ide->pos>=512)
        {
                ide->pos=0;
                ide->atastat = BUSY_STAT;
                idecallback[ide_board]=6*IDE_TIME;
//                callbackide(ide_board);
        }
}

void writeide(int ide_board, uint16_t addr, uint8_t val)
{
        IDE *ide = &ide_drives[cur_ide[ide_board]];
        uint8_t *idebufferb = (uint8_t *) ide->buffer;
#ifndef RPCEMU_IDE
/*        if (ide_board && (cr0&1) && !(eflags&VM_FLAG))
        {
//                pclog("Failed write IDE %04X:%08X\n",CS,pc);
                return;
        }*/
#endif
//        return;
        addr|=0x80;
//        if (ide_board) pclog("Write IDEb %04X %02X %04X(%08X):%04X %i  %02X %02X\n",addr,val,CS,cs,pc,ins,ide->atastat,ide_drives[0].atastat);
        /*if (idedebug) */
//        pclog("Write IDE %08X %02X %04X:%08X\n",addr,val,CS,pc);
//        int c;
//      rpclog("Write IDE %08X %02X %08X %08X\n",addr,val,PC,armregs[12]);

        if (ide->type == IDE_NONE && addr != 0x1f6) return;
        
        switch (addr)
        {
        case 0x1F0: /* Data */
                writeidew(ide_board, val | (val << 8));
                return;

        case 0x1F1: /* Features */
                ide->cylprecomp=val;
                return;

        case 0x1F2: /* Sector count */
                ide->secount=val;
                return;

        case 0x1F3: /* Sector */
                ide->sector=val;
                ide->lba_addr=(ide->lba_addr&0xFFFFF00)|val;
                return;

        case 0x1F4: /* Cylinder low */
                ide->cylinder=(ide->cylinder&0xFF00)|val;
                ide->lba_addr=(ide->lba_addr&0xFFF00FF)|(val<<8);
//                pclog("Write cylinder low %02X\n",val);
                return;

        case 0x1F5: /* Cylinder high */
                ide->cylinder=(ide->cylinder&0xFF)|(val<<8);
                ide->lba_addr=(ide->lba_addr&0xF00FFFF)|(val<<16);
                return;

        case 0x1F6: /* Drive/Head */
/*        if (val==0xB0)
        {
                dumpregs();
                exit(-1);
        }*/
                cur_ide[ide_board]=((val>>4)&1)+(ide_board<<1);
                ide = &ide_drives[cur_ide[ide_board]];
                
                ide->head=val&0xF;
                ide->lba=val&0x40;
                
                ide->lba_addr=(ide->lba_addr&0x0FFFFFF)|((val&0xF)<<24);
                
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
                        idecallback[ide_board]=100*IDE_TIME;
                        return;

                case WIN_RESTORE:
                case WIN_SEEK:
                        ide->atastat = READY_STAT;
                        idecallback[ide_board]=100*IDE_TIME;
                        return;

                case WIN_READ:
                case WIN_READ_NORETRY:
/*                        if (ide.secount>1)
                        {
                                fatal("Read %i sectors from sector %i cylinder %i head %i\n",ide.secount,ide.sector,ide.cylinder,ide.head);
                        }*/
#if 0
                        if (ide->lba) pclog("Read %i sectors from LBA addr %07X\n",ide->secount,ide->lba_addr);
                        else          pclog("Read %i sectors from sector %i cylinder %i head %i  %i\n",ide->secount,ide->sector,ide->cylinder,ide->head,ins);
#endif
                        ide->atastat = BUSY_STAT;
                        idecallback[ide_board]=200*IDE_TIME;
                        return;

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

                case WIN_VERIFY:
#if 0
                        if (ide->lba) pclog("Read verify %i sectors from LBA addr %07X\n",ide->secount,ide->lba_addr);
                        else          pclog("Read verify %i sectors from sector %i cylinder %i head %i\n",ide->secount,ide->sector,ide->cylinder,ide->head);
#endif
                        ide->atastat = BUSY_STAT;
                        idecallback[ide_board]=200*IDE_TIME;
                        return;

                case WIN_FORMAT:
//                        pclog("Format track %i head %i\n",ide.cylinder,ide.head);
                        ide->atastat = DRQ_STAT;
//                        idecallback[ide_board]=200;
                        ide->pos=0;
                        return;

                case WIN_SPECIFY: /* Initialize Drive Parameters */
                        ide->atastat = BUSY_STAT;
                        idecallback[ide_board]=200*IDE_TIME;
//                        pclog("SPECIFY\n");
//                        output=1;
                        return;

                case WIN_DRIVE_DIAGNOSTICS: /* Execute Drive Diagnostics */
                case WIN_PIDENTIFY: /* Identify Packet Device */
//                output=1;
                case WIN_SETIDLE1: /* Idle */
                        ide->atastat = BUSY_STAT;
                        idecallback[ide_board]=200*IDE_TIME;
                        return;

                case WIN_IDENTIFY: /* Identify Device */
                case 0xEF:
//                        output=3;
//                        timetolive=500;
                        ide->atastat = BUSY_STAT;
                        idecallback[ide_board]=200*IDE_TIME;
                        return;

                case WIN_PACKETCMD: /* ATAPI Packet */
                        ide->packetstatus=0;
                        ide->atastat = BUSY_STAT;
                        idecallback[ide_board]=1;//30*IDE_TIME;
                        ide->pos=0;
                        return;
                        
                case 0xF0:
                        default:
                	ide->atastat = READY_STAT | ERR_STAT | DSC_STAT;
                	ide->error = ABRT_ERR;
                        ide_irq_raise(ide);
                        return;
                }
//                fatal("Bad IDE command %02X\n", val);
                return;

        case 0x3F6: /* Device control */
                if ((ide->fdisk&4) && !(val&4) && (ide->type != IDE_NONE))
                {
                        idecallback[ide_board]=500*IDE_TIME;
                        ide->reset = 1;
                        ide->atastat = BUSY_STAT;
//                        pclog("IDE Reset\n");
                }
                ide->fdisk=val;
                ide_irq_update(ide);
                return;
        }
//        fatal("Bad IDE write %04X %02X\n", addr, val);
}

uint8_t readide(int ide_board, uint16_t addr)
{
        IDE *ide = &ide_drives[cur_ide[ide_board]];
        const uint8_t *idebufferb = (const uint8_t *) ide->buffer;
        uint8_t temp;
        uint16_t tempw;

        addr|=0x80;
#ifndef RPCEMU_IDE
/*        if (ide_board && (cr0&1) && !(eflags&VM_FLAG))
        {
//                pclog("Failed read IDE %04X:%08X\n",CS,pc);
                return 0xFF;
        }*/
#endif
//        return 0xFF;

        if (ide->type == IDE_NONE && addr != 0x1f6) return 0xff;
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
                if (ide->fdisk & 4)
                   temp = 0x80;
                else if (ide->type == IDE_CDROM)
                {
//                        pclog("Read CDROM status %02X\n",(ide->atastat & ~DSC_STAT) | (ide->service ? SERVICE_STAT : 0));
                        temp = (ide->atastat & ~DSC_STAT) | (ide->service ? SERVICE_STAT : 0);
                }
                else
                {
//                 && ide->service) return ide.atastat[ide.board]|SERVICE_STAT;
//                pclog("Return status %02X\n",ide->atastat);
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
                }
                else
                {
//                 && ide->service) return ide.atastat[ide.board]|SERVICE_STAT;
//                pclog("Return status %02X\n",ide->atastat);
                        temp = ide->atastat;
                }
                break;
        }
//        if (ide_board) pclog("Read IDEb %04X %02X   %02X %02X %i %04X:%04X %i\n", addr, temp, ide->atastat,(ide->atastat & ~DSC_STAT) | (ide->service ? SERVICE_STAT : 0),cur_ide[ide_board],CS,pc,ide_board);
        return temp;
//        fatal("Bad IDE read %04X\n", addr);
}

uint16_t readidew(int ide_board)
{
        IDE *ide = &ide_drives[cur_ide[ide_board]];
        uint16_t temp;
#ifndef RPCEMU_IDE
/*        if (ide_board && (cr0&1) && !(eflags&VM_FLAG))
        {
//                pclog("Failed read IDEw %04X:%08X\n",CS,pc);
                return 0xFFFF;
        }*/
#endif
//        return 0xFFFF;
//        pclog("Read IDEw %04X %04X:%04X %02X %i\n",ide->buffer[ide->pos >> 1],CS,pc,opcode,ins);
        
//if (idedebug) pclog("Read IDEW %08X\n",PC);

        temp = ide->buffer[ide->pos >> 1];
	#ifdef _RPCEMU_BIG_ENDIAN
		temp=(temp>>8)|(temp<<8);
	#endif
        ide->pos+=2;
        if ((ide->pos>=512 && ide->command != WIN_PACKETCMD) || (ide->command == WIN_PACKETCMD && ide->pos>=ide->packlen))
        {
//                pclog("Over! packlen %i %i\n",ide.packlen,ide.pos);
                ide->pos=0;
                if (ide->command == WIN_PACKETCMD)// && ide.packetstatus==6)
                {
//                        pclog("Call readCD\n");
                        callreadcd(ide);
                }
                else
                {
                        ide->atastat = READY_STAT | DSC_STAT;
                        ide->packetstatus=0;
                        if (ide->command == WIN_READ || ide->command == WIN_READ_NORETRY)
                        {
                                ide->secount--;
                                if (ide->secount)
                                {
                                        ide_next_sector(ide);
                                        ide->atastat = BUSY_STAT;
                                        idecallback[ide_board]=6*IDE_TIME;
//                                        callbackide(ide_board);
                                }
                        }
                }
        }
//        if (ide_board) pclog("Read IDEw %04X\n",temp);
        return temp;
}

int times30=0;
void callbackide(int ide_board)
{
        IDE *ide = &ide_drives[cur_ide[ide_board]];
        off64_t addr;
        int c;
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
                ide->atastat = READY_STAT | DSC_STAT;
                ide->error=1;
                ide->secount=1;
                ide->sector=1;
                ide->head=0;
                ide->cylinder=0;
                ide->reset = 0;
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
                if (IDE_DRIVE_IS_CDROM(ide)) {
                        ide->cylinder = 0xeb14;
                        ide->atastat = 0;
                } else {
                        ide->cylinder = 0;
                }
                ide_irq_raise(ide);
                if (IDE_DRIVE_IS_CDROM(ide))
                   ide->service = 0;
                return;

        case WIN_RESTORE:
        case WIN_SEEK:
                if (IDE_DRIVE_IS_CDROM(ide)) {
                        goto abort_cmd;
                }
//                pclog("Restore callback\n");
                ide->atastat = READY_STAT | DSC_STAT;
                ide_irq_raise(ide);
                return;

        case WIN_READ:
        case WIN_READ_NORETRY:
                if (IDE_DRIVE_IS_CDROM(ide)) {
                        goto abort_cmd;
                }
                addr = ide_get_sector(ide) * 512;
//                pclog("Read %i %i %i %08X\n",ide.cylinder,ide.head,ide.sector,addr);
                /*                if (ide.cylinder || ide.head)
                {
                        fatal("Read from other cylinder/head");
                }*/
                fseeko64(ide->hdfile, addr, SEEK_SET);
                fread(ide->buffer, 512, 1, ide->hdfile);
                ide->pos=0;
                ide->atastat = DRQ_STAT | READY_STAT | DSC_STAT;
//                pclog("Read sector callback %i %i %i offset %08X %i left %i %02X\n",ide.sector,ide.cylinder,ide.head,addr,ide.secount,ide.spt,ide.atastat[ide.board]);
//                if (addr) output=3;
                ide_irq_raise(ide);
#ifndef RPCEMU_IDE
                readflash=1;
#endif
                return;

        case WIN_WRITE:
        case WIN_WRITE_NORETRY:
                if (IDE_DRIVE_IS_CDROM(ide)) {
                        goto abort_cmd;
                }
                addr = ide_get_sector(ide) * 512;
//                pclog("Write sector callback %i %i %i offset %08X %i left %i\n",ide.sector,ide.cylinder,ide.head,addr,ide.secount,ide.spt);
                fseeko64(ide->hdfile, addr, SEEK_SET);
                fwrite(ide->buffer, 512, 1, ide->hdfile);
                ide_irq_raise(ide);
                ide->secount--;
                if (ide->secount)
                {
                        ide->atastat = DRQ_STAT | READY_STAT | DSC_STAT;
                        ide->pos=0;
                        ide_next_sector(ide);
                }
                else
                   ide->atastat = READY_STAT | DSC_STAT;
#ifndef RPCEMU_IDE
                readflash=1;
#endif
                return;

        case WIN_VERIFY:
                if (IDE_DRIVE_IS_CDROM(ide)) {
                        goto abort_cmd;
                }
                ide->pos=0;
                ide->atastat = READY_STAT | DSC_STAT;
//                pclog("Read verify callback %i %i %i offset %08X %i left\n",ide.sector,ide.cylinder,ide.head,addr,ide.secount);
                ide_irq_raise(ide);
#ifndef RPCEMU_IDE
                readflash=1;
#endif
                return;

        case WIN_FORMAT:
                if (IDE_DRIVE_IS_CDROM(ide)) {
                        goto abort_cmd;
                }
                addr = ide_get_sector(ide) * 512;
//                pclog("Format cyl %i head %i offset %08X %08X %08X secount %i\n",ide.cylinder,ide.head,addr,addr>>32,addr,ide.secount);
                fseeko64(ide->hdfile, addr, SEEK_SET);
                memset(ide->buffer, 0, 512);
                for (c=0;c<ide->secount;c++)
                {
                        fwrite(ide->buffer, 512, 1, ide->hdfile);
                }
                ide->atastat = READY_STAT | DSC_STAT;
                ide_irq_raise(ide);
#ifndef RPCEMU_IDE
                readflash=1;
#endif
                return;

        case WIN_DRIVE_DIAGNOSTICS:
                ide->error=1; /*No error detected*/
                ide->atastat = READY_STAT | DSC_STAT;
                ide_irq_raise(ide);
                return;

        case WIN_SPECIFY: /* Initialize Drive Parameters */
                if (IDE_DRIVE_IS_CDROM(ide)) {
#ifndef RPCEMU_IDE
                        pclog("IS CDROM - ABORT\n");
#endif
                        goto abort_cmd;
                }
                ide->spt=ide->secount;
                ide->hpc=ide->head+1;
                ide->atastat = READY_STAT | DSC_STAT;
#ifndef RPCEMU_IDE
//                pclog("SPECIFY - %i sectors per track, %i heads per cylinder  %i %i\n",ide->spt,ide->hpc,cur_ide[ide_board],ide_board);
#endif
                ide_irq_raise(ide);
                return;

        case WIN_PIDENTIFY: /* Identify Packet Device */
                if (IDE_DRIVE_IS_CDROM(ide)) {
//                        pclog("ATAPI identify\n");
                        ide_atapi_identify(ide);
                        ide->pos=0;
                        ide->error=0;
                        ide->atastat = DRQ_STAT | READY_STAT | DSC_STAT;
                        ide_irq_raise(ide);
                        return;
                }
//                pclog("Not ATAPI\n");
                goto abort_cmd;

        case WIN_SETIDLE1: /* Idle */
        case 0xEF:
                goto abort_cmd;

        case WIN_IDENTIFY: /* Identify Device */
                if (IDE_DRIVE_IS_CDROM(ide)) {
                        ide->secount=1;
                        ide->sector=1;
                        ide->cylinder=0xEB14;
                        ide->drive=ide->head=0;
                        goto abort_cmd;
                }
                if (ide->type != IDE_NONE)
                {
                        ide_identify(ide);
                        ide->pos=0;
                        ide->atastat = DRQ_STAT | READY_STAT | DSC_STAT;
//                pclog("ID callback\n");
                        ide_irq_raise(ide);
                }
                return;

        case WIN_PACKETCMD: /* ATAPI Packet */
                if (!IDE_DRIVE_IS_CDROM(ide)) goto abort_cmd;
//                pclog("Packet callback! %i %08X\n",ide->packetstatus,ide);
                if (!ide->packetstatus)
                {
                        ide->pos=0;
                        ide->secount = (uint8_t)((ide->secount&0xF8)|1);
                        ide->atastat = DRQ_STAT |(ide->atastat&ERR_STAT);
                        //ide_irq_raise(ide);
//                        pclog("1 Preparing to recieve packet max DRQ count %04X\n",ide->cylinder);
                }
                else if (ide->packetstatus==1)
                {
                        ide->atastat = BUSY_STAT|(ide->atastat&ERR_STAT);
//                        pclog("Running ATAPI command 2\n");
                        atapicommand(ide_board);
//                        exit(-1);
                }
                else if (ide->packetstatus==2)
                {
//                        pclog("packetstatus==2\n");
                        ide->atastat = READY_STAT;
                        ide->secount=3;
                        ide_irq_raise(ide);
//                        if (iomd.irqb.mask&2) output=1;
                }
                else if (ide->packetstatus==3)
                {
                        ide->atastat = DRQ_STAT|(ide->atastat&ERR_STAT);
//                        rpclog("Recieve data packet 3! %02X\n",ide->atastat);
                        ide_irq_raise(ide);
                        ide->packetstatus=0xFF;
                }
                else if (ide->packetstatus==4)
                {
                        ide->atastat = DRQ_STAT|(ide->atastat&ERR_STAT);
//                        pclog("Send data packet 4!\n");
                        ide_irq_raise(ide);
//                        ide.packetstatus=5;
                        ide->pos=2;
                }
                else if (ide->packetstatus==5)
                {
//                        pclog("Packetstatus 5 !\n");
                        atapicommand(ide_board);
                }
                else if (ide->packetstatus==6) /*READ CD callback*/
                {
                        ide->atastat = DRQ_STAT|(ide->atastat&ERR_STAT);
//                        pclog("Recieve data packet 6!\n");
                        ide_irq_raise(ide);
//                        ide.packetstatus=0xFF;
                }
                else if (ide->packetstatus==0x80) /*Error callback*/
                {
//                        pclog("Packet error\n");
                        ide->atastat = READY_STAT | ERR_STAT;
                        ide_irq_raise(ide);
                }
                return;
        }

abort_cmd:
	ide->atastat = READY_STAT | ERR_STAT | DSC_STAT;
	ide->error = ABRT_ERR;
	ide_irq_raise(ide);
}

/*ATAPI CD-ROM emulation
  This mostly seems to work. It is implemented only on Windows at the moment as
  I haven't had time to implement any interfaces for it in the generic gui.
  It mostly depends on driver files - cdrom-iso.c for ISO image files (in theory
  on any platform) and cdrom-ioctl.c for Win32 IOCTL access. There's an ATAPI
  interface defined in ide.h.
  There are a couple of bugs in the CD audio handling.
  */

struct
{
        int sensekey,asc,ascq;
} atapi_sense;

static void atapi_notready(IDE *ide)
{
        /*Medium not present is 02/3A/--*/
        /*cylprecomp is error number*/
        /*SENSE/ASC/ASCQ*/
        ide->atastat = READY_STAT | ERR_STAT;    /*CHECK CONDITION*/
        ide->error = (SENSE_NOT_READY << 4) | ABRT_ERR;
        if (ide->discchanged) {
                ide->error |= MCR_ERR;
                atapi_sense.sensekey=6;
                atapi_sense.asc=0x28;
        } else {
                atapi_sense.sensekey=2;
                atapi_sense.asc = ASC_MEDIUM_NOT_PRESENT;
        }
        ide->discchanged=0;
        ide->packetstatus=0x80;
        idecallback[ide->board]=50*IDE_TIME;
}

void atapi_discchanged()
{
        ext_ide->discchanged=1;
        atapi_sense.sensekey=6;
        atapi_sense.asc=0x28;
}

uint8_t atapi_prev;
int toctimes=0;
static void atapicommand(int ide_board)
{
        IDE *ide = &ide_drives[cur_ide[ide_board]];
        uint8_t *idebufferb = (uint8_t *) ide->buffer;
        int c;
        int len;
        int msf;
        int pos=0;
        unsigned char temp;
#ifndef RPCEMU_IDE
        pclog("New ATAPI command %02X %i\n",idebufferb[0],ins);
#endif
//        readflash=1;
                msf=idebufferb[1]&2;
                ide->cdlen=0;
        if (idebufferb[0]!=GPCMD_REQUEST_SENSE)
        {
                atapi_prev=idebufferb[0];
                atapi_sense.sensekey=0;
                atapi_sense.asc=0;
        }
        switch (idebufferb[0])
        {
        case GPCMD_TEST_UNIT_READY:
                if (!atapi->ready()) { atapi_notready(ide); return; }
//                if (atapi->ready())
//                {
                        ide->packetstatus=2;
                        idecallback[ide_board]=50*IDE_TIME;
//                }
//                else
//                {
//                        pclog("Medium not present!\n");
//                }
                break;

        case GPCMD_REQUEST_SENSE: /* Used by ROS 4+ */
                /*Will return 18 bytes of 0*/
                memset(idebufferb,0,512);
//                switch (atapi_prev)
//                {
//                        case GPCMD_TEST_UNIT_READY:
                        idebufferb[0]=0x80|0x70;
                        idebufferb[2]=atapi_sense.sensekey;
                        idebufferb[12]=atapi_sense.asc;
                        idebufferb[13]=atapi_sense.ascq;
//                        break;
//
//                        default:
//                        fatal("Bad REQUEST_SENSE following command %02X\n",atapi_prev);
//                }
                ide->packetstatus=3;
                ide->cylinder=18;
                ide->secount=2;
                ide->pos=0;
                idecallback[ide_board]=60*IDE_TIME;
                ide->packlen=18;
                break;

        case GPCMD_SET_SPEED:
                ide->packetstatus=2;
                idecallback[ide_board]=50*IDE_TIME;
                break;

        case GPCMD_READ_TOC_PMA_ATIP:
//                pclog("Read TOC ready? %08X\n",ide);
                if (!atapi->ready()) { atapi_notready(ide); return; }
                toctimes++;
//                if (toctimes==2) output=3;
//                pclog("Read TOC %02X\n",idebufferb[9]);
                switch (idebufferb[9]>>6)
                {
                        case 0: /*Normal*/
                        len=idebufferb[8]+(idebufferb[7]<<8);
                        len=atapi->readtoc(idebufferb,idebufferb[6],msf,len,0);
                        break;
                        case 1: /*Multi session*/
                        len=idebufferb[8]+(idebufferb[7]<<8);
                        atapi->readtoc_session(idebufferb,msf,len);
                        idebufferb[0]=0; idebufferb[1]=0xA;
                        break;
                        default:
                        ide->atastat = READY_STAT | ERR_STAT;    /*CHECK CONDITION*/
                        ide->error = (SENSE_ILLEGAL_REQUEST << 4) | ABRT_ERR;
                        if (ide->discchanged) {
                                ide->error |= MCR_ERR;
                        }
                        ide->discchanged=0;
                        atapi_sense.asc = ASC_ILLEGAL_OPCODE;
                        ide->packetstatus=0x80;
                        idecallback[ide_board]=50*IDE_TIME;
                        break;
/*                        pclog("Bad read TOC format\n");
                        pclog("Packet data :\n");
                        for (c=0;c<12;c++)
                            pclog("%02X ",idebufferb[c]);
                        pclog("\n");
                        exit(-1);*/
                }
//                pclog("ATAPI buffer len %i\n",len);
                ide->packetstatus=3;
                ide->cylinder=len;
                ide->secount=2;
                ide->pos=0;
                idecallback[ide_board]=60*IDE_TIME;
                ide->packlen=len;
                return;
                
        case GPCMD_READ_CD:
                if (!atapi->ready()) { atapi_notready(ide); return; }
//                pclog("Read CD : start LBA %02X%02X%02X%02X Length %02X%02X%02X Flags %02X\n",idebufferb[2],idebufferb[3],idebufferb[4],idebufferb[5],idebufferb[6],idebufferb[7],idebufferb[8],idebufferb[9]);
                if (idebufferb[9]!=0x10)
                {
                        ide->atastat = READY_STAT | ERR_STAT;    /*CHECK CONDITION*/
                        ide->error = (SENSE_ILLEGAL_REQUEST << 4) | ABRT_ERR;
                        if (ide->discchanged) {
                                ide->error |= MCR_ERR;
                        }
                        ide->discchanged=0;
                        atapi_sense.asc = ASC_ILLEGAL_OPCODE;
                        ide->packetstatus=0x80;
                        idecallback[ide_board]=50*IDE_TIME;
                        break;
//                        pclog("Bad flags bits %02X\n",idebufferb[9]);
//                        exit(-1);
                }
/*                if (idebufferb[6] || idebufferb[7] || (idebufferb[8]!=1))
                {
                        pclog("More than 1 sector!\n");
                        exit(-1);
                }*/
                ide->cdlen=(idebufferb[6]<<16)|(idebufferb[7]<<8)|idebufferb[8];
                ide->cdpos=(idebufferb[2]<<24)|(idebufferb[3]<<16)|(idebufferb[4]<<8)|idebufferb[5];
//                pclog("Read at %08X %08X\n",ide.cdpos,ide.cdpos*2048);
                atapi->readsector(idebufferb,ide->cdpos);
#ifndef RPCEMU_IDE
                readflash=1;
#endif
                ide->cdpos++;
                ide->cdlen--;
                if (ide->cdlen>=0) ide->packetstatus=6;
                else               ide->packetstatus=3;
                ide->cylinder=2048;
                ide->secount=2;
                ide->pos=0;
                idecallback[ide_board]=60*IDE_TIME;
                ide->packlen=2048;
                return;

        case GPCMD_READ_10:
                if (!atapi->ready()) { atapi_notready(ide); return; }
//                pclog("Read 10 : start LBA %02X%02X%02X%02X Length %02X%02X%02X Flags %02X\n",idebufferb[2],idebufferb[3],idebufferb[4],idebufferb[5],idebufferb[6],idebufferb[7],idebufferb[8],idebufferb[9]);

                ide->cdlen=(idebufferb[7]<<8)|idebufferb[8];
                ide->cdpos=(idebufferb[2]<<24)|(idebufferb[3]<<16)|(idebufferb[4]<<8)|idebufferb[5];
                if (!ide->cdlen)
                {
//                        pclog("All done - callback set\n");
                        ide->packetstatus=2;
                        idecallback[ide_board]=20*IDE_TIME;
                        break;
                }

                atapi->readsector(idebufferb,ide->cdpos);
#ifndef RPCEMU_IDE
                readflash=1;
#endif
                ide->cdpos++;
                ide->cdlen--;
                if (ide->cdlen>=0) ide->packetstatus=6;
                else               ide->packetstatus=3;
                ide->cylinder=2048;
                ide->secount=2;
                ide->pos=0;
                idecallback[ide_board]=60*IDE_TIME;
                ide->packlen=2048;
                return;

        case GPCMD_READ_HEADER:
                if (!atapi->ready()) { atapi_notready(ide); return; }
                if (msf)
                {
                        ide->atastat = READY_STAT | ERR_STAT;    /*CHECK CONDITION*/
                        ide->error = (SENSE_ILLEGAL_REQUEST << 4) | ABRT_ERR;
                        if (ide->discchanged) {
                                ide->error |= MCR_ERR;
                        }
                        ide->discchanged=0;
                        atapi_sense.asc = ASC_ILLEGAL_OPCODE;
                        ide->packetstatus=0x80;
                        idecallback[ide_board]=50*IDE_TIME;
                        break;
//                        pclog("Read Header MSF!\n");
//                        exit(-1);
                }
                for (c=0;c<4;c++) idebufferb[c+4]=idebufferb[c+2];
                idebufferb[0]=1; /*2048 bytes user data*/
                idebufferb[1]=idebufferb[2]=idebufferb[3]=0;
                
                ide->packetstatus=3;
                ide->cylinder=8;
                ide->secount=2;
                ide->pos=0;
                idecallback[ide_board]=60*IDE_TIME;
                ide->packlen=8;
                return;
                
        case GPCMD_MODE_SENSE_10:
//                output=3;
//                pclog("Mode sense - ready?\n");
                if (!atapi->ready()) { atapi_notready(ide); return; }
                        len=(idebufferb[8]|(idebufferb[7]<<8));

//pclog("Mode sense %02X %i\n",idebufferb[2],len);
                temp=idebufferb[2];
//                switch (idebufferb[2])
//                {
//                        case GPMODE_ALL_PAGES:
//                        case GPMODE_CAPABILITIES_PAGE:
                        for (c=0;c<len;c++) idebufferb[c]=0;
                        len = ide_atapi_mode_sense(ide,8,temp);
//                        break;
//                        default:
//                        for (c=0;c<len;c++) idebufferb[c]=0;
/*                        pclog("Bad mode sense\n");
                        pclog("Packet data :\n");
                        for (c=0;c<12;c++)
                            pclog("%02X\n",idebufferb[c]);
                        exit(-1);*/
//                }

                        /*Set mode parameter header - bytes 0 & 1 are data length (filled out later),
                          byte 2 is media type*/
                idebufferb[0]=len>>8;
                idebufferb[1]=len&255;
                idebufferb[2]=3; /*120mm data CD-ROM*/
//        pclog("ATAPI buffer len %i\n",len);
/*        for (c=0;c<len;c++) pclog("%02X ",idebufferb[c]);
        pclog("\n");*/
        ide->packetstatus=3;
        ide->cylinder=len;
        ide->secount=2;
//        ide.atastat = DRQ_STAT;
        ide->pos=0;
                idecallback[ide_board]=1000*IDE_TIME;
                ide->packlen=len;
//        pclog("Sending packet\n");
        return;

        case GPCMD_MODE_SELECT_10:
//                if (!atapi->ready()) { atapi_notready(); return; }
                if (ide->packetstatus==5)
                {
                        ide->atastat = 0;
//                        pclog("Recieve data packet!\n");
                        ide_irq_raise(ide);
                        ide->packetstatus=0xFF;
                        ide->pos=0;
  //                      pclog("Length - %02X%02X\n",idebufferb[0],idebufferb[1]);
//                        pclog("Page %02X length %02X\n",idebufferb[8],idebufferb[9]);
                }
                else
                {
                        len=(idebufferb[7]<<8)|idebufferb[8];
                        ide->packetstatus=4;
                        ide->cylinder=len;
                        ide->secount=2;
                        ide->pos=0;
                        idecallback[ide_board]=6*IDE_TIME;
                        ide->packlen=len;
/*                        pclog("Waiting for ARM to send packet %i\n",len);
                pclog("Packet data :\n");
                for (c=0;c<12;c++)
                    pclog("%02X ",idebufferb[c]);
                    pclog("\n");*/
                }
                return;

        case GPCMD_PLAY_AUDIO_12:
                if (!atapi->ready()) { atapi_notready(ide); return; }
                /*This is apparently deprecated in the ATAPI spec, and apparently
                  has been since 1995 (!). Hence I'm having to guess most of it*/
                pos=(idebufferb[3]<<16)|(idebufferb[4]<<8)|idebufferb[5];
                len=(idebufferb[7]<<16)|(idebufferb[8]<<8)|idebufferb[9];
                atapi->playaudio(pos,len,0);
                ide->packetstatus=2;
                idecallback[ide_board]=50*IDE_TIME;
                break;

        case GPCMD_PLAY_AUDIO_MSF:
                if (!atapi->ready()) { atapi_notready(ide); return; }
                pos=(idebufferb[3]<<16)|(idebufferb[4]<<8)|idebufferb[5];
                len=(idebufferb[6]<<16)|(idebufferb[7]<<8)|idebufferb[8];
                atapi->playaudio(pos,len,1);
                ide->packetstatus=2;
                idecallback[ide_board]=50*IDE_TIME;
                break;
                
        case GPCMD_READ_SUBCHANNEL:
                if (!atapi->ready()) { /*pclog("Read subchannel not ready\n"); */atapi_notready(ide); return; }
                temp=idebufferb[2]&0x40;
                if (idebufferb[3]!=1)
                {
//                        pclog("Read subchannel check condition %02X\n",idebufferb[3]);
                        ide->atastat = READY_STAT | ERR_STAT;    /*CHECK CONDITION*/
                        ide->error = (SENSE_ILLEGAL_REQUEST << 4) | ABRT_ERR;
                        if (ide->discchanged) {
                                ide->error |= MCR_ERR;
                        }
                        ide->discchanged=0;
                        atapi_sense.asc = ASC_ILLEGAL_OPCODE;
                        ide->packetstatus=0x80;
                        idecallback[ide_board]=50*IDE_TIME;
                        break;
/*                        pclog("Bad read subchannel!\n");
                        pclog("Packet data :\n");
                        for (c=0;c<12;c++)
                            pclog("%02X\n",idebufferb[c]);
                        dumpregs();
                        exit(-1);*/
                }
                pos=0;
                idebufferb[pos++]=0;
                idebufferb[pos++]=0; /*Audio status*/
                idebufferb[pos++]=0; idebufferb[pos++]=0; /*Subchannel length*/
                idebufferb[pos++]=1; /*Format code*/
                idebufferb[1]=atapi->getcurrentsubchannel(&idebufferb[5],msf);
//                pclog("Read subchannel complete - audio status %02X\n",idebufferb[1]);
                len=11+5;
                if (!temp) len=4;
                ide->packetstatus=3;
                ide->cylinder=len;
                ide->secount=2;
                ide->pos=0;
                idecallback[ide_board]=1000*IDE_TIME;
                ide->packlen=len;
                break;

        case GPCMD_START_STOP_UNIT:
                if (idebufferb[4]!=2 && idebufferb[4]!=3 && idebufferb[4])
                {
                        ide->atastat = READY_STAT | ERR_STAT;    /*CHECK CONDITION*/
                        ide->error = (SENSE_ILLEGAL_REQUEST << 4) | ABRT_ERR;
                        if (ide->discchanged) {
                                ide->error |= MCR_ERR;
                        }
                        ide->discchanged=0;
                        atapi_sense.asc = ASC_ILLEGAL_OPCODE;
                        ide->packetstatus=0x80;
                        idecallback[ide_board]=50*IDE_TIME;
                        break;
/*                        pclog("Bad start/stop unit command\n");
                        pclog("Packet data :\n");
                        for (c=0;c<12;c++)
                            pclog("%02X\n",idebufferb[c]);
                        exit(-1);*/
                }
                if (!idebufferb[4])        atapi->stop();
                else if (idebufferb[4]==2) atapi->eject();
                else                       atapi->load();
                ide->packetstatus=2;
                idecallback[ide_board]=50*IDE_TIME;
                break;
                
        case GPCMD_INQUIRY:
                idebufferb[0] = 5; /*CD-ROM*/
                idebufferb[1] = 0x80; /*Removable*/
                idebufferb[2] = 0;
                idebufferb[3] = 0x21;
                idebufferb[4] = 31;
                idebufferb[5] = 0;
                idebufferb[6] = 0;
                idebufferb[7] = 0;
#ifdef RPCEMU_IDE
                ide_padstr8(idebufferb + 8, 8, "RPCemu"); /* Vendor */
                ide_padstr8(idebufferb + 16, 16, "RPCemuCD"); /* Product */
#else
                ide_padstr8(idebufferb + 8, 8, "PCem"); /* Vendor */
                ide_padstr8(idebufferb + 16, 16, "PCemCD"); /* Product */
#endif
                ide_padstr8(idebufferb + 32, 4, "v1.0"); /* Revision */

                len=36;
                ide->packetstatus=3;
                ide->cylinder=len;
                ide->secount=2;
                ide->pos=0;
                idecallback[ide_board]=60*IDE_TIME;
                ide->packlen=len;
                break;

        case GPCMD_PREVENT_REMOVAL:
                if (!atapi->ready()) { atapi_notready(ide); return; }
                ide->packetstatus=2;
                idecallback[ide_board]=50*IDE_TIME;
                break;

        case GPCMD_PAUSE_RESUME:
                if (!atapi->ready()) { atapi_notready(ide); return; }
                if (idebufferb[8]&1) atapi->resume();
                else                 atapi->pause();
                ide->packetstatus=2;
                idecallback[ide_board]=50*IDE_TIME;
                break;

        case GPCMD_SEEK:
                if (!atapi->ready()) { atapi_notready(ide); return; }
                pos=(idebufferb[3]<<16)|(idebufferb[4]<<8)|idebufferb[5];
                atapi->seek(pos);
                ide->packetstatus=2;
                idecallback[ide_board]=50*IDE_TIME;
                break;

        case GPCMD_SEND_DVD_STRUCTURE:
        default:
                ide->atastat = READY_STAT | ERR_STAT;    /*CHECK CONDITION*/
                ide->error = (SENSE_ILLEGAL_REQUEST << 4) | ABRT_ERR;
                if (ide->discchanged) {
                        ide->error |= MCR_ERR;
                }
                ide->discchanged=0;
                atapi_sense.asc = ASC_ILLEGAL_OPCODE;
                ide->packetstatus=0x80;
                idecallback[ide_board]=50*IDE_TIME;
                break;
                
/*                default:
                pclog("Bad ATAPI command %02X\n",idebufferb[0]);
                pclog("Packet data :\n");
                for (c=0;c<12;c++)
                    pclog("%02X\n",idebufferb[c]);
                exit(-1);*/
        }
}

static void callreadcd(IDE *ide)
{
        ide_irq_lower(ide);
        if (ide->cdlen<=0)
        {
//                pclog("All done - callback set\n");
                ide->packetstatus=2;
                idecallback[ide->board]=20*IDE_TIME;
                return;
        }
//        pclog("Continue readcd! %i blocks left\n",ide->cdlen);
        ide->atastat = BUSY_STAT;
        
        atapi->readsector((uint8_t *) ide->buffer, ide->cdpos);
#ifndef RPCEMU_IDE
                readflash=1;
#endif
                ide->cdpos++;
                ide->cdlen--;
                ide->packetstatus=6;
                ide->cylinder=2048;
                ide->secount=2;
                ide->pos=0;
                idecallback[ide->board]=60*IDE_TIME;
                ide->packlen=2048;
}



void ide_write_pri(uint16_t addr, uint8_t val)
{
        writeide(0, addr, val);
}
void ide_write_pri_w(uint16_t addr, uint16_t val)
{
        writeidew(0, val);
}
uint8_t ide_read_pri(uint16_t addr)
{
        return readide(0, addr);
}
uint16_t ide_read_pri_w(uint16_t addr)
{
        return readidew(0);
}

void ide_write_sec(uint16_t addr, uint8_t val)
{
        writeide(1, addr, val);
}
void ide_write_sec_w(uint16_t addr, uint16_t val)
{
        writeidew(1, val);
}
uint8_t ide_read_sec(uint16_t addr)
{
        return readide(1, addr);
}
uint16_t ide_read_sec_w(uint16_t addr)
{
        return readidew(1);
}

void ide_init()
{
        io_sethandler(0x01f0, 0x0008, ide_read_pri, ide_read_pri_w, NULL, ide_write_pri, ide_write_pri_w, NULL);
        io_sethandler(0x03f6, 0x0001, ide_read_pri, NULL,           NULL, ide_write_pri, NULL,            NULL);
        io_sethandler(0x0170, 0x0008, ide_read_sec, ide_read_sec_w, NULL, ide_write_sec, ide_write_sec_w, NULL);
        io_sethandler(0x0376, 0x0001, ide_read_sec, NULL,           NULL, ide_write_sec, NULL,            NULL);
}
