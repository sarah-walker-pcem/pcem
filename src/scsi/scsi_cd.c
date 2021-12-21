#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "ibm.h"
#include "ide.h"
#include "ide_atapi.h"
#include "scsi.h"
#include "scsi_cd.h"
#include "timer.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define BUFFER_SIZE (256*1024)

#define MAX_NR_SECTORS 16

enum
{
        CMD_POS_IDLE = 0,
        CMD_POS_WAIT,
        CMD_POS_START_SECTOR,
        CMD_POS_TRANSFER,
        CMD_POS_COMPLETE
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
        
        int cmd_pos, new_cmd_pos;
	pc_timer_t callback_timer;
        int wait_time;
        scsi_bus_t *bus;
        
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

        int is_atapi;
        atapi_device_t *atapi_dev;
                
        int pio_mode, mdma_mode;
        
        int short_seek, long_seek;
        int max_speed, cur_speed;

        int cur_model;
} scsi_cd_data_t;

static scsi_cd_data_t *cd_data = NULL;

/*Note - all transfer speeds currently assume CAV behaviour. Drives above about 8x
  should use CLV instead, but this is not currently supported.*/
static struct
{
        int speed;
        int short_seek;
        int long_seek;
} cd_speeds[] =
{
        {1, 240, 1446},
        {2, 160, 1000},
        {3, 150, 900},
        {4, 112, 675},
        {6, 112, 675},
        {8, 112, 675},
        {12,  75, 400},
        {16,  58, 350},
        {20,  50, 300},
        {24,  45, 270},
        {32,  45, 270},
        {40,  50, 300},
        {44,  50, 300},
        {48,  50, 300},
        {52,  45, 270},
        {56,  45, 270},
        {72,  45, 270},
};

int cd_speed;

int cd_get_speed(int i)
{
        return cd_speeds[i].speed;
}

void cd_set_speed(int speed)
{
        if (cd_data)
        {
                int c = 0;
                
                while (1)
                {
                        if (cd_speeds[c].speed == speed)
                                break;
                        if (cd_speeds[c].speed >= MAX_CD_SPEED)
                                break;
                        
                        c++;
                }
                
                cd_data->max_speed = speed;
                cd_data->cur_speed = speed;
                cd_data->short_seek = cd_speeds[c].short_seek;
                cd_data->long_seek = cd_speeds[c].long_seek;
        }
}

static struct
{
        // suffix numbers are sizes from the specs
        char	*vendor_8;
        char 	*model_and_firmware_40;
        char	*serial_20;
        char	*model_16;
        char	*firmware_4;

        char	*serial2_20;
        char	*firmware2_8;
        char	*model2_40;

        // for PCem config only
        char	*model_string_40;
        char	*model_config_string_40;
        int      interfaces;
        int      speed; // Not an index, but a "speed" value. -1 == allow override
} const cd_models[] =
{
        {
                // Generic PCem CD
                "PCem",
                "PCemCD v1.0",
                "53R141",
                "PCemCD",
                "1.0",

                "",
                "v1.0",
                "PCemCD",
                "PCemCD",
                "pcemcd",
                CD_MODEL_INTERFACE_ALL,
                -1,
        },
        {
                // A 4x CD-ROM drive from Aztech. Choose if your system image has the SGIDECD.SYS driver
                "AZT",
                "AZT 46802I v1.15",
                "53R141", // TODO: clone serial from my real one
                "46802I",
                "1.15",

                "",
                "v1.15",
                "CDA46802I",
                "AZT CDA 468-02I 4X",
                "azt_cda_468_02i_4x",
                CD_MODEL_INTERFACE_IDE,
                4,
        },
};

char *cd_model = NULL;

char *cd_get_model(int i)
{
        return cd_models[i].model_string_40;
}

char *cd_get_config_model(int i)
{
        return cd_models[i].model_config_string_40;
}

void cd_set_model(char *model)
{
        if (cd_data)
        {
                int c = 0;

                while (1)
                {
                        if (!model)
                                break;
                        if (c > MAX_CD_MODEL)
                                break;
                        if (!strncmp(cd_models[c].model_string_40, model, 40))
                                break;

                        c++;
                }

                cd_data->cur_model = c;
        }
}

int cd_get_model_interfaces(int i)
{
        return cd_models[i].interfaces;
}

int cd_get_model_speed(int i)
{
        return cd_models[i].speed;
}

char *cd_model_to_config(char *model)
{
        int c = 0;

        while (1)
        {
                if (!model)
                        break;
                if (c > MAX_CD_MODEL)
                {
                        c = 0; // default
                        break;
                }
                if (!strncmp(cd_models[c].model_string_40, model, 40))
                        break;

                c++;
        }
        return cd_models[c].model_config_string_40;
}

char *cd_model_from_config(char *config)
{
        int c = 0;

        while (1)
        {
                if (!config)
                        break;
                if (c > MAX_CD_MODEL)
                {
                        c = 0; // default
                        break;
                }
                if (!strncmp(cd_models[c].model_config_string_40, config, 40))
                        break;

                c++;
        }
        return cd_models[c].model_string_40;
}

static void scsi_cd_callback(void *p)
{
        scsi_cd_data_t *data = p;

        if (data->cmd_pos == CMD_POS_WAIT)
        {
                data->wait_time--;
                if (!data->wait_time)
                {
                        data->cmd_pos = data->new_cmd_pos;
                        scsi_bus_kick(data->bus);
                }
                else
                        timer_advance_u64(&data->callback_timer, 1000 * TIMER_USEC);
        }
}

//#define SECTOR_TIME 13333 /*Time between sectors on 1x drive*/
#define SECTOR_TIME 12000 /*Adjusted from above value to account for transfer time on current SCSI/ATAPI code*/

/*The seek model is currently very basic. Seeks of less than MIN_SEEK sectors are treated as
  immediate. Seeks between MIN_SEEK and MAX_SEEK sectors scale linearly between short_seek and
  long seek.
  
  This really shouldn't be a linear scale, but this is a good enough approximation for now.*/
#define MIN_SEEK   2000
#define MAX_SEEK 333000

static int get_seek_time(scsi_cd_data_t *data, uint32_t start, uint32_t dest)
{
        uint32_t diff = ABS((int)(start - dest));

        if (diff < MIN_SEEK)
                return 0;
        if (diff > MAX_SEEK)
                diff = MAX_SEEK;
        
        diff -= MIN_SEEK;
        
        return data->short_seek + ((data->long_seek * diff) / (MAX_SEEK - MIN_SEEK));
}

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

        data->bus = bus;
        data->id = id;

	page_flags[GPMODE_CDROM_AUDIO_PAGE] &= 0xFD;		/* Clear changed flag for CDROM AUDIO mode page. */
	memset(mode_pages_in[GPMODE_CDROM_AUDIO_PAGE], 0, 256);	/* Clear the page itself. */
	page_flags[GPMODE_CDROM_AUDIO_PAGE] &= ~PAGE_CHANGED;

        timer_add(&data->callback_timer, scsi_cd_callback, data, 0);
        
        cd_data = data;
        cd_set_speed(cd_speed);
        cd_set_model(cd_model);
                
        return data;
}

static void *scsi_cd_atapi_init(scsi_bus_t *bus, int id, atapi_device_t *atapi_dev)
{
        scsi_cd_data_t *data = scsi_cd_init(bus, id);
        
        data->is_atapi = 1;
        data->atapi_dev = atapi_dev;
        
        return data;
}

static void scsi_cd_close(void *p)
{
        scsi_cd_data_t *data = p;
        
        cd_data = NULL;
        free(data);
}

static void scsi_cd_reset(void *p)
{
        scsi_cd_data_t *data = p;

        timer_disable(&data->callback_timer);
        data->cmd_pos = CMD_POS_IDLE;
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
			buf[pos++] = 5; /* Reserved */
			buf[pos++] = 4; /* Reserved */
			buf[pos++] = 0; /* Reserved */
			buf[pos++] = 0x80; /* Reserved */
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
        	buf[pos++] = 0x03; /* Some other stuff not supported */
        	buf[pos++] = (uint8_t) ((data->max_speed * 176) >> 8);
        	buf[pos++] = (uint8_t) (data->max_speed * 176); /* Maximum speed */
        	buf[pos++] = 1; buf[pos++] = 0; /* Number of audio levels - on and off only */
        	buf[pos++] = 0; buf[pos++] = 0; /* Buffer size - none */
        	buf[pos++] = (uint8_t) ((data->cur_speed * 176) >> 8);
        	buf[pos++] = (uint8_t) (data->cur_speed * 176); /* Current speed */
        	buf[pos++] = 0; /* Reserved */
        	buf[pos++] = 0; /* Drive digital format */
        	buf[pos++] = 0; /* Reserved */
        	buf[pos++] = 0; /* Reserved */
        }

	return pos;
}

static void cdrom_mode_select(scsi_cd_data_t *data, int data_len)
{
        int len = data->data_out[1] | (data->data_out[0] << 8);
        int pos = 8 + len; /*Skip over mode parameter header*/
        
        while (pos < data_len)
        {
                int page_code = data->data_out[pos] & 0x3f;
                len = data->data_out[pos+1];
                
                pos += 2;
                
                switch (page_code)
                {
                        case GPMODE_CDROM_AUDIO_PAGE:
                        if (len != 0xe)
                        {
                                atapi_cmd_error(data, KEY_ILLEGAL_REQ, ASC_INV_FIELD_IN_CMD_PACKET, 0);
                                return;
                        }
                        memcpy(mode_pages_in[GPMODE_CDROM_AUDIO_PAGE], &data->data_out[pos], 0xe);
                        page_flags[GPMODE_CDROM_AUDIO_PAGE] |= PAGE_CHANGED;
                        pos += len;
                        break;
                        
                        case GPMODE_R_W_ERROR_PAGE:
                        if (len != 0x6)
                        {
                                atapi_cmd_error(data, KEY_ILLEGAL_REQ, ASC_INV_FIELD_IN_CMD_PACKET, 0);
                                return;
                        }
                        pos += len;
                        break;
                        
                        case GPMODE_CDROM_PAGE:
                        if (len != 0x6)
                        {
                                atapi_cmd_error(data, KEY_ILLEGAL_REQ, ASC_INV_FIELD_IN_CMD_PACKET, 0);
                                return;
                        }
                        pos += len;
                        break;
                        
                        case GPMODE_CAPABILITIES_PAGE:
                        if (len != 0x12)
                        {
                                atapi_cmd_error(data, KEY_ILLEGAL_REQ, ASC_INV_FIELD_IN_CMD_PACKET, 0);
                                return;
                        }
                        pos += len;
                        break;
                        
                        default:
                        atapi_cmd_error(data, KEY_ILLEGAL_REQ, ASC_INV_FIELD_IN_CMD_PACKET, 0);
                        return;
                }
        }
}

uint32_t atapi_get_cd_channel(int channel)
{
	return (page_flags[GPMODE_CDROM_AUDIO_PAGE] & PAGE_CHANGED) ? mode_pages_in[GPMODE_CDROM_AUDIO_PAGE][channel ? 8 : 6] : (channel + 1);
}

uint32_t atapi_get_cd_volume(int channel)
{
	return (page_flags[GPMODE_CDROM_AUDIO_PAGE] & PAGE_CHANGED) ? mode_pages_in[GPMODE_CDROM_AUDIO_PAGE][channel ? 9 : 7] : 0xFF;
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
	int changed;
        
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
	changed = atapi->medium_changed();

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
	if ((atapi_cmd_table[cdb[0]] & CHECK_READY) && changed)
	{
                pclog("Medium changed\n");
		atapi_cmd_error(data, SENSE_UNIT_ATTENTION, ASC_MEDIUM_MAY_HAVE_CHANGED, 0);
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
                if (data->cmd_pos == CMD_POS_IDLE)
                {
                        uint32_t old_pos = data->cdpos;
                        int seek_time;
                        
                	data->cdpos = 0;

                        seek_time = get_seek_time(data, old_pos, data->cdpos);
                        if (seek_time)
                        {
                                data->cmd_pos = CMD_POS_WAIT;
                                data->new_cmd_pos = CMD_POS_COMPLETE;
                                data->wait_time = seek_time;
                                timer_set_delay_u64(&data->callback_timer, 1000 * TIMER_USEC);
                                return SCSI_PHASE_COMMAND;
                        }
                }
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

		if ((data->sense_key > 0) || (data->cd_status < CD_STATUS_PLAYING))
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
			data->data_in[13] = (data->cd_status == CD_STATUS_PLAYING) ? ASCQ_AUDIO_PLAY_OPERATION_IN_PROGRESS : ASCQ_AUDIO_PLAY_OPERATION_PAUSED;
		}

        	data->data_in[7] = 0;

		/* Clear the sense stuff as per the spec. */
		atapi_sense_clear(data, temp_command, 0);

		data->bytes_expected = data->data_pos_write = MIN(alloc_length, 18);
                return alloc_length ? SCSI_PHASE_DATA_IN : SCSI_PHASE_STATUS;

                case SCSI_SEEK_6:
                if (data->cmd_pos == CMD_POS_IDLE)
                {
                        uint32_t old_pos = data->cdpos;
                        int seek_time;
                        
                	data->cdpos = (((uint32_t)cdb[1] & 0x1f) << 16) | ((uint32_t)cdb[2] << 8) | (uint32_t)cdb[3];

                        seek_time = get_seek_time(data, old_pos, data->cdpos);
                        if (seek_time)
                        {
                                data->cmd_pos = CMD_POS_WAIT;
                                data->new_cmd_pos = CMD_POS_COMPLETE;
                                data->wait_time = seek_time;
                                timer_set_delay_u64(&data->callback_timer, 1000 * TIMER_USEC);
                                return SCSI_PHASE_COMMAND;
                        }
                }
                data->cmd_pos = CMD_POS_IDLE;
                return SCSI_PHASE_STATUS;

                case GPCMD_SET_SPEED:
                data->cur_speed = (cdb[3] | (cdb[2] << 8)) / 176;
                if (data->cur_speed < 1)
                        data->cur_speed = 1;
                else if (data->cur_speed > data->max_speed)
                        data->cur_speed = data->max_speed;
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
                data->bytes_expected = data->data_pos_write = MIN(alloc_length, 8);
                return alloc_length ? SCSI_PHASE_DATA_IN : SCSI_PHASE_STATUS;

                case GPCMD_READ_TOC_PMA_ATIP:
//                pclog("Read TOC ready? %08X\n",ide);
		toc_format = cdb[2] & 0xf;
		if (toc_format == 0)
			toc_format = (cdb[9] >> 6) & 3;
		alloc_length = cdb[8] + (cdb[7] << 8);
                switch (toc_format)
                {
                        case 0: /*Normal*/
//			pclog("ATAPI: READ TOC type requested: Normal\n");
                        len = atapi->readtoc(data->data_in, cdb[6], msf, alloc_length, 0);
                        break;
                        case 1: /*Multi session*/
//			pclog("ATAPI: READ TOC type requested: Multi-session\n");
                        len = atapi->readtoc_session(data->data_in, msf, alloc_length);
                        data->data_in[0] = 0;
                        data->data_in[1] = 0xA;
                        break;
			case 2: /*Raw*/
//			pclog("ATAPI: READ TOC type requested: Raw TOC\n");
			len = atapi->readtoc_raw(data->data_in, alloc_length);
			break;
                        default:
                        atapi_cmd_error(data, SENSE_ILLEGAL_REQUEST, ASC_LBA_OUT_OF_RANGE, 0);
                        data->cmd_pos = CMD_POS_IDLE;
                        return SCSI_PHASE_STATUS;
                }
                data->data_pos_read = 0;
                data->bytes_expected = data->data_pos_write = MIN(alloc_length, len);
                return alloc_length ? SCSI_PHASE_DATA_IN : SCSI_PHASE_STATUS;

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
                        uint32_t old_pos = data->cdpos;
                        int seek_time;
                        
                        data->cdlen = (cdb[6] << 16) | (cdb[7] << 8) | cdb[8];
                        data->cdpos = (cdb[2] << 24) | (cdb[3] << 16) | (cdb[4] << 8) | cdb[5];

                	data->cmd_pos = CMD_POS_START_SECTOR;
                	data->bytes_expected = data->cdlen * ((rcdmode == 0x10) ? 2048 : 2352);
                	if (data->is_atapi)
                	{
                        	if (rcdmode == 0x10)
                        	       atapi_set_transfer_granularity(data->atapi_dev, 2048);
                        	else
                        	       atapi_set_transfer_granularity(data->atapi_dev, 2352);
                        }

                        seek_time = get_seek_time(data, old_pos, data->cdpos);
                        if (seek_time)
                        {
                                data->cmd_pos = CMD_POS_WAIT;
                                data->new_cmd_pos = CMD_POS_START_SECTOR;
                                data->wait_time = seek_time;
                                timer_set_delay_u64(&data->callback_timer, 1000 * TIMER_USEC);
                                return SCSI_PHASE_COMMAND;
                        }
                }
                if (data->cmd_pos == CMD_POS_WAIT)
                        return SCSI_PHASE_COMMAND;
                if (!data->cdlen)
                {
                        data->cmd_pos = CMD_POS_IDLE;
                        return SCSI_PHASE_STATUS;
                }
                if (data->cmd_pos == CMD_POS_START_SECTOR)
                {
                        uint64_t time = (((uint64_t)SECTOR_TIME * TIMER_USEC) / data->cur_speed) * (uint64_t)data->cdlen;

                        data->cmd_pos = CMD_POS_WAIT;
                        data->new_cmd_pos = CMD_POS_TRANSFER;

                        if (time > (TIMER_USEC * 1000ull))
                        {
                                data->wait_time = (int)(time / (TIMER_USEC * 1000ull));
                                timer_set_delay_u64(&data->callback_timer, time % (TIMER_USEC * 1000ull));
                        }
                        else
                        {
                                timer_set_delay_u64(&data->callback_timer, time);
                                data->wait_time = 1;
                        }
                        return SCSI_PHASE_COMMAND;
                }

        	if (rcdmode == 0x10)
        	{
                        c = MIN(data->cdlen, MAX_NR_SECTORS);
                        if (atapi->readsector(data->data_in, data->cdpos, c))
                        {
//                                pclog("Read sector failed\n");
                                atapi_cmd_error(data, SENSE_ILLEGAL_REQUEST, ASC_LBA_OUT_OF_RANGE, 0);
                                data->cmd_pos = CMD_POS_IDLE;
                                return SCSI_PHASE_STATUS;
                        }
                        data->cdpos += c;
                        data->cdlen -= c;
                }
                else
                {
                        for (c = 0; c < MAX_NR_SECTORS; c++)
                        {
        	                atapi->readsector_raw(&data->data_in[c*2352], data->cdpos);

                                data->cdpos++;
                                data->cdlen--;
                                if (!data->cdlen)
                                {
                                        c++;
                                        break;
                                }
                        }
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
                        uint32_t old_pos = data->cdpos;
                        int seek_time;
                        
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
                	
                	data->cmd_pos = CMD_POS_START_SECTOR;
                	data->bytes_expected = data->cdlen * 2048;
                	if (data->is_atapi)
                                atapi_set_transfer_granularity(data->atapi_dev, 2048);
                        
                        seek_time = get_seek_time(data, old_pos, data->cdpos);

                        if (seek_time)
                        {
                                data->cmd_pos = CMD_POS_WAIT;
                                data->new_cmd_pos = CMD_POS_START_SECTOR;
                                data->wait_time = seek_time;
                                timer_set_delay_u64(&data->callback_timer, 1000 * TIMER_USEC);
                                return SCSI_PHASE_COMMAND;
                        }
                }
                if (data->cmd_pos == CMD_POS_WAIT)
                        return SCSI_PHASE_COMMAND;
                if (!data->cdlen)
                {
                        data->cmd_pos = CMD_POS_IDLE;
                        return SCSI_PHASE_STATUS;
                }
                if (data->cmd_pos == CMD_POS_START_SECTOR)
                {
                        uint64_t time = (((uint64_t)SECTOR_TIME * TIMER_USEC) / data->cur_speed) * (uint64_t)data->cdlen;

                        data->cmd_pos = CMD_POS_WAIT;
                        data->new_cmd_pos = CMD_POS_TRANSFER;
                        
                        if (time > (TIMER_USEC * 1000ull))
                        {
                                data->wait_time = (int)(time / (TIMER_USEC * 1000ull));
                                timer_set_delay_u64(&data->callback_timer, time % (TIMER_USEC * 1000ull));
                        }
                        else
                        {
                                timer_set_delay_u64(&data->callback_timer, time);
                                data->wait_time = 1;
                        }
                        return SCSI_PHASE_COMMAND;
                }
                
                readflash_set(READFLASH_HDC, data->id);
                c = MIN(data->cdlen, MAX_NR_SECTORS);
                if (atapi->readsector(data->data_in, data->cdpos, c))
                {
//                        pclog("Read sector failed\n");
                        atapi_cmd_error(data, SENSE_ILLEGAL_REQUEST, ASC_LBA_OUT_OF_RANGE, 0);
                        data->cmd_pos = CMD_POS_IDLE;
                        return SCSI_PHASE_STATUS;
                }
                data->cdpos += c;
                data->cdlen -= c;

                data->data_pos_read = 0;
                data->data_pos_write = c * 2048;
                return SCSI_PHASE_DATA_IN;

                case GPCMD_READ_HEADER:
                alloc_length = cdb[8] | (cdb[7] << 8);
                if (msf)
                {
                        atapi_cmd_error(data, SENSE_ILLEGAL_REQUEST, ASC_ILLEGAL_OPCODE, 0);
                        return SCSI_PHASE_STATUS;
                }
                for (c = 0; c < 4; c++)
                        data->data_in[c+4] = cdb[c+2];
                data->data_in[0] = 1; /*2048 bytes user data*/
                data->data_in[1] = data->data_in[2] = data->data_in[3] = 0;
                
                data->bytes_expected = data->data_pos_write = MIN(6, alloc_length);
                return alloc_length ? SCSI_PHASE_DATA_IN : SCSI_PHASE_STATUS;

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

                data->bytes_expected = data->data_pos_write = MIN(len, alloc_length);
                data->cmd_pos = CMD_POS_IDLE;
                return alloc_length ? SCSI_PHASE_DATA_IN : SCSI_PHASE_STATUS;

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
                        
                        data->cmd_pos = CMD_POS_TRANSFER;
                        data->bytes_required = len;
                        return SCSI_PHASE_DATA_OUT;
                }
                if (data->bytes_received == data->bytes_required)
                {
                        cdrom_mode_select(data, data->bytes_required);
                        data->cmd_pos = CMD_POS_IDLE;
                        return SCSI_PHASE_STATUS;
                }
                return SCSI_PHASE_DATA_OUT;

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

                data->bytes_expected = data->data_pos_write = MIN(alloc_length, len);
                data->cmd_pos = CMD_POS_IDLE;
                return alloc_length ? SCSI_PHASE_DATA_IN : SCSI_PHASE_STATUS;

		case GPCMD_READ_DISC_INFORMATION:
                alloc_length = cdb[8] | (cdb[7] << 8);
                
		data->data_in[1] = 32;
		data->data_in[2] = 0xe; /* last session complete, disc finalized */
		data->data_in[3] = 1; /* first track on disc */
		data->data_in[4] = 1; /* # of sessions */
		data->data_in[5] = 1; /* first track of last session */
		data->data_in[6] = 1; /* last track of last session */
		data->data_in[7] = 0x20; /* unrestricted use */
		data->data_in[8] = 0x00; /* CD-ROM */
			
                data->bytes_expected = data->data_pos_write = MIN(alloc_length, 34);
                data->cmd_pos = CMD_POS_IDLE;
                return alloc_length ? SCSI_PHASE_DATA_IN : SCSI_PHASE_STATUS;

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


		if ((cdrom_drive < 1) || (data->cd_status <= CD_STATUS_DATA_ONLY) ||
                    !atapi->is_track_audio(pos, (cdb[0] == GPCMD_PLAY_AUDIO_MSF) ? 1 : 0))
                {
                        atapi_cmd_error(data, SENSE_ILLEGAL_REQUEST, ASC_ILLEGAL_MODE_FOR_THIS_TRACK, 0);
                        return SCSI_PHASE_STATUS;
                }
				
                atapi->playaudio(pos, len, (cdb[0] == GPCMD_PLAY_AUDIO_MSF) ? 1 : 0);
                data->cmd_pos = CMD_POS_IDLE;
                return SCSI_PHASE_STATUS;
                
                case GPCMD_READ_SUBCHANNEL:
                alloc_length = cdb[8] | (cdb[7] << 8);
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

		data->bytes_expected = data->data_pos_write = MIN(alloc_length, len);
		return alloc_length ? SCSI_PHASE_DATA_IN : SCSI_PHASE_STATUS;

                case GPCMD_START_STOP_UNIT:
                if (!cdb[4])          atapi->stop();
                else if (cdb[4] == 2) atapi->eject();
                else if (cdb[4] == 3) atapi->load();
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
				ide_padstr8(data->data_in + idx, 8, cd_models[cd_data->cur_model].vendor_8); /* Vendor */
				idx += 8;
				ide_padstr8(data->data_in + idx, 40, cd_models[cd_data->cur_model].model_and_firmware_40); /* Product */
				idx += 40;
				ide_padstr8(data->data_in + idx, 20, cd_models[cd_data->cur_model].serial_20); /* Product */
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
			if (data->is_atapi)
			{
        			data->data_in[2] = 0; /*Not ANSI compliant*/
        			data->data_in[3] = 0x21; /*ATAPI compliant*/
                        }
			else
			{
        			data->data_in[2] = 2; /*SCSI-2 compliant*/
        			data->data_in[3] = 0x02;
                        }
			data->data_in[4] = 31;
			data->data_in[5] = 0;
			data->data_in[6] = 0;
			data->data_in[7] = 0;

                        ide_padstr8(data->data_in + 8, 8, cd_models[cd_data->cur_model].vendor_8); /* Vendor */
                        ide_padstr8(data->data_in + 16, 16, cd_models[cd_data->cur_model].model_16); /* Product */
                        ide_padstr8(data->data_in + 32, 4, cd_models[cd_data->cur_model].firmware_4); /* Revision */
					
                        idx = 36;
		}

atapi_out:
		data->data_in[size_idx] = idx - preamble_len;
				
		data->bytes_expected = data->data_pos_write = MIN(alloc_length, idx);
		return alloc_length ? SCSI_PHASE_DATA_IN : SCSI_PHASE_STATUS;

                case GPCMD_PREVENT_REMOVAL:
                data->cmd_pos = CMD_POS_IDLE;
                return SCSI_PHASE_STATUS;

                case GPCMD_PAUSE_RESUME:
                if (cdb[8] & 1) atapi->resume();
                else            atapi->pause();
                data->cmd_pos = CMD_POS_IDLE;
                return SCSI_PHASE_STATUS;

                case GPCMD_SEEK:
                if (data->cmd_pos == CMD_POS_IDLE)
                {
                        uint32_t old_pos = data->cdpos;
                        int seek_time;
                        
                	data->cdpos = (cdb[3] << 16) | (cdb[4] << 8) | cdb[5];
                	
                	atapi->seek(data->cdpos);

                        seek_time = get_seek_time(data, old_pos, data->cdpos);
                        if (seek_time)
                        {
                                data->cmd_pos = CMD_POS_WAIT;
                                data->new_cmd_pos = CMD_POS_COMPLETE;
                                data->wait_time = seek_time;
                                timer_set_delay_u64(&data->callback_timer, 1000 * TIMER_USEC);
                                return SCSI_PHASE_COMMAND;
                        }
                }
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

static uint8_t scsi_cd_get_sense_key(void *p)
{
        scsi_cd_data_t *data = p;
        
        return data->sense_key;
}

static int scsi_cd_get_bytes_required(void *p)
{
        scsi_cd_data_t *data = p;
        
        return data->bytes_required - data->bytes_received;
}

static void scsi_cd_atapi_identify(uint16_t *buffer, void *p)
{
        scsi_cd_data_t *data = p;

        memset(buffer, 0, 512);
        
        buffer[0] = 0x8000 | (5<<8) | 0x80 | (2<<5); /* ATAPI device, CD-ROM drive, removable media, accelerated DRQ */
	ide_padstr((char *)(buffer + 10), cd_models[cd_data->cur_model].serial2_20, 20); /* Serial Number */
	ide_padstr((char *)(buffer + 23), cd_models[cd_data->cur_model].firmware2_8, 8); /* Firmware */
	ide_padstr((char *)(buffer + 27), cd_models[cd_data->cur_model].model2_40, 40); /* Model */
	buffer[49] = 0x300; /*DMA and LBA supported*/
	buffer[51] = 120;
	buffer[52] = 120;
	buffer[53] = 2; /*Words 64-70 are valid*/
	buffer[62] = 0x0000;
	buffer[63] = 0x0007 | (0x100 << data->mdma_mode); /*Multi-word DMA 0, 1 & 2*/
	buffer[64] = 0x0003;  /*PIO Modes 3 & 4*/
	if (data->pio_mode >= 3)
        	buffer[64] |= (0x100 << (data->pio_mode - 3));
	buffer[65] = 120; /*Minimum multi-word cycle time*/
	buffer[66] = 120; /*Recommended multi-word cycle time*/
	buffer[67] = 120; /*Minimum PIO cycle time*/
	buffer[126] = 0xfffe; /* Interpret zero byte count limit as maximum length */
}

static int scsi_cd_atapi_set_feature(uint8_t feature, uint8_t val, void *p)
{
        scsi_cd_data_t *data = p;
        
        switch (feature)
        {
                case FEATURE_SET_TRANSFER_MODE:
                if (!(val & 0xfe))
                {
                        /*Default transfer mode*/
                        data->pio_mode = 0;
                        return 1;
                }
                else if ((val & 0xf8) == 0x08)
                {
                        /*PIO transfer mode*/
                        if ((val & 7) > 4)
                                return 0;
                        data->pio_mode = (val & 7);
                        return 1;
                }                
                else if ((val & 0xf8) == 0x20)
                {
                        /*Multi-word DMA transfer mode*/
                        if ((val & 7) > 2)
                                return 0;
                        data->mdma_mode = (val & 7);
                        return 1;
                }
                return 0; /*Invalid data*/
                
                case FEATURE_ENABLE_IRQ_OVERLAPPED:
                case FEATURE_ENABLE_IRQ_SERVICE:
                case FEATURE_DISABLE_REVERT:
                case FEATURE_DISABLE_IRQ_OVERLAPPED:
                case FEATURE_DISABLE_IRQ_SERVICE:
                case FEATURE_ENABLE_REVERT:
                return 1;
        }
        
        return 0; /*Feature not supported*/
}

scsi_device_t scsi_cd =
{
        scsi_cd_init,
        scsi_cd_atapi_init,
        scsi_cd_close,
        scsi_cd_reset,
        
        scsi_cd_start_command,
        scsi_cd_command,
        
        scsi_cd_get_status,
        scsi_cd_get_sense_key,
        scsi_cd_get_bytes_required,
        
        scsi_cd_atapi_identify,
        scsi_cd_atapi_set_feature,
        
        scsi_cd_read,
        scsi_cd_write,
        scsi_cd_read_complete,
        scsi_cd_write_complete,
};
