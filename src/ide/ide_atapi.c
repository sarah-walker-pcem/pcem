#include "ibm.h"
#include "ide.h"
#include "ide_atapi.h"
#include "scsi.h"
#include "timer.h"

#define IDE_TIME (100 * TIMER_USEC)

#define ATAPI_STATE_IDLE             0
#define ATAPI_STATE_COMMAND          1
#define ATAPI_STATE_GOT_COMMAND      2
#define ATAPI_STATE_NEXT_PHASE       3
#define ATAPI_STATE_READ_STATUS      4
#define ATAPI_STATE_READ_MESSAGE     5
#define ATAPI_STATE_END_PHASE        6
#define ATAPI_STATE_READ_DATA        7
#define ATAPI_STATE_READ_DATA_WAIT   8
#define ATAPI_STATE_WRITE_DATA       9
#define ATAPI_STATE_WRITE_DATA_WAIT 10
#define ATAPI_STATE_RETRY_READ_DMA  11
#define ATAPI_STATE_RETRY_WRITE_DMA 12

#define FEATURES_DMA     0x01
#define FEATURES_OVERLAP 0x02

ATAPI *atapi;

void atapi_data_write(atapi_device_t *atapi_dev, uint16_t val)
{
        switch (atapi_dev->state)
        {
                case ATAPI_STATE_COMMAND:
                atapi_dev->command[atapi_dev->command_pos++] = val & 0xff;
                atapi_dev->command[atapi_dev->command_pos++] = val >> 8;
                if (atapi_dev->command_pos >= 12)
                {
                        atapi_dev->state = ATAPI_STATE_GOT_COMMAND;
                        timer_set_delay_u64(&ide_timer[atapi_dev->board], 6 * IDE_TIME);
                        *atapi_dev->atastat = BUSY_STAT | (*atapi_dev->atastat & ERR_STAT);
                }
                break;

                case ATAPI_STATE_WRITE_DATA_WAIT:
                atapi_dev->data[atapi_dev->data_write_pos++] = val & 0xff;
                atapi_dev->data[atapi_dev->data_write_pos++] = val >> 8;
//                pclog("Write ATAPI data %i %i %i\n", atapi_dev->data_write_pos, atapi_dev->data_read_pos, idecallback[atapi_dev->board]);
                if (atapi_dev->data_write_pos >= atapi_dev->data_read_pos)
                {
                        atapi_dev->bus_state = 0;
                        atapi_dev->state = ATAPI_STATE_WRITE_DATA;
                        timer_set_delay_u64(&ide_timer[atapi_dev->board], 6 * IDE_TIME);
                        *atapi_dev->atastat = BUSY_STAT | (*atapi_dev->atastat & ERR_STAT);
                }
                break;
        }
}

uint16_t atapi_data_read(atapi_device_t *atapi_dev)
{
        uint16_t temp = 0xffff;
        
        switch (atapi_dev->state)
        {
                case ATAPI_STATE_READ_DATA_WAIT:
                if (atapi_dev->data_read_pos >= atapi_dev->data_write_pos)
                {
//                        pclog("ATAPI_STATE_READ_DATA_WAIT - out of data!\n");
                        break;
                }
                        
                temp = atapi_dev->data[atapi_dev->data_read_pos++];
                if (atapi_dev->data_read_pos < atapi_dev->data_write_pos)
                        temp |= (atapi_dev->data[atapi_dev->data_read_pos++] << 8);
//                pclog("Read data %04x\n", temp);
                if (atapi_dev->data_read_pos >= atapi_dev->data_write_pos)
                {
                        atapi_dev->state = ATAPI_STATE_NEXT_PHASE;
                        timer_set_delay_u64(&ide_timer[atapi_dev->board], 6*IDE_TIME);
                        *atapi_dev->atastat = BUSY_STAT | (*atapi_dev->atastat & ERR_STAT);
                }
                break;
        }

        return temp;
}

static int wait_for_bus(scsi_bus_t *bus, int state, int req_needed)
{
        int c;
        
        for (c = 0; c < 20; c++)
        {
                int bus_state = scsi_bus_read(bus);
                
                if ((bus_state & (BUS_IO | BUS_CD | BUS_MSG)) == state && (bus_state & BUS_BSY))
                {
                        if (!req_needed || (bus_state & BUS_REQ))
                                return 1;
                }
        }
        
        return 0;
}

void atapi_command_start(atapi_device_t *atapi, uint8_t features)
{
        scsi_bus_t *bus = &atapi->bus;
        
        atapi->use_dma = features & 1;
       
        scsi_bus_update(bus, BUS_SEL | BUS_SETDATA(1 << 0));
        if (!(scsi_bus_read(bus) & BUS_BSY))
                fatal("STATE_SCSI_SELECT failed to select target\n");

        scsi_bus_update(bus, 0);
        if (!(scsi_bus_read(bus) & BUS_BSY))
                fatal("STATE_SCSI_SELECT failed to select target 2\n");
                
        /*Device should now be selected*/
        if (!wait_for_bus(bus, BUS_CD, 1))
                fatal("Device failed to request command\n");
        
        atapi->state = ATAPI_STATE_COMMAND;
        atapi->command_pos = 0;
}

uint8_t atapi_read_iir(atapi_device_t *atapi_dev)
{
        uint8_t val = 0;
        int bus_status = atapi_dev->bus_state;//scsi_bus_read(&atapi_dev->bus);
        
        if (bus_status & BUS_CD)
                val |= 1;
        if (bus_status & BUS_IO)
                val |= 2;
        
//        pclog("Read IIR %02X %02x\n", val, bus_status);
        
        return val;
}

uint8_t atapi_read_drq(atapi_device_t *atapi_dev)
{
        return atapi_dev->bus_state & BUS_REQ;
}

void atapi_set_transfer_granularity(atapi_device_t *atapi_dev, int size)
{
        if (atapi_dev->max_transfer_len > size)
                atapi_dev->max_transfer_len -= (atapi_dev->max_transfer_len % size);
}

void atapi_process_packet(atapi_device_t *atapi_dev)
{
//        pclog("atapi_process_packet: state=%i\n", atapi_dev->state);
        switch (atapi_dev->state)
        {
                case ATAPI_STATE_COMMAND:
                *atapi_dev->atastat = READY_STAT | (*atapi_dev->atastat & ERR_STAT);
                atapi_dev->max_transfer_len = (*atapi_dev->cylinder) & ~1;
                if (!atapi_dev->max_transfer_len)
                        atapi_dev->max_transfer_len = 0xfffe;
                atapi_dev->bus_state = BUS_CD | BUS_REQ;
                break;

                case ATAPI_STATE_GOT_COMMAND:
                {
                        int pos = 0;
                        int bus_state;
                        int c;
                        
                        for (c = 0; c < 20; c++)
                        {
                                bus_state = scsi_bus_read(&atapi_dev->bus);
                                
                                if (!(bus_state & BUS_BSY))
                                        fatal("SEND_COMMAND - dropped BSY\n");
                                if ((bus_state & (BUS_IO | BUS_CD | BUS_MSG)) != BUS_CD)
                                        fatal("SEND_COMMAND - bus phase incorrect\n");
                                if (bus_state & BUS_REQ)
                                        break;
                        }
                        if (c == 20)
                                fatal("SEND_COMMAND timed out\n");

                        while (1)
                        {
                                int bus_out;
                                
                                for (c = 0; c < 20; c++)
                                {
                                        int bus_state = scsi_bus_read(&atapi_dev->bus);
                                
                                        if (!(bus_state & BUS_BSY))
                                                fatal("SEND_COMMAND - dropped BSY\n");
                                        if ((bus_state & (BUS_IO | BUS_CD | BUS_MSG)) != BUS_CD)
                                                break;
                                        if (bus_state & BUS_REQ)
                                                break;
                                }
                                if (c == 20)
                                {
//                                        pclog("SEND_COMMAND timed out %x\n", scsi_bus_read(&atapi_dev->bus));
                                        atapi_dev->state = ATAPI_STATE_NEXT_PHASE;
                                        timer_set_delay_u64(&ide_timer[atapi_dev->board], 6 * IDE_TIME);
                                        break;
                                }

                                bus_state = scsi_bus_read(&atapi_dev->bus);

                                if ((bus_state & (BUS_IO | BUS_CD | BUS_MSG | BUS_REQ)) != (BUS_CD | BUS_REQ))
                                {
//                                        pclog("SEND_COMMAND - bus state changed %x\n", bus_state);
                                        atapi_dev->state = ATAPI_STATE_NEXT_PHASE;
                                        timer_set_delay_u64(&ide_timer[atapi_dev->board], 6 * IDE_TIME);
                                        break;
                                }

                                if (pos >= atapi_dev->command_pos)
                                        bus_out = BUS_SETDATA(0); /*Pad out with zeroes*/
                                else
                                        bus_out = BUS_SETDATA(atapi_dev->command[pos++]);
                                scsi_bus_update(&atapi_dev->bus, bus_out | BUS_ACK);
                                scsi_bus_update(&atapi_dev->bus, bus_out & ~BUS_ACK);
                        
                                if (!(bus_state & BUS_BSY))
                                        fatal("SEND_COMMAND - dropped BSY\n");

//                                if (pos >= atapi_dev->command_pos)
//                                        fatal("Out of data!\n");
                        }
                }
                break;

                case ATAPI_STATE_NEXT_PHASE:
                {
                        int c;
                                
                        for (c = 0; c < 20; c++)
                        {
                                int bus_state = scsi_bus_read(&atapi_dev->bus);

//                                if (!c)
//                                        pclog("NEXT_PHASE - %x\n", bus_state);
                                if (!(bus_state & BUS_BSY))
                                        fatal("NEXT_PHASE - dropped BSY waiting\n");

                                if (bus_state & BUS_REQ)
                                {
                                        scsi_device_t *scsi_dev = atapi_dev->bus.devices[0];
                                        void *scsi_data = atapi_dev->bus.device_data[0];

                                        switch (bus_state & (BUS_IO | BUS_CD | BUS_MSG))
                                        {
                                                case 0:
                                                atapi_dev->data_read_pos = scsi_dev->get_bytes_required(scsi_data);
                                                                
                                                atapi_dev->data_write_pos = 0;

                                                if (atapi_dev->use_dma)
                                                {
                                                        if (ide_bus_master_write_data)
                                                        {
                                                                if (ide_bus_master_write_data(atapi_dev->board, atapi_dev->data, atapi_dev->data_read_pos, ide_bus_master_p))
                                                                {
                                                                        atapi_dev->state = ATAPI_STATE_RETRY_WRITE_DMA;
                                                                        timer_set_delay_u64(&ide_timer[atapi_dev->board], 1*IDE_TIME);
                                                                }
                                                                else
                                                                {
                                                                        atapi_dev->data_write_pos = atapi_dev->data_read_pos;
                                                                        atapi_dev->bus_state = 0;
                                                                        atapi_dev->state = ATAPI_STATE_WRITE_DATA;
                                                                        timer_set_delay_u64(&ide_timer[atapi_dev->board], 6 * IDE_TIME);
                                                                }
                                                        }
                                                        else
                                                        {
                                                                atapi_dev->state = ATAPI_STATE_RETRY_WRITE_DMA;
                                                                timer_set_delay_u64(&ide_timer[atapi_dev->board], 1*IDE_TIME);
                                                        }
                                                }
                                                else
                                                {
                                                        if (atapi_dev->data_read_pos > atapi_dev->max_transfer_len)
                                                                atapi_dev->data_read_pos = atapi_dev->max_transfer_len;

                                                        atapi_dev->state = ATAPI_STATE_WRITE_DATA_WAIT;
                                                        
                                                        *atapi_dev->cylinder = atapi_dev->data_read_pos;
                                                        
//                                                        pclog("WRITE_DATA: bytes=%i\n", *atapi_dev->cylinder);
                                                        *atapi_dev->atastat = READY_STAT | DRQ_STAT | (*atapi_dev->atastat & ERR_STAT);
                                                        atapi_dev->bus_state = BUS_REQ;
                                                        ide_irq_raise(atapi_dev->ide);
                                                }
                                                break;

                                                case BUS_IO:
                                                atapi_dev->state = ATAPI_STATE_READ_DATA;
                                                atapi_dev->data_read_pos = atapi_dev->data_write_pos = 0;
                                                break;

                                                case (BUS_IO | BUS_CD):
                                                atapi_dev->state = ATAPI_STATE_READ_STATUS;
                                                break;

                                                case (BUS_CD | BUS_IO | BUS_MSG):
                                                atapi_dev->state = ATAPI_STATE_READ_MESSAGE;
                                                break;

                                                default:
                                                fatal("NEXT_PHASE - bus state changed %x\n", bus_state);
                                        }
                                        break;
                                }
                        }
                        timer_set_delay_u64(&ide_timer[atapi_dev->board], 6 * IDE_TIME);
                }
                break;
                                                
                case ATAPI_STATE_READ_STATUS:
                {
                        int c;

                        for (c = 0; c < 20; c++)
                        {
                                int bus_state = scsi_bus_read(&atapi_dev->bus);

                                if (!(bus_state & BUS_BSY))
                                        fatal("READ_STATUS - dropped BSY waiting\n");

                                if ((bus_state & (BUS_IO | BUS_CD | BUS_MSG)) != (BUS_CD | BUS_IO))
                                        fatal("READ_STATUS - changed phase\n");
                                
                                if (bus_state & BUS_REQ)
                                {
//                                        uint8_t status = BUS_GETDATA(bus_state);
                                        int bus_out = 0;

//                                        pclog("Read status %02x\n", status);
                                
                                        scsi_bus_update(&atapi_dev->bus, bus_out | BUS_ACK);
                                        scsi_bus_update(&atapi_dev->bus, bus_out & ~BUS_ACK);
                                      
                                        atapi_dev->state = ATAPI_STATE_NEXT_PHASE;
                                        break;
                                }
                        }
                        timer_set_delay_u64(&ide_timer[atapi_dev->board], 6 * IDE_TIME);
                }
                break;

                case ATAPI_STATE_READ_MESSAGE:
                {
                        int c;

                        for (c = 0; c < 20; c++)
                        {
                                int bus_state = scsi_bus_read(&atapi_dev->bus);

                                if (!(bus_state & BUS_BSY))
                                        fatal("READ_MESSAGE - dropped BSY waiting\n");

                                if ((bus_state & (BUS_IO | BUS_CD | BUS_MSG)) != (BUS_CD | BUS_IO | BUS_MSG))
                                {
//                                        pclog("READ_MESSAGE - changed phase\n");
                                        atapi_dev->state = ATAPI_STATE_NEXT_PHASE;
                                        break;
                                }
                                
                                if (bus_state & BUS_REQ)
                                {
                                        uint8_t msg = BUS_GETDATA(bus_state);
                                        int bus_out = 0;
                                
//                                        pclog("Read message %02x\n", msg);
                                        scsi_bus_update(&atapi_dev->bus, bus_out | BUS_ACK);
                                        scsi_bus_update(&atapi_dev->bus, bus_out & ~BUS_ACK);
                                
                                        switch (msg)
                                        {
                                                case MSG_COMMAND_COMPLETE:
                                                atapi_dev->state = ATAPI_STATE_END_PHASE;
                                                break;
                                        
                                                default:
                                                fatal("READ_MESSAGE - unknown message %02x\n", msg);
                                        }
                                        break;
                                }
                        }
                        timer_set_delay_u64(&ide_timer[atapi_dev->board], 6 * IDE_TIME);
                }
                break;

                case ATAPI_STATE_READ_DATA:
                {
                        while (atapi_dev->data_write_pos < atapi_dev->max_transfer_len)
                        {
                                int c;
                        
                                for (c = 0; c < 20; c++)
                                {
                                        int bus_state = scsi_bus_read(&atapi_dev->bus);

                                        if (!(bus_state & BUS_BSY))
                                                fatal("READ_DATA - dropped BSY waiting\n");

                                        if ((bus_state & (BUS_IO | BUS_CD | BUS_MSG)) != BUS_IO)
                                        {
//                                                pclog("READ_DATA - changed phase\n");
                                                break;
                                        }
                                
                                        if (bus_state & BUS_REQ)
                                        {
                                                uint8_t data = BUS_GETDATA(bus_state);
                                                int bus_out = 0;
                                                        
                                                atapi_dev->data[atapi_dev->data_write_pos++] = data;
                                
//                                                pclog("Read data %02x %i\n", data, atapi_dev->data_write_pos);
                                                scsi_bus_update(&atapi_dev->bus, bus_out | BUS_ACK);
                                                scsi_bus_update(&atapi_dev->bus, bus_out & ~BUS_ACK);
                                                break;
                                        }
                                }                       
                                if ((scsi_bus_read(&atapi_dev->bus) & (BUS_IO | BUS_CD | BUS_MSG)) != BUS_IO)
                                        break;
                        }
                        if (atapi_dev->use_dma)
                        {
                                if (ide_bus_master_read_data)
                                {
                                        if (ide_bus_master_read_data(atapi_dev->board, atapi_dev->data, atapi_dev->data_write_pos, ide_bus_master_p))
                                        {
                                                atapi_dev->state = ATAPI_STATE_RETRY_READ_DMA;
                                                timer_set_delay_u64(&ide_timer[atapi_dev->board], 1*IDE_TIME);
                                        }
                                        else
                                        {
                                                atapi_dev->state = ATAPI_STATE_NEXT_PHASE;
                                                timer_set_delay_u64(&ide_timer[atapi_dev->board], 1*IDE_TIME);
                                        }
                                }
                                else
                                {
                                        atapi_dev->state = ATAPI_STATE_RETRY_READ_DMA;
                                        timer_set_delay_u64(&ide_timer[atapi_dev->board], 1*IDE_TIME);
                                }
                        }
                        else
                        {
                                atapi_dev->state = ATAPI_STATE_READ_DATA_WAIT;
                                *atapi_dev->atastat = READY_STAT | DRQ_STAT | (*atapi_dev->atastat & ERR_STAT);
                                *atapi_dev->cylinder = atapi_dev->data_write_pos;
//                                pclog("READ_DATA: bytes=%i\n", *atapi_dev->cylinder);
                                atapi_dev->bus_state = BUS_IO | BUS_REQ;
                                ide_irq_raise(atapi_dev->ide);
                        }
                }
                break;

                case ATAPI_STATE_WRITE_DATA:
                {
                        atapi_dev->data_read_pos = 0;
//                        pclog("ATAPI_STATE_WRITE_DATA - %i\n", atapi_dev->data_write_pos);
                        while (atapi_dev->data_read_pos < atapi_dev->data_write_pos)
                        {
                                int bus_state = scsi_bus_read(&atapi_dev->bus);

                                if (!(bus_state & BUS_BSY))
                                        fatal("WRITE_DATA - dropped BSY waiting\n");

                                if ((bus_state & (BUS_IO | BUS_CD | BUS_MSG)) != 0)
                                {
//                                        pclog("WRITE_DATA - changed phase\n");
                                        atapi_dev->state = ATAPI_STATE_NEXT_PHASE;
                                        break;
                                }
                                
                                if (bus_state & BUS_REQ)
                                {
                                        uint8_t data = atapi_dev->data[atapi_dev->data_read_pos++];
                                        int bus_out;
                                                                        
//                                        pclog("Write data %02x %i\n", data, atapi_dev->data_read_pos-1);
                                        bus_out = BUS_SETDATA(data);
                                        scsi_bus_update(&atapi_dev->bus, bus_out | BUS_ACK);
                                        scsi_bus_update(&atapi_dev->bus, bus_out & ~BUS_ACK);
                                }
                        }
                                
                        atapi_dev->state = ATAPI_STATE_NEXT_PHASE;
                        timer_set_delay_u64(&ide_timer[atapi_dev->board], 1 * IDE_TIME);
                }
                break;
                        
                case ATAPI_STATE_END_PHASE:
                {
                        int c;
                        /*Wait for SCSI command to move to next phase*/
                        for (c = 0; c < 20; c++)
                        {
                                int bus_state = scsi_bus_read(&atapi_dev->bus);

                                if (!(bus_state & BUS_BSY))
                                {
//                                        pclog("END_PHASE - dropped BSY waiting\n");
                                        atapi_dev->state = ATAPI_STATE_IDLE;
                                        break;
                                }

                                if (bus_state & BUS_REQ)
                                        fatal("END_PHASE - unexpected REQ\n");
                        }
                        if (atapi_dev->state == ATAPI_STATE_IDLE)
                        {
                                scsi_device_t *scsi_dev = atapi_dev->bus.devices[0];
                                void *scsi_data = atapi_dev->bus.device_data[0];
                                        
                                *atapi_dev->atastat = READY_STAT;
                                atapi_dev->bus_state = BUS_IO | BUS_CD;
                                if (scsi_dev->get_status(scsi_data) != STATUS_GOOD)
                                {
                                        *atapi_dev->error = (scsi_dev->get_sense_key(scsi_data) << 4) | ABRT_ERR;
                                        if (scsi_dev->get_sense_key(scsi_data) == KEY_UNIT_ATTENTION)
                                                *atapi_dev->error |= MCR_ERR;
                                        *atapi_dev->atastat |= ERR_STAT;
                                }
                                ide_irq_raise(atapi_dev->ide);
                        }
                        else
                                timer_set_delay_u64(&ide_timer[atapi_dev->board], 6 * IDE_TIME);
                }
                break;
                
                case ATAPI_STATE_RETRY_READ_DMA:
                {
                        if (ide_bus_master_read_data(atapi_dev->board, atapi_dev->data, atapi_dev->data_write_pos, ide_bus_master_p))
                        {
                                timer_set_delay_u64(&ide_timer[atapi_dev->board], 1*IDE_TIME);
                        }
                        else
                        {
                                atapi_dev->state = ATAPI_STATE_NEXT_PHASE;
                                timer_set_delay_u64(&ide_timer[atapi_dev->board], 6*IDE_TIME);
                        }
                }
                break;

                case ATAPI_STATE_RETRY_WRITE_DMA:
                {
                        if (ide_bus_master_write_data(atapi_dev->board, atapi_dev->data, atapi_dev->data_read_pos, ide_bus_master_p))
                        {
                                timer_set_delay_u64(&ide_timer[atapi_dev->board], 1*IDE_TIME);
                        }
                        else
                        {
                                atapi_dev->bus_state = 0;
                                atapi_dev->state = ATAPI_STATE_WRITE_DATA;
                                timer_set_delay_u64(&ide_timer[atapi_dev->board], 6 * IDE_TIME);
                        }
                }
                break;
        }
}

void atapi_reset(atapi_device_t *atapi_dev)
{
        pclog("atapi_reset\n");
        atapi_dev->state = ATAPI_STATE_IDLE;
        atapi_dev->command_pos = 0;
        atapi_dev->data_read_pos = 0;
        atapi_dev->data_write_pos = 0;
        scsi_bus_reset(&atapi_dev->bus);
}
