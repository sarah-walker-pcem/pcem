#include "ibm.h"
#include "mem.h"
#include "ps2_mca.h"
#include "rom.h"
#include "lpt.h"
#include "mca.h"

//static uint8_t ps2_92, ps2_94, ps2_102, ps2_103, ps2_104, ps2_105, ps2_190;

/*static struct
{
        uint8_t status, int_status;
        uint8_t attention, ctrl;
} ps2_hd;*/

static struct
{
        uint8_t adapter_setup;
        uint8_t option[4];
        uint8_t pos_vga;
        uint8_t setup;
        uint8_t sys_ctrl_port_a;
        uint8_t subaddr_lo, subaddr_hi;
        
        uint8_t io_id;
} ps2;

#define PS2_SETUP_IO  0x80
#define PS2_SETUP_VGA 0x20

#define PS2_ADAPTER_SETUP 0x08

uint8_t ps2_mca_read(uint16_t port, void *p)
{
        uint8_t temp;

        switch (port)
        {
                case 0x91:
                fatal("Read 91 setup=%02x adapter=%02x\n", ps2.setup, ps2.adapter_setup);
                case 0x92:
                temp = ps2.sys_ctrl_port_a;
                break;
                case 0x94:
                temp = ps2.setup;
                break;
                case 0x96:
                temp = ps2.adapter_setup | 0x70;
                break;
                case 0x100:
                if (!(ps2.setup & PS2_SETUP_IO))
                        temp = ps2.io_id;
                else if (!(ps2.setup & PS2_SETUP_VGA))
                        temp = 0xfd;
                else if (ps2.adapter_setup & PS2_ADAPTER_SETUP)
                        temp = mca_read(port);
                else
                        temp = 0xff;
                break;
                case 0x101:
                if (!(ps2.setup & PS2_SETUP_IO))
                        temp = 0xfb;
                else if (!(ps2.setup & PS2_SETUP_VGA))
                        temp = 0xef;
                else if (ps2.adapter_setup & PS2_ADAPTER_SETUP)
                        temp = mca_read(port);
                else
                        temp = 0xff;
                break;
                case 0x102:
                if (!(ps2.setup & PS2_SETUP_IO))
                        temp = ps2.option[0];
                else if (!(ps2.setup & PS2_SETUP_VGA))
                        temp = ps2.pos_vga;
                else if (ps2.adapter_setup & PS2_ADAPTER_SETUP)
                        temp = mca_read(port);
                else
                        temp = 0xff;
                break;
                case 0x103:
                if (!(ps2.setup & PS2_SETUP_IO))
                        temp = ps2.option[1];
                else if ((ps2.setup & PS2_SETUP_VGA) && (ps2.adapter_setup & PS2_ADAPTER_SETUP))
                        temp = mca_read(port);
                else
                        temp = 0xff;
                break;
                case 0x104:
                if (!(ps2.setup & PS2_SETUP_IO))
                        temp = ps2.option[2];
                else if ((ps2.setup & PS2_SETUP_VGA) && (ps2.adapter_setup & PS2_ADAPTER_SETUP))
                        temp = mca_read(port);
                else
                        temp = 0xff;
                break;
                case 0x105:
                if (!(ps2.setup & PS2_SETUP_IO))
                        temp = ps2.option[3];
                else if ((ps2.setup & PS2_SETUP_VGA) && (ps2.adapter_setup & PS2_ADAPTER_SETUP))
                        temp = mca_read(port);
                else
                        temp = 0xff;
                break;
                case 0x106:
                if (!(ps2.setup & PS2_SETUP_IO))
                        temp = ps2.subaddr_lo;
                else if ((ps2.setup & PS2_SETUP_VGA) && (ps2.adapter_setup & PS2_ADAPTER_SETUP))
                        temp = mca_read(port);
                else
                        temp = 0xff;
                break;
                case 0x107:
                if (!(ps2.setup & PS2_SETUP_IO))
                        temp = ps2.subaddr_hi;
                else if ((ps2.setup & PS2_SETUP_VGA) && (ps2.adapter_setup & PS2_ADAPTER_SETUP))
                        temp = mca_read(port);
                else
                        temp = 0xff;
                break;
                
                default:
                temp = 0xff;
                break;
        }
        
//        pclog("ps2_read: port=%04x temp=%02x\n", port, temp);
        
        return temp;
}

static void ps2_mca_write(uint16_t port, uint8_t val, void *p)
{
//        pclog("ps2_write: port=%04x val=%02x\n", port, val);

        switch (port)
        {
                case 0x0092:
                if ((val & 1) && !(ps2.sys_ctrl_port_a & 1))
                {
                        softresetx86();
                        cpu_set_edx();
                }                        
                ps2.sys_ctrl_port_a = val;    
                mem_a20_alt = val & 2;
                mem_a20_recalc();
                break;
                case 0x94:
                ps2.setup = val;
                break;
                case 0x96:
                ps2.adapter_setup = val;
                mca_set_index(val & 7);
                break;
                case 0x100:
                if (!(ps2.setup & PS2_SETUP_IO))
                        ps2.io_id = val;
                else if ((ps2.setup & PS2_SETUP_VGA) && (ps2.adapter_setup & PS2_ADAPTER_SETUP))
                        mca_write(port, val);
                break;
                case 0x101:
                if ((ps2.setup & PS2_SETUP_VGA) && (ps2.setup & PS2_SETUP_VGA) && (ps2.adapter_setup & PS2_ADAPTER_SETUP))
                        mca_write(port, val);
                break;
                case 0x102:
                if (!(ps2.setup & PS2_SETUP_IO))
                {
                        lpt1_remove();
                        serial1_remove();
                        if (val & 0x04)
                        {
                                if (val & 0x08)
                                        serial1_init(0x3f8, 4);
                                else
                                        serial1_init(0x2f8, 3);
                        }
                        else
                                serial1_remove();
                        if (val & 0x10)
                        {
                                switch ((val >> 5) & 3)
                                {
                                        case 0:
                                        lpt1_init(0x3bc);
                                        break;
                                        case 1:
                                        lpt1_init(0x378);
                                        break;
                                        case 2:
                                        lpt1_init(0x278);
                                        break;
                                }
                        }
                        ps2.option[0] = val;
                }
                else if (!(ps2.setup & PS2_SETUP_VGA))
                        ps2.pos_vga = val;
                else if (ps2.adapter_setup & PS2_ADAPTER_SETUP)
                        mca_write(port, val);
                break;
                case 0x103:
                if (!(ps2.setup & PS2_SETUP_IO))
                        ps2.option[1] = val;
                else if (ps2.adapter_setup & PS2_ADAPTER_SETUP)
                        mca_write(port, val);
                break;
                case 0x104:
                if (!(ps2.setup & PS2_SETUP_IO))
                        ps2.option[2] = val;
                else if (ps2.adapter_setup & PS2_ADAPTER_SETUP)
                        mca_write(port, val);
                break;
                case 0x105:
                if (!(ps2.setup & PS2_SETUP_IO))
                        ps2.option[3] = val;
                else if (ps2.adapter_setup & PS2_ADAPTER_SETUP)
                        mca_write(port, val);
                break;
                case 0x106:
                if (!(ps2.setup & PS2_SETUP_IO))
                        ps2.subaddr_lo = val;
                else if (ps2.adapter_setup & PS2_ADAPTER_SETUP)
                        mca_write(port, val);
                break;
                case 0x107:
                if (!(ps2.setup & PS2_SETUP_IO))
                        ps2.subaddr_hi = val;
                else if (ps2.adapter_setup & PS2_ADAPTER_SETUP)
                        mca_write(port, val);
                break;
        }
}

void ps2_mca_board_init()
{
        io_sethandler(0x0091, 0x0002, ps2_mca_read, NULL, NULL, ps2_mca_write, NULL, NULL, NULL);
        io_sethandler(0x0094, 0x0001, ps2_mca_read, NULL, NULL, ps2_mca_write, NULL, NULL, NULL);
        io_sethandler(0x0096, 0x0001, ps2_mca_read, NULL, NULL, ps2_mca_write, NULL, NULL, NULL);
        io_sethandler(0x0100, 0x0008, ps2_mca_read, NULL, NULL, ps2_mca_write, NULL, NULL, NULL);
        
        ps2.setup = 0xff;
        
        lpt1_remove();
        lpt2_remove();
        lpt1_init(0x3bc);
        
        serial1_remove();
        serial2_remove();
        
        ps2.io_id = 0xfb;

        mem_remap_top_384k();
        
        mca_init(4);
}
