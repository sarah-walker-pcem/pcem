/*CS8230 CHIPSet consists of :
        82c301 bus controller
        82c302 page/interleave memory controller
        82a303 high address buffer
        82a304 low address buffer
        82b305 data buffer
        82a306 control buffer*/
#include "ibm.h"
#include "cs8230.h"
#include "io.h"
#include "mem.h"

static struct
{
        int idx;
        uint8_t regs[256];
} cs8230;

static void shadow_control(uint32_t addr, uint32_t size, int state)
{
//        pclog("shadow_control: addr=%08x size=%04x state=%02x\n", addr, size, state);
        switch (state)
        {
                case 0x00:
                mem_set_mem_state(addr, size, MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
                break;
                case 0x01:
                mem_set_mem_state(addr, size, MEM_READ_EXTERNAL | MEM_WRITE_INTERNAL);
                break;
                case 0x10:
                mem_set_mem_state(addr, size, MEM_READ_INTERNAL | MEM_WRITE_EXTERNAL);
                break;
                case 0x11:
                mem_set_mem_state(addr, size, MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
                break;
        }
        flushmmucache_nopc();
}

static void rethink_shadow_mappings(void)
{
        int c;
        
        for (c = 0; c < 4*8; c++) /*Addresses 40000-bffff in 16k blocks*/
        {
                if (cs8230.regs[0xa + (c >> 3)] & (1 << (c & 7)))
                        mem_set_mem_state(0x40000 + c*0x4000, 0x4000, MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL); /*IO channel*/
                else
                        mem_set_mem_state(0x40000 + c*0x4000, 0x4000, MEM_READ_INTERNAL | MEM_WRITE_INTERNAL); /*System board*/
        }
        for (c = 0; c < 2*8; c++) /*Addresses c0000-fffff in 16k blocks. System board ROM can be mapped here*/
        {
                if (cs8230.regs[0xe + (c >> 3)] & (1 << (c & 7)))
                        mem_set_mem_state(0xc0000 + c*0x4000, 0x4000, MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL); /*IO channel*/
                else
                        shadow_control(0xc0000 + c*0x4000, 0x4000, (cs8230.regs[9] >> (3-(c >> 2))) & 0x11);
        }
}

static uint8_t cs8230_read(uint16_t port, void *p)
{
        uint8_t ret = 0xff;
        
        if (port & 1)
        {
                switch (cs8230.idx)
                {
                        case 0x04: /*82C301 ID/version*/
                        ret = cs8230.regs[cs8230.idx] & ~0xe3;
                        break;

                        case 0x08: /*82C302 ID/Version*/
                        ret = cs8230.regs[cs8230.idx] & ~0xe0;
                        break;
                        
                        case 0x05: case 0x06: /*82C301 registers*/
                        case 0x09: case 0x0a: case 0x0b: case 0x0c: /*82C302 registers*/
                        case 0x0d: case 0x0e: case 0x0f: 
                        case 0x10: case 0x11: case 0x12: case 0x13:
                        case 0x28: case 0x29: case 0x2a:
                        ret = cs8230.regs[cs8230.idx];
                        break;
                }
        }
        
        return ret;
}

static void cs8230_write(uint16_t port, uint8_t val, void *p)
{
        if (!(port & 1))
                cs8230.idx = val;
        else
        {
//                pclog("cs8230_write: reg=%02x val=%02x\n", cs8230.idx, val);
                cs8230.regs[cs8230.idx] = val;
                switch (cs8230.idx)
                {
                        case 0x09: /*RAM/ROM Configuration in boot area*/
                        case 0x0a: case 0x0b: case 0x0c: case 0x0d: case 0x0e: case 0x0f: /*Address maps*/
                        rethink_shadow_mappings();
                        break;
                }
        }
}

void cs8230_init(void)
{
        memset(&cs8230, 0, sizeof(cs8230));
        
        io_sethandler(0x0022, 0x0002,
                        cs8230_read, NULL, NULL,
                        cs8230_write, NULL, NULL,
                        NULL);
}
