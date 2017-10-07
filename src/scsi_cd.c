#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "ibm.h"
#include "ide_atapi.h"
#include "scsi.h"
#include "scsi_cd.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))


#define BUFFER_SIZE (256*1024)

#define MAX_NR_SECTORS 2

enum
{
        CMD_POS_IDLE = 0,
        CMD_POS_WAIT,
        CMD_POS_START_SECTOR,
        CMD_POS_TRANSFER
};

/* ATAPI Commands */
#define GPCMD_TEST_UNIT_READY           0x00
#define GPCMD_REQUEST_SENSE		0x03
#define GPCMD_READ_6			0x08
#define GPCMD_INQUIRY			0x12
#define GPCMD_MODE_SELECT_6		0x15
#define GPCMD_MODE_SENSE_6		0x1a
#define GPCMD_START_STOP_UNIT		0x1b
#define GPCMD_PREVENT_REMOVAL          	0x1e
#define GPCMD_READ_CDROM_CAPACITY	0x25
#define GPCMD_READ_10                   0x28
#define GPCMD_SEEK			0x2b
#define GPCMD_READ_SUBCHANNEL		0x42
#define GPCMD_READ_TOC_PMA_ATIP		0x43
#define GPCMD_READ_HEADER		0x44
#define GPCMD_PLAY_AUDIO_10		0x45
#define GPCMD_PLAY_AUDIO_MSF	        0x47
#define GPCMD_GET_EVENT_STATUS_NOTIFICATION	0x4a
#define GPCMD_PAUSE_RESUME		0x4b
#define GPCMD_STOP_PLAY_SCAN            0x4e
#define GPCMD_READ_DISC_INFORMATION	0x51
#define GPCMD_MODE_SELECT_10		0x55
#define GPCMD_MODE_SENSE_10		0x5a
#define GPCMD_PLAY_AUDIO_12		0xa5
#define GPCMD_READ_12                   0xa8
#define GPCMD_SEND_DVD_STRUCTURE	0xad
#define GPCMD_SET_SPEED			0xbb
#define GPCMD_MECHANISM_STATUS		0xbd
#define GPCMD_READ_CD			0xbe

/* Mode page codes for mode sense/set */
#define GPMODE_R_W_ERROR_PAGE		0x01
#define GPMODE_CDROM_PAGE		0x0d
#define GPMODE_CDROM_AUDIO_PAGE		0x0e
#define GPMODE_CAPABILITIES_PAGE	0x2a
#define GPMODE_ALL_PAGES		0x3f

/* ATAPI Sense Keys */
#define SENSE_NONE			0
#define SENSE_NOT_READY			2
#define SENSE_ILLEGAL_REQUEST		5
#define SENSE_UNIT_ATTENTION		6

/* ATAPI Additional Sense Codes */
#define ASC_AUDIO_PLAY_OPERATION	0x00
#define ASC_ILLEGAL_OPCODE		0x20
#define	ASC_LBA_OUT_OF_RANGE            0x21
#define	ASC_INV_FIELD_IN_CMD_PACKET	0x24
#define ASC_MEDIUM_MAY_HAVE_CHANGED	0x28
#define ASC_MEDIUM_NOT_PRESENT		0x3a
#define ASC_DATA_PHASE_ERROR		0x4b
#define ASC_ILLEGAL_MODE_FOR_THIS_TRACK	0x64

#define ASCQ_AUDIO_PLAY_OPERATION_IN_PROGRESS	0x11
#define ASCQ_AUDIO_PLAY_OPERATION_PAUSED	0x12
#define ASCQ_AUDIO_PLAY_OPERATION_COMPLETED	0x13

/* Tell RISC OS that we have a 4x CD-ROM drive (600kb/sec data, 706kb/sec raw).
   Not that it means anything */
#define CDROM_SPEED	706

/* Event notification classes for GET EVENT STATUS NOTIFICATION */
#define GESN_NO_EVENTS                0
#define GESN_OPERATIONAL_CHANGE       1
#define GESN_POWER_MANAGEMENT         2
#define GESN_EXTERNAL_REQUEST         3
#define GESN_MEDIA                    4
#define GESN_MULTIPLE_HOSTS           5
#define GESN_DEVICE_BUSY              6

/* Event codes for MEDIA event status notification */
#define MEC_NO_CHANGE                 0
#define MEC_EJECT_REQUESTED           1
#define MEC_NEW_MEDIA                 2
#define MEC_MEDIA_REMOVAL             3 /* only for media changers */
#define MEC_MEDIA_CHANGED             4 /* only for media changers */
#define MEC_BG_FORMAT_COMPLETED       5 /* MRW or DVD+RW b/g format completed */
#define MEC_BG_FORMAT_RESTARTED       6 /* MRW or DVD+RW b/g format restarted */
#define MS_TRAY_OPEN                  1
#define MS_MEDIA_PRESENT              2

#define CHECK_READY		2
#define ALLOW_UA		1

/* Table of all ATAPI commands and their flags, needed for the new disc change / not ready handler. */
static uint8_t atapi_cmd_table[0x100] =
{
	[GPCMD_TEST_UNIT_READY]               = CHECK_READY,
	[GPCMD_REQUEST_SENSE]                 = ALLOW_UA,
	[GPCMD_READ_6]                        = CHECK_READY,
	[GPCMD_INQUIRY]                       = ALLOW_UA,
	[GPCMD_MODE_SELECT_6]                 = 0,
	[GPCMD_MODE_SENSE_6]                  = 0,
	[GPCMD_START_STOP_UNIT]               = 0,
	[GPCMD_PREVENT_REMOVAL]               = CHECK_READY,
	[GPCMD_READ_CDROM_CAPACITY]           = CHECK_READY,
	[GPCMD_READ_10]                       = CHECK_READY,
	[GPCMD_SEEK]                          = CHECK_READY,
	[GPCMD_READ_SUBCHANNEL]               = CHECK_READY,
	[GPCMD_READ_TOC_PMA_ATIP]             = CHECK_READY | ALLOW_UA,		/* Read TOC - can get through UNIT_ATTENTION, per VIDE-CDD.SYS */
	[GPCMD_READ_HEADER]                   = CHECK_READY,
	[GPCMD_PLAY_AUDIO_10]                 = CHECK_READY,
	[GPCMD_PLAY_AUDIO_MSF]                = CHECK_READY,
	[GPCMD_GET_EVENT_STATUS_NOTIFICATION] = ALLOW_UA,
	[GPCMD_PAUSE_RESUME]                  = CHECK_READY,
	[GPCMD_STOP_PLAY_SCAN]                = CHECK_READY,
	[GPCMD_READ_DISC_INFORMATION]         = CHECK_READY,
	[GPCMD_MODE_SELECT_10]                = 0,
	[GPCMD_MODE_SENSE_10]                 = 0,
	[GPCMD_PLAY_AUDIO_12]                 = CHECK_READY,
	[GPCMD_READ_12]                       = CHECK_READY,
	[GPCMD_SEND_DVD_STRUCTURE]            = CHECK_READY,		/* Read DVD structure (NOT IMPLEMENTED YET) */
	[GPCMD_SET_SPEED]                     = 0,
	[GPCMD_MECHANISM_STATUS]              = 0,
	[GPCMD_READ_CD]                       = CHECK_READY,
	[0xBF] = CHECK_READY	/* Send DVD structure (NOT IMPLEMENTED YET) */
};

#define IMPLEMENTED		1

static uint8_t mode_sense_pages[0x40] =
{
	[GPMODE_R_W_ERROR_PAGE]    = IMPLEMENTED,
	[GPMODE_CDROM_PAGE]        = IMPLEMENTED,
	[GPMODE_CDROM_AUDIO_PAGE]  = IMPLEMENTED,
	[GPMODE_CAPABILITIES_PAGE] = IMPLEMENTED,
	[GPMODE_ALL_PAGES]         = IMPLEMENTED
};

uint8_t mode_pages_in[256][256];
#define PAGE_CHANGEABLE		1
#define PAGE_CHANGED		2
static uint8_t page_flags[256] =
{
	[GPMODE_R_W_ERROR_PAGE]    = 0,
	[GPMODE_CDROM_PAGE]        = 0,
	[GPMODE_CDROM_AUDIO_PAGE]  = PAGE_CHANGEABLE,
	[GPMODE_CAPABILITIES_PAGE] = 0,
};

extern int cd_status;

typedef struct scsi_cd_data_t
{
        uint8_t data_in[BUFFER_SIZE];
        uint8_t data_out[BUFFER_SIZE];

        int blocks;
        
        int cmd_pos;
        
        int addr, len;
        int sector_pos;
        
        uint8_t buf[512];
        
        uint8_t status;
        
        int data_pos_read, data_pos_write;
        int data_bytes_read, bytes_expected;
                
        int cdlen, cdpos;
        
        int bytes_received, bytes_required;

        
        int sense_key, asc, ascq;
        
        int id;


        int received_cdb;
        uint8_t cdb[CDB_MAX_LEN];

        int prev_status;
        int cd_status;

        uint8_t prefix_len;
        uint8_t page_current;

} scsi_cd_data_t;

static void scsi_add_data(scsi_cd_data_t *data, uint8_t val)
{
        data->data_in[data->data_pos_write++] = val;
        data->bytes_expected++;
}

static void atapi_cmd_error(scsi_cd_data_t *data, int sensekey, int asc, int ascq)
{
        data->status = STATUS_CHECK_CONDITION;
        data->sense_key = sensekey;
        data->asc = asc;
        data->ascq = ascq;
}

static void atapi_sense_clear(scsi_cd_data_t *data, int command, int ignore_ua)
{
	if ((data->sense_key == SENSE_UNIT_ATTENTION) || ignore_ua)
	{
		data->sense_key = 0;
		data->asc = 0;
		data->ascq = 0;
	}
}

static int atapi_event_status(uint8_t *buffer)
{
	uint8_t event_code, media_status = 0;

	if (buffer[5])
	{
		media_status = MS_TRAY_OPEN;
		atapi->stop();
	}
        else
	{
		media_status = MS_MEDIA_PRESENT;
	}
	
	event_code = MEC_NO_CHANGE;
	if (media_status != MS_TRAY_OPEN)
	{
		if (!buffer[4])
		{
			event_code = MEC_NEW_MEDIA;
			atapi->load();
		}
		else if (buffer[4]==2)
		{
			event_code = MEC_EJECT_REQUESTED;
			atapi->eject();
		}
	}
	
	buffer[4] = event_code;
	buffer[5] = media_status;
	buffer[6] = 0;
	buffer[7] = 0;
	
	return 8;
}

static void ide_padstr8(uint8_t *buf, int buf_size, const char *src)
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

static void *scsi_cd_init(scsi_bus_t *bus, int id)
{
        scsi_cd_data_t *data = malloc(sizeof(scsi_cd_data_t));
        memset(data, 0, sizeof(scsi_cd_data_t));

        data->id = id;

	page_flags[GPMODE_CDROM_AUDIO_PAGE] &= 0xFD;		/* Clear changed flag for CDROM AUDIO mode page. */
	memset(mode_pages_in[GPMODE_CDROM_AUDIO_PAGE], 0, 256);	/* Clear the page itself. */
	page_flags[GPMODE_CDROM_AUDIO_PAGE] &= ~PAGE_CHANGED;
                
        return data;
}

static void scsi_cd_close(void *p)
{
        scsi_cd_data_t *data = p;
        
        free(data);
}

static void scsi_cd_illegal(scsi_cd_data_t *data)
{
        atapi_cmd_error(data, KEY_ILLEGAL_REQ, ASC_INVALID_LUN, 0);
}

/**
 * Fill in ide->buffer with the output of the ATAPI "MODE SENSE" command
 *
 * @param pos Offset within the buffer to start filling in data
 *
 * @return Offset within the buffer after the end of the data
 */
static uint32_t ide_atapi_mode_sense(scsi_cd_data_t *data, uint32_t pos, uint8_t type)
{
	uint8_t *buf = (uint8_t *)data->data_in;
//        pclog("ide_atapi_mode_sense %02X\n",type);
        if (type==GPMODE_ALL_PAGES || type==GPMODE_R_W_ERROR_PAGE)
        {
        	/* &01 - Read error recovery */
        	buf[pos++] = GPMODE_R_W_ERROR_PAGE;
        	buf[pos++] = 6; /* Page length */
        	buf[pos++] = 0; /* Error recovery parameters */
        	buf[pos++] = 5; /* Read retry count */
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
		if (page_flags[GPMODE_CDROM_AUDIO_PAGE] & PAGE_CHANGED)
		{
                        int i;
                        
			for (i = 0; i < 14; i++)
			{
				buf[pos++] = mode_pages_in[GPMODE_CDROM_AUDIO_PAGE][i];
			}
		}
		else
		{
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

static int scsi_cd_command(uint8_t *cdb, void *p)
{
        scsi_cd_data_t *data = p;
	uint8_t rcdmode = 0;
        int c;
        int len;
        int msf;
        int pos=0;
//        unsigned char temp;
        uint32_t size;
	int is_error;
	uint8_t page_code;
	int max_len;
	unsigned idx = 0;
	unsigned size_idx;
	unsigned preamble_len;
	int toc_format;
	int temp_command;
	int alloc_length;
	int completed;
        
        if (!data->received_cdb)
                memcpy(data->cdb, cdb, CDB_MAX_LEN);

        if ((cdb[0] != SCSI_REQUEST_SENSE/* && cdb[0] != SCSI_INQUIRY*/) && (cdb[1] & 0xe0))
        {
                pclog("non-zero LUN\n");
                /*Non-zero LUN - abort command*/
                scsi_cd_illegal(data);
                return BUS_CD | BUS_IO;
        }

        pclog("New CD command %02X %i\n", cdb[0], ins);
        msf = cdb[1] & 2;

	is_error = 0;

	if (atapi->medium_changed())
	{
		atapi_insert_cdrom();
	}
	/*If UNIT_ATTENTION is set, error out with NOT_READY.
	  VIDE-CDD.SYS will then issue a READ_TOC, which can pass through UNIT_ATTENTION and will clear sense.
	  NT 3.1 / AZTIDECD.SYS will then issue a REQUEST_SENSE, which can also pass through UNIT_ATTENTION but will clear sense AFTER sending it back.
	  In any case, if the command cannot pass through, set our state to errored.*/
	if (!(atapi_cmd_table[cdb[0]] & ALLOW_UA) && data->sense_key == SENSE_UNIT_ATTENTION)
	{
//		fatal("atapi_cmd_error(ide, data->sense_key, data->asc);\n");
		is_error = 1;
	}
	/*Unless the command issued was a REQUEST_SENSE or TEST_UNIT_READY, clear sense.
          This is important because both VIDE-CDD.SYS and NT 3.1 / AZTIDECD.SYS rely on this behaving VERY specifically.
          VIDE-CDD.SYS will clear sense through READ_TOC, while NT 3.1 / AZTIDECD.SYS will issue a REQUEST_SENSE.*/
        if ((cdb[0] != GPCMD_REQUEST_SENSE) && (cdb[0] != GPCMD_TEST_UNIT_READY))
	{
		/* GPCMD_TEST_UNIT_READY is NOT supposed to clear sense! */
		atapi_sense_clear(data, cdb[0], 1);
	}

	/*If our state has been set to errored, clear it, and return.*/
	if (is_error)
	{
//                fatal("Clear error state\n");
		is_error = 0;
		return SCSI_PHASE_STATUS;
	}
		
	if ((atapi_cmd_table[cdb[0]] & CHECK_READY) && !atapi->ready())
	{
		atapi_cmd_error(data, SENSE_NOT_READY, ASC_MEDIUM_NOT_PRESENT, 0);
		return SCSI_PHASE_STATUS;
	}

	data->prev_status = data->cd_status;
	data->cd_status = atapi->status();
	if (((data->prev_status == CD_STATUS_PLAYING) || (data->prev_status == CD_STATUS_PAUSED)) && ((data->cd_status != CD_STATUS_PLAYING) && (data->cd_status != CD_STATUS_PAUSED)))
		completed = 1;
	else
		completed = 0;
	
//	pclog("   msf %i completed %i\n", msf, completed);

        data->status = STATUS_GOOD;

        switch (cdb[0])
        {
                case GPCMD_TEST_UNIT_READY:
                data->cmd_pos = CMD_POS_IDLE;
                return SCSI_PHASE_STATUS;

                case SCSI_REZERO_UNIT:
                data->cmd_pos = CMD_POS_IDLE;
                return SCSI_PHASE_STATUS;

                case SCSI_REQUEST_SENSE: /* Used by ROS 4+ */
//                pclog("CD REQ SENSE\n");
		alloc_length = cdb[4];
		temp_command = cdb[0];
		//atapi_command_send_init(ide, temp_command, 18, alloc_length);
				
                /*Will return 18 bytes of 0*/
                memset(data->data_in, 0, 512);

		data->data_in[0] = 0x80 | 0x70;

		if ((data->sense_key > 0) || (cd_status < CD_STATUS_PLAYING))
		{
			if (completed)
			{
//                                pclog("completed\n");
				data->data_in[2] = SENSE_ILLEGAL_REQUEST;
				data->data_in[12] = ASC_AUDIO_PLAY_OPERATION;
				data->data_in[13] = ASCQ_AUDIO_PLAY_OPERATION_COMPLETED;
			}
			else
			{
//                                pclog("!completed %02x %02x %02x\n", data->sense_key, data->asc, data->ascq);
				data->data_in[2] = data->sense_key;
				data->data_in[12] = data->asc;
				data->data_in[13] = data->ascq;
			}
		}
		else
		{
			data->data_in[2] = SENSE_ILLEGAL_REQUEST;
			data->data_in[12] = ASC_AUDIO_PLAY_OPERATION;
			data->data_in[13] = (cd_status == CD_STATUS_PLAYING) ? ASCQ_AUDIO_PLAY_OPERATION_IN_PROGRESS : ASCQ_AUDIO_PLAY_OPERATION_PAUSED;
		}

		//data->data_in[7] = 10;
		data->data_in[7] = 0;

		/* Clear the sense stuff as per the spec. */
		atapi_sense_clear(data, temp_command, 0);

		data->bytes_expected = data->data_pos_write = MIN(alloc_length, 18);
		return SCSI_PHASE_DATA_IN;

                case SCSI_SEEK_6:
                data->cmd_pos = CMD_POS_IDLE;
                return SCSI_PHASE_STATUS;

                case GPCMD_SET_SPEED:
                data->cmd_pos = CMD_POS_IDLE;
                return SCSI_PHASE_STATUS;

		case GPCMD_MECHANISM_STATUS: /*0xbd*/
		alloc_length = (cdb[7]<<16) | (cdb[8]<<8) | cdb[9];

		if (alloc_length == 0)
			fatal("Zero allocation length to MECHANISM STATUS not impl.\n");

		data->data_in[0] = 0;
		data->data_in[1] = 0;
		data->data_in[2] = 0;
		data->data_in[3] = 0;
		data->data_in[4] = 0;
		data->data_in[5] = 1;
		data->data_in[6] = 0;
		data->data_in[7] = 0;
		// len = 8;
			
                data->data_pos_read = 0;
                data->bytes_expected = data->data_pos_write = alloc_length;
                return SCSI_PHASE_DATA_IN;

                case GPCMD_READ_TOC_PMA_ATIP:
//                pclog("Read TOC ready? %08X\n",ide);
		toc_format = cdb[2] & 0xf;
		if (toc_format == 0)
			toc_format = (cdb[9] >> 6) & 3;
                switch (toc_format)
                {
                        case 0: /*Normal*/
//			pclog("ATAPI: READ TOC type requested: Normal\n");
                        len = cdb[8] + (cdb[7] << 8);
                        len = atapi->readtoc(data->data_in, cdb[6], msf, len, 0);
                        break;
                        case 1: /*Multi session*/
//			pclog("ATAPI: READ TOC type requested: Multi-session\n");
                        len = cdb[8] + (cdb[7] << 8);
                        len = atapi->readtoc_session(data->data_in, msf, len);
                        data->data_in[0] = 0;
                        data->data_in[1] = 0xA;
                        break;
			case 2: /*Raw*/
//			pclog("ATAPI: READ TOC type requested: Raw TOC\n");
			len = cdb[8] + (cdb[7] << 8);
			len = atapi->readtoc_raw(data->data_in, len);
			break;
                        default:
                        atapi_cmd_error(data, SENSE_ILLEGAL_REQUEST, ASC_LBA_OUT_OF_RANGE, 0);
                        data->cmd_pos = CMD_POS_IDLE;
                        return SCSI_PHASE_STATUS;
                }
                data->data_pos_read = 0;
                data->bytes_expected = data->data_pos_write = len;
                return SCSI_PHASE_DATA_IN;

                case GPCMD_READ_CD:
//                pclog("Read CD : start LBA %02X%02X%02X%02X Length %02X%02X%02X Flags %02X\n",cdb[2],cdb[3],cdb[4],cdb[5],cdb[6],cdb[7],cdb[8],cdb[9]);
		rcdmode = cdb[9] & 0xF8;
                if ((rcdmode != 0x10) && (rcdmode != 0xF8))
                {
                        atapi_cmd_error(data, SENSE_ILLEGAL_REQUEST, ASC_ILLEGAL_OPCODE, 0);
                        data->cmd_pos = CMD_POS_IDLE;
                        return SCSI_PHASE_STATUS;
                }

                if (data->cmd_pos == CMD_POS_IDLE)
                {
                        data->cdlen = (cdb[6] << 16) | (cdb[7] << 8) | cdb[8];
                        data->cdpos = (cdb[2] << 24) | (cdb[3] << 16) | (cdb[4] << 8) | cdb[5];

                	data->cmd_pos = CMD_POS_TRANSFER;
                	data->bytes_expected = data->cdlen * ((rcdmode == 0x10) ? 2048 : 2352);
                }
                if (!data->cdlen)
                {
                        data->cmd_pos = CMD_POS_IDLE;
                        return SCSI_PHASE_STATUS;
                }

                for (c = 0; c < MAX_NR_SECTORS; c++)
                {
        		if (rcdmode == 0x10)
        		{
                                if (atapi->readsector(&data->data_in[c*2048], data->cdpos))
                                {
//                                        pclog("Read sector failed\n");
                                        atapi_cmd_error(data, SENSE_ILLEGAL_REQUEST, ASC_LBA_OUT_OF_RANGE, 0);
                                        data->cmd_pos = CMD_POS_IDLE;
                                        return SCSI_PHASE_STATUS;
                                }
                        }
        		else
        		{
        	                atapi->readsector_raw(&data->data_in[c*2352], data->cdpos);
                        }
                        data->cdpos++;
                        data->cdlen--;
                        if (!data->cdlen)
                                break;
                }
                readflash_set(READFLASH_HDC, data->id);

                data->data_pos_read = 0;
                data->data_pos_write = c * ((rcdmode == 0x10) ? 2048 : 2352);
                return SCSI_PHASE_DATA_IN;

		case GPCMD_READ_6:
		case GPCMD_READ_10:
		case GPCMD_READ_12:
//                pclog("Read: cmd_pos=%i cdlen=%i %p\n", data->cmd_pos, data->cdlen, &data->data_in[0]);

//		readcdmode = 0;
                if (data->cmd_pos == CMD_POS_IDLE)
                {
                	if (cdb[0] == GPCMD_READ_6)
                	{
                		data->cdlen = cdb[4];
                		data->cdpos = (((uint32_t)cdb[1] & 0x1f) << 16) | ((uint32_t)cdb[2] << 8) | (uint32_t)cdb[3];
                	}
                	else if (cdb[0] == GPCMD_READ_10)
               		{
               			data->cdlen = (cdb[7] << 8) | cdb[8];
               			data->cdpos = (cdb[2] << 24) | (cdb[3] << 16) | (cdb[4] << 8) | cdb[5];
                	}
                	else
               		{
               			data->cdlen = ((uint32_t)cdb[6] << 24) | ((uint32_t)cdb[7] << 16) | ((uint32_t)cdb[8] << 8) | (uint32_t)cdb[9];
               			data->cdpos = ((uint32_t)cdb[2] << 24) | ((uint32_t)cdb[3] << 16) | ((uint32_t)cdb[4] << 8) | (uint32_t)cdb[5];
               		}
//                	pclog("cdpos=%08x cdlen=%08x\n", data->cdpos, data->cdlen);
                	
                	data->cmd_pos = CMD_POS_TRANSFER;
                	data->bytes_expected = data->cdlen * 2048;
                }

                if (!data->cdlen)
                {
                        data->cmd_pos = CMD_POS_IDLE;
                        return SCSI_PHASE_STATUS;
                }
                readflash_set(READFLASH_HDC, data->id);
                for (c = 0; c < MAX_NR_SECTORS; c++)
                {
                        if (atapi->readsector(&data->data_in[c*2048], data->cdpos))
                        {
                                pclog("Read sector failed\n");
                                atapi_cmd_error(data, SENSE_ILLEGAL_REQUEST, ASC_LBA_OUT_OF_RANGE, 0);
                                data->cmd_pos = CMD_POS_IDLE;
                                return SCSI_PHASE_STATUS;
                        }
                        data->cdpos++;
                        data->cdlen--;
                        if (!data->cdlen)
                                break;
                }

                data->data_pos_read = 0;
                data->data_pos_write = c * 2048;
                return SCSI_PHASE_DATA_IN;

                case GPCMD_READ_HEADER:
                if (msf)
                {
                        atapi_cmd_error(data, SENSE_ILLEGAL_REQUEST, ASC_ILLEGAL_OPCODE, 0);
                        return SCSI_PHASE_STATUS;
                }
                for (c = 0; c < 4; c++)
                        data->data_in[c+4] = cdb[c+2];
                data->data_in[0] = 1; /*2048 bytes user data*/
                data->data_in[1] = data->data_in[2] = data->data_in[3] = 0;
                
                data->bytes_expected = data->data_pos_write = 6;
                return SCSI_PHASE_DATA_IN;

		case GPCMD_MODE_SENSE_6:
		case GPCMD_MODE_SENSE_10:
		temp_command = cdb[0];
		
		if (temp_command == GPCMD_MODE_SENSE_6)
			len = cdb[4];
		else
                        len = cdb[8] | (cdb[7] << 8);

                c = cdb[2] & 0x3F;

		memset(data->data_in, 0, len);
		alloc_length = len;
		// for (c=0;c<len;c++) idebufferb[c]=0;
		if (!(mode_sense_pages[c] & IMPLEMENTED))
		{
                        atapi_cmd_error(data, SENSE_ILLEGAL_REQUEST, ASC_INV_FIELD_IN_CMD_PACKET, 0);
                        return SCSI_PHASE_STATUS;
		}
			
		if (temp_command == GPCMD_MODE_SENSE_6)
		{
			len = ide_atapi_mode_sense(data, 4, c);
			data->data_in[0] = len - 1;
			data->data_in[1] = 3; /*120mm data CD-ROM*/
		}
        	else
		{
			len = ide_atapi_mode_sense(data, 8, c);
			data->data_in[0] = (len - 2)>>8;
			data->data_in[1] = (len - 2)&255;
			data->data_in[2] = 3; /*120mm data CD-ROM*/
		}				

                data->bytes_expected = data->data_pos_write = len;
                data->cmd_pos = CMD_POS_IDLE;
                return SCSI_PHASE_DATA_IN;

		case GPCMD_MODE_SELECT_6:
                case GPCMD_MODE_SELECT_10:
                if (data->cmd_pos == CMD_POS_IDLE)
                {
                        int len;
                        
			if (cdb[0] == GPCMD_MODE_SELECT_6)
			{
				len = cdb[4];
				data->prefix_len = 6;
			}
			else
			{
				len = (cdb[7]<<8) | cdb[8];
				data->prefix_len = 10;
			}
			data->page_current = cdb[2];
			if (page_flags[data->page_current] & PAGE_CHANGEABLE)
                                page_flags[GPMODE_CDROM_AUDIO_PAGE] |= PAGE_CHANGED;
                        
                        data->cmd_pos = CMD_POS_TRANSFER;
                        data->bytes_required = len;
                        pclog("MODE_SELECT len=%i\n", len);
                        return SCSI_PHASE_DATA_OUT;
                }
                pclog("MODE_SELECT complete\n");
                data->cmd_pos = CMD_POS_IDLE;
                return SCSI_PHASE_STATUS;

                case GPCMD_GET_EVENT_STATUS_NOTIFICATION: /*0x4a*/
                temp_command = cdb[0];
                alloc_length = (cdb[7] << 16) | (cdb[8] << 8) | cdb[9];
                
                {
                        struct __attribute__((__packed__))
                        {
                                uint8_t opcode;
                                uint8_t polled;
                                uint8_t reserved2[2];
                                uint8_t class;
                                uint8_t reserved3[2];
                                uint16_t len;
                                uint8_t control;
                        } *gesn_cdb;
            
                        struct __attribute__((__packed__))
                        {
                                uint16_t len;
                                uint8_t notification_class;
                                uint8_t supported_events;
                        } *gesn_event_header;
                        unsigned int used_len;
            
                        gesn_cdb = (void *)cdb;
                        gesn_event_header = (void *)data->data_in;
            
                        /* It is fine by the MMC spec to not support async mode operations */
                        if (!(gesn_cdb->polled & 0x01))
                        {   /* asynchronous mode */
                                /* Only polling is supported, asynchronous mode is not. */
                		atapi_cmd_error(data, SENSE_ILLEGAL_REQUEST, ASC_ILLEGAL_OPCODE, 0);
                                return SCSI_PHASE_STATUS;
                        }
            
                        /* polling mode operation */
            
                        /*
                         * These are the supported events.
                         *
                         * We currently only support requests of the 'media' type.
                         * Notification class requests and supported event classes are bitmasks,
                         * but they are built from the same values as the "notification class"
                         * field.
                         */
                        gesn_event_header->supported_events = 1 << GESN_MEDIA;
            
                        /*
                         * We use |= below to set the class field; other bits in this byte
                         * are reserved now but this is useful to do if we have to use the
                         * reserved fields later.
                         */
                        gesn_event_header->notification_class = 0;
            
                        /*
                         * Responses to requests are to be based on request priority.  The
                         * notification_class_request_type enum above specifies the
                         * priority: upper elements are higher prio than lower ones.
                         */
                        if (gesn_cdb->class & (1 << GESN_MEDIA))
                        {
                                gesn_event_header->notification_class |= GESN_MEDIA;
                                used_len = atapi_event_status(data->data_in);
                        }
                        else
                        {
                                gesn_event_header->notification_class = 0x80; /* No event available */
                                used_len = sizeof(*gesn_event_header);
                        }
                        len = used_len;
                        gesn_event_header->len = used_len - sizeof(*gesn_event_header);
                }

                data->bytes_expected = data->data_pos_write = len;
                data->cmd_pos = CMD_POS_IDLE;
                return SCSI_PHASE_DATA_IN;

		case GPCMD_READ_DISC_INFORMATION:
		data->data_in[1] = 32;
		data->data_in[2] = 0xe; /* last session complete, disc finalized */
		data->data_in[3] = 1; /* first track on disc */
		data->data_in[4] = 1; /* # of sessions */
		data->data_in[5] = 1; /* first track of last session */
		data->data_in[6] = 1; /* last track of last session */
		data->data_in[7] = 0x20; /* unrestricted use */
		data->data_in[8] = 0x00; /* CD-ROM */
			
                data->bytes_expected = data->data_pos_write = 34;
                data->cmd_pos = CMD_POS_IDLE;
                return SCSI_PHASE_DATA_IN;

                case GPCMD_PLAY_AUDIO_10:
                case GPCMD_PLAY_AUDIO_12:
                case GPCMD_PLAY_AUDIO_MSF:
		/*This is apparently deprecated in the ATAPI spec, and apparently
                  has been since 1995 (!). Hence I'm having to guess most of it*/
		if (cdb[0] == GPCMD_PLAY_AUDIO_10)
		{
			pos = (cdb[2] << 24) | (cdb[3] << 16) | (cdb[4] << 8) | cdb[5];
			len = (cdb[7] << 8) | cdb[8];
		}
		else if (cdb[0] == GPCMD_PLAY_AUDIO_MSF)
		{
			pos = (cdb[3] << 16) | (cdb[4] << 8) | cdb[5];
			len = (cdb[6] << 16) | (cdb[7] << 8) | cdb[8];
		}
		else
		{
			pos = (cdb[3] << 16) | (cdb[4] << 8) | cdb[5];
			len = (cdb[7] << 16) | (cdb[8] << 8) | cdb[9];
		}


		if ((cdrom_drive < 1) || (cd_status <= CD_STATUS_DATA_ONLY) ||
                    !atapi->is_track_audio(pos, (cdb[0] == GPCMD_PLAY_AUDIO_MSF) ? 1 : 0))
                {
                        atapi_cmd_error(data, SENSE_ILLEGAL_REQUEST, ASC_ILLEGAL_MODE_FOR_THIS_TRACK, 0);
                        return SCSI_PHASE_STATUS;
                }
				
                atapi->playaudio(pos, len, (cdb[0] == GPCMD_PLAY_AUDIO_MSF) ? 1 : 0);
                data->cmd_pos = CMD_POS_IDLE;
                return SCSI_PHASE_STATUS;
                
                case GPCMD_READ_SUBCHANNEL:
                if (cdb[3] != 1)
                {
                        atapi_cmd_error(data, SENSE_ILLEGAL_REQUEST, ASC_ILLEGAL_OPCODE, 0);
                        return SCSI_PHASE_STATUS;
                }
                pos = 0;
                data->data_in[pos++] = 0;
                data->data_in[pos++] = 0; /*Audio status*/
                data->data_in[pos++] = 0; data->data_in[pos++] = 0; /*Subchannel length*/
                data->data_in[pos++] = 1; /*Format code*/
                data->data_in[1] = atapi->getcurrentsubchannel(&data->data_in[5], msf);
//                pclog("Read subchannel complete - audio status %02X\n",idebufferb[1]);
                len = 11+5;
                if (!(cdb[2] & 0x40)) len=4;

		data->bytes_expected = data->data_pos_write = len;
		return SCSI_PHASE_DATA_IN;

                case GPCMD_START_STOP_UNIT:
                if (cdb[4] != 2 && cdb[4] != 3 && cdb[4])
                {
			atapi_cmd_error(data, SENSE_ILLEGAL_REQUEST, ASC_ILLEGAL_OPCODE, 0);
                        return SCSI_PHASE_STATUS;
                }
                if (!cdb[4])          atapi->stop();
                else if (cdb[4] == 2) atapi->eject();
                else                  atapi->load();
                data->cmd_pos = CMD_POS_IDLE;
                return SCSI_PHASE_STATUS;

                case GPCMD_INQUIRY:
		page_code = cdb[2];
		max_len = cdb[4];
		alloc_length = max_len;
		temp_command = cdb[0];
				
		if (cdb[1] & 1)
		{
			preamble_len = 4;
			size_idx = 3;
					
			data->data_in[idx++] = 05;
			data->data_in[idx++] = page_code;
			data->data_in[idx++] = 0;
					
			idx++;
			
			switch (page_code)
			{
				case 0x00:
				data->data_in[idx++] = 0x00;
				data->data_in[idx++] = 0x83;
				break;
				
        			case 0x83:
				if (idx + 24 > max_len)
				{
					atapi_cmd_error(data, SENSE_ILLEGAL_REQUEST, ASC_DATA_PHASE_ERROR, 0);
                                        return SCSI_PHASE_STATUS;
				}
				data->data_in[idx++] = 0x02;
				data->data_in[idx++] = 0x00;
				data->data_in[idx++] = 0x00;
				data->data_in[idx++] = 20;
				ide_padstr8(data->data_in + idx, 20, "53R141");	/* Serial */
				idx += 20;

				if (idx + 72 > max_len)
				{
					goto atapi_out;
				}
				data->data_in[idx++] = 0x02;
				data->data_in[idx++] = 0x01;
				data->data_in[idx++] = 0x00;
				data->data_in[idx++] = 68;
				ide_padstr8(data->data_in + idx, 8, "PCem"); /* Vendor */
				idx += 8;
				ide_padstr8(data->data_in + idx, 40, "PCemCD v1.0"); /* Product */
				idx += 40;
				ide_padstr8(data->data_in + idx, 20, "53R141"); /* Product */
				idx += 20;				
				break;
				
				default:
				atapi_cmd_error(data, SENSE_ILLEGAL_REQUEST, ASC_INV_FIELD_IN_CMD_PACKET, 0);
                                return SCSI_PHASE_STATUS;
        		}
		}
		else
		{
			preamble_len = 5;
			size_idx = 4;

			data->data_in[0] = 5; /*CD-ROM*/
			data->data_in[1] = 0x80; /*Removable*/
			data->data_in[2] = 0;
			data->data_in[3] = 0x21;
			data->data_in[4] = 31;
			data->data_in[5] = 0;
			data->data_in[6] = 0;
			data->data_in[7] = 0;

                        ide_padstr8(data->data_in + 8, 8, "PCem"); /* Vendor */
                        ide_padstr8(data->data_in + 16, 16, "PCemCD"); /* Product */
                        ide_padstr8(data->data_in + 32, 4, "1.0"); /* Revision */
					
                        idx = 36;
		}

atapi_out:
		data->data_in[size_idx] = idx - preamble_len;
				
		data->bytes_expected = data->data_pos_write = MIN(alloc_length, idx);
		return SCSI_PHASE_DATA_IN;

                case GPCMD_PREVENT_REMOVAL:
                data->cmd_pos = CMD_POS_IDLE;
                return SCSI_PHASE_STATUS;

                case GPCMD_PAUSE_RESUME:
                if (cdb[8] & 1) atapi->resume();
                else            atapi->pause();
                data->cmd_pos = CMD_POS_IDLE;
                return SCSI_PHASE_STATUS;

                case GPCMD_SEEK:
                pos = (cdb[3] << 16) | (cdb[4] << 8) | cdb[5];
                atapi->seek(pos);
                data->cmd_pos = CMD_POS_IDLE;
                return SCSI_PHASE_STATUS;

                case GPCMD_READ_CDROM_CAPACITY:
                size = atapi->size();
                scsi_add_data(data, (size >> 24) & 0xff);
                scsi_add_data(data, (size >> 16) & 0xff);
                scsi_add_data(data, (size >> 8) & 0xff);
                scsi_add_data(data, size & 0xff);
                scsi_add_data(data, (2048 >> 24) & 0xff);
                scsi_add_data(data, (2048 >> 16) & 0xff);
                scsi_add_data(data, (2048 >> 8) & 0xff);
                scsi_add_data(data, 2048 & 0xff);
                pclog("READ_CDROM_CAPACITY %08x\n", size);
                data->cmd_pos = CMD_POS_IDLE;
                return SCSI_PHASE_DATA_IN;

                case GPCMD_STOP_PLAY_SCAN:
                atapi->stop();
                data->cmd_pos = CMD_POS_IDLE;
                return SCSI_PHASE_STATUS;

                case GPCMD_SEND_DVD_STRUCTURE:
                default:
		atapi_cmd_error(data, SENSE_ILLEGAL_REQUEST, ASC_ILLEGAL_OPCODE, 0);
                return SCSI_PHASE_STATUS;
        }
}

static uint8_t scsi_cd_read(void *p)
{
        scsi_cd_data_t *data = p;
        uint8_t temp;

        data->data_bytes_read++;
        temp = data->data_in[data->data_pos_read++];

        if (data->data_pos_read == data->data_pos_write && data->data_bytes_read < data->bytes_expected)
                scsi_cd_command(data->cdb, data);
        
        return temp;
}

static void scsi_cd_write(uint8_t val, void *p)
{
        scsi_cd_data_t *data = p;

        data->data_out[data->data_pos_write++] = val;
        
        data->bytes_received++;

        if (data->data_pos_write > BUFFER_SIZE)
                fatal("Exceeded data_out buffer size\n");
}

static int scsi_cd_read_complete(void *p)
{
        scsi_cd_data_t *data = p;

        return (data->data_bytes_read == data->bytes_expected);
}
static int scsi_cd_write_complete(void *p)
{
        scsi_cd_data_t *data = p;

        return (data->bytes_received == data->bytes_required);
}

static void scsi_cd_start_command(void *p)
{
        scsi_cd_data_t *data = p;

        data->bytes_received = 0;
        data->bytes_required = 0;
        data->data_pos_read = data->data_pos_write = 0;
        data->data_bytes_read = data->bytes_expected = 0;
        
        data->received_cdb = 0;
        
        data->cmd_pos = CMD_POS_IDLE;
        memset(data->data_in, 0, 256);
}

static uint8_t scsi_cd_get_status(void *p)
{
        scsi_cd_data_t *data = p;
        
        return data->status;
}

scsi_device_t scsi_cd =
{
        scsi_cd_init,
        scsi_cd_close,
        
        scsi_cd_start_command,
        scsi_cd_command,
        
        scsi_cd_get_status,
        
        scsi_cd_read,
        scsi_cd_write,
        scsi_cd_read_complete,
        scsi_cd_write_complete,
};
