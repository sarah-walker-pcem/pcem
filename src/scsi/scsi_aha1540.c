/*AH1542C firmware notes :
        
  d1a - command dispatch
  193c - send data
  1954 - get data

  FW data - 164e
  
  BIOS command table - f4b*/
#include <stdlib.h>
#include "ibm.h"
#include "device.h"
#include "io.h"
#include "mem.h"
#include "nvr.h"
#include "pic.h"
#include "rom.h"
#include "scsi.h"
#include "scsi_aha1540.h"
#include "timer.h"

int primed = 0;
typedef enum
{
        SCSI_AHA1542C,
        SCSI_BT545S
} scsi_type_t;

/*Firmware emulation is implemented via three state machines :

  - Command state - foreground command processing
  - CCB state - CCB/mailbox processing - triggered by commands 0x02/0x82
  - SCSI state - SCSI bus handling / data transfer*/
typedef enum
{
        CMD_STATE_RESET,
        CMD_STATE_IDLE,
        CMD_STATE_GET_PARAMS,
        CMD_STATE_CMD_IN_PROGRESS,
        CMD_STATE_SEND_RESULT
} cmd_state_t;

typedef enum
{
        CCB_STATE_IDLE,
        CCB_STATE_SEND_COMMAND,
        CCB_STATE_WAIT_COMMAND,
        CCB_STATE_SEND_REQUEST_SENSE,
        CCB_STATE_WAIT_REQUEST_SENSE
} ccb_state_t;

typedef enum
{
        SCSI_STATE_IDLE,
        SCSI_STATE_SELECT,
        SCSI_STATE_SELECT_FAILED,
        SCSI_STATE_SEND_COMMAND,
        SCSI_STATE_NEXT_PHASE,
        SCSI_STATE_END_PHASE,
        SCSI_STATE_READ_DATA,
        SCSI_STATE_WRITE_DATA,
        SCSI_STATE_READ_STATUS,
        SCSI_STATE_READ_MESSAGE
} scsi_state_t;

typedef enum
{
        MB_FORMAT_4,
        MB_FORMAT_8
} mb_format_t;

typedef struct aha154x_t
{
        rom_t bios_rom;
        mem_mapping_t mapping;
        uint32_t bios_addr;
        
        uint8_t shadow_ram[0x4000];
        uint8_t shadow;
        
        uint32_t bios_bank;
        
        uint8_t eeprom[256];
        char fn[256];
        
        scsi_type_t type;
        
        scsi_bus_t bus;
        
        uint8_t isr;
        uint8_t isr_pending;
        uint8_t status;
        
        int mbo_irq_enable;
        
        uint8_t cmd_data;
        uint8_t data_in;
        
        uint8_t command;
        int command_len;
        int cur_param;
        uint8_t params[64];
        
        int result_pos, result_len;
        uint8_t result[256];
        
        uint8_t dma_buffer[64];
        
        uint8_t bon, boff;
        uint8_t atbs;
        
        int mbc;
        uint32_t mba;
        uint32_t mba_i;
        int mbo_req;
        mb_format_t mb_format;

        int bios_mbc;
        uint32_t bios_mba;
        int bios_mbo_req;
        int bios_mbo_inited;
        
        cmd_state_t cmd_state;
        ccb_state_t ccb_state;
        scsi_state_t scsi_state;

        int bios_cmd_state;
                
        pc_timer_t timer;

        int current_mbo;
        int current_mbo_is_bios;
        int current_mbi;
        
        uint8_t mblt, mbu;
        
        uint8_t int_buffer[512];
        
        uint8_t e_d;
        uint16_t to;
        
        uint8_t dipsw;
        int irq, dma, host_id;

        struct
        {
                uint32_t addr;
                
                uint8_t opcode;
                int target_id, transfer_dir, lun;
                int scsi_cmd_len;
                int req_sense_len;
                uint32_t data_len;
                uint32_t data_pointer;
                uint32_t link_pointer;
                uint8_t link_id;
                
                uint8_t cdb[256];
                                
                uint8_t status;
                
                int from_mailbox;
                
                uint32_t data_seg_list_pointer;
                uint32_t data_seg_list_len;
                uint32_t data_seg_list_idx;
                
                int residual;
                
                int bytes_transferred;
                uint32_t total_data_len;
        } ccb;

        struct
        {
                int idx, len;
                uint8_t data[256];
                
                int data_idx, data_len;
                uint32_t data_pointer;
                
                uint8_t last_status;

                uint32_t data_seg_list_pointer;
                uint32_t data_seg_list_len;
                uint32_t data_seg_list_idx;
                
                int scatter_gather;
                int bytes_transferred;
        } cdb;
        
        int reg3_idx;
} aha154x_t;

#define STATUS_INVDCMD 0x01
#define STATUS_DF      0x04
#define STATUS_CDF     0x08
#define STATUS_IDLE    0x10
#define STATUS_INIT    0x20
#define STATUS_STST    0x80

#define CTRL_BRST  0x10
#define CTRL_IRST  0x20
#define CTRL_SRST  0x40
#define CTRL_RESET 0x80

#define ISR_MBIF    0x01
#define ISR_MBOA    0x02
#define ISR_HACC    0x04
#define ISR_ANYINTR 0x80

#define COMMAND_NOP                       0x00
#define COMMAND_MAILBOX_INITIALIZATION    0x01
#define COMMAND_START_SCSI_COMMAND        0x02
#define COMMAND_START_BIOS_COMMAND        0x03
#define COMMAND_ADAPTER_INQUIRY           0x04
#define COMMAND_MAILBOX_OUT_IRQ_ENA       0x05
#define COMMAND_SET_SELECTION_TIMEOUT     0x06
#define COMMAND_SET_BUS_ON_TIME           0x07
#define COMMAND_SET_BUS_OFF_TIME          0x08
#define COMMAND_SET_AT_BUS_SPEED          0x09
#define COMMAND_INQUIRE_INSTALLED_DEVICES 0x0a
#define COMMAND_RETURN_CONFIG_DATA        0x0b
#define COMMAND_RETURN_SETUP_DATA         0x0d
#define COMMAND_WRITE_CHANNEL_2_BUF       0x1a
#define COMMAND_READ_CHANNEL_2_BUF        0x1b
#define COMMAND_ECHO                      0x1f

#define COMMAND_ADAPTEC_PROGRAM_EEPROM      0x22
#define COMMAND_ADAPTEC_RETURN_EEPROM_DATA  0x23
#define COMMAND_ADAPTEC_SET_SHADOW_PARAMS   0x24
#define COMMAND_ADAPTEC_BIOS_MAILBOX_INIT   0x25
#define COMMAND_ADAPTEC_SET_BIOS_BANK_1     0x26
#define COMMAND_ADAPTEC_SET_BIOS_BANK_2     0x27
#define COMMAND_ADAPTEC_GET_EXT_BIOS_INFO   0x28
#define COMMAND_ENABLE_MAILBOX_INTERFACE    0x29
#define COMMAND_ADAPTEC_START_BIOS_SCSI_CMD 0x82

#define COMMAND_BUSLOGIC_22                  0x22
#define COMMAND_BUSLOGIC_81                  0x81
#define COMMAND_BUSLOGIC_84                  0x84
#define COMMAND_BUSLOGIC_85                  0x85
#define COMMAND_BUSLOGIC_ADAPTER_MODEL_NR    0x8b
#define COMMAND_BUSLOGIC_INQUIRY_SYNC_PERIOD 0x8c
#define COMMAND_BUSLOGIC_EXTENDED_SETUP_INFO 0x8d
#define COMMAND_BUSLOGIC_STRICT_RR           0x8f
#define COMMAND_BUSLOGIC_SET_CCB_FORMAT      0x96

#define COMMAND_FREE_CCB  0
#define COMMAND_START_CCB 1
#define COMMAND_ABORT_CCB 2

#define CCB_INITIATOR                         0
#define CCB_INITIATOR_SCATTER_GATHER          2
#define CCB_INITIATOR_RESIDUAL                3
#define CCB_INITIATOR_SCATTER_GATHER_RESIDUAL 4

#define CCB_SENSE_14   0x00
#define CCB_SENSE_NONE 0x01

#define MBI_CCB_COMPLETE            0x01
#define MBI_CCB_ABORTED             0x02
#define MBI_CCB_COMPLETE_WITH_ERROR 0x04

#define HOST_STATUS_COMMAND_COMPLETE   0x00
#define HOST_STATUS_SELECTION_TIME_OUT 0x11

#define POLL_TIME_US 10
#define MAX_BYTES_TRANSFERRED_PER_POLL 50
/*10us poll period with 50 bytes transferred per poll = 5MB/sec*/

static void aha1542c_eeprom_save(aha154x_t *scsi);
static void process_cmd(aha154x_t *scsi);

static void set_irq(aha154x_t *scsi, uint8_t val)
{
        picint(1 << scsi->irq);
        scsi->isr |= val | ISR_ANYINTR;
}

static void clear_irq(aha154x_t *scsi)
{
        picintc(1 << scsi->irq);
}

static void aha154x_out(uint16_t port, uint8_t val, void *p)
{
        aha154x_t *scsi = (aha154x_t *)p;

        if (CS != 0xc800)
        {
//                pclog("aha154x_out: port=%04x val=%02x %04x:%04x %i\n", port, val, CS, cpu_state.pc, scsi->cmd_state);
        }

        switch (port & 3)
        {
                case 0: /*Control register*/
                if (val & CTRL_RESET)
                {
                        scsi->isr = 0;
                        clear_irq(scsi);
                        scsi->status = STATUS_STST;
                        pclog("hardware reset\n");
                        scsi->cmd_state = CMD_STATE_RESET;
                        scsi->ccb_state = CCB_STATE_IDLE;
                        scsi->scsi_state = SCSI_STATE_IDLE;
                        scsi->mb_format = MB_FORMAT_4;
                        scsi->current_mbi = 0;
                        scsi->mbo_irq_enable = 0;
                }
                if (val & CTRL_SRST)
                {
                        scsi->isr = 0;
                        clear_irq(scsi);
                        scsi->status = STATUS_INIT;
//                        pclog("software reset\n");
                        scsi->cmd_state = CMD_STATE_IDLE;
                        scsi->ccb_state = CCB_STATE_IDLE;
                        scsi->scsi_state = SCSI_STATE_IDLE;
                        scsi->mb_format = MB_FORMAT_4;
                        scsi->current_mbi = 0;
                        scsi->mbo_irq_enable = 0;
                }
                if (val & CTRL_IRST)
                {
                        scsi->isr = 0;
                        clear_irq(scsi);
//                        update_irq(scsi);
                        if (scsi->type == SCSI_AHA1542C)
                                scsi->status &= ~STATUS_INVDCMD;
                }
                if (val & CTRL_BRST)
                {
                        scsi->ccb_state = CCB_STATE_IDLE;
                        scsi->scsi_state = SCSI_STATE_IDLE;
//                        output = 3;
                }
                break;
                
                case 1: /*Command/data register*/
                if (scsi->status & STATUS_CDF)
                        break;
                if (scsi->type == SCSI_BT545S)
                        scsi->status &= ~STATUS_INVDCMD;
//                        fatal("Write command reg while full\n");
                scsi->status |= STATUS_CDF;
                scsi->cmd_data = val;
//                pclog("Write command %02x %04x(%08x):%08x\n", val, CS,cs,cpu_state.pc);
                process_cmd(scsi);
                break;
                
                case 2:
                if (scsi->type == SCSI_BT545S)
                        scsi->isr = val;
                break;
        }        
}

static uint8_t aha154x_in(uint16_t port, void *p)
{
        aha154x_t *scsi = (aha154x_t *)p;
        uint8_t temp = 0xff;
        
//        if (primed)
//                output = 3;
        switch (port & 3)
        {
                case 0: /*Status register*/
                temp = scsi->status;
                if (scsi->cmd_state == CMD_STATE_IDLE && scsi->ccb_state == CCB_STATE_IDLE && scsi->scsi_state == SCSI_STATE_IDLE)
                        temp |= STATUS_IDLE;
                break;
                
                case 1: /*Data in register*/
/*                if (!(scsi->status & STATUS_DF))
                        fatal("Read data in while empty\n");*/
                scsi->status &= ~STATUS_DF;
                temp = scsi->data_in;
                break;

                case 2: /*Interrupt status register*/
                if (scsi->type == SCSI_BT545S)
                        temp = scsi->isr;
                else
                        temp = scsi->isr & ~0x70;
                break;
                
                case 3: /*???*/
                if (scsi->type == SCSI_BT545S)
                        temp = 0x29; /*BT-545S BIOS expects this to return not zero or 0xff for six consecutive reads*/
                else
                {
                        scsi->reg3_idx++;
                        switch (scsi->reg3_idx & 3)
                        {
                                case 0: temp = 'A'; break;
                                case 1: temp = 'D'; break;
                                case 2: temp = 'A'; break;
                                case 3: temp = 'P'; break;
                        }
                }
                break;
        }
        if (CS != 0xc800)
        {
//                pclog("aha154x_in: port=%04x val=%02x %04x(%08x):%04x %i %i\n", port, temp, CS, cs, cpu_state.pc, ins, scsi->cmd_state);
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

static void process_cdb(aha154x_t *scsi)
{
        int c;
        uint8_t temp;
        
        scsi->ccb.opcode = mem_readb_phys(scsi->ccb.addr);
        switch (scsi->ccb.opcode)
        {
                case CCB_INITIATOR:
                case CCB_INITIATOR_RESIDUAL:
                temp = mem_readb_phys(scsi->ccb.addr + 0x01);
//                pclog("Read addr+ctrl %02x %06x\n", temp, scsi->ccb.addr+0x01);
                scsi->ccb.transfer_dir = (temp >> 3) & 3;
                scsi->ccb.scsi_cmd_len = mem_readb_phys(scsi->ccb.addr + 0x02);
                scsi->ccb.req_sense_len = mem_readb_phys(scsi->ccb.addr + 0x03);

                if (scsi->mb_format == MB_FORMAT_4)
                {
                        scsi->ccb.lun = temp & 7;
                        scsi->ccb.target_id = (temp >> 5) & 7;
                                                
                        scsi->ccb.data_len = mem_readb_phys(scsi->ccb.addr + 0x06) | (mem_readb_phys(scsi->ccb.addr + 0x05) << 8) | (mem_readb_phys(scsi->ccb.addr + 0x04) << 16);
                        scsi->ccb.data_pointer = mem_readb_phys(scsi->ccb.addr + 0x09) | (mem_readb_phys(scsi->ccb.addr + 0x08) << 8) | (mem_readb_phys(scsi->ccb.addr + 0x07) << 16);
                        scsi->ccb.link_pointer = mem_readb_phys(scsi->ccb.addr + 0x0c) | (mem_readb_phys(scsi->ccb.addr + 0x0b) << 8) | (mem_readb_phys(scsi->ccb.addr + 0x0a) << 16);
                        scsi->ccb.link_id = mem_readb_phys(scsi->ccb.addr + 0x0d);
                }
                else
                {
                        scsi->ccb.data_len = mem_readl_phys(scsi->ccb.addr + 0x04);
                        scsi->ccb.data_pointer = mem_readl_phys(scsi->ccb.addr + 0x08);

                        scsi->ccb.target_id = mem_readb_phys(scsi->ccb.addr + 0x10);
                        scsi->ccb.lun = mem_readb_phys(scsi->ccb.addr + 0x11) & 0x1f;
//                        pclog("Target ID = %02x  addr=%08x len=%08x\n", scsi->ccb.target_id, scsi->ccb.data_pointer, scsi->ccb.data_len);
                }
                for (c = 0; c < scsi->ccb.scsi_cmd_len; c++)
                        scsi->ccb.cdb[c] = mem_readb_phys(scsi->ccb.addr+0x12+c);
//                pclog("CCB_INITIATOR: target=%i lun=%i\n", scsi->ccb.target_id, scsi->ccb.lun);
//                pclog("  scsi_len=%i data_len=%06x data_pointer=%06x\n", scsi->ccb.scsi_cmd_len, scsi->ccb.data_len, scsi->ccb.data_pointer);
                scsi->ccb_state = CCB_STATE_SEND_COMMAND;
                scsi->ccb.status = 0;
                scsi->ccb.from_mailbox = 1;
                scsi->ccb.residual = (scsi->ccb.opcode == CCB_INITIATOR_RESIDUAL);
                scsi->ccb.total_data_len = scsi->ccb.data_len;
                break;
                                        
                case CCB_INITIATOR_SCATTER_GATHER:
                case CCB_INITIATOR_SCATTER_GATHER_RESIDUAL:
//                pclog("Scatter-gather : %08x %08x %08x %08x %08x  %i  %02x\n", mem_readl_phys(scsi->ccb.addr),mem_readl_phys(scsi->ccb.addr+4),mem_readl_phys(scsi->ccb.addr+8),mem_readl_phys(scsi->ccb.addr+12),mem_readl_phys(scsi->ccb.addr+16), scsi->mb_format == MB_FORMAT_4,  scsi->ccb.opcode);
                temp = mem_readb_phys(scsi->ccb.addr + 0x01);
//                pclog("Read addr+ctrl %02x %06x\n", temp, scsi->ccb.addr+0x01);
                scsi->ccb.transfer_dir = (temp >> 3) & 3;
                scsi->ccb.scsi_cmd_len = mem_readb_phys(scsi->ccb.addr + 0x02);
                scsi->ccb.req_sense_len = mem_readb_phys(scsi->ccb.addr + 0x03);
                                        
                if (scsi->mb_format == MB_FORMAT_4)
                {
                        scsi->ccb.lun = temp & 7;
                        scsi->ccb.target_id = (temp >> 5) & 7;
                                                
                        scsi->ccb.data_seg_list_len = mem_readb_phys(scsi->ccb.addr + 0x06) | (mem_readb_phys(scsi->ccb.addr + 0x05) << 8) | (mem_readb_phys(scsi->ccb.addr + 0x04) << 16);
                        scsi->ccb.data_seg_list_pointer = mem_readb_phys(scsi->ccb.addr + 0x09) | (mem_readb_phys(scsi->ccb.addr + 0x08) << 8) | (mem_readb_phys(scsi->ccb.addr + 0x07) << 16);
                        scsi->ccb.data_seg_list_idx = 0;
                        if (scsi->ccb.data_seg_list_len < 6)
                                pclog("Scatter gather table < 6 bytes\n");
                        
                        scsi->ccb.link_pointer = mem_readb_phys(scsi->ccb.addr + 0x0c) | (mem_readb_phys(scsi->ccb.addr + 0x0b) << 8) | (mem_readb_phys(scsi->ccb.addr + 0x0a) << 16);
                        scsi->ccb.link_id = mem_readb_phys(scsi->ccb.addr + 0x0d);
                }
                else
                {
                        scsi->ccb.data_seg_list_len = mem_readl_phys(scsi->ccb.addr + 0x04);
                        scsi->ccb.data_seg_list_pointer = mem_readl_phys(scsi->ccb.addr + 0x08);
                        scsi->ccb.data_seg_list_idx = 0;
                        if (scsi->ccb.data_seg_list_len < 8)
                                pclog("Scatter gather table < 8 bytes\n");
                                                                                
                        scsi->ccb.target_id = mem_readb_phys(scsi->ccb.addr + 0x10);
                        scsi->ccb.lun = mem_readb_phys(scsi->ccb.addr + 0x11) & 0x1f;
//                        pclog("Target ID = %02x\n", scsi->ccb.target_id);
                }
                for (c = 0; c < scsi->ccb.scsi_cmd_len; c++)
                        scsi->ccb.cdb[c] = mem_readb_phys(scsi->ccb.addr+0x12+c);
//                pclog("CCB_INITIATOR_SCATTER_GATHER: target=%i lun=%i\n", scsi->ccb.target_id, scsi->ccb.lun);
//                pclog("  scsi_len=%i data_len=%06x data_pointer=%06x\n", scsi->ccb.scsi_cmd_len, scsi->ccb.data_len, scsi->ccb.data_pointer);
                scsi->ccb_state = CCB_STATE_SEND_COMMAND;
                scsi->ccb.status = 0;
                scsi->ccb.from_mailbox = 1;
                scsi->ccb.residual = (scsi->ccb.opcode == CCB_INITIATOR_SCATTER_GATHER_RESIDUAL);

                if (scsi->ccb.residual)
                {
                        /*Read total data length from scatter list, to use when calculating residual*/
                        uint32_t addr = scsi->ccb.data_seg_list_pointer;
                        
                        scsi->ccb.total_data_len = 0;
                        
                        if (scsi->mb_format == MB_FORMAT_4)
                        {
                                for (c = 0; c < scsi->ccb.data_seg_list_len; c += 6)
                                {
                                        uint32_t len = mem_readb_phys(addr + c + 2) |
                                                (mem_readb_phys(addr + c + 1) << 8) |
                                                (mem_readb_phys(addr + c) << 16);
                                        
                                        scsi->ccb.total_data_len += len;                                
                                }
                        }
                        else
                        {
                                for (c = 0; c < scsi->ccb.data_seg_list_len; c += 6)
                                {
                                        uint32_t len = mem_readl_phys(addr + c);
                                        
                                        scsi->ccb.total_data_len += len;                                
                                }
                        }
                }
                break;
                                        
                default:
                fatal("Unknown CCD opcode %02x\n", scsi->ccb.opcode);
        }
        pclog("Start CCB at %06x\n", scsi->ccb.addr);
}

static void process_cmd(aha154x_t *scsi)
{
        uint32_t addr;
        int c;
        
        switch (scsi->cmd_state)
        {
                case CMD_STATE_RESET:
                scsi->status = STATUS_INIT;
                scsi->cmd_state = CMD_STATE_IDLE;
                pclog("AHA154x reset complete\n");
                break;
                
                case CMD_STATE_IDLE:
                if (scsi->status & STATUS_CDF)
                {
                        int invalid = 0;
                        
                        scsi->status &= ~STATUS_CDF;
                        
                        scsi->command = scsi->cmd_data;
                        pclog("New command %02x\n", scsi->command);
//                        if (scsi->command == 0x22)
//                                fatal("Here\n");
//                                output = 3;
                        switch (scsi->command)
                        {
                                case COMMAND_NOP:
                                case COMMAND_START_SCSI_COMMAND:
                                case COMMAND_ADAPTER_INQUIRY:
                                case COMMAND_INQUIRE_INSTALLED_DEVICES:
                                case COMMAND_RETURN_CONFIG_DATA:
                                scsi->command_len = 0;
                                break;

                                case COMMAND_ADAPTEC_SET_BIOS_BANK_1:
                                case COMMAND_ADAPTEC_SET_BIOS_BANK_2:
                                case COMMAND_ADAPTEC_GET_EXT_BIOS_INFO:
                                case COMMAND_ADAPTEC_START_BIOS_SCSI_CMD:
                                if (scsi->type == SCSI_AHA1542C)
                                        scsi->command_len = 0;
                                else
                                        invalid = 1;
                                break;
                                        
                                case COMMAND_BUSLOGIC_84:
                                case COMMAND_BUSLOGIC_85:
                                if (scsi->type == SCSI_BT545S)
                                        scsi->command_len = 0;
                                else
                                        invalid = 1;
                                break;
                                
                                case COMMAND_ECHO:
                                case COMMAND_MAILBOX_OUT_IRQ_ENA:
                                case COMMAND_SET_BUS_ON_TIME:
                                case COMMAND_SET_BUS_OFF_TIME:
                                case COMMAND_SET_AT_BUS_SPEED:
                                case COMMAND_RETURN_SETUP_DATA:
                                scsi->command_len = 1;
                                break;

                                case COMMAND_ADAPTEC_SET_SHADOW_PARAMS:
                                if (scsi->type == SCSI_AHA1542C)
                                        scsi->command_len = 1;
                                else
                                        invalid = 1;
                                break;

                                case 0x22:
                                if (scsi->type == SCSI_AHA1542C)
                                        scsi->command_len = 35; /*COMMAND_ADAPTEC_PROGRAM_EEPROM*/
                                else
                                {
//                                        scsi->command_len = 1;  /*COMMAND_BUSLOGIC_22*/
                                        pclog("Get 22\n");
//                                        scsi->command_len = 0;
                                        invalid = 1;
                                        primed = 1;
                                }
                                break;

                                case COMMAND_BUSLOGIC_SET_CCB_FORMAT:
                                case COMMAND_BUSLOGIC_STRICT_RR:
                                case COMMAND_BUSLOGIC_EXTENDED_SETUP_INFO:
                                case COMMAND_BUSLOGIC_ADAPTER_MODEL_NR:
                                case COMMAND_BUSLOGIC_INQUIRY_SYNC_PERIOD:
                                if (scsi->type == SCSI_BT545S)
                                        scsi->command_len = 1;
                                else
                                        invalid = 1;
                                break;

                                case COMMAND_ENABLE_MAILBOX_INTERFACE:
                                if (scsi->type == SCSI_AHA1542C)
                                        scsi->command_len = 2;
                                else
                                        invalid = 1;
                                break;
                                
                                case COMMAND_WRITE_CHANNEL_2_BUF:
                                case COMMAND_READ_CHANNEL_2_BUF:
                                scsi->command_len = 3;
                                break;

                                case COMMAND_SET_SELECTION_TIMEOUT:
                                scsi->command_len = 4;
                                break;

                                case COMMAND_ADAPTEC_RETURN_EEPROM_DATA:
                                if (scsi->type == SCSI_AHA1542C)
                                        scsi->command_len = 3;
                                else
                                        invalid = 1;
                                break;

                                case COMMAND_MAILBOX_INITIALIZATION:
                                scsi->command_len = 4;
                                break;

                                case COMMAND_BUSLOGIC_81:
                                if (scsi->type == SCSI_BT545S)
                                        scsi->command_len = 5;
                                else
                                        invalid = 1;
                                break;

                                case COMMAND_START_BIOS_COMMAND:
                                scsi->command_len = 10;
                                scsi->bios_cmd_state = 0;
                                break;
                                
                                case COMMAND_ADAPTEC_BIOS_MAILBOX_INIT:
                                if (scsi->type == SCSI_AHA1542C)
                                        scsi->command_len = 4;
                                else
                                        invalid = 1;
                                break;

/*                                case 0x40: case 0x42: case 0x44: case 0xff:
                                invalid = 1;
                                break;*/
                                
                                default:
                                if (scsi->type == SCSI_AHA1542C)
                                {
                                        if ((scsi->command > 0xd && scsi->command < 0x1a) || (scsi->command > 0x2a && scsi->command != 0x82))
                                                invalid = 1;
                                        else
                                                fatal("Bad AHA154x command %02x\n", scsi->command);
                                }
                                else
                                {
                                        if (scsi->command > 0x22 && (scsi->command < 0xfa || scsi->command > 0xfc) &&
                                            !(scsi->command >= 0x81 && scsi->command <= 0x9d) &&
                                            scsi->command != 0xda && scsi->command != 0xdc)
                                                invalid = 1;
                                        else
                                                fatal("Bad BT545S command %02x\n", scsi->command);
                                }
                                break;
                        }
                        
                        if (invalid)
                        {
                                scsi->status |= STATUS_INVDCMD;
                                set_irq(scsi, ISR_HACC);
                                scsi->cmd_state = CMD_STATE_IDLE;
                        }
                        else if (!scsi->command_len)
                                scsi->cmd_state = CMD_STATE_CMD_IN_PROGRESS;
                        else
                        {
                                scsi->cmd_state = CMD_STATE_GET_PARAMS;
                                scsi->cur_param = 0;
                        }
                }
                if (scsi->cmd_state != CMD_STATE_CMD_IN_PROGRESS)
                {
//                        pclog("Command not fallthrough\n");
                        break;
                }
//                pclog("Command fallthrough\n");
                /*Fallthrough*/
                
                case CMD_STATE_CMD_IN_PROGRESS:
                switch (scsi->command)
                {
                        case COMMAND_NOP:
//                        pclog("NOP complete\n");
                        set_irq(scsi, ISR_HACC);
                        scsi->cmd_state = CMD_STATE_IDLE;
                        break;
                        
                        case COMMAND_MAILBOX_INITIALIZATION:
                        scsi->mbc = scsi->params[0];
                        scsi->mba = scsi->params[3] | (scsi->params[2] << 8) | (scsi->params[1] << 16);
                        scsi->mba_i = scsi->mba + scsi->mbc*4;
                        scsi->mb_format = MB_FORMAT_4;
                        scsi->current_mbo = 0;
                        scsi->current_mbi = 0;
                        pclog("Mailbox init: MBC=%02x MBC=%06x\n", scsi->mbc, scsi->mba);
                        scsi->status &= ~STATUS_INIT;
                        set_irq(scsi, ISR_HACC);
                        scsi->cmd_state = CMD_STATE_IDLE;
                        break;
                        
                        case COMMAND_START_SCSI_COMMAND:
                        pclog("START_SCSI_COMMAND %i %i\n", scsi->ccb_state, scsi->scsi_state);
                        scsi->mbo_req++;
                        scsi->cmd_state = CMD_STATE_IDLE;
                        /*Don't send IRQ until we actually start processing a mailbox*/
                        break;
                        
                        case COMMAND_START_BIOS_COMMAND:
                        if (scsi->ccb_state != CCB_STATE_IDLE)
                                break;
                        if (!scsi->bios_cmd_state)
                                pclog("Start BIOS command %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
                                        scsi->params[0], scsi->params[1], scsi->params[2], scsi->params[3], scsi->params[4],
                                        scsi->params[5], scsi->params[6], scsi->params[7], scsi->params[8], scsi->params[9]);
                        if (scsi->params[0] >= 0x16)
                        {
                                /*Only commands below 0x16 are implemented*/
                                scsi->status |= STATUS_INVDCMD;
                                set_irq(scsi, ISR_HACC);
                                scsi->cmd_state = CMD_STATE_IDLE;
                        }
                        else switch (scsi->params[0])
                        {
                                case 0x00: /*Reset Disk System*/
                                scsi->data_in = 0;
                                set_irq(scsi, ISR_HACC);
                                scsi->status |= STATUS_DF;
                                scsi->cmd_state = CMD_STATE_IDLE;
                                break;

                                case 0x01: /*Get Status of Last Operation*/
                                scsi->data_in = 0;
                                set_irq(scsi, ISR_HACC);
                                scsi->status |= STATUS_DF;
                                scsi->cmd_state = CMD_STATE_IDLE;
                                break;
                                
                                case 0x02: /*Read Sector(s) Into Memory*/
                                switch (scsi->bios_cmd_state)
                                {
                                        case 0:
                                        {
                                                int sector = scsi->params[5];
                                                int head = (scsi->params[4] & 0xf) | ((scsi->params[3] & 3) << 4);
                                                int cylinder = (scsi->params[3] >> 2) | ((scsi->params[2] & 0xf) << 6);
                                                
                                                pclog("Cylinder=%i head=%i sector=%i\n", cylinder, head, sector);
                                                addr = sector + (head * 32) + (cylinder * 64 * 32);
                                        }
                                        scsi->ccb.cdb[0] = SCSI_READ_10;
                                        scsi->ccb.cdb[1] = 0;
                                        scsi->ccb.cdb[2] = addr >> 24;
                                        scsi->ccb.cdb[3] = addr >> 16;
                                        scsi->ccb.cdb[4] = addr >> 8;
                                        scsi->ccb.cdb[5] = addr & 0xff;
                                        scsi->ccb.cdb[6] = 0;
                                        scsi->ccb.cdb[7] = 0;
                                        scsi->ccb.cdb[8] = scsi->params[6];
                                        scsi->ccb.cdb[9] = 0;
                                        scsi->ccb.lun = 0;
                                        scsi->ccb.target_id = scsi->params[1] & 7;
                                        scsi->ccb.scsi_cmd_len = 10;
                                        scsi->ccb.data_pointer = scsi->params[9] | (scsi->params[8] << 8) | (scsi->params[7] << 16);;
                                        scsi->ccb.data_len = 512 * scsi->params[6];
                                        scsi->ccb.req_sense_len = CCB_SENSE_14;
                                        scsi->ccb_state = CCB_STATE_SEND_COMMAND;
                                        pclog("Read sector %08x %06x\n", addr, scsi->ccb.data_pointer);
                                        scsi->bios_cmd_state++;
                                        break;
                                        case 1:
                                        if (scsi->ccb.status)
                                                fatal("Read sector fail %02x\n", scsi->ccb.status);
                                        scsi->data_in = 0x00;
                                        set_irq(scsi, ISR_HACC);
                                        scsi->status |= STATUS_DF;
                                        scsi->cmd_state = CMD_STATE_IDLE;
                                        break;
                                }
                                break;

                                case 0x03: /*Write Disk Sector(s)*/
                                switch (scsi->bios_cmd_state)
                                {
                                        case 0:
                                        {
                                                int sector = scsi->params[5];
                                                int head = (scsi->params[4] & 0xf) | ((scsi->params[3] & 3) << 4);
                                                int cylinder = (scsi->params[3] >> 2) | ((scsi->params[2] & 0xf) << 6);
                                                
                                                addr = sector + (head * 32) + (cylinder * 64 * 32);
                                        }
                                        scsi->ccb.cdb[0] = SCSI_WRITE_10;
                                        scsi->ccb.cdb[1] = 0;
                                        scsi->ccb.cdb[2] = addr >> 24;
                                        scsi->ccb.cdb[3] = addr >> 16;
                                        scsi->ccb.cdb[4] = addr >> 8;
                                        scsi->ccb.cdb[5] = addr & 0xff;
                                        scsi->ccb.cdb[6] = 0;
                                        scsi->ccb.cdb[7] = 0;
                                        scsi->ccb.cdb[8] = scsi->params[6];
                                        scsi->ccb.cdb[9] = 0;
                                        scsi->ccb.lun = 0;
                                        scsi->ccb.target_id = scsi->params[1] & 7;
                                        scsi->ccb.scsi_cmd_len = 10;
                                        scsi->ccb.data_pointer = scsi->params[9] | (scsi->params[8] << 8) | (scsi->params[7] << 16);;
                                        scsi->ccb.data_len = 512 * scsi->params[6];
                                        scsi->ccb.req_sense_len = CCB_SENSE_14;
                                        scsi->ccb_state = CCB_STATE_SEND_COMMAND;
                                        pclog("Write sector %08x %06x\n", addr, scsi->ccb.data_pointer);
                                        scsi->bios_cmd_state++;
                                        break;
                                        case 1:
                                        if (scsi->ccb.status)
                                                fatal("Write sector fail %02x\n", scsi->ccb.status);
                                        scsi->data_in = 0x00;
                                        set_irq(scsi, ISR_HACC);
                                        scsi->status |= STATUS_DF;
                                        scsi->cmd_state = CMD_STATE_IDLE;
                                        break;
                                }
                                break;

                                case 0x04: /*Verify Disk Sector(s)*/
                                switch (scsi->bios_cmd_state)
                                {
                                        case 0:
                                        {
                                                int sector = scsi->params[5];
                                                int head = (scsi->params[4] & 0xf) | ((scsi->params[3] & 3) << 4);
                                                int cylinder = (scsi->params[3] >> 2) | ((scsi->params[2] & 0xf) << 6);
                                                
                                                addr = sector + (head * 32) + (cylinder * 64 * 32);
                                        }
                                        scsi->ccb.cdb[0] = SCSI_VERIFY_10;
                                        scsi->ccb.cdb[1] = 0;
                                        scsi->ccb.cdb[2] = addr >> 24;
                                        scsi->ccb.cdb[3] = addr >> 16;
                                        scsi->ccb.cdb[4] = addr >> 8;
                                        scsi->ccb.cdb[5] = addr & 0xff;
                                        scsi->ccb.cdb[6] = 0;
                                        scsi->ccb.cdb[7] = 0;
                                        scsi->ccb.cdb[8] = scsi->params[6];
                                        scsi->ccb.cdb[9] = 0;
                                        scsi->ccb.lun = 0;
                                        scsi->ccb.target_id = scsi->params[1] & 7;
                                        scsi->ccb.scsi_cmd_len = 10;
                                        scsi->ccb.data_pointer = scsi->params[9] | (scsi->params[8] << 8) | (scsi->params[7] << 16);;
                                        scsi->ccb.data_len = 512 * scsi->params[6];
                                        scsi->ccb.req_sense_len = CCB_SENSE_14;
                                        scsi->ccb_state = CCB_STATE_SEND_COMMAND;
                                        pclog("Verify sector %08x %06x\n", addr, scsi->ccb.data_pointer);
                                        scsi->bios_cmd_state++;
                                        break;
                                        case 1:
                                        if (scsi->ccb.status)
                                                fatal("Verify sector fail %02x\n", scsi->ccb.status);
                                        scsi->data_in = 0x00;
                                        set_irq(scsi, ISR_HACC);
                                        scsi->status |= STATUS_DF;
                                        scsi->cmd_state = CMD_STATE_IDLE;
                                        break;
                                }
                                break;

                                /*AHA1542C firmware just fails all of these*/                                
                                case 0x05: case 0x06: case 0x07:
                                case 0x0a: case 0x0b:
                                case 0x12: case 0x13:
                                scsi->status |= STATUS_INVDCMD;
                                set_irq(scsi, ISR_HACC);
                                scsi->cmd_state = CMD_STATE_IDLE;
                                break;

                                case 0x08: /*Get Drive Parameters*/
                                switch (scsi->bios_cmd_state)
                                {
                                        case 0:
                                        scsi->ccb.cdb[0] = SCSI_READ_CAPACITY_10;
                                        scsi->ccb.cdb[1] = 0;
                                        scsi->ccb.cdb[2] = 0;
                                        scsi->ccb.cdb[3] = 0;
                                        scsi->ccb.cdb[4] = 0;
                                        scsi->ccb.cdb[5] = 0;
                                        scsi->ccb.cdb[6] = 0;
                                        scsi->ccb.cdb[7] = 0;
                                        scsi->ccb.cdb[8] = 0;
                                        scsi->ccb.cdb[9] = 0;
                                        scsi->ccb.lun = 0;
                                        scsi->ccb.target_id = scsi->params[1] & 7;
                                        scsi->ccb.scsi_cmd_len = 10;
                                        scsi->ccb.data_pointer = -1;
                                        scsi->ccb.data_len = 8;
                                        scsi->ccb.req_sense_len = CCB_SENSE_NONE;
                                        scsi->ccb_state = CCB_STATE_SEND_COMMAND;
                                        scsi->bios_cmd_state++;
                                        break;
                                        case 1:
                                        addr = scsi->params[9] | (scsi->params[8] << 8) | (scsi->params[7] << 16);
                                        mem_writeb_phys(addr,   scsi->int_buffer[0]);
                                        mem_writeb_phys(addr+1, scsi->int_buffer[1]);
                                        mem_writeb_phys(addr+2, scsi->int_buffer[2]);
                                        mem_writeb_phys(addr+3, scsi->int_buffer[3]);
                                        pclog("Got size %06x %02x%02x%02x%02x\n", addr, scsi->int_buffer[3],scsi->int_buffer[2],scsi->int_buffer[1],scsi->int_buffer[0]);
                                        scsi->data_in = 0;
                                        set_irq(scsi, ISR_HACC);
                                        scsi->status |= STATUS_DF;
                                        scsi->cmd_state = CMD_STATE_IDLE;
                                        break;
                                }
                                break;
                                
                                case 0x09: /*Initialize Drive Parameters*/
                                case 0x0c: /*Seek To Cylinder*/
                                case 0x0d: /*Reset Hard Disks*/
                                case 0x0e: /*Read Sector Buffer*/
                                case 0x0f: /*Write Sector Buffer*/
                                case 0x10: /*Check If Drive Ready*/
                                case 0x11: /*Recalibrate Drive*/
                                case 0x14: /*Controller Internal Diagnostic*/
                                pclog("Unimplemented AHA1542 BIOS command - 0x%02x\n", scsi->params[0]);
                                scsi->data_in = 0;
                                set_irq(scsi, ISR_HACC);
                                scsi->status |= STATUS_DF;
                                scsi->cmd_state = CMD_STATE_IDLE;
                                break;
                                                                
                                case 0x15: /*Get Disk Type*/
                                switch (scsi->bios_cmd_state)
                                {
                                        case 0:
                                        scsi->ccb.cdb[0] = SCSI_INQUIRY;
                                        scsi->ccb.cdb[1] = 0;
                                        scsi->ccb.cdb[2] = 0;
                                        scsi->ccb.cdb[3] = 0;
                                        scsi->ccb.cdb[4] = 36;
                                        scsi->ccb.cdb[5] = 0;
                                        scsi->ccb.lun = 0;
                                        scsi->ccb.target_id = scsi->params[1] & 7;
                                        scsi->ccb.scsi_cmd_len = 6;
                                        scsi->ccb.data_pointer = -1;
                                        scsi->ccb.data_len = 36;
                                        scsi->ccb.req_sense_len = CCB_SENSE_NONE;
                                        scsi->ccb_state = CCB_STATE_SEND_COMMAND;
                                        scsi->ccb.from_mailbox = 0;
                                        scsi->bios_cmd_state++;
                                        break;
                                        case 1:
                                        if (scsi->int_buffer[0] & 0x1f)
                                        {
                                                /*Not a direct-access device*/
                                                fatal("Not a direct-access device\n");
                                        }
                                        scsi->ccb.cdb[0] = SCSI_READ_CAPACITY_10;
                                        scsi->ccb.cdb[1] = 0;
                                        scsi->ccb.cdb[2] = 0;
                                        scsi->ccb.cdb[3] = 0;
                                        scsi->ccb.cdb[4] = 0;
                                        scsi->ccb.cdb[5] = 0;
                                        scsi->ccb.cdb[6] = 0;
                                        scsi->ccb.cdb[7] = 0;
                                        scsi->ccb.cdb[8] = 0;
                                        scsi->ccb.cdb[9] = 0;
                                        scsi->ccb.lun = 0;
                                        scsi->ccb.target_id = scsi->params[1] & 7;
                                        scsi->ccb.scsi_cmd_len = 10;
                                        scsi->ccb.data_pointer = -1;
                                        scsi->ccb.data_len = 8;
                                        scsi->ccb.req_sense_len = CCB_SENSE_NONE;
                                        scsi->ccb_state = CCB_STATE_SEND_COMMAND;
                                        scsi->bios_cmd_state++;
                                        break;
                                        case 2:
                                        addr = scsi->params[9] | (scsi->params[8] << 8) | (scsi->params[7] << 16);
                                        mem_writeb_phys(addr,   scsi->int_buffer[0]);
                                        mem_writeb_phys(addr+1, scsi->int_buffer[1]);
                                        mem_writeb_phys(addr+2, scsi->int_buffer[2]);
                                        mem_writeb_phys(addr+3, scsi->int_buffer[3]);
                                        pclog("Got size %06x %02x%02x%02x%02x\n", addr, scsi->int_buffer[3],scsi->int_buffer[2],scsi->int_buffer[1],scsi->int_buffer[0]);
                                        scsi->data_in = 0;
                                        set_irq(scsi, ISR_HACC);
                                        scsi->status |= STATUS_DF;
                                        scsi->cmd_state = CMD_STATE_IDLE;
                                        break;
                                        
                                        default:
                                        fatal("Get Disk Type state %i\n", scsi->bios_cmd_state);
                                }
                                //fatal("data_pointer %06x\n", scsi->ccb.data_pointer);
                                break;
                                
                                default:
                                scsi->status |= STATUS_INVDCMD;
                                set_irq(scsi, ISR_HACC);
                                scsi->cmd_state = CMD_STATE_IDLE;
                                break;
                        }
                        break;
                        
                        case COMMAND_ADAPTER_INQUIRY:
                        if (scsi->type == SCSI_BT545S)
                        {
                                scsi->result[0] = 0x41;
                                scsi->result[1] = 0x41; /*Standard model*/
                                scsi->result[2] = 0x33; /*Firmware revision*/
                                scsi->result[3] = 0x33;
                        }
                        else
                        {
                                scsi->result[0] = 0x44; /*AHA-1542CF*/
                                scsi->result[1] = 0x30;
                                scsi->result[2] = 0x30; /*Firmware revision*/
                                scsi->result[3] = 0x31;
                        }
                        scsi->cmd_state = CMD_STATE_SEND_RESULT;
                        scsi->result_pos = 0;
                        scsi->result_len = 4;
                        break;
                        
                        case COMMAND_MAILBOX_OUT_IRQ_ENA:
                        scsi->mbo_irq_enable = scsi->params[0];
                        set_irq(scsi, ISR_HACC);
                        scsi->cmd_state = CMD_STATE_IDLE;
                        break;
                                
                        case COMMAND_SET_SELECTION_TIMEOUT:
                        scsi->e_d = scsi->params[0];
                        scsi->to = (scsi->params[1] << 8) | scsi->params[2];
                        set_irq(scsi, ISR_HACC);
                        scsi->cmd_state = CMD_STATE_IDLE;
                        break;

                        case COMMAND_SET_BUS_ON_TIME:
                        scsi->bon = scsi->params[0];
                        set_irq(scsi, ISR_HACC);
                        scsi->cmd_state = CMD_STATE_IDLE;
                        break;
                        
                        case COMMAND_SET_BUS_OFF_TIME:
                        scsi->boff = scsi->params[0];
                        set_irq(scsi, ISR_HACC);
                        scsi->cmd_state = CMD_STATE_IDLE;
                        break;
                        
                        case COMMAND_SET_AT_BUS_SPEED:
                        scsi->atbs = scsi->params[0];
                        set_irq(scsi, ISR_HACC);
                        scsi->cmd_state = CMD_STATE_IDLE;
                        break;
                        
                        case COMMAND_INQUIRE_INSTALLED_DEVICES:
                        for (c = 0; c < 8; c++)
                                scsi->result[c] = scsi->bus.devices[c] ? 1 : 0;
                        scsi->cmd_state = CMD_STATE_SEND_RESULT;
                        scsi->result_pos = 0;
                        scsi->result_len = 8;
                        break;

                        case COMMAND_RETURN_CONFIG_DATA:
                        scsi->result[0] = 1 << scsi->dma;
                        scsi->result[1] = 1 << (scsi->irq-9);
                        scsi->result[2] = scsi->host_id;
                        scsi->cmd_state = CMD_STATE_SEND_RESULT;
                        scsi->result_pos = 0;
                        scsi->result_len = 3;
                        break;

                        case COMMAND_ECHO:
                        pclog("ECHO\n");
                        scsi->result[0] = scsi->params[0];
                        scsi->cmd_state = CMD_STATE_SEND_RESULT;
                        scsi->result_pos = 0;
                        scsi->result_len = 1;
                        break;
                        
                        case COMMAND_RETURN_SETUP_DATA:
                        pclog("Return setup data %i  %02x %06x\n", scsi->params[0], scsi->mbc, scsi->mba);
                        scsi->result[0] = 0x00; /*SPS*/
                        scsi->result[1] = scsi->atbs; /*AT bus speed*/
                        scsi->result[2] = scsi->bon; /*BON*/
                        scsi->result[3] = scsi->boff; /*BOFF*/
                        scsi->result[4] = scsi->mbc; /*DS:E8 - mailbox count*/
                        scsi->result[5] = scsi->mba >> 16; /*DS:DA - set by command 0x81?*/
                        scsi->result[6] = scsi->mba >> 8; /*DS:DB*/
                        scsi->result[7] = scsi->mba & 0xff; /*DS:DC*/
                        scsi->result[8] = 0x00; /*STA0*/
                        scsi->result[9] = 0x00; /*STA1*/
                        scsi->result[10] = 0x00; /*STA2*/
                        scsi->result[11] = 0x00; /*STA3*/
                        scsi->result[12] = 0x00; /*STA4*/
                        scsi->result[13] = 0x00; /*STA5*/
                        scsi->result[14] = 0x00; /*STA6*/
                        scsi->result[15] = 0x00; /*STA7*/
                        scsi->result[16] = 0x00; /*DS (DS:1e5b)*/
                        if (scsi->type == SCSI_BT545S)
                        {
                                scsi->result[17] = 0x42; /*Firmware version*/
                                scsi->result[18] = 0x44;
                                scsi->result[19] = 0x41;
                        }
                        else
                        {
                                for (c = 0; c < 20; c++)
                                        scsi->result[c+17] = 0; /*Customer signature*/
                                scsi->result[37] = 0; /*Auto-retry*/
                                scsi->result[38] = 0; /*Board switches*/
                                scsi->result[39] = 0xa3; /*Checksum*/
                                scsi->result[40] = 0xc2;
                                scsi->result[41] = scsi->bios_mba >> 16;
                                scsi->result[42] = scsi->bios_mba >> 8;
                                scsi->result[43] = scsi->bios_mba & 0xff;
                        }
                        scsi->cmd_state = CMD_STATE_SEND_RESULT;
                        scsi->result_pos = 0;
                        scsi->result_len = MIN(scsi->params[0], 20);
                        for (; scsi->result_len < scsi->params[0]; scsi->result_len++)
                                scsi->result[scsi->result_len] = 0;
                        break;
                        
                        case COMMAND_WRITE_CHANNEL_2_BUF:
                        addr = scsi->params[2] | (scsi->params[1] << 8) | (scsi->params[0] << 16);
                        pclog("WRITE_CHANNEL_2_BUF %06x\n", addr);
                        for (c = 0; c < 64; c++)
                                scsi->dma_buffer[c] = mem_readb_phys(addr + c);
                        set_irq(scsi, ISR_HACC);
                        scsi->cmd_state = CMD_STATE_IDLE;
                        pclog("Command complete\n");
                        break;
                        
                        case COMMAND_READ_CHANNEL_2_BUF:
                        addr = scsi->params[2] | (scsi->params[1] << 8) | (scsi->params[0] << 16);
                        pclog("READ_CHANNEL_2_BUF %06x\n", addr);
                        for (c = 0; c < 64; c++)
                                mem_writeb_phys(addr + c, scsi->dma_buffer[c]);
                        set_irq(scsi, ISR_HACC);
                        scsi->cmd_state = CMD_STATE_IDLE;
                        pclog("Command complete\n");
                        break;

                        case 0x22:
                        if (scsi->type == SCSI_AHA1542C)
                        {
                                /*COMMAND_ADAPTEC_PROGRAM_EEPROM*/
                                for (c = 0; c < scsi->params[1]; c++)
                                {
                                        pclog("EEPROM %02x = %02x\n", (scsi->params[2] + c) & 0xff, scsi->params[c+3]);
                                        scsi->eeprom[(scsi->params[2] + c) & 0xff] = scsi->params[c+3];
                                }
                                scsi->host_id = scsi->eeprom[0] & 7;
                                scsi->dma = (scsi->eeprom[1] >> 4) & 7;
                                scsi->irq = (scsi->eeprom[1] & 7) + 9;
                                pclog("Program EEPROM: IRQ=%i DMA=%i host_id=%i\n", scsi->irq, scsi->dma, scsi->host_id);

                                aha1542c_eeprom_save(scsi);
                                set_irq(scsi, ISR_HACC);
                                scsi->cmd_state = CMD_STATE_IDLE;
                        }
                        else
                        {
                                /*COMMAND_BUSLOGIC_22*/
                                pclog("BUSLOGIC_22\n");
                        set_irq(scsi, ISR_HACC);
                        scsi->cmd_state = CMD_STATE_IDLE;
                        primed = 1;
//                                scsi->result[0] = 1 << scsi->dma; /*DMA7*/
//                                scsi->result[1] = 1 << (scsi->irq-9); /*IRQ10*/
//                                scsi->result[2] = scsi->host_id; /*SCSI ID*/
//                                scsi->cmd_state = CMD_STATE_SEND_RESULT;
//                                scsi->result_pos = 0;
//                                scsi->result_len = 3;
                        }
                        break;

                        case COMMAND_ADAPTEC_RETURN_EEPROM_DATA:
                        pclog("Return EEPROM data %02x %02x %02x\n", scsi->params[0], scsi->params[1], scsi->params[2]);
                        for (c = 0; c < scsi->params[1]; c++)
                        {
                                scsi->result[c] = scsi->eeprom[(scsi->params[2] + c) & 0xff];
                                pclog("  EEPROM data %02x %02x\n", (scsi->params[2] + c) & 0xff, scsi->result[c]);
                        }
                        scsi->cmd_state = CMD_STATE_SEND_RESULT;
                        scsi->result_pos = 0;
                        scsi->result_len = c;
                        break;
                        
                        case COMMAND_ADAPTEC_SET_SHADOW_PARAMS:
                        pclog("Set shadow params %02x\n", scsi->params[0]);
                        scsi->shadow = scsi->params[0];
                        set_irq(scsi, ISR_HACC);
                        scsi->cmd_state = CMD_STATE_IDLE;
                        break;
                        
                        case COMMAND_ADAPTEC_BIOS_MAILBOX_INIT:
                        scsi->bios_mbc = scsi->params[0];
                        scsi->bios_mba = scsi->params[3] | (scsi->params[2] << 8) | (scsi->params[1] << 16);
                        scsi->bios_mbo_inited = 1;
                        pclog("BIOS mailbox init: MBC=%02x MBC=%06x\n", scsi->bios_mbc, scsi->bios_mba);
                        set_irq(scsi, ISR_HACC);
                        scsi->cmd_state = CMD_STATE_IDLE;
                        break;

                        case COMMAND_ADAPTEC_SET_BIOS_BANK_1:
                        mem_mapping_set_exec(&scsi->mapping, &scsi->bios_rom.rom[0]);
                        scsi->bios_bank = 0x0000;
                        set_irq(scsi, ISR_HACC);
                        scsi->cmd_state = CMD_STATE_IDLE;
                        break;

                        case COMMAND_ADAPTEC_SET_BIOS_BANK_2:
                        mem_mapping_set_exec(&scsi->mapping, &scsi->bios_rom.rom[0x4000]);
                        scsi->bios_bank = 0x4000;
                        set_irq(scsi, ISR_HACC);
                        scsi->cmd_state = CMD_STATE_IDLE;
                        break;

                        case COMMAND_ADAPTEC_GET_EXT_BIOS_INFO:
                        scsi->result[0] = 0;
                        scsi->result[1] = scsi->mblt;
                        scsi->cmd_state = CMD_STATE_SEND_RESULT;
                        scsi->result_pos = 0;
                        scsi->result_len = 2;
                        break;
                        
                        case COMMAND_ENABLE_MAILBOX_INTERFACE:
                        scsi->mbu = scsi->params[0];
                        scsi->mblt = scsi->params[1];
                        set_irq(scsi, ISR_HACC);
                        scsi->cmd_state = CMD_STATE_IDLE;
                        break;
                                        
                        case COMMAND_ADAPTEC_START_BIOS_SCSI_CMD:
                        pclog("START_BIOS_SCSI_COMMAND %02x %08x %i\n", scsi->bios_mbc, scsi->bios_mba, scsi->ccb_state);
                        scsi->bios_mbo_req++;
                        scsi->cmd_state = CMD_STATE_IDLE;
                        break;
                                
                        case COMMAND_BUSLOGIC_81:
                        scsi->mbc = scsi->params[0];
                        scsi->mba = scsi->params[1] | (scsi->params[2] << 8) | (scsi->params[3] << 16) | (scsi->params[4] << 24);
                        scsi->mba_i = scsi->mba + scsi->mbc*8;
                        scsi->mb_format = MB_FORMAT_8;
                        scsi->current_mbo = 0;
                        scsi->current_mbi = 0;
                        pclog("Mailbox init: MBC=%02x MBC=%08x\n", scsi->mbc, scsi->mba);
                        scsi->status &= ~STATUS_INIT;
                        set_irq(scsi, ISR_HACC);
                        scsi->cmd_state = CMD_STATE_IDLE;
                        break;

                        case COMMAND_BUSLOGIC_84:
                        scsi->result[0] = 0x31; /*Firmware sub-version?*/
                        scsi->cmd_state = CMD_STATE_SEND_RESULT;
                        scsi->result_pos = 0;
                        scsi->result_len = 1;
                        break;

                        case COMMAND_BUSLOGIC_85:
                        scsi->result[0] = 0x20; /*???*/
                        scsi->cmd_state = CMD_STATE_SEND_RESULT;
                        scsi->result_pos = 0;
                        scsi->result_len = 1;
                        break;

                        case COMMAND_BUSLOGIC_ADAPTER_MODEL_NR:
                        scsi->result[0] = '5';
                        scsi->result[1] = '4';
                        scsi->result[2] = '2';
                        scsi->result[3] = 'B';
                        scsi->result[4] = 'H';
                        scsi->cmd_state = CMD_STATE_SEND_RESULT;
                        scsi->result_pos = 0;
                        scsi->result_len = MIN(scsi->params[0], 5);
                        for (; scsi->result_len < scsi->params[0]; scsi->result_len++)
                                scsi->result[scsi->result_len] = 0;
                        break;

                        case COMMAND_BUSLOGIC_INQUIRY_SYNC_PERIOD:
                        for (c = 0; c < 8; c++)
                                scsi->result[c] = 0;
                        scsi->cmd_state = CMD_STATE_SEND_RESULT;
                        scsi->result_pos = 0;
                        scsi->result_len = MIN(scsi->params[0], 8);
                        for (; scsi->result_len < scsi->params[0]; scsi->result_len++)
                                scsi->result[scsi->result_len] = 0;
                        break;
                                
                                
                        case COMMAND_BUSLOGIC_EXTENDED_SETUP_INFO:
                        pclog("Extended setup info %i\n", scsi->params[0]);
                        scsi->result[0] = 0x41;
                        scsi->result[1] = scsi->bios_addr >> 12; /*BIOS addr*/
                        scsi->result[2] = 0x00;
                        scsi->result[3] = 0x20;
                        scsi->result[4] = 0x00; /*DS:E8 >> 3*/
                        scsi->result[5] = 0x00; /*DS:DA - set by command 0x81?*/
                        scsi->result[6] = 0x00; /*DS:DB*/
                        scsi->result[7] = 0x00; /*DS:DC*/
                        scsi->result[8] = 0x00; /*DS:DD*/
                        scsi->result[9] = 0x33; /*FFFF:9*/
                        scsi->result[10] = 0x33; /*FFFF:A*/
                        scsi->result[11] = 0x31; /*FFFF:B*/
                        scsi->cmd_state = CMD_STATE_SEND_RESULT;
                        scsi->result_pos = 0;
                        scsi->result_len = MIN(scsi->params[0], 12);
                        for (; scsi->result_len < scsi->params[0]; scsi->result_len++)
                                scsi->result[scsi->result_len] = 0;
                        break;
                        
                        case COMMAND_BUSLOGIC_STRICT_RR:
                        set_irq(scsi, ISR_HACC);
                        scsi->cmd_state = CMD_STATE_IDLE;
                        break;
                        
                        case COMMAND_BUSLOGIC_SET_CCB_FORMAT:
                        set_irq(scsi, ISR_HACC);
                        scsi->cmd_state = CMD_STATE_IDLE;
                        break;

                        default:
                        fatal("Bad AHA154x command in progress %02x\n", scsi->command);
                }
                break;
                
                case CMD_STATE_GET_PARAMS:
                if (scsi->status & STATUS_CDF)
                {
                        scsi->status &= ~STATUS_CDF;
                        
                        scsi->params[scsi->cur_param++] = scsi->cmd_data;
                        
                        if (scsi->cur_param == scsi->command_len)
                                scsi->cmd_state = CMD_STATE_CMD_IN_PROGRESS;
                }
                break;
                
                case CMD_STATE_SEND_RESULT:
                if (!(scsi->status & STATUS_DF))
                {
                        if (scsi->result_pos == scsi->result_len)
                        {
                                set_irq(scsi, ISR_HACC);
                                scsi->cmd_state = CMD_STATE_IDLE;
                                pclog("Command complete\n");
                        }
                        else
                        {
                                scsi->status |= STATUS_DF;
                                scsi->data_in = scsi->result[scsi->result_pos++];
                                pclog("Return data %02x  %i %i\n", scsi->data_in, scsi->result_pos-1, scsi->result_len);
                        }
                }
                break;

                default:
                fatal("Unknown state %d\n", scsi->cmd_state);
        }
}

static void process_ccb(aha154x_t *scsi)
{
        int c;
        
        switch (scsi->ccb_state)
        {                
                case CCB_STATE_IDLE:
                if (!(scsi->status & STATUS_INIT) && scsi->mbo_req)
                {
//                        pclog("mbo_req %i\n", scsi->mbc);
                        for (c = 0; c < scsi->mbc; c++)
                        {
                                uint32_t ccb;
                                uint8_t command;
                        
                                if (scsi->mb_format == MB_FORMAT_4)
                                {
                                        uint32_t mbo = mem_readl_phys(scsi->mba + c*4);
                                        command = mbo & 0xff;
                                        ccb = (mbo >> 24) | ((mbo >> 8) & 0xff00) | ((mbo << 8) & 0xff0000);
//                                        pclog("MBO%02x = %08x\n", c, mbo);
                                }
                                else
                                {
                                        ccb = mem_readl_phys(scsi->mba + c*8);
                                        command = mem_readb_phys(scsi->mba + c*8 + 7);
//                                        pclog("MBO%02x = %08x %02x\n", c, ccb, command);
                                }

                                if (command == COMMAND_START_CCB)
                                {
                                        pclog("Start CCB %08x %i\n", ccb, c);
                                
                                        scsi->current_mbo = c;
                                        scsi->current_mbo_is_bios = 0;
                                        scsi->ccb.addr = ccb;
                                        
                                        process_cdb(scsi);

                                        scsi->mbo_req--;
                                        
                                        //set_irq(scsi, ISR_HACC);
                                        break;
                                }                                
                                else if (command == COMMAND_ABORT_CCB)
                                {
//                                        fatal("Abort command\n");
                                        scsi->mbo_req--;

                                        if (scsi->mb_format == MB_FORMAT_4)
                                        {
                                                mem_writeb_phys(scsi->mba + c*8, 0);

                                                mem_writeb_phys(scsi->mba_i + scsi->current_mbi*4, MBI_CCB_ABORTED);
                                                mem_writeb_phys(scsi->mba_i + scsi->current_mbi*4 + 1, ccb >> 16);
                                                mem_writeb_phys(scsi->mba_i + scsi->current_mbi*4 + 2, ccb >> 8);
                                                mem_writeb_phys(scsi->mba_i + scsi->current_mbi*4 + 3, ccb & 0xff);
                                        }
                                        else
                                        {
                                                mem_writeb_phys(scsi->mba + c*8 + 7, 0);

                                                mem_writel_phys(scsi->mba_i + scsi->current_mbi*8, ccb);
                                                mem_writel_phys(scsi->mba_i + scsi->current_mbi*8 + 4, MBI_CCB_ABORTED);
                                        }

                                        scsi->current_mbi++;
                                        if (scsi->current_mbi >= scsi->mbc)
                                                scsi->current_mbi = 0;
                                        set_irq(scsi, (scsi->mbo_irq_enable ? ISR_MBOA : 0) | ISR_MBIF | ISR_HACC);
                                        break;
                                }
                        }
                }
                if (scsi->bios_mbo_inited && scsi->bios_mbo_req)
                {
                        for (c = 0; c < scsi->bios_mbc; c++)
                        {
                                uint32_t ccb;
                                uint8_t command;
                        
                                uint32_t mbo = mem_readl_phys(scsi->bios_mba + c*4);
                                command = mbo & 0xff;
                                ccb = (mbo >> 24) | ((mbo >> 8) & 0xff00) | ((mbo << 8) & 0xff0000);

                                if (command == COMMAND_START_CCB)
                                {
                                        pclog("Start BIOS CCB %08x %i\n", ccb, c);
                                
                                        scsi->current_mbo = c;
                                        scsi->current_mbo_is_bios = 1;
                                        scsi->ccb.addr = ccb;
                                        
                                        process_cdb(scsi);

                                        scsi->bios_mbo_req--;
                                        set_irq(scsi, ISR_HACC);
                                        break;
                                }                                
                        }
                }
                break;

                case CCB_STATE_SEND_COMMAND:
                if (scsi->scsi_state != SCSI_STATE_IDLE)
                        break; /*Wait until SCSI state machine is ready for new command*/
                for (c = 0; c < scsi->ccb.scsi_cmd_len; c++)
                {
                        scsi->cdb.data[c] = scsi->ccb.cdb[c];
//                        pclog(" CDB[%02x]=%02x\n", c, scsi->cdb.data[c]);
                }
                scsi->cdb.len = scsi->ccb.scsi_cmd_len;
                scsi->cdb.idx = 0;
                if (scsi->ccb.opcode == CCB_INITIATOR_SCATTER_GATHER || scsi->ccb.opcode == CCB_INITIATOR_SCATTER_GATHER_RESIDUAL)
                {
                        scsi->cdb.data_seg_list_len = scsi->ccb.data_seg_list_len;
                        scsi->cdb.data_seg_list_pointer = scsi->ccb.data_seg_list_pointer;
                        scsi->cdb.data_seg_list_idx = 0;
                        scsi->cdb.scatter_gather = 1;
                        scsi->cdb.data_len = 0;
                        scsi->cdb.data_idx = 0;
                }
                else
                {
                        scsi->cdb.data_pointer = scsi->ccb.data_pointer;
                        scsi->cdb.data_len = scsi->ccb.data_len;
                        scsi->cdb.scatter_gather = 0;
                }
                scsi->cdb.bytes_transferred = 0;
                scsi->cdb.data_idx = 0;
                scsi->scsi_state = SCSI_STATE_SELECT;
                /*SCSI will now select the device and execute the command*/
                scsi->ccb_state = CCB_STATE_WAIT_COMMAND;
                break;

                case CCB_STATE_WAIT_COMMAND:
                if (scsi->scsi_state == SCSI_STATE_SELECT_FAILED)
                {
                        pclog("Command select has failed\n");
                        if (scsi->ccb.from_mailbox)
                        {
                                mem_writeb_phys(scsi->ccb.addr + 0xe, HOST_STATUS_SELECTION_TIME_OUT);
                                
                                if (scsi->mb_format == MB_FORMAT_4)
                                {
                                        if (scsi->current_mbo_is_bios)
                                        {
                                                mem_writeb_phys(scsi->bios_mba + scsi->current_mbo*4, COMMAND_FREE_CCB);
                                                mem_writeb_phys(scsi->ccb.addr + 0xd, MBI_CCB_COMPLETE_WITH_ERROR);
                                        }
                                        else
                                        {
                                                mem_writeb_phys(scsi->mba + scsi->current_mbo*4, COMMAND_FREE_CCB);

                                                mem_writeb_phys(scsi->mba_i + scsi->current_mbi*4, MBI_CCB_COMPLETE_WITH_ERROR);
                                                mem_writeb_phys(scsi->mba_i + scsi->current_mbi*4 + 1, scsi->ccb.addr >> 16);
                                                mem_writeb_phys(scsi->mba_i + scsi->current_mbi*4 + 2, scsi->ccb.addr >> 8);
                                                mem_writeb_phys(scsi->mba_i + scsi->current_mbi*4 + 3, scsi->ccb.addr & 0xff);
                                        }
                                }
                                else
                                {
                                        if (scsi->current_mbo_is_bios)
                                                fatal("MBO_IS_BIOS MB_FORMAT_8\n");
                                        mem_writeb_phys(scsi->mba + scsi->current_mbo*8 + 7, COMMAND_FREE_CCB);

                                        mem_writel_phys(scsi->mba_i + scsi->current_mbi*8, scsi->ccb.addr);
                                        mem_writel_phys(scsi->mba_i + scsi->current_mbi*8 + 4, MBI_CCB_COMPLETE_WITH_ERROR << 24);
                                }
                                                        
                                if (!scsi->current_mbo_is_bios)
                                {
                                        scsi->current_mbi++;
                                        if (scsi->current_mbi >= scsi->mbc)
                                                scsi->current_mbi = 0;
                                        set_irq(scsi, (scsi->mbo_irq_enable ? ISR_MBOA : 0) | ISR_MBIF);
                                }
                                else if (scsi->mbo_irq_enable)
                                        set_irq(scsi, ISR_MBOA);
                        }
//                        set_irq(scsi, ISR_HACC);
                        scsi->ccb_state = CCB_STATE_IDLE;
                        scsi->scsi_state = SCSI_STATE_IDLE;
                        break;
                }
                if (scsi->scsi_state != SCSI_STATE_IDLE)
                        break; /*Wait until SCSI state machine has completed command*/
                scsi->ccb.status = scsi->cdb.last_status;
                scsi->ccb.bytes_transferred = scsi->cdb.bytes_transferred;
                scsi->ccb_state = CCB_STATE_SEND_REQUEST_SENSE;
                break;

                case CCB_STATE_SEND_REQUEST_SENSE:
                if (scsi->ccb.req_sense_len == CCB_SENSE_NONE)
                {
//                        pclog("No sense data\n");
                        if (scsi->ccb.from_mailbox)
                        {
                                uint32_t residue = scsi->ccb.total_data_len - scsi->ccb.bytes_transferred;
                                
//                                pclog("Residue=%08x %x %x    %i MBI=%i\n", residue, scsi->ccb.total_data_len, scsi->ccb.bytes_transferred, scsi->ccb.residual, scsi->current_mbi);
                                mem_writeb_phys(scsi->ccb.addr + 0xe, HOST_STATUS_COMMAND_COMPLETE);
                                mem_writeb_phys(scsi->ccb.addr + 0xf, scsi->ccb.status);
                                
                                if (scsi->mb_format == MB_FORMAT_4)
                                {
                                        if (scsi->ccb.residual)
                                        {
                                                mem_writeb_phys(scsi->ccb.addr + 0x4, residue >> 16);
                                                mem_writeb_phys(scsi->ccb.addr + 0x5, residue >> 8);
                                                mem_writeb_phys(scsi->ccb.addr + 0x6, residue & 0xff);
                                        }
                                        
                                        if (scsi->current_mbo_is_bios)
                                        {
                                                mem_writeb_phys(scsi->bios_mba + scsi->current_mbo*4, COMMAND_FREE_CCB);

                                                if (scsi->ccb.status == STATUS_GOOD)
                                                        mem_writeb_phys(scsi->ccb.addr + 0xd, MBI_CCB_COMPLETE);
                                                else
                                                        mem_writeb_phys(scsi->ccb.addr + 0xd, MBI_CCB_COMPLETE_WITH_ERROR);
                                        }
                                        else
                                        {
                                                mem_writeb_phys(scsi->mba + scsi->current_mbo*4, COMMAND_FREE_CCB);
                                
                                                if (scsi->ccb.status == STATUS_GOOD)
                                                        mem_writeb_phys(scsi->mba_i + scsi->current_mbi*4, MBI_CCB_COMPLETE);
                                                else
                                                        mem_writeb_phys(scsi->mba_i + scsi->current_mbi*4, MBI_CCB_COMPLETE_WITH_ERROR);
                                                mem_writeb_phys(scsi->mba_i + scsi->current_mbi*4 + 1, scsi->ccb.addr >> 16);
                                                mem_writeb_phys(scsi->mba_i + scsi->current_mbi*4 + 2, scsi->ccb.addr >> 8);
                                                mem_writeb_phys(scsi->mba_i + scsi->current_mbi*4 + 3, scsi->ccb.addr & 0xff);
                                        }
                                }
                                else
                                {
                                        if (scsi->current_mbo_is_bios)
                                                fatal("MBO_IS_BIOS MB_FORMAT_8\n");

                                        if (scsi->ccb.residual)
                                                mem_writel_phys(scsi->ccb.addr + 0x4, residue);

                                        mem_writeb_phys(scsi->mba + scsi->current_mbo*8 + 7, COMMAND_FREE_CCB);

                                        mem_writel_phys(scsi->mba_i + scsi->current_mbi*8, scsi->ccb.addr);
                                        if (scsi->ccb.status == STATUS_GOOD)
                                                mem_writel_phys(scsi->mba_i + scsi->current_mbi*8 + 4, MBI_CCB_COMPLETE << 24);
                                        else
                                                mem_writel_phys(scsi->mba_i + scsi->current_mbi*8 + 4, (MBI_CCB_COMPLETE_WITH_ERROR << 24) | (scsi->ccb.status << 8));
                                }
                                
                                if (!scsi->current_mbo_is_bios)
                                {
                                        scsi->current_mbi++;
                                        if (scsi->current_mbi >= scsi->mbc)
                                                scsi->current_mbi = 0;
                                        set_irq(scsi, (scsi->mbo_irq_enable ? ISR_MBOA : 0) | ISR_MBIF);
                                }
                                else if (scsi->mbo_irq_enable)
                                        set_irq(scsi, ISR_MBOA);
                        }
                        scsi->ccb_state = CCB_STATE_IDLE;
                        break;
                }
                /*Build request sense command*/
                scsi->cdb.data[0] = SCSI_REQUEST_SENSE;
                scsi->cdb.data[1] = scsi->ccb.lun << 5;
                scsi->cdb.data[2] = 0x00;
                scsi->cdb.data[3] = 0x00;
                if (scsi->ccb.req_sense_len == CCB_SENSE_14)
                        scsi->cdb.data[4] = 14;
                else
                        scsi->cdb.data[4] = scsi->ccb.req_sense_len;
                scsi->cdb.data[5] = 14;
                scsi->cdb.len = 6;
                scsi->cdb.idx = 0;
                if (scsi->ccb.from_mailbox)
                        scsi->cdb.data_pointer = -1;
                else
                        scsi->cdb.data_pointer = scsi->ccb.addr + 0x12 + scsi->ccb.scsi_cmd_len;
                scsi->cdb.data_idx = 0;
                scsi->cdb.data_len = scsi->cdb.data[4];
                scsi->cdb.scatter_gather = 0;
                scsi->scsi_state = SCSI_STATE_SELECT;
                scsi->ccb_state = CCB_STATE_WAIT_REQUEST_SENSE;
                break;

                case CCB_STATE_WAIT_REQUEST_SENSE:
                if (scsi->scsi_state != SCSI_STATE_IDLE)
                        break; /*Wait until SCSI state machine has completed command*/

                if (scsi->ccb.from_mailbox)
                {
                        uint32_t residue = scsi->ccb.total_data_len - scsi->ccb.bytes_transferred;
//                        pclog("residue=%x %x %x\n", residue, scsi->ccb.total_data_len, scsi->ccb.bytes_transferred);
                        mem_writeb_phys(scsi->ccb.addr + 0xe, HOST_STATUS_COMMAND_COMPLETE);
                        mem_writeb_phys(scsi->ccb.addr + 0xf, scsi->ccb.status);

                        if (scsi->mb_format == MB_FORMAT_4)
                        {                                
                                if (scsi->ccb.residual)
                                {
                                        mem_writeb_phys(scsi->ccb.addr + 0x4, residue >> 16);
                                        mem_writeb_phys(scsi->ccb.addr + 0x5, residue >> 8);
                                        mem_writeb_phys(scsi->ccb.addr + 0x6, residue & 0xff);
                                }

                                if (scsi->current_mbo_is_bios)
                                {
                                        mem_writeb_phys(scsi->bios_mba + scsi->current_mbo*4, COMMAND_FREE_CCB);

                                        if (scsi->ccb.status == STATUS_GOOD)
                                                mem_writeb_phys(scsi->ccb.addr + 0xd, MBI_CCB_COMPLETE);
                                        else
                                                mem_writeb_phys(scsi->ccb.addr + 0xd, MBI_CCB_COMPLETE_WITH_ERROR);
                                }
                                else
                                {
                                        mem_writeb_phys(scsi->mba + scsi->current_mbo*4, COMMAND_FREE_CCB);

//                        pclog("CCB status %02x\n", scsi->ccb.status);
                                        if (scsi->ccb.status == STATUS_GOOD)
                                                mem_writeb_phys(scsi->mba_i + scsi->current_mbi*4, MBI_CCB_COMPLETE);
                                        else
                                                mem_writeb_phys(scsi->mba_i + scsi->current_mbi*4, MBI_CCB_COMPLETE_WITH_ERROR);
                                        mem_writeb_phys(scsi->mba_i + scsi->current_mbi*4 + 1, scsi->ccb.addr >> 16);
                                        mem_writeb_phys(scsi->mba_i + scsi->current_mbi*4 + 2, scsi->ccb.addr >> 8);
                                        mem_writeb_phys(scsi->mba_i + scsi->current_mbi*4 + 3, scsi->ccb.addr & 0xff);
                                }
                        }
                        else
                        {                                       
                                if (scsi->current_mbo_is_bios)
                                        fatal("MBO_IS_BIOS MB_FORMAT_8\n");

                                if (scsi->ccb.residual)
                                        mem_writel_phys(scsi->ccb.addr + 0x4, residue);

                                mem_writeb_phys(scsi->mba + scsi->current_mbo*8 + 7, COMMAND_FREE_CCB);

                                mem_writel_phys(scsi->mba_i + scsi->current_mbi*8, scsi->ccb.addr);
                                if (scsi->ccb.status == STATUS_GOOD)
                                        mem_writel_phys(scsi->mba_i + scsi->current_mbi*8 + 4, MBI_CCB_COMPLETE << 24);
                                else
                                        mem_writel_phys(scsi->mba_i + scsi->current_mbi*8 + 4, (MBI_CCB_COMPLETE_WITH_ERROR << 24) | (scsi->ccb.status << 8));
                        }
                        
                        if (!scsi->current_mbo_is_bios)
                        {
                                scsi->current_mbi++;
                                if (scsi->current_mbi >= scsi->mbc)
                                        scsi->current_mbi = 0;
                                set_irq(scsi, (scsi->mbo_irq_enable ? ISR_MBOA : 0) | ISR_MBIF);
                        }
                        else if (scsi->mbo_irq_enable)
                                set_irq(scsi, ISR_MBOA);
                }
//                pclog("CCB_STATE_WAIT_REQUEST_SENSE over %i\n", scsi->ccb.from_mailbox);
                scsi->ccb_state = CCB_STATE_IDLE;
                break;

                default:
                fatal("Unknown CCB_state %d\n", scsi->ccb_state);
        }
}

static void process_scsi(aha154x_t *scsi)
{
        int c;
        int bytes_transferred = 0;
        
        switch (scsi->scsi_state)
        {
                case SCSI_STATE_IDLE:
                break;
                
                case SCSI_STATE_SELECT:
                scsi->cdb.last_status = 0;
//                pclog("Select target ID %i\n", scsi->ccb.target_id);
                scsi_bus_update(&scsi->bus, BUS_SEL | BUS_SETDATA(1 << scsi->ccb.target_id));
                if (!(scsi_bus_read(&scsi->bus) & BUS_BSY) || scsi->ccb.target_id > 6)
                {
                        pclog("STATE_SCSI_SELECT failed to select target %i\n", scsi->ccb.target_id);
                        scsi->scsi_state = SCSI_STATE_SELECT_FAILED;
                        break;
                }

                scsi_bus_update(&scsi->bus, 0);
                if (!(scsi_bus_read(&scsi->bus) & BUS_BSY))
                {
                        pclog("STATE_SCSI_SELECT failed to select target %i 2\n", scsi->ccb.target_id);
                        scsi->scsi_state = SCSI_STATE_SELECT_FAILED;
                        break;
                }
                
                /*Device should now be selected*/
                if (!wait_for_bus(&scsi->bus, BUS_CD, 1))
                {
                        pclog("Device failed to request command\n");
                        scsi->scsi_state = SCSI_STATE_SELECT_FAILED;
                        break;
                }

                scsi->scsi_state = SCSI_STATE_SEND_COMMAND;
                break;
                
                case SCSI_STATE_SELECT_FAILED:
                break;

                case SCSI_STATE_SEND_COMMAND:
                while (scsi->cdb.idx < scsi->cdb.len && bytes_transferred < MAX_BYTES_TRANSFERRED_PER_POLL)
                {
                        int bus_out;
                        
                        for (c = 0; c < 20; c++)
                        {
                                int bus_state = scsi_bus_read(&scsi->bus);
                                
                                if (!(bus_state & BUS_BSY))
                                        fatal("SEND_COMMAND - dropped BSY\n");
                                if ((bus_state & (BUS_IO | BUS_CD | BUS_MSG)) != BUS_CD)
                                        fatal("SEND_COMMAND - bus phase incorrect\n");
                                if (bus_state & BUS_REQ)
                                        break;
                        }
                        if (c == 20)
                        {
                                pclog("SEND_COMMAND timed out\n");
                                break;
                        }
                        
                        bus_out = BUS_SETDATA(scsi->cdb.data[scsi->cdb.idx]);
//                        pclog("  Command send %02x %i\n", scsi->cdb.data[scsi->cdb.idx], scsi->cdb.len);
//                        if (!scsi->cdb.idx && scsi->cdb.data[scsi->cdb.idx] == 0x25 && scsi->current_mbo == 1)
//                                output = 3;
                        scsi->cdb.idx++;
                        bytes_transferred++;
                        	
                        scsi_bus_update(&scsi->bus, bus_out | BUS_ACK);
                        scsi_bus_update(&scsi->bus, bus_out & ~BUS_ACK);                        
                }
                if (scsi->cdb.idx == scsi->cdb.len)
                        scsi->scsi_state = SCSI_STATE_NEXT_PHASE;
                break;
                
                case SCSI_STATE_NEXT_PHASE:
                /*Wait for SCSI command to move to next phase*/
                for (c = 0; c < 20; c++)
                {
                        int bus_state = scsi_bus_read(&scsi->bus);

                        if (!(bus_state & BUS_BSY))
                                fatal("NEXT_PHASE - dropped BSY waiting\n");

                        if (bus_state & BUS_REQ)
                        {
                                int bus_out;
//                                pclog("SCSI next phase - %x\n", bus_state);
                                switch (bus_state & (BUS_IO | BUS_CD | BUS_MSG))
                                {
                                        case 0:
//                                        pclog("Move to write data\n");
                                        scsi->scsi_state = SCSI_STATE_WRITE_DATA;
                                        break;
                                        
                                        case BUS_IO:
//                                        pclog("Move to read data\n");
                                        scsi->scsi_state = SCSI_STATE_READ_DATA;
                                        break;

                                        case (BUS_CD | BUS_IO):
//                                        pclog("Move to read status\n");
                                        scsi->scsi_state = SCSI_STATE_READ_STATUS;
                                        break;
                                        
                                        case (BUS_CD | BUS_IO | BUS_MSG):
//                                        pclog("Move to read message\n");
                                        scsi->scsi_state = SCSI_STATE_READ_MESSAGE;
                                        break;
                                        
                                        case BUS_CD:
                                        bus_out = BUS_SETDATA(0);
                        	
                                        scsi_bus_update(&scsi->bus, bus_out | BUS_ACK);
                                        scsi_bus_update(&scsi->bus, bus_out & ~BUS_ACK);
                                        break;
                                        
                                        default:
                                        fatal(" Bad new phase %x\n", bus_state);
                                }
                                break;
                        }
                }
                break;

                case SCSI_STATE_END_PHASE:
                /*Wait for SCSI command to move to next phase*/
                for (c = 0; c < 20; c++)
                {
                        int bus_state = scsi_bus_read(&scsi->bus);

                        if (!(bus_state & BUS_BSY))
                        {
                                pclog("END_PHASE - dropped BSY waiting\n");
//                                if (scsi->ccb.req_sense_len == CCB_SENSE_NONE)
//                                        fatal("No sense data\n");
                                scsi->scsi_state = SCSI_STATE_IDLE;
                                break;
                        }

                        if (bus_state & BUS_REQ)
                                fatal("END_PHASE - unexpected REQ\n");
                }
                break;
                
                case SCSI_STATE_READ_DATA:
//pclog("READ_DATA %i,%i %i\n", scsi->cdb.data_idx,scsi->cdb.data_len, scsi->cdb.scatter_gather);
                while (scsi->cdb.data_idx < scsi->cdb.data_len && scsi->scsi_state == SCSI_STATE_READ_DATA && bytes_transferred < MAX_BYTES_TRANSFERRED_PER_POLL)
                {
                        int d;
                        
                        for (d = 0; d < 20; d++)
                        {
                                int bus_state = scsi_bus_read(&scsi->bus);

                                if (!(bus_state & BUS_BSY))
                                        fatal("READ_DATA - dropped BSY waiting\n");

                                if ((bus_state & (BUS_IO | BUS_CD | BUS_MSG)) != BUS_IO)
                                {
                                        pclog("READ_DATA - changed phase\n");
                                        scsi->scsi_state = SCSI_STATE_NEXT_PHASE;
                                        break;
                                }
                                
                                if (bus_state & BUS_REQ)
                                {
                                        uint8_t data = BUS_GETDATA(bus_state);
                                        int bus_out = 0;
                                
                                        if (scsi->cdb.data_pointer == -1)
                                                scsi->int_buffer[scsi->cdb.data_idx] = data;
                                        else
                                                mem_writeb_phys(scsi->cdb.data_pointer + scsi->cdb.data_idx, data);
                                        scsi->cdb.data_idx++;
                                        scsi->cdb.bytes_transferred++;
                                
//                                        pclog("Read data %02x %i %06x\n", data, scsi->cdb.data_idx, scsi->cdb.data_pointer + scsi->cdb.data_idx);
                                        scsi_bus_update(&scsi->bus, bus_out | BUS_ACK);
                                        scsi_bus_update(&scsi->bus, bus_out & ~BUS_ACK);
                                        break;
                                }
                        }
                        
                        bytes_transferred++;
                }
                if (scsi->cdb.data_idx == scsi->cdb.data_len)
                {
                        if (scsi->cdb.scatter_gather)
                        {
                                if (scsi->cdb.data_seg_list_idx >= scsi->cdb.data_seg_list_len)
                                        scsi->scsi_state = SCSI_STATE_NEXT_PHASE;
                                else
                                {
                                        if (scsi->mb_format == MB_FORMAT_4)
                                        {
                                                scsi->cdb.data_len = mem_readb_phys(scsi->cdb.data_seg_list_pointer + scsi->cdb.data_seg_list_idx + 2) |
                                                                        (mem_readb_phys(scsi->cdb.data_seg_list_pointer + scsi->cdb.data_seg_list_idx + 1) << 8) |
                                                                        (mem_readb_phys(scsi->cdb.data_seg_list_pointer + scsi->cdb.data_seg_list_idx) << 16);
                                                scsi->cdb.data_pointer = mem_readb_phys(scsi->cdb.data_seg_list_pointer + scsi->cdb.data_seg_list_idx + 5) |
                                                                        (mem_readb_phys(scsi->cdb.data_seg_list_pointer + scsi->cdb.data_seg_list_idx + 4) << 8) |
                                                                        (mem_readb_phys(scsi->cdb.data_seg_list_pointer + scsi->cdb.data_seg_list_idx + 3) << 16);
                                                scsi->cdb.data_idx = 0;
                                                scsi->cdb.data_seg_list_idx += 6;
                                        }
                                        else
                                        {
                                                scsi->cdb.data_len = mem_readl_phys(scsi->cdb.data_seg_list_pointer + scsi->cdb.data_seg_list_idx);
                                                scsi->cdb.data_pointer = mem_readl_phys(scsi->cdb.data_seg_list_pointer + scsi->cdb.data_seg_list_idx + 4);
                                                scsi->cdb.data_idx = 0;
                                                scsi->cdb.data_seg_list_idx += 8;
//                                                output = 3;
                                        }
                                        pclog("Got scatter gather %08x %08x\n", scsi->cdb.data_pointer, scsi->cdb.data_len);
                                }
                        }
                        else
                                scsi->scsi_state = SCSI_STATE_NEXT_PHASE;
                }
                break;
                
                case SCSI_STATE_WRITE_DATA:
                while (scsi->cdb.data_idx < scsi->cdb.data_len && bytes_transferred < MAX_BYTES_TRANSFERRED_PER_POLL)
                {
                        int d;
                        
                        for (d = 0; d < 20; d++)
                        {
                                int bus_state = scsi_bus_read(&scsi->bus);

                                if (!(bus_state & BUS_BSY))
                                        fatal("WRITE_DATA - dropped BSY waiting\n");

                                if ((bus_state & (BUS_IO | BUS_CD | BUS_MSG)) != 0)
                                {
                                        pclog("WRITE_DATA - changed phase\n");
                                        scsi->scsi_state = SCSI_STATE_NEXT_PHASE;
                                        break;
                                }
                                
                                if (bus_state & BUS_REQ)
                                {
                                        uint8_t data;// = BUS_GETDATA(bus_state);
                                        int bus_out;
                                                                        
                                        if (scsi->cdb.data_pointer == -1)
                                                data = scsi->int_buffer[scsi->cdb.data_idx];
                                        else
                                                data = mem_readb_phys(scsi->cdb.data_pointer + scsi->cdb.data_idx);
                                        scsi->cdb.data_idx++;
                                        scsi->cdb.bytes_transferred++;
                                                                        
//                                        pclog("Write data %02x %i\n", data, scsi->cdb.data_idx);
                                        bus_out = BUS_SETDATA(data);
                                        scsi_bus_update(&scsi->bus, bus_out | BUS_ACK);
                                        scsi_bus_update(&scsi->bus, bus_out & ~BUS_ACK);
                                        break;
                                }
                        }
                        
                        bytes_transferred++;
                }
                if (scsi->cdb.data_idx == scsi->cdb.data_len)
                {
                        if (scsi->cdb.scatter_gather)
                        {
                                if (scsi->cdb.data_seg_list_idx >= scsi->cdb.data_seg_list_len)
                                        scsi->scsi_state = SCSI_STATE_NEXT_PHASE;
                                else
                                {
                                        if (scsi->mb_format == MB_FORMAT_4)
                                        {
                                                scsi->cdb.data_len = mem_readb_phys(scsi->cdb.data_seg_list_pointer + scsi->cdb.data_seg_list_idx + 2) |
                                                                        (mem_readb_phys(scsi->cdb.data_seg_list_pointer + scsi->cdb.data_seg_list_idx + 1) << 8) |
                                                                        (mem_readb_phys(scsi->cdb.data_seg_list_pointer + scsi->cdb.data_seg_list_idx) << 16);
                                                scsi->cdb.data_pointer = mem_readb_phys(scsi->cdb.data_seg_list_pointer + scsi->cdb.data_seg_list_idx + 5) |
                                                                        (mem_readb_phys(scsi->cdb.data_seg_list_pointer + scsi->cdb.data_seg_list_idx + 4) << 8) |
                                                                        (mem_readb_phys(scsi->cdb.data_seg_list_pointer + scsi->cdb.data_seg_list_idx + 3) << 16);
                                                scsi->cdb.data_idx = 0;
                                                scsi->cdb.data_seg_list_idx += 6;
                                        }
                                        else
                                        {
                                                scsi->cdb.data_len = mem_readl_phys(scsi->cdb.data_seg_list_pointer + scsi->cdb.data_seg_list_idx);
                                                scsi->cdb.data_pointer = mem_readl_phys(scsi->cdb.data_seg_list_pointer + scsi->cdb.data_seg_list_idx + 4);
                                                scsi->cdb.data_idx = 0;
                                                scsi->cdb.data_seg_list_idx += 8;
                                        }
                                        pclog("Got scatter gather %08x %08x\n", scsi->cdb.data_pointer, scsi->cdb.data_len);
                                }
                        }
                        else
                                scsi->scsi_state = SCSI_STATE_NEXT_PHASE;
                }
                break;

                case SCSI_STATE_READ_STATUS:
                for (c = 0; c < 20; c++)
                {
                        int bus_state = scsi_bus_read(&scsi->bus);

                        if (!(bus_state & BUS_BSY))
                                fatal("READ_STATUS - dropped BSY waiting\n");

                        if ((bus_state & (BUS_IO | BUS_CD | BUS_MSG)) != (BUS_CD | BUS_IO))
                        {
                                pclog("READ_STATUS - changed phase\n");
                                scsi->scsi_state = SCSI_STATE_NEXT_PHASE;
                                break;
                        }
                                
                        if (bus_state & BUS_REQ)
                        {
                                uint8_t status = BUS_GETDATA(bus_state);                
                                int bus_out = 0;

//                                pclog("Read status %02x\n", status);
                                if (scsi->ccb.from_mailbox)
                                        mem_writeb_phys(scsi->ccb.addr + 0xf, status);
                                scsi->cdb.last_status = status;
                                
                                scsi_bus_update(&scsi->bus, bus_out | BUS_ACK);
                                scsi_bus_update(&scsi->bus, bus_out & ~BUS_ACK);                        
                                
                                scsi->scsi_state = SCSI_STATE_NEXT_PHASE;
                                break;
                        }
                }
                break;
                
                case SCSI_STATE_READ_MESSAGE:
                for (c = 0; c < 20; c++)
                {
                        int bus_state = scsi_bus_read(&scsi->bus);

                        if (!(bus_state & BUS_BSY))
                                fatal("READ_MESSAGE - dropped BSY waiting\n");

                        if ((bus_state & (BUS_IO | BUS_CD | BUS_MSG)) != (BUS_CD | BUS_IO | BUS_MSG))
                        {
                                pclog("READ_MESSAGE - changed phase\n");
                                scsi->scsi_state = SCSI_STATE_NEXT_PHASE;
                                break;
                        }
                                
                        if (bus_state & BUS_REQ)
                        {
                                uint8_t msg = BUS_GETDATA(bus_state);
                                int bus_out = 0;
                                
//                                pclog("Read message %02x\n", msg);
                                scsi_bus_update(&scsi->bus, bus_out | BUS_ACK);
                                scsi_bus_update(&scsi->bus, bus_out & ~BUS_ACK);     
                                
                                switch (msg)
                                {
                                        case MSG_COMMAND_COMPLETE:
                                        scsi->scsi_state = SCSI_STATE_END_PHASE;
                                        break;
                                        
                                        default:
                                        fatal("READ_MESSAGE - unknown message %02x\n", msg);
                                }
                                break;
                        }
                }
                break;
                
                default:
                fatal("Unknown SCSI_state %d\n", scsi->scsi_state);
        }
}

static void aha154x_callback(void *p)
{
        aha154x_t *scsi = (aha154x_t *)p;

        timer_advance_u64(&scsi->timer, TIMER_USEC * POLL_TIME_US);

//        pclog("poll %i\n", scsi->cmd_state);
        process_cmd(scsi);
        process_ccb(scsi);
        process_scsi(scsi);
}

static uint8_t aha1542c_read(uint32_t addr, void *p)
{
        aha154x_t *scsi = (aha154x_t *)p;
        uint8_t temp = 0xff;
        
        addr &= 0x3fff;
        

        if (addr == 0x3f7e) /*DIP switches*/
                temp = scsi->dipsw;
        else if (addr == 0x3f7f) /*Negation of DIP switches, to satisfy BIOS checksum!*/
                temp = (scsi->dipsw ^ 0xff) + 1;
        else if (addr >= 0x3f80 && (scsi->shadow & 2))
                temp = scsi->shadow_ram[addr];
        else
                temp = scsi->bios_rom.rom[addr | scsi->bios_bank];

        return temp;
}
static void aha1542c_write(uint32_t addr, uint8_t val, void *p)
{
        aha154x_t *scsi = (aha154x_t *)p;
        
        addr &= 0x3fff;
        
        if (addr >= 0x3f80 && (scsi->shadow & 1))
                scsi->shadow_ram[addr] = val;
}

static void aha1542c_eeprom_load(aha154x_t *scsi, char *fn)
{
        FILE *f;

        strcpy(scsi->fn, fn);
        f = nvrfopen(scsi->fn, "rb");
        if (!f)
        {
                memset(scsi->eeprom, 0, 35);
                return;
        }
        fread(scsi->eeprom, 1, 32, f);
        fclose(f);
}

static void aha1542c_eeprom_save(aha154x_t *scsi)
{
        FILE *f = nvrfopen(scsi->fn, "wb");
        
        if (!f)
                return;
        fwrite(scsi->eeprom, 1, 32, f);
        fclose(f);
}

static uint16_t port_sw_mapping[8] =
{
        0x330, 0x334, 0x230, 0x234, 0x130, 0x134, -1, -1
};

static void *scsi_aha1542c_init(char *bios_fn)
{
        aha154x_t *scsi = malloc(sizeof(aha154x_t));
        uint32_t addr;
        int c;
        memset(scsi, 0, sizeof(aha154x_t));

        rom_init(&scsi->bios_rom, "adaptec_aha1542c_bios_534201-00.bin", 0xd8000, 0x8000, 0x7fff, 0, MEM_MAPPING_EXTERNAL);
        mem_mapping_disable(&scsi->bios_rom.mapping);

        addr = device_get_config_int("bios_addr");
        mem_mapping_add(&scsi->mapping, addr, 0x4000, 
                    aha1542c_read, NULL, NULL,
                    aha1542c_write, NULL, NULL,
                    scsi->bios_rom.rom, 0, scsi);
        
        scsi->status = 0;
        scsi->cmd_state = CMD_STATE_IDLE;
        scsi->ccb_state = CCB_STATE_IDLE;
        scsi->scsi_state = SCSI_STATE_IDLE;
        
        timer_add(&scsi->timer, aha154x_callback, scsi, 1);
        
        addr = device_get_config_int("addr");
        io_sethandler(addr, 0x0004,
                        aha154x_in, NULL, NULL,
                        aha154x_out, NULL, NULL,
                        scsi);
        for (c = 0; c < 8; c++)
        {
                if (port_sw_mapping[c] == addr)
                {
                        scsi->dipsw = c;
                        break;
                }
        }
                        
        scsi_bus_init(&scsi->bus);

        scsi->type = SCSI_AHA1542C;
        
        aha1542c_eeprom_load(scsi, "aha1542c.nvr");

        scsi->host_id = scsi->eeprom[0] & 7;
        scsi->dma = (scsi->eeprom[1] >> 4) & 7;
        scsi->irq = (scsi->eeprom[1] & 7) + 9;
        pclog("Init IRQ=%i DMA=%i ID=%i\n", scsi->irq, scsi->dma, scsi->host_id);
        
        return scsi;
}

static void *scsi_bt545s_init(char *bios_fn)
{
        aha154x_t *scsi = malloc(sizeof(aha154x_t));
        uint32_t addr;
        memset(scsi, 0, sizeof(aha154x_t));

        addr = device_get_config_int("bios_addr");
        rom_init(&scsi->bios_rom, "BusLogic_BT-545S_U15_27128_5002026-4.50.bin", addr, 0x4000, 0x3fff, 0, MEM_MAPPING_EXTERNAL);
        scsi->bios_addr = addr;

        scsi->status = 0;
        scsi->cmd_state = CMD_STATE_IDLE;
        scsi->ccb_state = CCB_STATE_IDLE;
        scsi->scsi_state = SCSI_STATE_IDLE;
        
        timer_add(&scsi->timer, aha154x_callback, scsi, 1);
        
        addr = device_get_config_int("addr");
        io_sethandler(addr, 0x0004,
                        aha154x_in, NULL, NULL,
                        aha154x_out, NULL, NULL,
                        scsi);
                        
        scsi_bus_init(&scsi->bus);

        scsi->type = SCSI_BT545S;

        scsi->host_id = device_get_config_int("host_id");
        scsi->dma = device_get_config_int("dma");
        scsi->irq = device_get_config_int("irq");
                
        return scsi;
}

static void scsi_aha1542c_close(void *p)
{
        aha154x_t *scsi = (aha154x_t *)p;
        
        scsi_bus_close(&scsi->bus);
        
        free(scsi);
}

static int scsi_aha1542c_available()
{
        return rom_present("adaptec_aha1542c_bios_534201-00.bin");
}

static int scsi_bt545s_available()
{
        return rom_present("BusLogic_BT-545S_U15_27128_5002026-4.50.bin");
}

static device_config_t aha1542c_config[] =
{
        {
                .name = "addr",
                .description = "Address",
                .type = CONFIG_SELECTION,
                .selection =
                {
                        {
                                .description = "130",
                                .value = 0x130
                        },
                        {
                                .description = "134",
                                .value = 0x134
                        },
                        {
                                .description = "230",
                                .value = 0x230
                        },
                        {
                                .description = "234",
                                .value = 0x234
                        },
                        {
                                .description = "330",
                                .value = 0x330
                        },
                        {
                                .description = "334",
                                .value = 0x334
                        },
                        {
                                .description = ""
                        }
                },
                .default_int = 0x334
        },
        {
                .name = "bios_addr",
                .description = "BIOS address",
                .type = CONFIG_SELECTION,
                .selection =
                {
                        {
                                .description = "C8000",
                                .value = 0xc8000
                        },
                        {
                                .description = "CC000",
                                .value = 0xcc000
                        },
                        {
                                .description = "D0000",
                                .value = 0xd0000
                        },
                        {
                                .description = "D4000",
                                .value = 0xd4000
                        },
                        {
                                .description = "D8000",
                                .value = 0xd8000
                        },
                        {
                                .description = "DC000",
                                .value = 0xdc000
                        },
                        {
                                .description = ""
                        }
                },
                .default_int = 0xdc000
        },
        {
                .type = -1
        }
};

static device_config_t bt545s_config[] =
{
        {
                .name = "addr",
                .description = "Address",
                .type = CONFIG_SELECTION,
                .selection =
                {
                        {
                                .description = "130",
                                .value = 0x130
                        },
                        {
                                .description = "134",
                                .value = 0x134
                        },
                        {
                                .description = "230",
                                .value = 0x230
                        },
                        {
                                .description = "234",
                                .value = 0x234
                        },
                        {
                                .description = "330",
                                .value = 0x330
                        },
                        {
                                .description = "334",
                                .value = 0x334
                        },
                        {
                                .description = ""
                        }
                },
                .default_int = 0x334
        },
        {
                .name = "bios_addr",
                .description = "BIOS address",
                .type = CONFIG_SELECTION,
                .selection =
                {
                        {
                                .description = "C8000",
                                .value = 0xc8000
                        },
                        {
                                .description = "D8000",
                                .value = 0xd8000
                        },
                        {
                                .description = "DC000",
                                .value = 0xdc000
                        },
                        {
                                .description = ""
                        }
                },
                .default_int = 0xdc000
        },
        {
                .name = "irq",
                .description = "IRQ",
                .type = CONFIG_SELECTION,
                .selection =
                {
                        {
                                .description = "IRQ 9",
                                .value = 9
                        },
                        {
                                .description = "IRQ 10",
                                .value = 10
                        },
                        {
                                .description = "IRQ 11",
                                .value = 11
                        },
                        {
                                .description = "IRQ 12",
                                .value = 12
                        },
                        {
                                .description = "IRQ 14",
                                .value = 14
                        },
                        {
                                .description = "IRQ 15",
                                .value = 15
                        },
                        {
                                .description = ""
                        }
                },
                .default_int = 10
        },
        {
                .name = "dma",
                .description = "DMA",
                .type = CONFIG_SELECTION,
                .selection =
                {
                        {
                                .description = "DMA 5",
                                .value = 5
                        },
                        {
                                .description = "DMA 6",
                                .value = 6
                        },
                        {
                                .description = "DMA 7",
                                .value = 7
                        },
                        {
                                .description = ""
                        }
                },
                .default_int = 7
        },
        {
                .name = "host_id",
                .description = "Host ID",
                .type = CONFIG_SELECTION,
                .selection =
                {
                        {
                                .description = "ID 0",
                                .value = 0
                        },
                        {
                                .description = "ID 1",
                                .value = 1
                        },
                        {
                                .description = "ID 2",
                                .value = 2
                        },
                        {
                                .description = "ID 3",
                                .value = 3
                        },
                        {
                                .description = "ID 4",
                                .value = 4
                        },
                        {
                                .description = "ID 5",
                                .value = 5
                        },
                        {
                                .description = "ID 6",
                                .value = 6
                        },
                        {
                                .description = "ID 7",
                                .value = 7
                        },
                        {
                                .description = ""
                        }
                },
                .default_int = 7
        },
        {
                .type = -1
        }
};

device_t scsi_aha1542c_device =
{
        "Adaptec AHA-1542C (SCSI)",
        DEVICE_AT,
        scsi_aha1542c_init,
        scsi_aha1542c_close,
        scsi_aha1542c_available,
        NULL,
        NULL,
        NULL,
        aha1542c_config
};
device_t scsi_bt545s_device =
{
        "BusLogic BT-545S (SCSI)",
        DEVICE_AT,
        scsi_bt545s_init,
        scsi_aha1542c_close,
        scsi_bt545s_available,
        NULL,
        NULL,
        NULL,
        bt545s_config
};
