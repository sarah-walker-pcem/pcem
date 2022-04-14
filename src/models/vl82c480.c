#include "ibm.h"
#include "io.h"
#include "mem.h"
#include "vl82c480.h"

static struct
{
        int cfg_index;
        uint8_t cfg_regs[256];

        uint8_t port_92;
} vl82c480;

#define CFG_ID     0x00
#define CFG_AAXS   0x0d
#define CFG_BAXS   0x0e
#define CFG_CAXS   0x0f
#define CFG_DAXS   0x10
#define CFG_EAXS   0x11
#define CFG_FAXS   0x12

#define ID_VL82C480 0x90

static void shadow_control(uint32_t addr, uint32_t size, int state)
{
//        pclog("shadow_control: addr=%08x size=%04x state=%i\n", addr, size, state);
        switch (state)
        {
                case 0:
                mem_set_mem_state(addr, size, MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
                break;
                case 1:
                mem_set_mem_state(addr, size, MEM_READ_EXTERNAL | MEM_WRITE_INTERNAL);
                break;
                case 2:
                mem_set_mem_state(addr, size, MEM_READ_INTERNAL | MEM_WRITE_EXTERNAL);
                break;
                case 3:
                mem_set_mem_state(addr, size, MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
                break;
        }
        flushmmucache_nopc();
}

void vl82c480_write(uint16_t addr, uint8_t val, void *p)
{
//        if (addr != 0x92) pclog("vl82c480_write: addr=%04x val=%02x\n", addr, val);
        switch (addr)
        {
                case 0x92:
                if ((mem_a20_alt ^ val) & 2)
                {
                        mem_a20_alt = val & 2;
                        mem_a20_recalc();
                }
                if ((~vl82c480.port_92 & val) & 1)
                {
                        softresetx86();
                        cpu_set_edx();
                }
                vl82c480.port_92 = val;
                break;

                case 0xec:
                vl82c480.cfg_index = val;
                break;

                case 0xed:
                if (vl82c480.cfg_index >= 0x01 && vl82c480.cfg_index <= 0x24)
                {
                        vl82c480.cfg_regs[vl82c480.cfg_index] = val;
                        switch (vl82c480.cfg_index)
                        {
                                case CFG_AAXS:
                                shadow_control(0xa0000, 0x4000, val & 3);
                                shadow_control(0xa4000, 0x4000, (val >> 2) & 3);
                                shadow_control(0xa8000, 0x4000, (val >> 4) & 3);
                                shadow_control(0xac000, 0x4000, (val >> 6) & 3);
                                break;
                                case CFG_BAXS:
                                shadow_control(0xb0000, 0x4000, val & 3);
                                shadow_control(0xb4000, 0x4000, (val >> 2) & 3);
                                shadow_control(0xb8000, 0x4000, (val >> 4) & 3);
                                shadow_control(0xbc000, 0x4000, (val >> 6) & 3);
                                break;
                                case CFG_CAXS:
                                shadow_control(0xc0000, 0x4000, val & 3);
                                shadow_control(0xc4000, 0x4000, (val >> 2) & 3);
                                shadow_control(0xc8000, 0x4000, (val >> 4) & 3);
                                shadow_control(0xcc000, 0x4000, (val >> 6) & 3);
                                break;
                                case CFG_DAXS:
                                shadow_control(0xd0000, 0x4000, val & 3);
                                shadow_control(0xd4000, 0x4000, (val >> 2) & 3);
                                shadow_control(0xd8000, 0x4000, (val >> 4) & 3);
                                shadow_control(0xdc000, 0x4000, (val >> 6) & 3);
                                break;
                                case CFG_EAXS:
                                shadow_control(0xe0000, 0x4000, val & 3);
                                shadow_control(0xe4000, 0x4000, (val >> 2) & 3);
                                shadow_control(0xe8000, 0x4000, (val >> 4) & 3);
                                shadow_control(0xec000, 0x4000, (val >> 6) & 3);
                                break;
                                case CFG_FAXS:
                                shadow_control(0xf0000, 0x4000, val & 3);
                                shadow_control(0xf4000, 0x4000, (val >> 2) & 3);
                                shadow_control(0xf8000, 0x4000, (val >> 4) & 3);
                                shadow_control(0xfc000, 0x4000, (val >> 6) & 3);
                                break;
                        }
                }
                break;
                
                case 0xee:
                if (mem_a20_alt)
                {
                        vl82c480.port_92 &= ~2;
                        mem_a20_alt = 0;
                        mem_a20_recalc();
                }
                break;
        }
}

uint8_t vl82c480_read(uint16_t addr, void *p)
{
        uint8_t ret = 0xff;

        switch (addr)
        {
                case 0x92:
                ret = vl82c480.port_92;
                break;

                case 0xec:
                ret = vl82c480.cfg_index;
                break;

                case 0xed:
                ret = vl82c480.cfg_regs[vl82c480.cfg_index];
                break;

                case 0xee:
                if (!mem_a20_alt)
                {
                        vl82c480.port_92 |= 2;
                        mem_a20_alt = 1;
                        mem_a20_recalc();
                }
                break;

                case 0xef:
                softresetx86();
                cpu_set_edx();
                break;
        }

//        pclog("vl82c480_read: addr=%04x ret=%02x\n", addr, ret);
        return ret;
}

void vl82c480_init(void)
{
        memset(&vl82c480, 0, sizeof(vl82c480));
        vl82c480.cfg_regs[CFG_ID] = ID_VL82C480;

        io_sethandler(0x0092, 0x0001,  vl82c480_read, NULL, NULL, vl82c480_write, NULL, NULL,  NULL);
        io_sethandler(0x00ec, 0x0004,  vl82c480_read, NULL, NULL, vl82c480_write, NULL, NULL,  NULL);
}
