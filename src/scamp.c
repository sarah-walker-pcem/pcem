#include "ibm.h"
#include "io.h"
#include "mem.h"
#include "scamp.h"

static struct
{
        int cfg_index;
        uint8_t cfg_regs[256];
        int cfg_enable;
        
        uint8_t port_92;
} scamp;

#define CFG_ID     0x00
#define CFG_SLTPTR 0x02
#define CFG_RAMMAP 0x03
#define CFG_ABAXS  0x0e
#define CFG_CAXS   0x0f
#define CFG_DAXS   0x10
#define CFG_FEAXS  0x11

#define ID_VL82C311 0xd6

#define RAMMAP_REMP386 (1 << 4)

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

void scamp_write(uint16_t addr, uint8_t val, void *p)
{
//        pclog("scamp_write: addr=%04x val=%02x\n", addr, val);
        switch (addr)
        {
                case 0x92:
                if ((mem_a20_alt ^ val) & 2)
                {
                        mem_a20_alt = val & 2;
                        mem_a20_recalc();
                }
                if ((~scamp.port_92 & val) & 1)
                {
                        softresetx86();
                        cpu_set_edx();
                }
                scamp.port_92 = val;
                break;
                
                case 0xec:
                if (scamp.cfg_enable)
                        scamp.cfg_index = val;
                break;
                
                case 0xed:
                if (scamp.cfg_enable)
                {
                        if (scamp.cfg_index >= 0x02 && scamp.cfg_index <= 0x16)
                        {
                                scamp.cfg_regs[scamp.cfg_index] = val;
//                                pclog("SCAMP CFG[%02x]=%02x\n", scamp.cfg_index, val);
                                switch (scamp.cfg_index)
                                {
                                        case CFG_SLTPTR:
#if 0
                                        if (!val)
                                        {
                                                /*Disable all RAM*/
                                                mem_mapping_disable(&ram_low_mapping);
                                                mem_mapping_disable(&ram_high_mapping);
                                        }
                                        else if (val < 0x10)
                                        {
                                                mem_mapping_set_addr(&ram_low_mapping, 0, (val > 9) ? 0xa0000 : (val << 16));
                                                mem_mapping_disable(&ram_high_mapping);
                                        }
                                        else
                                        {
                                                mem_mapping_enable(&ram_low_mapping);
                                                mem_mapping_enable(&ram_high_mapping);
                                        }
#endif
                                        break;

                                        case CFG_RAMMAP:
                                        mem_mapping_disable(&ram_remapped_mapping);
                                        if (scamp.cfg_regs[CFG_RAMMAP] & RAMMAP_REMP386)
                                        {
                                                /*Enabling remapping will disable all shadowing*/
                                                mem_remap_top_384k();
                                                shadow_control(0xa0000, 0x60000, 0);
                                        }
                                        else
                                        {
                                                shadow_control(0xa0000, 0x8000, scamp.cfg_regs[CFG_ABAXS] & 3);
                                                shadow_control(0xa8000, 0x8000, (scamp.cfg_regs[CFG_ABAXS] >> 2) & 3);
                                                shadow_control(0xb0000, 0x8000, (scamp.cfg_regs[CFG_ABAXS] >> 4) & 3);
                                                shadow_control(0xb8000, 0x8000, (scamp.cfg_regs[CFG_ABAXS] >> 6) & 3);

                                                shadow_control(0xc0000, 0x4000, scamp.cfg_regs[CFG_ABAXS] & 3);
                                                shadow_control(0xc4000, 0x4000, (scamp.cfg_regs[CFG_ABAXS] >> 2) & 3);
                                                shadow_control(0xc8000, 0x4000, (scamp.cfg_regs[CFG_ABAXS] >> 4) & 3);
                                                shadow_control(0xcc000, 0x4000, (scamp.cfg_regs[CFG_ABAXS] >> 6) & 3);

                                                shadow_control(0xd0000, 0x4000, scamp.cfg_regs[CFG_ABAXS] & 3);
                                                shadow_control(0xd4000, 0x4000, (scamp.cfg_regs[CFG_ABAXS] >> 2) & 3);
                                                shadow_control(0xd8000, 0x4000, (scamp.cfg_regs[CFG_ABAXS] >> 4) & 3);
                                                shadow_control(0xdc000, 0x4000, (scamp.cfg_regs[CFG_ABAXS] >> 6) & 3);

                                                shadow_control(0xe0000, 0x8000, scamp.cfg_regs[CFG_ABAXS] & 3);
                                                shadow_control(0xe8000, 0x8000, (scamp.cfg_regs[CFG_ABAXS] >> 2) & 3);
                                                shadow_control(0xf0000, 0x8000, (scamp.cfg_regs[CFG_ABAXS] >> 4) & 3);
                                                shadow_control(0xf8000, 0x8000, (scamp.cfg_regs[CFG_ABAXS] >> 6) & 3);
                                        }
                                        break;

                                        case CFG_ABAXS:
                                        if (!(scamp.cfg_regs[CFG_RAMMAP] & RAMMAP_REMP386))
                                        {
                                                shadow_control(0xa0000, 0x8000, val & 3);
                                                shadow_control(0xa8000, 0x8000, (val >> 2) & 3);
                                                shadow_control(0xb0000, 0x8000, (val >> 4) & 3);
                                                shadow_control(0xb8000, 0x8000, (val >> 6) & 3);
                                        }
                                        break;
                                        
                                        case CFG_CAXS:
                                        if (!(scamp.cfg_regs[CFG_RAMMAP] & RAMMAP_REMP386))
                                        {
                                                shadow_control(0xc0000, 0x4000, val & 3);
                                                shadow_control(0xc4000, 0x4000, (val >> 2) & 3);
                                                shadow_control(0xc8000, 0x4000, (val >> 4) & 3);
                                                shadow_control(0xcc000, 0x4000, (val >> 6) & 3);
                                        }
                                        break;
                                        
                                        case CFG_DAXS:
                                        if (!(scamp.cfg_regs[CFG_RAMMAP] & RAMMAP_REMP386))
                                        {
                                                shadow_control(0xd0000, 0x4000, val & 3);
                                                shadow_control(0xd4000, 0x4000, (val >> 2) & 3);
                                                shadow_control(0xd8000, 0x4000, (val >> 4) & 3);
                                                shadow_control(0xdc000, 0x4000, (val >> 6) & 3);
                                        }
                                        break;
                                        
                                        case CFG_FEAXS:
                                        if (!(scamp.cfg_regs[CFG_RAMMAP] & RAMMAP_REMP386))
                                        {
                                                shadow_control(0xe0000, 0x8000, val & 3);
                                                shadow_control(0xe8000, 0x8000, (val >> 2) & 3);
                                                shadow_control(0xf0000, 0x8000, (val >> 4) & 3);
                                                shadow_control(0xf8000, 0x8000, (val >> 6) & 3);
                                        }
                                        break;
                                }
                        }
                }
                break;

                case 0xee:
                if (scamp.cfg_enable && mem_a20_alt)
                {
                        scamp.port_92 &= ~2;
                        mem_a20_alt = 0;
                        mem_a20_recalc();
                }
                break;
        }
}

uint8_t scamp_read(uint16_t addr, void *p)
{
        uint8_t ret = 0xff;
        
        switch (addr)
        {
                case 0x92:
                ret = scamp.port_92;
                break;

                case 0xee:
                if (!mem_a20_alt)
                {
                        scamp.port_92 |= 2;
                        mem_a20_alt = 1;
                        mem_a20_recalc();
                }
                break;
                
                case 0xef:
                softresetx86();
                cpu_set_edx();
                break;
        }

//        pclog("scamp_read: addr=%04x ret=%02x\n", addr, ret);
        return ret;
}

void scamp_init(void)
{
        memset(&scamp, 0, sizeof(scamp));
        scamp.cfg_regs[CFG_ID] = ID_VL82C311;
        scamp.cfg_enable = 1;

        io_sethandler(0x0092, 0x0001,  scamp_read, NULL, NULL, scamp_write, NULL, NULL,  NULL);
        io_sethandler(0x00e8, 0x0001,  scamp_read, NULL, NULL, scamp_write, NULL, NULL,  NULL);
        io_sethandler(0x00ea, 0x0006,  scamp_read, NULL, NULL, scamp_write, NULL, NULL,  NULL);
        io_sethandler(0x00f4, 0x0002,  scamp_read, NULL, NULL, scamp_write, NULL, NULL,  NULL);
        io_sethandler(0x00f9, 0x0001,  scamp_read, NULL, NULL, scamp_write, NULL, NULL,  NULL);
        io_sethandler(0x00fb, 0x0001,  scamp_read, NULL, NULL, scamp_write, NULL, NULL,  NULL);
}
