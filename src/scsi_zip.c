#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "ibm.h"
#include "hdd_file.h"
#include "ide.h"
#include "ide_atapi.h"
#include "scsi.h"
#include "scsi_zip.h"
#include "timer.h"

#define BUFFER_SIZE (256*1024)

#define ZIP_SECTORS (96*2048)

#define RW_DELAY (TIMER_USEC * 500)

#define SCSI_IOMEGA_SENSE                 0x06
#define SCSI_IOMEGA_EJECT                 0x0d /*ATAPI only?*/

                /*Page 1 - 0x58 bytes*/
                /*Page 2 (Disk Status Page) - 0x3d bytes
                  Byte 21 bits 0-3 - protection code
                        0 - not write-protected
                        2 - write-protected
                        3 - password write-protected
                  Bytes 22 - 61 - serial number
                */
#define SCSI_IOMEGA_SET_PROTECTION_MODE   0x0c
                /*cmd[1] = mode
                  cmd[6-x] = password*/

#define ATAPI_READ_FORMAT_CAPACITIES      0x23

enum
{
        CMD_POS_IDLE = 0,
        CMD_POS_WAIT,
        CMD_POS_START_SECTOR,
        CMD_POS_TRANSFER
};

typedef struct scsi_zip_data
{
        int blocks;
        
        int cmd_pos, new_cmd_pos;
        
        int addr, len;
        int sector_pos;
        
        uint8_t buf[512];
        
        uint8_t status;
        
        uint8_t data_in[BUFFER_SIZE];
        uint8_t data_out[BUFFER_SIZE];
        int data_pos_read, data_pos_write;
        
        int bytes_received, bytes_required;

        hdd_file_t hdd;
        int disc_loaded;
        int disc_changed;
        
        int sense_key, asc, ascq;
        
        int hd_id;
        
        pc_timer_t callback_timer;
        
        scsi_bus_t *bus;
        
        int is_atapi;
        atapi_device_t *atapi_dev;
        
        int read_only;

        int pio_mode, mdma_mode;
} scsi_zip_data;

static scsi_zip_data *zip_data;

#define CHECK_READY		2

static uint8_t scsi_zip_cmd_flags[0x100] =
{
	[SCSI_TEST_UNIT_READY]               = CHECK_READY,
	[SCSI_REQUEST_SENSE]                 = 0,
	[SCSI_READ_6]                        = CHECK_READY,
	[SCSI_INQUIRY]                       = 0,
	[SCSI_MODE_SELECT_6]                 = 0,
	[SCSI_MODE_SENSE_6]                  = 0,
	[SCSI_START_STOP_UNIT]               = 0,
	[SCSI_PREVENT_ALLOW_MEDIUM_REMOVAL]  = CHECK_READY,
	[SCSI_READ_10]                       = CHECK_READY,
	[SCSI_SEEK_6]                        = CHECK_READY,
	[SCSI_SEEK_10]                       = CHECK_READY,
        [SCSI_IOMEGA_SENSE]                  = 0,
        [SCSI_REZERO_UNIT]                   = CHECK_READY,
        [SCSI_READ_CAPACITY_10]              = CHECK_READY,
        [SCSI_WRITE_6]                       = CHECK_READY,
        [SCSI_WRITE_10]                      = CHECK_READY,
        [SCSI_WRITE_AND_VERIFY]              = CHECK_READY,
        [SCSI_VERIFY_10]                     = CHECK_READY,
        [SCSI_FORMAT]                        = CHECK_READY,
        [SCSI_RESERVE]                       = 0,
        [SCSI_RELEASE]                       = 0,
        [SEND_DIAGNOSTIC]                    = 0

};

void zip_load(char *fn)
{
        if (zip_data)
        {
                FILE *f;
                int read_only = 0;
                
                f = fopen(fn, "rb+");
                if (!f)
                {
                        f = fopen(fn, "rb");
                        read_only = 1;
                }
                if (f)
                {
                        int size;

                        fseek(f, -1, SEEK_END);
                        size = ftell(f)+1;
                        fclose(f);
                        
                        if (size != ZIP_SECTORS*512)
                        {
                                warning("File is incorrect size for Zip image\nMust be exactly %i bytes\n", ZIP_SECTORS*512);
                                return;
                        }
                
                        if (zip_data->disc_loaded)
                                hdd_close(&zip_data->hdd);

                        hdd_load_ext(&zip_data->hdd, fn, 2048,1,96, read_only);
        
                        if (zip_data->hdd.f)
                        {
                                zip_data->disc_loaded = 1;
                                zip_data->disc_changed = 1;
                                zip_data->read_only = read_only;
                        }
                }
        }
}

void zip_eject()
{
        if (zip_data)
        {
                if (zip_data->disc_loaded)
                {
                        hdd_close(&zip_data->hdd);
                        zip_data->disc_loaded = 0;
                }
        }
}

int zip_loaded()
{
        if (zip_data)
                return zip_data->disc_loaded;
        return 0;
}

static void scsi_zip_callback(void *p)
{
        scsi_zip_data *data = p;

        if (data->cmd_pos == CMD_POS_WAIT)
        {
                data->cmd_pos = data->new_cmd_pos;
                scsi_bus_kick(data->bus);
        }
}

static void *scsi_zip_init(scsi_bus_t *bus, int id)
{
        scsi_zip_data *data = malloc(sizeof(scsi_zip_data));
        memset(data, 0, sizeof(scsi_zip_data));

        data->disc_loaded = 0;
        
        data->hd_id = id;
        data->bus = bus;
        timer_add(&data->callback_timer, scsi_zip_callback, data, 0);

        zip_data = data;        
        return data;
}

static void *scsi_zip_atapi_init(scsi_bus_t *bus, int id, atapi_device_t *atapi_dev)
{
        scsi_zip_data *data = scsi_zip_init(bus, id);
        
        data->is_atapi = 1;
        data->atapi_dev = atapi_dev;
        
        return data;
}

static void scsi_zip_close(void *p)
{
        scsi_zip_data *data = p;
        
        if (data->disc_loaded)
                hdd_close(&data->hdd);
        free(data);
        zip_data = NULL;
}

static void scsi_zip_reset(void *p)
{
        scsi_zip_data *data = p;

        timer_disable(&data->callback_timer);
        data->cmd_pos = CMD_POS_IDLE;
}

static int scsi_add_data(uint8_t val, void *p)
{
        scsi_zip_data *data = p;

//        pclog("scsi_add_data : %04x %02x\n", data->data_pos_write, val);
        
        data->data_in[data->data_pos_write++] = val;
        
//        if (data->bus_out & BUS_REQ)
//                return -1;
                
//        data->bus_out = (data->bus_out & ~BUS_DATAMASK) | BUS_SETDATA(val) | BUS_DBP | BUS_REQ;
        
        return 0;
}

static int scsi_get_data(void *p)
{
        scsi_zip_data *data = p;
        uint8_t val = data->data_out[data->data_pos_read++];
        
        if (data->data_pos_read > BUFFER_SIZE)
                fatal("scsi_get_data beyond buffer limits\n");

        return val;
}

static void scsi_zip_illegal(scsi_zip_data *data)
{
        data->status = STATUS_CHECK_CONDITION;
        data->sense_key = KEY_ILLEGAL_REQ;
        data->asc = ASC_INVALID_LUN;
        data->ascq = 0;
}

static void scsi_zip_cmd_error(scsi_zip_data *data, int sensekey, int asc, int ascq)
{
        data->status = STATUS_CHECK_CONDITION;
        data->sense_key = sensekey;
        data->asc = asc;
        data->ascq = ascq;
}

#define add_data_len(v)                         \
        do                                      \
        {                                       \
                if (i < len)                    \
                        scsi_add_data(v, data); \
                i++;                            \
        } while (0)


static int scsi_zip_command(uint8_t *cdb, void *p)
{
        scsi_zip_data *data = p;
        int /*addr, */len;
        int i = 0, c;
        int desc;
        int bus_state = 0;

        if (data->cmd_pos == CMD_POS_IDLE)
                pclog("SCSI ZIP command %02x %i %p\n", cdb[0], data->disc_loaded, data);
        data->status = STATUS_GOOD;
        data->data_pos_read = data->data_pos_write = 0;

        if ((cdb[0] != SCSI_REQUEST_SENSE/* && cdb[0] != SCSI_INQUIRY*/) && (cdb[1] & 0xe0))
        {
                pclog("non-zero LUN\n");
                /*Non-zero LUN - abort command*/
                scsi_zip_illegal(data);
                bus_state = BUS_CD | BUS_IO;
                return bus_state;
        }

	if ((scsi_zip_cmd_flags[cdb[0]] & CHECK_READY) && !data->disc_loaded)
	{
                pclog("Disc not loaded\n");
		scsi_zip_cmd_error(data, KEY_NOT_READY, ASC_MEDIUM_NOT_PRESENT, 0);
		bus_state = BUS_CD | BUS_IO;
		return bus_state;
	}
	if ((scsi_zip_cmd_flags[cdb[0]] & CHECK_READY) && data->disc_changed)
	{
                pclog("Disc changed\n");
		scsi_zip_cmd_error(data, KEY_UNIT_ATTENTION, ASC_MEDIUM_MAY_HAVE_CHANGED, 0);
		data->disc_changed = 0;
		bus_state = BUS_CD | BUS_IO;
		return bus_state;
	}
        
        switch (cdb[0])
        {
                case SCSI_TEST_UNIT_READY:
                bus_state = BUS_CD | BUS_IO;
                break;

                case SCSI_IOMEGA_SENSE:
                len = cdb[4];
                if (cdb[2] == 1)
                {
                        pclog("SCSI_IOMEGA_SENSE 1 %02x\n", len);
                        add_data_len(0x58);
                        add_data_len(0);
                        /*This page is related to disc health status - setting this page to 0
                          makes disc health read as 'marginal'*/
                        for (c = 2; c < 0x58; c++)
                                add_data_len(0xff);
                        while (i < len)
                                add_data_len(0);
                        data->cmd_pos = CMD_POS_IDLE;
                        bus_state = BUS_IO;
                }
                else if (cdb[2] == 2)
                {
                        pclog("SCSI_IOMEGA_SENSE 2 %02x\n", len);
                        add_data_len(0x3d);
                        add_data_len(0);
                        for (c = 0; c < 19; c++)
                                add_data_len(0);
                        if (data->read_only)
                                add_data_len(2);
                        else
                                add_data_len(0);
                        for (c = 0; c < 39; c++)
                                add_data_len(0);
                        while (i < len)
                                add_data_len(0);
                        data->cmd_pos = CMD_POS_IDLE;
                        bus_state = BUS_IO;
                }
                else
                {
                        pclog("IOMEGA_SENSE %02x %02x\n", cdb[2], len);
                        scsi_zip_illegal(data);
                        bus_state = BUS_CD | BUS_IO;
                }
                break;

                case SCSI_REZERO_UNIT:
                bus_state = BUS_CD | BUS_IO;
                break;

                case SCSI_REQUEST_SENSE:
                desc = cdb[1] & 1;
                len = cdb[4];
                
                if (!desc)
                {
                        add_data_len(0x70);
                        add_data_len(0);
                        add_data_len(data->sense_key); /*Sense key*/
                        add_data_len(0); /*Information (4 bytes)*/
                        add_data_len(0);
                        add_data_len(0);
                        add_data_len(0);
                        add_data_len(0); /*Additional sense length*/
                        add_data_len(0); /*Command specific information (4 bytes)*/
                        add_data_len(0);
                        add_data_len(0);
                        add_data_len(0);
                        add_data_len(data->asc); /*ASC*/
                        add_data_len(data->ascq); /*ASCQ*/
                        add_data_len(0); /*FRU code*/
                        add_data_len(0); /*Sense key specific (3 bytes)*/
                        add_data_len(0);
                        add_data_len(0);
                }
                else
                {
                        add_data_len(0x72);
                        add_data_len(data->sense_key); /*Sense Key*/
                        add_data_len(data->asc); /*ASC*/
                        add_data_len(data->ascq); /*ASCQ*/
                        add_data_len(0);
                        add_data_len(0);
                        add_data_len(0);
                        add_data_len(0); /*additional sense length*/
                }

                data->sense_key = data->asc = data->ascq = 0;
                
                data->cmd_pos = CMD_POS_IDLE;
                bus_state = BUS_IO;
                break;
                
                case SCSI_INQUIRY:
                len = cdb[4] | (cdb[3] << 8);
//                        pclog("INQUIRY %d  %02x %02x %02x %02x %02x %02x  %i\n", len, cdb[0],cdb[1],cdb[2],cdb[3],cdb[4],cdb[5],  data->hd_id);

                if (cdb[1] & 0xe0)                        
                {
                        add_data_len(0 | (3 << 5)); /*No physical device on this LUN*/
                }
                else
                {
                        add_data_len(0 | (0 << 5)); /*Hard disc*/
                }
                add_data_len(0x80);         /*Removeable device*/
		if (data->is_atapi)
		{
        		add_data_len(0); /*Not ANSI compliant*/
        		add_data_len(0x21); /*ATAPI compliant*/
                }
		else
		{
        		add_data_len(2); /*SCSI-2 compliant*/
        		add_data_len(0x02);
                }
//                add_data_len(0);            /*No version*/
//                add_data_len(2);            /*Response data*/
                add_data_len(0);            /*Additional length*/
                add_data_len(0);
                add_data_len(0);
                add_data_len(2);
                
                add_data_len('I');
                add_data_len('O');
                add_data_len('M');
                add_data_len('E');
                add_data_len('G');
                add_data_len('A');
                add_data_len(' ');
                add_data_len(' ');
                
                add_data_len('Z');
                add_data_len('I');
                add_data_len('P');
                add_data_len(' ');
                add_data_len('1');
                add_data_len('0');
                add_data_len('0');
                add_data_len(' ');

                add_data_len(' ');
                add_data_len(' ');
                add_data_len(' ');
                add_data_len(' ');
                add_data_len(' ');
                add_data_len(' ');
                add_data_len(' ');
                add_data_len(' ');

                add_data_len('E'); /*Product revision level*/
                add_data_len('.');                
                add_data_len('0');
                add_data_len('8');                

                add_data_len(0); /*Drive serial number*/
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                

                add_data_len(0); /*Vendor unique*/
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                

                add_data_len(0);
                add_data_len(0);                

                add_data_len(0); /*Vendor descriptor*/
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                

                add_data_len(0); /*Reserved*/
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                

                data->cmd_pos = CMD_POS_IDLE;
                bus_state = BUS_IO;
                break;

                case SCSI_PREVENT_ALLOW_MEDIUM_REMOVAL:
                bus_state = BUS_CD | BUS_IO;
                break;

                case SCSI_MODE_SENSE_6:
                case SCSI_MODE_SENSE_10:
                pclog("MODE_SENSE_6 %02x %02x\n", cdb[2], cdb[4]);
                if ((cdb[2] & 0x3f) != 0x01 && (cdb[2] & 0x3f) != 0x02 && (cdb[2] & 0x3f) != 0x2f && (cdb[2] & 0x3f) != 0x3f)
                {
                        pclog("Invalid MODE_SENSE\n");
                        scsi_zip_cmd_error(data, KEY_ILLEGAL_REQ, ASC_INV_FIELD_IN_CMD_PACKET, 0);
                        bus_state = BUS_CD | BUS_IO;
                        break;
                }
                if (cdb[0] == SCSI_MODE_SENSE_6)
                {
                        len = cdb[4];

                        add_data_len(0);
                        add_data_len(0);

                        if (data->read_only)
                                add_data_len(0x80);
                        else
                                add_data_len(0);
                        add_data_len(0x08);
                }
                else
                {
                        len = cdb[8] | (cdb[7] << 8);

                        add_data_len(0);
                        add_data_len(0);
                        add_data_len(0);

                        if (data->read_only)
                                add_data_len(0x80);
                        else
                                add_data_len(0);

                        add_data_len(0);
                        add_data_len(0);

                        add_data_len(0);
                        add_data_len(0x08);
                }

                add_data_len((ZIP_SECTORS >> 24) & 0xff);
                add_data_len((ZIP_SECTORS >> 16) & 0xff);
                add_data_len((ZIP_SECTORS >> 8)  & 0xff);
                add_data_len( ZIP_SECTORS        & 0xff);
                add_data_len((512    >> 24) & 0xff);
                add_data_len((512    >> 16) & 0xff);
                add_data_len((512    >> 8)  & 0xff);
                add_data_len( 512           & 0xff);
                
                if ((cdb[2] & 0x3f) == 0x01 || (cdb[2] & 0x3f) == 0x3f)
                {
                        add_data_len(0x01);
                        add_data_len(0x0a);
                        add_data_len(0xc8); /*Automatic read and write allocation enable, most expedient error recovery*/
                        add_data_len(22); /*Read retry count = 22*/
                        add_data_len(0);
                        add_data_len(0);
                        add_data_len(0);
                        add_data_len(0);
                        add_data_len(90); /*Write retry count = 90*/
                        add_data_len(0);
                        add_data_len(20512 >> 8);
                        add_data_len(20512 & 0xff);
                }

                if ((cdb[2] & 0x3f) == 0x02 || (cdb[2] & 0x3f) == 0x3f)
                {
                        add_data_len(0x02);
                        add_data_len(0x0e);
                        add_data_len(0); 
                        add_data_len(0);
                        add_data_len(0);
                        add_data_len(0);
                        add_data_len(0);
                        add_data_len(0);
                        add_data_len(0);
                        add_data_len(0);
                        add_data_len(0);
                        add_data_len(0);
                        add_data_len(0);
                        add_data_len(0);
                        add_data_len(0);
                        add_data_len(0);
                }                

                if ((cdb[2] & 0x3f) == 0x2f || (cdb[2] & 0x3f) == 0x3f)
                {
                        add_data_len(0x2f);
                        add_data_len(0x04);
                        add_data_len(0x5c);
                        add_data_len(0x0f);
                        add_data_len(0xff);
                        add_data_len(0x0f);
                }
                
                while (i < len)
                        add_data_len(0);

                len = data->data_pos_write;
                if (cdb[0] == SCSI_MODE_SENSE_6)
                {
			data->data_in[0] = len - 1;
                }
                else
                {
			data->data_in[0] = (len - 2) >> 8;
			data->data_in[1] = (len - 2) & 255;
                }

//                scsi_illegal_field();
                data->cmd_pos = CMD_POS_IDLE;
                bus_state = BUS_IO;
                break;

                case SCSI_READ_CAPACITY_10:
//                len = 96 * 2048;
                scsi_add_data((ZIP_SECTORS >> 24) & 0xff, data);
                scsi_add_data((ZIP_SECTORS >> 16) & 0xff, data);
                scsi_add_data((ZIP_SECTORS >> 8)  & 0xff, data);
                scsi_add_data( ZIP_SECTORS        & 0xff, data);
                scsi_add_data((512    >> 24) & 0xff, data);
                scsi_add_data((512    >> 16) & 0xff, data);
                scsi_add_data((512    >> 8)  & 0xff, data);
                scsi_add_data( 512           & 0xff, data);
                data->cmd_pos = CMD_POS_IDLE;
                bus_state = BUS_IO;
                break;

                case SCSI_READ_6:
//                pclog("SCSI_READ_6 %i\n", data->cmd_pos);
                if (data->cmd_pos == CMD_POS_WAIT)
                {
                        bus_state = BUS_CD;
                        break;
                }

                if (data->cmd_pos == CMD_POS_IDLE)
                {
                        readflash_set(READFLASH_HDC, data->hd_id);
                        data->addr = cdb[3] | (cdb[2] << 8) | ((cdb[1] & 0x1f) << 16);
                        data->len = cdb[4];
                        if (!data->len)
                                data->len = 256;
//                        pclog("SCSI_READ_6: addr=%08x len=%04x\n", data->addr, data->len);
                        
                        data->cmd_pos = CMD_POS_WAIT;
                        timer_set_delay_u64(&data->callback_timer, RW_DELAY);
                        data->new_cmd_pos = CMD_POS_START_SECTOR;
                        data->sector_pos = 0;
                        
                        bus_state = BUS_CD;
                        atapi_set_transfer_granularity(data->atapi_dev, 512);
                        break;
                }
//                else
//                        pclog("SCSI_READ_6 continue addr=%08x len=%04x sector_pos=%02x\n", data->addr, data->len, data->sector_pos);
                while (data->len)
                {
                        if (data->cmd_pos == CMD_POS_START_SECTOR)
                        {
                                hdd_read_sectors(&data->hdd, data->addr, 1, data->buf);
                                readflash_set(READFLASH_HDC, data->hd_id);
                        }

                        data->cmd_pos = CMD_POS_TRANSFER;
                        for (; data->sector_pos < 512; data->sector_pos++)
                        {
                                int ret = scsi_add_data(data->buf[data->sector_pos], data);
                                
                                if (ret == -1)
                                {
                                        fatal("scsi_add_data -1\n");
                                        break;
                                }
                                if (ret & 0x100)
                                {
                                        fatal("scsi_add_data 0x100\n");
                                        data->len = 1;
                                        break;
                                }
                        }
                        data->cmd_pos = CMD_POS_START_SECTOR;

                        data->sector_pos = 0;
                        data->len--;
                        data->addr++;
                }
                data->cmd_pos = CMD_POS_IDLE;
                bus_state = BUS_IO;
                break;

                case SCSI_READ_10:
//                pclog("SCSI_READ_10 %i\n", data->cmd_pos);
                if (data->cmd_pos == CMD_POS_WAIT)
                {
                        bus_state = BUS_CD;
                        break;
                }

                if (data->cmd_pos == CMD_POS_IDLE)
                {
                        readflash_set(READFLASH_HDC, data->hd_id);
                        data->addr = cdb[5] | (cdb[4] << 8) | (cdb[3] << 16) | (cdb[2] << 24);
                        data->len = cdb[8] | (cdb[7] << 8);
//                        pclog("SCSI_READ_10: addr=%08x len=%04x\n", data->addr, data->len);
                        
                        data->cmd_pos = CMD_POS_WAIT;
                        timer_set_delay_u64(&data->callback_timer, RW_DELAY);
                        data->new_cmd_pos = CMD_POS_START_SECTOR;
                        data->sector_pos = 0;
                        
                        bus_state = BUS_CD;
                        atapi_set_transfer_granularity(data->atapi_dev, 512);
                        break;
                }
//                else
//                        pclog("SCSI_READ_10 continue addr=%08x len=%04x sector_pos=%02x\n", data->addr, data->len, data->sector_pos);
                while (data->len)
                {
                        if (data->cmd_pos == CMD_POS_START_SECTOR)
                        {
                                hdd_read_sectors(&data->hdd, data->addr, 1, data->buf);
                                readflash_set(READFLASH_HDC, data->hd_id);
                        }
                        data->cmd_pos = CMD_POS_TRANSFER;
                        for (; data->sector_pos < 512; data->sector_pos++)
                        {
                                int ret = scsi_add_data(data->buf[data->sector_pos], data);
//                                pclog("SCSI_READ_10 sector_pos=%i\n", data->sector_pos);                                
                                if (ret == -1)
                                {
                                        fatal("scsi_add_data -1\n");
                                        break;
                                }
                                if (ret & 0x100)
                                {
                                        fatal("scsi_add_data 0x100\n");
                                        data->len = 1;
                                        break;
                                }
                        }
                        data->cmd_pos = CMD_POS_START_SECTOR;

                        data->sector_pos = 0;
                        data->len--;
                        data->addr++;
                }
                data->cmd_pos = CMD_POS_IDLE;
                bus_state = BUS_IO;
                break;

                case SCSI_WRITE_6:
                if (data->read_only)
                {
                        pclog("zip read only\n");
                        scsi_zip_cmd_error(data, KEY_DATA_PROTECT, ASC_WRITE_PROTECT, 0);
                        bus_state = BUS_CD | BUS_IO;
                        break;
                }
//                pclog("SCSI_WRITE_6 %i\n", data->cmd_pos);
                if (data->cmd_pos == CMD_POS_WAIT)
                {
                        bus_state = BUS_CD;
                        break;
                }
                if (data->cmd_pos == CMD_POS_IDLE)
                {
                        data->addr = cdb[3] | (cdb[2] << 8) | ((cdb[1] & 0x1f) << 16);
                        data->len = cdb[4];
                        if (!data->len)
                                data->len = 256;
                        readflash_set(READFLASH_HDC, data->hd_id);
//                        pclog("SCSI_WRITE_6: addr=%08x len=%04x\n", data->addr, data->len);
                        data->bytes_required = data->len * 512;
                        
                        data->cmd_pos = CMD_POS_WAIT;
                        timer_set_delay_u64(&data->callback_timer, RW_DELAY);
                        data->new_cmd_pos = CMD_POS_TRANSFER;
                        data->sector_pos = 0;
                        
                        bus_state = BUS_CD;
                        atapi_set_transfer_granularity(data->atapi_dev, 512);
                        break;
                }
                if (data->cmd_pos == CMD_POS_TRANSFER && data->bytes_received != data->bytes_required)
                {
                        bus_state = 0;
                        break;
                }

                while (data->len)
                {
                        for (; data->sector_pos < 512; data->sector_pos++)
                        {
                                int ret = scsi_get_data(data);
                                if (ret == -1)
                                {
                                        fatal("scsi_get_data -1\n");
                                        break;
                                }
                                data->buf[data->sector_pos] = ret & 0xff;
                                if (ret & 0x100)
                                {
                                        fatal("scsi_get_data 0x100\n");
                                        data->len = 1;
                                        break;
                                }
                        }

                        hdd_write_sectors(&data->hdd, data->addr, 1, data->buf);
                        readflash_set(READFLASH_HDC, data->hd_id);
                                
                        data->sector_pos = 0;
                        data->len--;
                        data->addr++;
                }
                data->cmd_pos = CMD_POS_IDLE;
                bus_state = BUS_CD | BUS_IO;
                break;

                case SCSI_WRITE_10:
                case SCSI_WRITE_AND_VERIFY:
                if (data->read_only)
                {
                        pclog("zip read only\n");
                        scsi_zip_cmd_error(data, KEY_DATA_PROTECT, ASC_WRITE_PROTECT, 0);
                        bus_state = BUS_CD | BUS_IO;
                        break;
                }
//                pclog("SCSI_WRITE_10 %i\n", data->cmd_pos);
                if (data->cmd_pos == CMD_POS_WAIT)
                {
                        bus_state = BUS_CD;
                        break;
                }
                if (data->cmd_pos == CMD_POS_IDLE)
                {
                        data->addr = cdb[5] | (cdb[4] << 8) | (cdb[3] << 16) | (cdb[2] << 24);
                        data->len = cdb[8] | (cdb[7] << 8);
                        readflash_set(READFLASH_HDC, data->hd_id);
//                        pclog("SCSI_WRITE_10: addr=%08x len=%04x\n", data->addr, data->len);
                        data->bytes_required = data->len * 512;
                        
                        data->cmd_pos = CMD_POS_WAIT;
                        timer_set_delay_u64(&data->callback_timer, RW_DELAY);
                        data->new_cmd_pos = CMD_POS_TRANSFER;
                        data->sector_pos = 0;
                        
                        bus_state = BUS_CD;
                        atapi_set_transfer_granularity(data->atapi_dev, 512);
                        break;
                }
                if (data->cmd_pos == CMD_POS_TRANSFER && data->bytes_received != data->bytes_required)
                {
                        bus_state = 0;
                        break;
                }
                
                while (data->len)
                {
                        for (; data->sector_pos < 512; data->sector_pos++)
                        {
                                int ret = scsi_get_data(data);
//                                pclog("SCSI_WRITE_10 sector_pos=%i\n", data->sector_pos);
                                if (ret == -1)
                                {
                                        fatal("scsi_get_data -1\n");
                                        break;
                                }
                                data->buf[data->sector_pos] = ret & 0xff;
                                if (ret & 0x100)
                                {
                                        fatal("scsi_get_data 0x100\n");
                                        data->len = 1;
                                        break;
                                }
                        }

                        hdd_write_sectors(&data->hdd, data->addr, 1, data->buf);
                        readflash_set(READFLASH_HDC, data->hd_id);

                        data->sector_pos = 0;
                        data->len--;
                        data->addr++;
                }
                data->cmd_pos = CMD_POS_IDLE;
                bus_state = BUS_CD | BUS_IO;
//                output = 3;
                break;

                case SCSI_VERIFY_10:
                data->cmd_pos = CMD_POS_IDLE;
                bus_state = BUS_CD | BUS_IO;
                break;

                case SCSI_MODE_SELECT_6:
                if (!data->bytes_received)
                {
                        data->bytes_required = cdb[4];
//                        pclog("SCSI_MODE_SELECT_6 bytes_required=%i\n", data->bytes_required);
                        bus_state = 0;
                        break;
                }
//                pclog("SCSI_MODE_SELECT_6 complete\n");
                data->cmd_pos = CMD_POS_IDLE;
                bus_state = BUS_CD | BUS_IO;
                break;

                case SCSI_FORMAT:
                if (data->read_only)
                {
                        scsi_zip_cmd_error(data, KEY_DATA_PROTECT, ASC_WRITE_PROTECT, 0);
                        bus_state = BUS_CD | BUS_IO;
                        break;
                }
                readflash_set(READFLASH_HDC, 0);
                hdd_format_sectors(&data->hdd, 0, ZIP_SECTORS);

                data->cmd_pos = CMD_POS_IDLE;
                bus_state = BUS_CD | BUS_IO;
                break;

                case SCSI_START_STOP_UNIT:
                /*if ((cdb[4] & (START_STOP_LOEJ | START_STOP_START)) == START_STOP_LOEJ)*/
                /*This is incorrect based on the SCSI spec, but it's what the Iomega Win9x drivers send to eject a disc*/
                if ((cdb[4] & (START_STOP_LOEJ | START_STOP_START)) == 0)
                        zip_eject();
                bus_state = BUS_CD | BUS_IO;
                break;

                /*These aren't really meaningful in a single initiator system, so just return*/
                case SCSI_RESERVE:
                case SCSI_RELEASE:
                bus_state = BUS_CD | BUS_IO;
                break;

                case SCSI_SEEK_6:
                case SCSI_SEEK_10:
                bus_state = BUS_CD | BUS_IO;
                break;
                                        
                case SEND_DIAGNOSTIC:
                if (cdb[1] & (1 << 2))
                {
                        /*Self-test*/
                        /*Always passes*/
                        bus_state = BUS_CD | BUS_IO;
                }
                else
                {
                        /*Treat all other diagnostic requests as illegal*/
                        scsi_zip_illegal(data);
                        bus_state = BUS_CD | BUS_IO;
                }
                break;
                
                case SCSI_IOMEGA_EJECT:
                if (data->is_atapi)
                        zip_eject();
                else
                        scsi_zip_illegal(data);
                
                bus_state = BUS_CD | BUS_IO;
                break;
                
                case ATAPI_READ_FORMAT_CAPACITIES:
                if (data->is_atapi)
                {
                        len = cdb[8] | (cdb[7] << 8);
                        
                        /*List header*/
                        scsi_add_data(0, data);
                        scsi_add_data(0, data);
                        scsi_add_data(0, data);
                        scsi_add_data(16, data); /*list length*/
                        
                        /*Current/Maximum capacity header*/
                        scsi_add_data((ZIP_SECTORS >> 24) & 0xff, data);
                        scsi_add_data((ZIP_SECTORS >> 16) & 0xff, data);
                        scsi_add_data((ZIP_SECTORS >> 8)  & 0xff, data);
                        scsi_add_data( ZIP_SECTORS        & 0xff, data);
                        if (data->disc_loaded)
                                scsi_add_data(2, data); /*Formatted media - current media capacity*/
                        else
                                scsi_add_data(3, data); /*Maximum formattable capacity*/
                        scsi_add_data(512 >> 16, data);
                        scsi_add_data(512 >> 8, data);
                        scsi_add_data(512 & 0xff, data);

                        /*Formattable capacity descriptor*/
                        scsi_add_data((ZIP_SECTORS >> 24) & 0xff, data);
                        scsi_add_data((ZIP_SECTORS >> 16) & 0xff, data);
                        scsi_add_data((ZIP_SECTORS >> 8)  & 0xff, data);
                        scsi_add_data( ZIP_SECTORS        & 0xff, data);
                        scsi_add_data(0, data);
                        scsi_add_data(512 >> 16, data);
                        scsi_add_data(512 >> 8, data);
                        scsi_add_data(512 & 0xff, data);

                        data->cmd_pos = CMD_POS_IDLE;
                        bus_state = BUS_IO;
                }
                else
                {
                        scsi_zip_illegal(data);                
                        bus_state = BUS_CD | BUS_IO;
                }
                break;

                default:
                pclog("Bad SCSI ZIP command %02x\n", cdb[0]);
                scsi_zip_illegal(data);
                bus_state = BUS_CD | BUS_IO;
                break;
        }
        
        return bus_state;
}

static uint8_t scsi_zip_read(void *p)
{
        scsi_zip_data *data = p;

        return data->data_in[data->data_pos_read++];
}

static void scsi_zip_write(uint8_t val, void *p)
{
        scsi_zip_data *data = p;

        data->data_out[data->data_pos_write++] = val;
        
        data->bytes_received++;

        if (data->data_pos_write > BUFFER_SIZE)
                fatal("Exceeded data_out buffer size\n");
}

static int scsi_zip_read_complete(void *p)
{
        scsi_zip_data *data = p;

        return (data->data_pos_read == data->data_pos_write);
}
static int scsi_zip_write_complete(void *p)
{
        scsi_zip_data *data = p;

        return (data->bytes_received == data->bytes_required);
}

static void scsi_zip_start_command(void *p)
{
        scsi_zip_data *data = p;

        data->bytes_received = 0;
        data->bytes_required = 0;
        data->data_pos_read = data->data_pos_write = 0;
}

static uint8_t scsi_zip_get_status(void *p)
{
        scsi_zip_data *data = p;
        
        return data->status;
}

static uint8_t scsi_zip_get_sense_key(void *p)
{
        scsi_zip_data *data = p;
        
        return data->sense_key;
}

static int scsi_zip_get_bytes_required(void *p)
{
        scsi_zip_data *data = p;
        
        return data->bytes_required - data->bytes_received;
}

static void scsi_zip_atapi_identify(uint16_t *buffer, void *p)
{
        scsi_zip_data *data = p;
        
        memset(buffer, 0, 512);
        
        buffer[0] = 0x8000 | (0<<8) | 0x80 | (2<<5); /* ATAPI device, direct-access device, removable media, accelerated DRQ */
	ide_padstr((char *)(buffer + 10), "", 20); /* Serial Number */
	ide_padstr((char *)(buffer + 23), "E.08", 8); /* Firmware */
	ide_padstr((char *)(buffer + 27), "IOMEGA ZIP 100 ATAPI", 40); /* Model */
	buffer[49] = 0x300; /*DMA and LBA supported*/
	buffer[51] = 120;
	buffer[52] = 120;
	buffer[53] = 2; /*Words 64-70 are valid*/
	buffer[62] = 0x0000;
	buffer[63] = 0x0003 | (0x100 << data->mdma_mode); /*Multi-word DMA 0 & 1*/
	buffer[64] = 0x0001;  /*PIO Mode 3*/
	if (data->pio_mode >= 3)
        	buffer[64] |= (0x100 << (data->pio_mode - 3));
	buffer[65] = 120; /*Minimum multi-word cycle time*/
	buffer[66] = 120; /*Recommended multi-word cycle time*/
	buffer[67] = 120; /*Minimum PIO cycle time*/
	buffer[126] = 0xfffe; /* Interpret zero byte count limit as maximum length */
}

static int scsi_zip_atapi_set_feature(uint8_t feature, uint8_t val, void *p)
{
        scsi_zip_data *data = p;
        
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
                        if ((val & 7) > 3)
                                return 0;
                        data->pio_mode = (val & 7);
                        return 1;
                }                
                else if ((val & 0xf8) == 0x20)
                {
                        /*Multi-word DMA transfer mode*/
                        if ((val & 7) > 1)
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

scsi_device_t scsi_zip =
{
        scsi_zip_init,
        scsi_zip_atapi_init,
        scsi_zip_close,
        scsi_zip_reset,
        
        scsi_zip_start_command,
        scsi_zip_command,
        
        scsi_zip_get_status,
        scsi_zip_get_sense_key,
        scsi_zip_get_bytes_required,
        
        scsi_zip_atapi_identify,
        scsi_zip_atapi_set_feature,
        
        scsi_zip_read,
        scsi_zip_write,
        scsi_zip_read_complete,
        scsi_zip_write_complete,
};
