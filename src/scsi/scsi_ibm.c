#include <stdlib.h>
#include <stdio.h>
#include "ibm.h"

#include "device.h"
#include "dma.h"
#include "io.h"
#include "mca.h"
#include "mem.h"
#include "pic.h"
#include "rom.h"
#include "scsi.h"
#include "timer.h"

#include "scsi_ibm.h"

#define SCSI_TIME (20)
#define POLL_TIME (TIMER_USEC * 10)
#define MAX_BYTES_TRANSFERRED_PER_POLL 50

extern char ide_fn[4][512];

typedef enum
{
        SCB_STATE_IDLE,
        SCB_STATE_START,
        SCB_STATE_WAIT,
        SCB_STATE_COMPLETE
} sbc_state_t;

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

typedef struct scb_t
{
        uint16_t command;
        uint16_t enable;
        uint32_t lba_addr;
        uint32_t sys_buf_addr;
        uint32_t sys_buf_byte_count;
        uint32_t term_status_block_addr;
        uint32_t scb_chain_addr;
        uint16_t block_count;
        uint16_t block_length;
} scb_t;

typedef struct scsi_ibm_t
{
        rom_t bios_rom;
        
        uint8_t pos_regs[8];

        uint8_t basic_ctrl;
        uint32_t command;
        
        uint8_t attention;
        uint8_t attention_pending;
        int attention_waiting;
        
        uint8_t cir[4];
        uint8_t cir_pending[4];

        uint8_t irq_status;
        
        uint32_t scb_addr;
        
        uint8_t status;

        int in_invalid;
        int in_reset;
        sbc_state_t scb_state;
                
        scb_t scb;
        int scb_id;
        
        int cmd_status;
        int cir_status;

        uint8_t pacing;
        
        uint8_t buf[0x600];

        uint8_t int_buffer[512];

        struct
        {
                int phys_id;
                int lun_id;
        } dev_id[16];
        
        struct
        {
                int idx, len;
                uint8_t data[256];

                int data_idx, data_len;
                uint32_t data_pointer;

                uint8_t last_status;

                int bytes_transferred;

                int target_id;
        } cdb;
        
        int irq_requests[16];

        int cmd_timer;
        pc_timer_t callback_timer;

        scsi_state_t scsi_state;
	scsi_bus_t bus;
} scsi_ibm_t;

#define CTRL_RESET   (1 << 7)
#define CTRL_DMA_ENA (1 << 1)
#define CTRL_IRQ_ENA (1 << 0)

#define STATUS_CMD_FULL  (1 << 3)
#define STATUS_CMD_EMPTY (1 << 2)
#define STATUS_IRQ       (1 << 1)
#define STATUS_BUSY      (1 << 0)

#define ENABLE_PT (1 << 12)

#define CMD_MASK                 0xff3f
#define CMD_ASSIGN               0x040e
#define CMD_DEVICE_INQUIRY       0x1c0b
#define CMD_DMA_PACING_CONTROL   0x040d
#define CMD_FEATURE_CONTROL      0x040c
#define CMD_GET_POS_INFO         0x1c0a
#define CMD_INVALID_412          0x0412
#define CMD_GET_COMPLETE_STATUS  0x1c07
#define CMD_READ_DATA            0x1c01
#define CMD_READ_DEVICE_CAPACITY 0x1c09
#define CMD_REQUEST_SENSE        0x1c08
#define CMD_RESET                0x0400
#define CMD_SEND_OTHER_SCSI      0x241f
#define CMD_UNKNOWN_1c10         0x1c10
#define CMD_UNKNOWN_1c11         0x1c11
#define CMD_WRITE_DATA           0x1c02
#define CMD_VERIFY               0x1c03

#define IRQ_TYPE_NONE               0x0
#define IRQ_TYPE_SCB_COMPLETE       0x1
#define IRQ_TYPE_SCB_COMPLETE_RETRY 0x5
#define IRQ_TYPE_ADAPTER_HW_FAILURE 0x7
#define IRQ_TYPE_IMM_CMD_COMPLETE   0xa
#define IRQ_TYPE_COMMAND_FAIL       0xc
#define IRQ_TYPE_COMMAND_ERROR      0xe
#define IRQ_TYPE_SW_SEQ_ERROR       0xf
#define IRQ_TYPE_RESET_COMPLETE     0x10

static void scsi_rethink_irqs(scsi_ibm_t *scsi)
{
        int irq_pending = 0;
        int c;
        
/*        pclog("scsi_rethink_irqs: basic_ctrl=%02x status=%02x\n", scsi->basic_ctrl, scsi->irq_status);
        pclog("   ");
        for (c = 0; c < 16; c++)
                pclog(" %02x", scsi->irq_requests[c]);
        pclog("\n");*/
                
        if (!scsi->irq_status)
        {
                for (c = 0; c < 16; c++)
                {
                        if (scsi->irq_requests[c] != IRQ_TYPE_NONE)
                        {
//                                pclog("  Found IRQ on %i\n", c);
                                /*Found IRQ*/
                                scsi->irq_status = c | (scsi->irq_requests[c] << 4);
                                scsi->status |= STATUS_IRQ;
                                irq_pending = 1;
                                break;
                        }
                }
        }
        else
                irq_pending = 1;

        if (scsi->basic_ctrl & CTRL_IRQ_ENA)
        {
                if (irq_pending)
                {
                        picintlevel(1 << 14);
//                        pclog("  SCSI assert INT\n");
                }
                else
                {
                        /*No IRQs pending, clear IRQ state*/
                        scsi->irq_status = 0;
                        scsi->status &= ~STATUS_IRQ;
                        picintc(1 << 14);
//                        pclog("  SCSI clear INT - no IRQ\n");
                }
        }
        else
        {
                picintc(1 << 14);
//                pclog("  SCSI clear INT - IRQs disabled\n");
        }
}

static inline void scsi_ibm_set_irq(scsi_ibm_t *scsi, int id, int type)
{
//        pclog("scsi_ibm_set_irq id=%i type=%x %02x\n", id, type, scsi->irq_status);
        scsi->irq_requests[id] = type;
        if (!scsi->irq_status) /*Don't change IRQ status if one is currently being processed*/
                scsi_rethink_irqs(scsi);
}
static inline void scsi_clear_irq(scsi_ibm_t *scsi, int id)
{
        scsi->irq_requests[id] = IRQ_TYPE_NONE;
        scsi_rethink_irqs(scsi);
}

static uint8_t scsi_read(uint16_t port, void *p)
{
        scsi_ibm_t *scsi = (scsi_ibm_t *)p;
        uint8_t temp = 0xff;
        
        switch (port & 7)
        {
                case 0: case 1: case 2: case 3: /*Command Interface Register*/
                temp = scsi->cir_pending[port & 3];
                break;

                case 4: /*Attention Register*/
                temp = scsi->attention_pending;
                break;
                case 5: /*Basic Control Register*/
                temp = scsi->basic_ctrl;
                break;
                case 6: /*IRQ status*/
                temp = scsi->irq_status;
                break;
                case 7: /*Status*/
                temp = scsi->status;
                if (scsi->cir_status == 0)
                        temp |= STATUS_CMD_EMPTY;
                if (scsi->cir_status == 3)
                        temp |= STATUS_CMD_FULL;
                break;
        }
        
//        pclog("scsi_read: port=%04x val=%02x %04x(%05x):%04x %i %02x\n", port, temp,CS,cs,cpu_state.pc, 0/*scsi->callback*/, BH);
        return temp;        
}

static void scsi_write(uint16_t port, uint8_t val, void *p)
{
        scsi_ibm_t *scsi = (scsi_ibm_t *)p;

//        pclog("scsi_write: port=%04x val=%02x %04x:%04x\n", port, val,CS,cpu_state.pc);
        
        switch (port & 7)
        {
                case 0: case 1: case 2: case 3: /*Command Interface Register*/
                scsi->cir_pending[port & 3] = val;
                if (port & 2)
                        scsi->cir_status |= 2;
                else
                        scsi->cir_status |= 1;
                break;

                case 4: /*Attention Register*/
                scsi->attention_pending = val;
                scsi->attention_waiting = 2;
                scsi->status |= STATUS_BUSY;
                break;
                
                case 5: /*Basic Control Register*/
                if ((scsi->basic_ctrl & CTRL_RESET) && !(val & CTRL_RESET))
                {
//                        pclog("SCSI reset\n");
                        scsi->in_reset = 1;
                        scsi->cmd_timer = SCSI_TIME*2;
                        scsi->status |= STATUS_BUSY;
                }
                scsi->basic_ctrl = val;
                scsi_rethink_irqs(scsi);
                break;

//                default:
//                fatal("scsi_write port=%04x val=%02x\n", port, val);
        }
}

static uint16_t scsi_readw(uint16_t port, void *p)
{
        scsi_ibm_t *scsi = (scsi_ibm_t *)p;
        uint16_t temp = 0xffff;

        switch (port & 7)
        {
                case 0: /*Command Interface Register*/
                temp = scsi->cir_pending[0] | (scsi->cir_pending[1] << 8);
                break;
                case 2: /*Command Interface Register*/
                temp = scsi->cir_pending[2] | (scsi->cir_pending[3] << 8);
                break;

//                default:
//                fatal("scsi_readw port=%04x\n", port);
        }
        
//        pclog("scsi_readw: port=%04x val=%04x\n", port, temp);
        return temp;        
}

static void scsi_writew(uint16_t port, uint16_t val, void *p)
{
        scsi_ibm_t *scsi = (scsi_ibm_t *)p;

        switch (port & 7)
        {
                case 0: /*Command Interface Register*/
                scsi->cir_pending[0] = val & 0xff;
                scsi->cir_pending[1] = val >> 8;
                scsi->cir_status |= 1;
                break;
                case 2: /*Command Interface Register*/
                scsi->cir_pending[2] = val & 0xff;
                scsi->cir_pending[3] = val >> 8;
                scsi->cir_status |= 2;
                break;
                
//                default:
//                fatal("scsi_writew port=%04x val=%04x\n", port, val);
        }

//        pclog("scsi_writew: port=%04x val=%04x\n", port, val);
}

static void sg_fetch_next(scsi_ibm_t *scsi)
{
        if (scsi->scb.sys_buf_byte_count > 0)
        {
                scsi->cdb.data_idx = 0;
                scsi->cdb.data_pointer = mem_readl_phys(scsi->scb.sys_buf_addr);
                scsi->cdb.data_len = mem_readl_phys(scsi->scb.sys_buf_addr + 4);
//                pclog("SG list: from %08x  addr=%08x len=%08x\n", scsi->scb.sys_buf_addr, scsi->cdb.data_pointer, scsi->cdb.data_len);
                scsi->scb.sys_buf_addr += 8;
                scsi->scb.sys_buf_byte_count -= 8;
        }
}

static void process_ibm_cmd(void *p)
{
        scsi_ibm_t *scsi = (scsi_ibm_t *)p;
        scb_t *scb = &scsi->scb;
        int c;
        int old_scb_state;

//        pclog("scsi_callback: in_reset=%i command=%x\n", scsi->in_reset, scsi->command);
        if (scsi->in_reset)
        {
                scsi->status &= ~STATUS_BUSY;
                
                scsi->irq_status = 0;
                for (c = 0; c < 16; c++)
                        scsi_clear_irq(scsi, c);
                if (scsi->in_reset == 1)
                {
                        scsi->basic_ctrl |= CTRL_IRQ_ENA;
                        scsi_ibm_set_irq(scsi, 0xf, IRQ_TYPE_RESET_COMPLETE);
                }
                else
                        scsi_ibm_set_irq(scsi, 0xf, IRQ_TYPE_RESET_COMPLETE);

                /*Reset device mappings*/
                for (c = 0; c < 7; c++)
                {
                        scsi->dev_id[c].phys_id = c;
                        scsi->dev_id[c].lun_id = 0;
                }
                for (; c < 15; c++)
                        scsi->dev_id[c].phys_id = -1;

                scsi->in_reset = 0;
//                pclog("scsi reset complete\n");
                return;
        }
        if (scsi->in_invalid)
        {
                scsi_ibm_set_irq(scsi, scsi->attention & 0x0f, IRQ_TYPE_COMMAND_ERROR);
                scsi->in_invalid = 0;
                return;
        }
        do
        {
                old_scb_state = scsi->scb_state;
                
//                pclog("scb_state=%i\n", scsi->scb_state);
                switch (scsi->scb_state)
                {
                        case SCB_STATE_IDLE:
                        break;
                
                        case SCB_STATE_START:
                        if (scsi->dev_id[scsi->scb_id].phys_id == -1)
                        {
                                scsi_ibm_set_irq(scsi, scsi->scb_id, IRQ_TYPE_COMMAND_FAIL);
                                scsi->scb_state = SCB_STATE_IDLE;
                                mem_writew_phys(scb->term_status_block_addr + 0x7*2, (0xe << 8) | 0);
                                mem_writew_phys(scb->term_status_block_addr + 0x8*2, (0xa << 8) | 0);
                                break;
                        }

                        scb->command = mem_readw_phys(scsi->scb_addr);
                        scb->enable = mem_readw_phys(scsi->scb_addr + 0x02);
                        scb->lba_addr = mem_readl_phys(scsi->scb_addr + 0x04);
                        scb->sys_buf_addr = mem_readl_phys(scsi->scb_addr + 0x08);
                        scb->sys_buf_byte_count = mem_readl_phys(scsi->scb_addr + 0x0c);
                        scb->term_status_block_addr = mem_readl_phys(scsi->scb_addr + 0x10);
                        scb->scb_chain_addr = mem_readl_phys(scsi->scb_addr + 0x14);
                        scb->block_count = mem_readw_phys(scsi->scb_addr + 0x18);
                        scb->block_length = mem_readw_phys(scsi->scb_addr + 0x1a);
                
/*                        pclog("SCB : \n"
                              "  Command = %04x\n"
                              "  Enable = %04x\n"
                              "  LBA addr = %08x\n"
                              "  System buffer addr = %08x\n"
                              "  System buffer byte count = %08x\n"
                              "  Terminate status block addr = %08x\n"
                              "  SCB chain address = %08x\n"
                              "  Block count = %04x\n"
                              "  Block length = %04x\n",
                                scb->command, scb->enable, scb->lba_addr,
                                scb->sys_buf_addr, scb->sys_buf_byte_count,
                                scb->term_status_block_addr, scb->scb_chain_addr,
                                scb->block_count, scb->block_length);*/
                                
                        if (scb->enable & ENABLE_PT)
                                sg_fetch_next(scsi);
                        else
                        {
                                scsi->cdb.data_idx = 0;
                                scsi->cdb.data_pointer = scb->sys_buf_addr;
                                scsi->cdb.data_len = scb->sys_buf_byte_count;
                        }

                        switch (scb->command & CMD_MASK)
                        {
                                case CMD_GET_COMPLETE_STATUS:
                                mem_writew_phys(scb->sys_buf_addr + 0x0*2, 0x201); /*SCB status*/
                                mem_writew_phys(scb->sys_buf_addr + 0x1*2, 0); /*Retry counts*/
                                mem_writel_phys(scb->sys_buf_addr + 0x2*2, 0); /*Residual byte count*/
                                mem_writel_phys(scb->sys_buf_addr + 0x4*2, 0); /*Scatter/gather list element address*/
                                mem_writew_phys(scb->sys_buf_addr + 0x6*2, 0xc); /*Device dependent status length*/
                                mem_writew_phys(scb->sys_buf_addr + 0x7*2, scsi->cmd_status << 8);
                                mem_writew_phys(scb->sys_buf_addr + 0x8*2, 0); /*Error*/
                                mem_writew_phys(scb->sys_buf_addr + 0x9*2, 0); /*Reserved*/
                                mem_writew_phys(scb->sys_buf_addr + 0xa*2, 0); /*Cache information status*/
                                mem_writel_phys(scb->sys_buf_addr + 0xb*2, scsi->scb_addr);
                                scsi->scb_state = SCB_STATE_COMPLETE;
                                break;

                                case CMD_UNKNOWN_1c10:
                                for (c = 0; c < 0x600 && c < scb->sys_buf_byte_count; c++)
                                        scsi->buf[c] = mem_readb_phys(scb->sys_buf_addr + c);
                                scsi->scb_state = SCB_STATE_COMPLETE;
                                break;
                                case CMD_UNKNOWN_1c11:
                                for (c = 0; c < 0x600 && c < scb->sys_buf_byte_count; c++)
                                        mem_writeb_phys(scb->sys_buf_addr + c, scsi->buf[c]);
                                scsi->scb_state = SCB_STATE_COMPLETE;
                                break;

                                case CMD_GET_POS_INFO:
                                mem_writew_phys(scb->sys_buf_addr + 0x0*2, 0x8eff);
                                mem_writew_phys(scb->sys_buf_addr + 0x1*2, scsi->pos_regs[3] | (scsi->pos_regs[2] << 8));
                                mem_writew_phys(scb->sys_buf_addr + 0x2*2, 0x0e | (scsi->pos_regs[4] << 8));
                                mem_writew_phys(scb->sys_buf_addr + 0x3*2, 1 << 12);
                                mem_writew_phys(scb->sys_buf_addr + 0x4*2, (7 << 8) | 8);
                                mem_writew_phys(scb->sys_buf_addr + 0x5*2, (16 << 8) | scsi->pacing);
                                mem_writew_phys(scb->sys_buf_addr + 0x6*2, (30 << 8) | 1);
                                mem_writew_phys(scb->sys_buf_addr + 0x7*2, 0);
                                mem_writew_phys(scb->sys_buf_addr + 0x8*2, 0);
                                scsi->scb_state = SCB_STATE_COMPLETE;
                                break;

                                case CMD_DEVICE_INQUIRY:
                                scsi->cdb.data[0] = SCSI_INQUIRY;
                                scsi->cdb.data[1] = scsi->dev_id[scsi->scb_id].lun_id << 5; /*LUN*/
                                scsi->cdb.data[2] = 0; /*Page code*/
                                scsi->cdb.data[3] = 0;
                                scsi->cdb.data[4] = scb->sys_buf_byte_count; /*Allocation length*/
                                scsi->cdb.data[5] = 0; /*Control*/
                                scsi->cdb.idx = 0;
                                scsi->cdb.len = 6;
                                scsi->cdb.target_id = scsi->dev_id[scsi->scb_id].phys_id;
                                scsi->cdb.data_pointer = scb->sys_buf_addr;
                                scsi->cdb.data_idx = 0;
                                scsi->cdb.data_len = scb->sys_buf_byte_count;
                                scsi->scsi_state = SCSI_STATE_SELECT;
                                scsi->scb_state = SCB_STATE_WAIT;
                                return;

                                case CMD_SEND_OTHER_SCSI:
//                                pclog("SEND_OTHER_SCSI:");
                                for (c = 0; c < 12; c++)
                                {
                                        scsi->cdb.data[c] = mem_readb_phys(scsi->scb_addr + 0x18 + c);
//                                        pclog(" %02x", scsi->cdb.data[c]);
                                }
//                                pclog("\n");
                                scsi->cdb.data[1] = (scsi->cdb.data[1] & 0x1f) | (scsi->dev_id[scsi->scb_id].lun_id << 5); /*Patch correct LUN into command*/
                                scsi->cdb.idx = 0;
                                scsi->cdb.len = (scb->lba_addr & 0xff) ? (scb->lba_addr & 0xff) : 6;
//                                pclog("SEND_OTHER_SCSI: %04x\n", scb->lba_addr & 0xffff);
                                scsi->cdb.target_id = scsi->dev_id[scsi->scb_id].phys_id;
                                scsi->scsi_state = SCSI_STATE_SELECT;
                                scsi->scb_state = SCB_STATE_WAIT;
                                return;

                                case CMD_READ_DEVICE_CAPACITY:
                                scsi->cdb.data[0] = SCSI_READ_CAPACITY_10;
                                scsi->cdb.data[1] = scsi->dev_id[scsi->scb_id].lun_id << 5; /*LUN*/
                                scsi->cdb.data[2] = 0; /*LBA*/
                                scsi->cdb.data[3] = 0;
                                scsi->cdb.data[4] = 0;
                                scsi->cdb.data[5] = 0;
                                scsi->cdb.data[6] = 0; /*Reserved*/
                                scsi->cdb.data[7] = 0;
                                scsi->cdb.data[8] = 0;
                                scsi->cdb.data[9] = 0; /*Control*/
                                scsi->cdb.idx = 0;
                                scsi->cdb.len = 10;
                                scsi->cdb.target_id = scsi->dev_id[scsi->scb_id].phys_id;
                                scsi->scsi_state = SCSI_STATE_SELECT;
                                scsi->scb_state = SCB_STATE_WAIT;
                                return;

                                case CMD_READ_DATA:
                                scsi->cdb.data[0] = SCSI_READ_10;
                                scsi->cdb.data[1] = scsi->dev_id[scsi->scb_id].lun_id << 5; /*LUN*/
                                scsi->cdb.data[2] = (scb->lba_addr >> 24) & 0xff; /*LBA*/
                                scsi->cdb.data[3] = (scb->lba_addr >> 16) & 0xff;
                                scsi->cdb.data[4] = (scb->lba_addr >> 8) & 0xff;
                                scsi->cdb.data[5] = scb->lba_addr & 0xff;
                                scsi->cdb.data[6] = 0; /*Reserved*/
                                scsi->cdb.data[7] = (scb->block_count >> 8) & 0xff;
                                scsi->cdb.data[8] = scb->block_count & 0xff;
                                scsi->cdb.data[9] = 0; /*Control*/
                                scsi->cdb.idx = 0;
                                scsi->cdb.len = 10;
                                scsi->cdb.target_id = scsi->dev_id[scsi->scb_id].phys_id;
                                scsi->scsi_state = SCSI_STATE_SELECT;
                                scsi->scb_state = SCB_STATE_WAIT;
                                return;
                                
                                case CMD_REQUEST_SENSE:
                                scsi->cdb.data[0] = SCSI_REQUEST_SENSE;
                                scsi->cdb.data[1] = scsi->dev_id[scsi->scb_id].lun_id << 5; /*LUN*/
                                scsi->cdb.data[2] = 0;
                                scsi->cdb.data[3] = 0;
                                scsi->cdb.data[4] = scb->block_count & 0xff; /*Allocation length*/
                                scsi->cdb.data[5] = 0;
                                scsi->cdb.idx = 0;
                                scsi->cdb.len = 6;
                                scsi->cdb.target_id = scsi->dev_id[scsi->scb_id].phys_id;
                                scsi->scsi_state = SCSI_STATE_SELECT;
                                scsi->scb_state = SCB_STATE_WAIT;
                                return;

                                case CMD_WRITE_DATA:
                                scsi->cdb.data[0] = SCSI_WRITE_10;
                                scsi->cdb.data[1] = scsi->dev_id[scsi->scb_id].lun_id << 5; /*LUN*/
                                scsi->cdb.data[2] = (scb->lba_addr >> 24) & 0xff; /*LBA*/
                                scsi->cdb.data[3] = (scb->lba_addr >> 16) & 0xff;
                                scsi->cdb.data[4] = (scb->lba_addr >> 8) & 0xff;
                                scsi->cdb.data[5] = scb->lba_addr & 0xff;
                                scsi->cdb.data[6] = 0; /*Reserved*/
                                scsi->cdb.data[7] = (scb->block_count >> 8) & 0xff;
                                scsi->cdb.data[8] = scb->block_count & 0xff;
                                scsi->cdb.data[9] = 0; /*Control*/
                                scsi->cdb.idx = 0;
                                scsi->cdb.len = 10;
                                scsi->cdb.target_id = scsi->dev_id[scsi->scb_id].phys_id;
                                scsi->scsi_state = SCSI_STATE_SELECT;
                                scsi->scb_state = SCB_STATE_WAIT;
                                return;

                                case CMD_VERIFY:
                                scsi->cdb.data[0] = SCSI_VERIFY_10;
                                scsi->cdb.data[1] = scsi->dev_id[scsi->scb_id].lun_id << 5; /*LUN*/
                                scsi->cdb.data[2] = (scb->lba_addr >> 24) & 0xff; /*LBA*/
                                scsi->cdb.data[3] = (scb->lba_addr >> 16) & 0xff;
                                scsi->cdb.data[4] = (scb->lba_addr >> 8) & 0xff;
                                scsi->cdb.data[5] = scb->lba_addr & 0xff;
                                scsi->cdb.data[6] = 0; /*Reserved*/
                                scsi->cdb.data[7] = (scb->block_count >> 8) & 0xff;
                                scsi->cdb.data[8] = scb->block_count & 0xff;
                                scsi->cdb.data[9] = 0; /*Control*/
                                scsi->cdb.idx = 0;
                                scsi->cdb.len = 10;
                                scsi->cdb.target_id = scsi->dev_id[scsi->scb_id].phys_id;
                                scsi->cdb.data_len = 0;
                                scsi->scsi_state = SCSI_STATE_SELECT;
                                scsi->scb_state = SCB_STATE_WAIT;
                                return;

#ifndef RELEASE_BUILD
                                default:
                                fatal("SCB command %04x\n", scb->command);
#endif
                        }
                        break;
                        
                        case SCB_STATE_WAIT:
                        if (scsi->scsi_state == SCSI_STATE_IDLE)
                        {
                                if (scsi->cdb.last_status == STATUS_GOOD)
                                        scsi->scb_state = SCB_STATE_COMPLETE;
                                else if (scsi->cdb.last_status == STATUS_CHECK_CONDITION)
                                {
                                        scsi_ibm_set_irq(scsi, scsi->scb_id, IRQ_TYPE_COMMAND_FAIL);
                                        scsi->scb_state = SCB_STATE_IDLE;
                                        mem_writew_phys(scb->term_status_block_addr + 0x7*2, (0xc << 8) | 2);
                                        mem_writew_phys(scb->term_status_block_addr + 0x8*2, 0x20);
                                        mem_writew_phys(scb->term_status_block_addr + 0xb*2, scsi->scb_addr & 0xffff);
                                        mem_writew_phys(scb->term_status_block_addr + 0xc*2, scsi->scb_addr >> 16);
                                }
#ifndef RELEASE_BUILD
                                else
                                        fatal("SCB unknown result %02x\n", scsi->cdb.last_status);
#endif
                        }
                        else if (scsi->scsi_state == SCSI_STATE_SELECT_FAILED)
                        {
//                                pclog("SCB_STATE_WAIT SELECT_FAILED\n");
                                scsi_ibm_set_irq(scsi, scsi->scb_id, IRQ_TYPE_COMMAND_FAIL);
                                scsi->scb_state = SCB_STATE_IDLE;
                                mem_writew_phys(scb->term_status_block_addr + 0x7*2, (0xc << 8) | 2);
                                mem_writew_phys(scb->term_status_block_addr + 0x8*2, 0x10);
                        }
#ifndef RELEASE_BUILD
                        else
                                fatal("SCB_STATE_WAIT other %i\n", scsi->scsi_state);
#endif
                        break;
                        
                        case SCB_STATE_COMPLETE:
                        if (scb->enable & 1)
                        {
                                scsi->scb_state = SCB_STATE_START;
                                scsi->scb_addr = scb->scb_chain_addr;
//                                pclog("Next SCB - %08x\n", scsi->scb_addr);
                        }
                        else
                        {
                                scsi_ibm_set_irq(scsi, scsi->scb_id, IRQ_TYPE_SCB_COMPLETE);
                                scsi->scb_state = SCB_STATE_IDLE;
                        }
                        break;
                }
        } while (scsi->scb_state != old_scb_state);
}

static void process_immediate_command(scsi_ibm_t *scsi)
{
        switch (scsi->command & CMD_MASK)
        {
                case CMD_ASSIGN:
//                pclog("Assign: adapter dev=%x scsi ID=%i LUN=%i\n", (scsi->command >> 16) & 0xf, (scsi->command >> 20) & 7, (scsi->command >> 24) & 7);
                if (((scsi->command >> 16) & 0xf) == 0xf && ((scsi->command >> 20) & 7) == 7) /*Device 15 always adapter*/
                        scsi_ibm_set_irq(scsi, scsi->attention & 0x0f, IRQ_TYPE_IMM_CMD_COMPLETE);
                else if (((scsi->command >> 16) & 0xf) == 0xf) /*Can not re-assign device 15 (always adapter)*/
                        scsi_ibm_set_irq(scsi, scsi->attention & 0x0f, IRQ_TYPE_COMMAND_FAIL);
                else if (scsi->command & (1 << 23))
                {
                        scsi->dev_id[(scsi->command >> 16) & 0xf].phys_id = -1;
                        scsi_ibm_set_irq(scsi, scsi->attention & 0x0f, IRQ_TYPE_IMM_CMD_COMPLETE);
                }
                else if (((scsi->command >> 20) & 7) == 7) /*Can not assign adapter*/
                        scsi_ibm_set_irq(scsi, scsi->attention & 0x0f, IRQ_TYPE_COMMAND_FAIL);
                else
                {
                        scsi->dev_id[(scsi->command >> 16) & 0xf].phys_id = (scsi->command >> 20) & 7;
                        scsi->dev_id[(scsi->command >> 16) & 0xf].lun_id = (scsi->command >> 24) & 7;
                        scsi_ibm_set_irq(scsi, scsi->attention & 0x0f, IRQ_TYPE_IMM_CMD_COMPLETE);
                }
                break;

                case CMD_DMA_PACING_CONTROL:
                scsi->pacing = scsi->cir[2];
//                pclog("Pacing control: %i\n", scsi->pacing);
                scsi_ibm_set_irq(scsi, scsi->attention & 0x0f, IRQ_TYPE_IMM_CMD_COMPLETE);
                break;

                case CMD_FEATURE_CONTROL:
//                pclog("Feature control: timeout=%is d-rate=%i\n", (scsi->command >> 16) & 0x1fff, scsi->command >> 29);
                scsi_ibm_set_irq(scsi, scsi->attention & 0x0f, IRQ_TYPE_IMM_CMD_COMPLETE);
                break;

                case CMD_INVALID_412:
                scsi_ibm_set_irq(scsi, scsi->attention & 0x0f, IRQ_TYPE_IMM_CMD_COMPLETE);
                break;

                case CMD_RESET:
                if ((scsi->attention & 0xf) == 0xf) /*Adapter reset*/
                {
                        scsi_bus_reset(&scsi->bus);
                        scsi->scb_state = SCB_STATE_IDLE;
                }
                else /*Device reset*/
                {
                }
                scsi_ibm_set_irq(scsi, scsi->attention & 0x0f, IRQ_TYPE_IMM_CMD_COMPLETE);
                break;

                default:
                fatal("scsi_callback: Bad command %02x\n", scsi->command);
        }
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

static void process_scsi(scsi_ibm_t *scsi)
{
        int c;
        int bytes_transferred = 0;

        switch (scsi->scsi_state)
        {
                case SCSI_STATE_IDLE:
                break;

                case SCSI_STATE_SELECT:
                scsi->cdb.last_status = 0;
//                pclog("Select target ID %i\n", scsi->cdb.target_id);
                scsi_bus_update(&scsi->bus, BUS_SEL | BUS_SETDATA(1 << scsi->cdb.target_id));
                if (!(scsi_bus_read(&scsi->bus) & BUS_BSY) || scsi->cdb.target_id > 6)
                {
//                        pclog("STATE_SCSI_SELECT failed to select target %i\n", scsi->cdb.target_id);
                        scsi->scsi_state = SCSI_STATE_SELECT_FAILED;
                        if (!scsi->cmd_timer)
                                scsi->cmd_timer = 1;
                        break;
                }

                scsi_bus_update(&scsi->bus, 0);
                if (!(scsi_bus_read(&scsi->bus) & BUS_BSY))
                {
//                        pclog("STATE_SCSI_SELECT failed to select target %i 2\n", scsi->cdb.target_id);
                        scsi->scsi_state = SCSI_STATE_SELECT_FAILED;
                        if (!scsi->cmd_timer)
                                scsi->cmd_timer = 1;
                        break;
                }

                /*Device should now be selected*/
                if (!wait_for_bus(&scsi->bus, BUS_CD, 1))
                {
//                        pclog("Device failed to request command\n");
                        scsi->scsi_state = SCSI_STATE_SELECT_FAILED;
                        if (!scsi->cmd_timer)
                                scsi->cmd_timer = 1;
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

#ifndef RELEASE_BUILD
                                if (!(bus_state & BUS_BSY))
                                        fatal("SEND_COMMAND - dropped BSY\n");
                                if ((bus_state & (BUS_IO | BUS_CD | BUS_MSG)) != BUS_CD)
                                        fatal("SEND_COMMAND - bus phase incorrect\n");
#endif
                                if (bus_state & BUS_REQ)
                                        break;
                        }
                        if (c == 20)
                        {
//                                pclog("SEND_COMMAND timed out\n");
                                break;
                        }

                        bus_out = BUS_SETDATA(scsi->cdb.data[scsi->cdb.idx]);
//                        pclog("  Command send %02x %i\n", scsi->cdb.data[scsi->cdb.idx], scsi->cdb.len);
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
#ifndef RELEASE_BUILD
                        if (!(bus_state & BUS_BSY))
                                fatal("NEXT_PHASE - dropped BSY waiting\n");
#endif
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

#ifndef RELEASE_BUILD
                                        default:
                                        fatal(" Bad new phase %x\n", bus_state);
#endif
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
//                                pclog("END_PHASE - dropped BSY waiting\n");
//                                if (scsi->ccb.req_sense_len == CCB_SENSE_NONE)
//                                        fatal("No sense data\n");
                                scsi->scsi_state = SCSI_STATE_IDLE;
//                                pclog("End of command\n");
                                if (!scsi->cmd_timer)
                                        scsi->cmd_timer = 1;
                                break;
                        }
#ifndef RELEASE_BUILD
                        if (bus_state & BUS_REQ)
                                fatal("END_PHASE - unexpected REQ\n");
#endif
                }
                break;

                case SCSI_STATE_READ_DATA:
                while (scsi->scsi_state == SCSI_STATE_READ_DATA && bytes_transferred < MAX_BYTES_TRANSFERRED_PER_POLL)
                {
                        int d;

                        for (d = 0; d < 20; d++)
                        {
                                int bus_state = scsi_bus_read(&scsi->bus);
#ifndef RELEASE_BUILD
                                if (!(bus_state & BUS_BSY))
                                        fatal("READ_DATA - dropped BSY waiting\n");
#endif
                                if ((bus_state & (BUS_IO | BUS_CD | BUS_MSG)) != BUS_IO)
                                {
//                                        pclog("READ_DATA - changed phase\n");
                                        scsi->scsi_state = SCSI_STATE_NEXT_PHASE;
                                        break;
                                }

                                if (bus_state & BUS_REQ)
                                {
                                        uint8_t data = BUS_GETDATA(bus_state);
                                        int bus_out = 0;

                                        if (scsi->cdb.data_idx < scsi->cdb.data_len)
                                        {
                                                if (scsi->cdb.data_pointer == -1)
                                                        scsi->int_buffer[scsi->cdb.data_idx] = data;
                                                else
                                                        mem_writeb_phys(scsi->cdb.data_pointer + scsi->cdb.data_idx, data);
                                                scsi->cdb.data_idx++;

                                                if ((scsi->scb.enable & ENABLE_PT) && scsi->cdb.data_idx == scsi->cdb.data_len)
                                                        sg_fetch_next(scsi);
                                        }
                                        scsi->cdb.bytes_transferred++;

//                                        pclog("Read data %02x %i %06x\n", data, scsi->cdb.data_idx, scsi->cdb.data_pointer + scsi->cdb.data_idx);
                                        scsi_bus_update(&scsi->bus, bus_out | BUS_ACK);
                                        scsi_bus_update(&scsi->bus, bus_out & ~BUS_ACK);
                                        break;
                                }
                        }

                        bytes_transferred++;
                }
                break;

                case SCSI_STATE_WRITE_DATA:
                while (scsi->scsi_state == SCSI_STATE_WRITE_DATA && bytes_transferred < MAX_BYTES_TRANSFERRED_PER_POLL)
                {
                        int d;

                        for (d = 0; d < 20; d++)
                        {
                                int bus_state = scsi_bus_read(&scsi->bus);
#ifndef RELEASE_BUILD
                                if (!(bus_state & BUS_BSY))
                                        fatal("WRITE_DATA - dropped BSY waiting\n");
#endif
                                if ((bus_state & (BUS_IO | BUS_CD | BUS_MSG)) != 0)
                                {
//                                        pclog("WRITE_DATA - changed phase\n");
                                        scsi->scsi_state = SCSI_STATE_NEXT_PHASE;
                                        break;
                                }

                                if (bus_state & BUS_REQ)
                                {
                                        uint8_t data;// = BUS_GETDATA(bus_state);
                                        int bus_out;

                                        if (scsi->cdb.data_idx < scsi->cdb.data_len)
                                        {
                                                if (scsi->cdb.data_pointer == -1)
                                                        data = scsi->int_buffer[scsi->cdb.data_idx];
                                                else
                                                        data = mem_readb_phys(scsi->cdb.data_pointer + scsi->cdb.data_idx);

                                                scsi->cdb.data_idx++;
                                                if ((scsi->scb.enable & ENABLE_PT) && scsi->cdb.data_idx == scsi->cdb.data_len)
                                                        sg_fetch_next(scsi);
                                        }
                                        else
                                                data = 0xff;
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
//                if (scsi->cdb.data_idx == scsi->cdb.data_len)
//                        scsi->scsi_state = SCSI_STATE_NEXT_PHASE;
                break;

                case SCSI_STATE_READ_STATUS:
                for (c = 0; c < 20; c++)
                {
                        int bus_state = scsi_bus_read(&scsi->bus);
#ifndef RELEASE_BUILD
                        if (!(bus_state & BUS_BSY))
                                fatal("READ_STATUS - dropped BSY waiting\n");
#endif
                        if ((bus_state & (BUS_IO | BUS_CD | BUS_MSG)) != (BUS_CD | BUS_IO))
                        {
//                                pclog("READ_STATUS - changed phase\n");
                                scsi->scsi_state = SCSI_STATE_NEXT_PHASE;
                                break;
                        }

                        if (bus_state & BUS_REQ)
                        {
                                uint8_t status = BUS_GETDATA(bus_state);
                                int bus_out = 0;

//                                pclog("Read status %02x\n", status);
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
#ifndef RELEASE_BUILD
                        if (!(bus_state & BUS_BSY))
                                fatal("READ_MESSAGE - dropped BSY waiting\n");
#endif
                        if ((bus_state & (BUS_IO | BUS_CD | BUS_MSG)) != (BUS_CD | BUS_IO | BUS_MSG))
                        {
//                                pclog("READ_MESSAGE - changed phase\n");
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
#ifndef RELEASE_BUILD
                                        default:
                                        fatal("READ_MESSAGE - unknown message %02x\n", msg);
#endif
                                }
                                break;
                        }
                }
                break;

#ifndef RELEASE_BUILD
                default:
                fatal("Unknown SCSI_state %d\n", scsi->scsi_state);
#endif
        }
}

static void scsi_callback(void *p)
{
        scsi_ibm_t *scsi = (scsi_ibm_t *)p;

        timer_advance_u64(&scsi->callback_timer, POLL_TIME);

//        pclog("poll %i\n", scsi->cmd_state);
        if (scsi->cmd_timer)
        {
                scsi->cmd_timer--;
                if (!scsi->cmd_timer)
                        process_ibm_cmd(scsi);
        }
        if (scsi->attention_waiting &&
                (scsi->scb_state == SCB_STATE_IDLE || (scsi->attention_pending & 0xf0) == 0xe0))
        {
                scsi->attention_waiting--;
                if (!scsi->attention_waiting)
                {
//                        pclog("Process attention request\n");
                        scsi->attention = scsi->attention_pending;
                        scsi->status &= ~STATUS_BUSY;
                        scsi->cir[0] = scsi->cir_pending[0];
                        scsi->cir[1] = scsi->cir_pending[1];
                        scsi->cir[2] = scsi->cir_pending[2];
                        scsi->cir[3] = scsi->cir_pending[3];
                        scsi->cir_status = 0;

                        switch (scsi->attention >> 4)
                        {
                                case 0x1: /*Immediate command*/
                                scsi->cmd_status = 0xa;
                                scsi->command = scsi->cir[0] | (scsi->cir[1] << 8) | (scsi->cir[2] << 16) | (scsi->cir[3] << 24);
                                switch (scsi->command & CMD_MASK)
                                {
                                        case CMD_ASSIGN:
                                        case CMD_DMA_PACING_CONTROL:
                                        case CMD_FEATURE_CONTROL:
                                        case CMD_INVALID_412:
                                        case CMD_RESET:
                                        process_immediate_command(scsi);
                                        break;

#ifndef RELEASE_BUILD
                                        default:
                                        fatal("Unknown immediate command %08x\n", scsi->command);
#endif
                                }
                                break;

                                case 0x3: case 0x4: case 0xf: /*Start SCB*/
                                scsi->cmd_status = 0x1;
                                scsi->scb_addr = scsi->cir[0] | (scsi->cir[1] << 8) | (scsi->cir[2] << 16) | (scsi->cir[3] << 24);
                                scsi->scb_state = SCB_STATE_START;
                                scsi->scb_id = scsi->attention & 0xf;
                                scsi->cmd_timer = SCSI_TIME*2;
//                                pclog("Start SCB at %08x\n", scsi->scb_addr);
                                break;

                                case 5: /*Invalid*/
                                case 0xa: /*Invalid*/
                                scsi->in_invalid = 1;
                                scsi->cmd_timer = SCSI_TIME*2;
                                break;

                                case 0xe: /*EOI*/
                                scsi->irq_status = 0;
                                scsi_clear_irq(scsi, scsi->attention & 0xf);
                                break;
#ifndef RELEASE_BUILD
                                default:
                                fatal("Attention Register 5 %02x\n", scsi->attention);
#endif
                        }
                }
        }
        process_scsi(scsi);
}

static uint8_t scsi_ibm_mca_read(int port, void *p)
{
        scsi_ibm_t *scsi = (scsi_ibm_t *)p;
        
//        pclog("scsi_mca_read: port=%04x %02x %04x:%04x\n", port, scsi->pos_regs[port & 7], CS,cpu_state.pc);
        
        return scsi->pos_regs[port & 7];
}

static void scsi_ibm_mca_write(int port, uint8_t val, void *p)
{
        scsi_ibm_t *scsi = (scsi_ibm_t *)p;

        if (port < 0x102)
                return;
        
//        pclog("scsi_mca_write: port=%04x val=%02x %04x:%04x\n", port, val, CS, cpu_state.pc);

        io_removehandler((((scsi->pos_regs[2] >> 1) & 7) * 8) + 0x3540, 0x0008, scsi_read, scsi_readw, NULL, scsi_write, scsi_writew, NULL, scsi);
        mem_mapping_disable(&scsi->bios_rom.mapping);
        
        scsi->pos_regs[port & 7] = val;

        if (scsi->pos_regs[2] & 1)
        {
//                pclog(" scsi io = %04x\n", (((scsi->pos_regs[2] >> 1) & 7) * 8) + 0x3540);
                io_sethandler((((scsi->pos_regs[2] >> 1) & 7) * 8) + 0x3540, 0x0008, scsi_read, scsi_readw, NULL, scsi_write, scsi_writew, NULL, scsi);
                if ((scsi->pos_regs[2] & 0xf0) != 0xf0)
                {
                        mem_mapping_enable(&scsi->bios_rom.mapping);
                        mem_mapping_set_addr(&scsi->bios_rom.mapping, ((scsi->pos_regs[2] >> 4) * 0x2000) + 0xc0000, 0x8000);
//                        pclog("SCSI BIOS ROM at %05x %02x\n", ((scsi->pos_regs[2] >> 4) * 0x2000) + 0xc0000, scsi->pos_regs[2]);
                }
        }
}

static void scsi_ibm_reset(void *p)
{
        scsi_ibm_t *scsi = (scsi_ibm_t *)p;

//        pclog("scsi_ibm_reset\n");
        scsi->in_reset = 2;
        scsi->cmd_timer = SCSI_TIME * 50;
        scsi->status = STATUS_BUSY;
        scsi->scsi_state = SCSI_STATE_IDLE;
        scsi->scb_state = SCB_STATE_IDLE;
        scsi->in_invalid = 0;
        scsi->attention_waiting = 0;
        scsi->basic_ctrl = 0;
        scsi_bus_reset(&scsi->bus);
}

static void *scsi_ibm_init()
{
        int c;
        scsi_ibm_t *scsi = malloc(sizeof(scsi_ibm_t));
        memset(scsi, 0, sizeof(scsi_ibm_t));

        rom_init_interleaved(&scsi->bios_rom, "92F2244.U68", "92F2245.U69", 0xc8000, 0x8000, 0x7fff, 0x4000, MEM_MAPPING_EXTERNAL);
        mem_mapping_disable(&scsi->bios_rom.mapping);

        timer_add(&scsi->callback_timer, scsi_callback, scsi, 1);

        mca_add(scsi_ibm_mca_read, scsi_ibm_mca_write, scsi_ibm_reset, scsi);
        
        scsi->pos_regs[0] = 0xff;
        scsi->pos_regs[1] = 0x8e;
        
        scsi->in_reset = 2;
        scsi->cmd_timer = SCSI_TIME * 50;
        scsi->status = STATUS_BUSY;

        scsi_bus_init(&scsi->bus);
        
/*        for (c = 0; c < 7; c++)
        {
                scsi->dev_id[c].phys_id = c;
                scsi->dev_id[c].lun_id = 0;
        }*/
        for (c = 0; c < 15; c++)
                scsi->dev_id[c].phys_id = -1;
        scsi->dev_id[15].phys_id = 7;

        return scsi;
}

static void scsi_ibm_close(void *p)
{
        scsi_ibm_t *scsi = (scsi_ibm_t *)p;
        
        free(scsi);
}

static int scsi_ibm_available()
{
        return rom_present("92F2244.U68") && rom_present("92F2245.U69");
}

device_t scsi_ibm_device =
{
        "IBM SCSI Adapter with Cache (MCA)",
        DEVICE_MCA,
        scsi_ibm_init,
        scsi_ibm_close,
        scsi_ibm_available,
        NULL,
        NULL,
        NULL,
        NULL
};
