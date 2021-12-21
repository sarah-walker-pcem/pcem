#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "ibm.h"
#include "hdd_file.h"
#include "scsi.h"
#include "scsi_hd.h"
#include "timer.h"

extern char ide_fn[7][512];

#define BUFFER_SIZE (256*1024)

#define RW_DELAY (TIMER_USEC * 500)

enum
{
        CMD_POS_IDLE = 0,
        CMD_POS_WAIT,
        CMD_POS_START_SECTOR,
        CMD_POS_TRANSFER
};

typedef struct scsi_hd_data
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
        
        int sense_key, asc, ascq;
        
        int hd_id;
        
        pc_timer_t callback_timer;
        
        scsi_bus_t *bus;
} scsi_hd_data;

static void scsi_hd_callback(void *p)
{
        scsi_hd_data *data = p;

        if (data->cmd_pos == CMD_POS_WAIT)
        {
                data->cmd_pos = data->new_cmd_pos;
                scsi_bus_kick(data->bus);
        }
}

static void *scsi_hd_init(scsi_bus_t *bus, int id)
{
        scsi_hd_data *data = malloc(sizeof(scsi_hd_data));
        memset(data, 0, sizeof(scsi_hd_data));

        hdd_load(&data->hdd, id, ide_fn[id]);
        
        if (!data->hdd.f)
        {
                free(data);
                return NULL;
        }
        
        data->hd_id = id;
        data->bus = bus;
        timer_add(&data->callback_timer, scsi_hd_callback, data, 0);
        
        return data;
}

static void scsi_hd_close(void *p)
{
        scsi_hd_data *data = p;
        
        hdd_close(&data->hdd);
        free(data);
}

static int scsi_add_data(uint8_t val, void *p)
{
        scsi_hd_data *data = p;

//        pclog("scsi_add_data : %04x %02x\n", data->data_pos_write, val);
        
        data->data_in[data->data_pos_write++] = val;
        
//        if (data->bus_out & BUS_REQ)
//                return -1;
                
//        data->bus_out = (data->bus_out & ~BUS_DATAMASK) | BUS_SETDATA(val) | BUS_DBP | BUS_REQ;
        
        return 0;
}

static int scsi_get_data(void *p)
{
        scsi_hd_data *data = p;
        uint8_t val = data->data_out[data->data_pos_read++];
        
        if (data->data_pos_read > BUFFER_SIZE)
                fatal("scsi_get_data beyond buffer limits\n");

        return val;
}

static void scsi_hd_illegal(scsi_hd_data *data)
{
        data->status = STATUS_CHECK_CONDITION;
        data->sense_key = KEY_ILLEGAL_REQ;
        data->asc = ASC_INVALID_LUN;
        data->ascq = 0;
}

#define add_data_len(v)                 \
        if (i < len)                    \
                scsi_add_data(v, data); \
        i++;


static int scsi_hd_command(uint8_t *cdb, void *p)
{
        scsi_hd_data *data = p;
        int /*addr, */len;
        int i = 0;
        int desc;
        int bus_state = 0;

//        if (data->cmd_pos == CMD_POS_IDLE)
//                pclog("SCSI HD command %02x\n", cdb[0]);
        data->status = STATUS_GOOD;
        data->data_pos_read = data->data_pos_write = 0;

        if ((cdb[0] != SCSI_REQUEST_SENSE/* && cdb[0] != SCSI_INQUIRY*/) && (cdb[1] & 0xe0))
        {
//                pclog("non-zero LUN\n");
                /*Non-zero LUN - abort command*/
                scsi_hd_illegal(data);
                bus_state = BUS_CD | BUS_IO;
                return bus_state;
        }
        
        switch (cdb[0])
        {
                case SCSI_TEST_UNIT_READY:
                bus_state = BUS_CD | BUS_IO;
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
                add_data_len(0);            /*Not removeable*/
                add_data_len(0);            /*No version*/
                add_data_len(2);            /*Response data*/
                add_data_len(0);            /*Additional length*/
                add_data_len(0);
                add_data_len(0);
                add_data_len(2);
                
                add_data_len('P');
                add_data_len('C');
                add_data_len('e');
                add_data_len('m');
                add_data_len(' ');
                add_data_len(' ');
                add_data_len(' ');
                add_data_len(' ');
                
                add_data_len('S');
                add_data_len('C');
                add_data_len('S');
                add_data_len('I');
                add_data_len('_');
                add_data_len('H');
                add_data_len('D');
                add_data_len(' ');

                add_data_len(' ');
                add_data_len(' ');
                add_data_len(' ');
                add_data_len(' ');
                add_data_len(' ');
                add_data_len(' ');
                add_data_len(' ');
                add_data_len(' ');

                add_data_len(0); /*Product revision level*/
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                

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
                len = cdb[4];

                add_data_len(0);
                add_data_len(0);

                add_data_len(0x08);
                add_data_len(0);

                add_data_len((data->hdd.sectors >> 24) & 0xff);
                add_data_len((data->hdd.sectors >> 16) & 0xff);
                add_data_len((data->hdd.sectors >> 8)  & 0xff);
                add_data_len( data->hdd.sectors        & 0xff);
                add_data_len((512    >> 24) & 0xff);
                add_data_len((512    >> 16) & 0xff);
                add_data_len((512    >> 8)  & 0xff);
                add_data_len( 512           & 0xff);
                
                if ((cdb[2] & 0x3f) == 0x03 || (cdb[2] & 0x3f) == 0x3f)
                {
                        add_data_len(3);
                        add_data_len(0x16);
                        add_data_len(0); add_data_len(1); /*Tracks per zone*/
                        add_data_len(0); add_data_len(1); /*Alternate sectors per zone*/
                        add_data_len(0); add_data_len(1); /*Alternate tracks per zone*/
                        add_data_len(0); add_data_len(1); /*Alternate tracks per volume*/
                        add_data_len(1); add_data_len(0); /*Sectors per track*/
                        add_data_len(2); add_data_len(0); /*Data bytes per physical sector*/
                        add_data_len(0); add_data_len(0); /*Interleave*/
                        add_data_len(0); add_data_len(0); /*Track skew*/
                        add_data_len(0); add_data_len(0); /*Cylinder skew*/
                        add_data_len(0);                  /*Drive type*/
                        add_data_len(0); add_data_len(0); add_data_len(0);
                }

                if ((cdb[2] & 0x3f) == 0x04 || (cdb[2] & 0x3f) == 0x3f)
                {
                        add_data_len(4);
                        add_data_len(0x16);
                        add_data_len(0); add_data_len(0x10); add_data_len(0); /*Cylinder count*/
                        add_data_len(64); /*Heads*/
                        add_data_len(0); add_data_len(0); add_data_len(0); /*Write recomp*/
                        add_data_len(0); add_data_len(0); add_data_len(0); /*Reduced write current*/
                        add_data_len(0); add_data_len(0); /*Drive step rate*/
                        add_data_len(0); add_data_len(0); add_data_len(0); /*Landing zone cylinder*/
                        add_data_len(0); /*RPL*/
                        add_data_len(0); /*Rotational offset*/
                        add_data_len(0);
                        add_data_len(0x10); add_data_len(0); /*Rotain rate*/
                        add_data_len(0); add_data_len(0);
                }                

                if ((cdb[2] & 0x3f) == 0x30 || (cdb[2] & 0x3f) == 0x3f)
                {
                        add_data_len(0xb0);
                        add_data_len(0x16);
                        add_data_len('P');
                        add_data_len('C');
                        add_data_len('E');
                        add_data_len('M');
                        add_data_len(' ');
                        add_data_len(' ');
                        add_data_len(' ');
                        add_data_len(' ');
                        add_data_len(' ');
                        add_data_len(' ');
                        add_data_len(' ');
                        add_data_len(' ');
                        add_data_len(' ');
                        add_data_len(' ');
                        add_data_len(' ');
                        add_data_len(' ');
                        add_data_len(' ');
                        add_data_len(' ');
                        add_data_len(' ');
                        add_data_len(' ');
                        add_data_len(' ');
                        add_data_len(' ');
                }       
                
                for (; len >= 0; len--)
                {
                        add_data_len(0);
                }
                
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
                scsi_add_data((data->hdd.sectors >> 24) & 0xff, data);
                scsi_add_data((data->hdd.sectors >> 16) & 0xff, data);
                scsi_add_data((data->hdd.sectors >> 8)  & 0xff, data);
                scsi_add_data( data->hdd.sectors        & 0xff, data);
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
                bus_state = BUS_CD | BUS_IO;
                break;
                
                case SCSI_START_STOP_UNIT:
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
                        scsi_hd_illegal(data);
                        bus_state = BUS_CD | BUS_IO;
                }
                break;
                                
                default:
                pclog("Bad SCSI HD command %02x\n", cdb[0]);
                scsi_hd_illegal(data);
                bus_state = BUS_CD | BUS_IO;
                break;
        }
        
        return bus_state;
}

static uint8_t scsi_hd_read(void *p)
{
        scsi_hd_data *data = p;

        return data->data_in[data->data_pos_read++];
}

static void scsi_hd_write(uint8_t val, void *p)
{
        scsi_hd_data *data = p;

        data->data_out[data->data_pos_write++] = val;
        
        data->bytes_received++;

        if (data->data_pos_write > BUFFER_SIZE)
                fatal("Exceeded data_out buffer size\n");
}

static int scsi_hd_read_complete(void *p)
{
        scsi_hd_data *data = p;

        return (data->data_pos_read == data->data_pos_write);
}
static int scsi_hd_write_complete(void *p)
{
        scsi_hd_data *data = p;

        return (data->bytes_received == data->bytes_required);
}

static void scsi_hd_start_command(void *p)
{
        scsi_hd_data *data = p;

        data->bytes_received = 0;
        data->bytes_required = 0;
        data->data_pos_read = data->data_pos_write = 0;
}

static uint8_t scsi_hd_get_status(void *p)
{
        scsi_hd_data *data = p;
        
        return data->status;
}

static uint8_t scsi_hd_get_sense_key(void *p)
{
        scsi_hd_data *data = p;
        
        return data->sense_key;
}

static int scsi_hd_get_bytes_required(void *p)
{
        scsi_hd_data *data = p;
        
        return data->bytes_required - data->bytes_received;
}

scsi_device_t scsi_hd =
{
        scsi_hd_init,
        NULL,
        scsi_hd_close,
        NULL,
        
        scsi_hd_start_command,
        scsi_hd_command,
        
        scsi_hd_get_status,
        scsi_hd_get_sense_key,
        scsi_hd_get_bytes_required,
        
        NULL,
        NULL,
        
        scsi_hd_read,
        scsi_hd_write,
        scsi_hd_read_complete,
        scsi_hd_write_complete,
};
