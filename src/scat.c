/*This is the chipset used in the Award 286 clone model*/
#include "ibm.h"
#include "cpu.h"
#include "io.h"
#include "mem.h"
#include "scat.h"
#include "x86.h"
#include "nmi.h"

static uint8_t scat_regs[256];
static int scat_index;
static uint8_t scat_port_92 = 0;
static uint8_t scat_ems_reg_2xA = 0;
static mem_mapping_t scat_low_mapping[32];
static mem_mapping_t scat_ems_mapping[32];
static mem_mapping_t scat_high_mapping[16];
static scat_t scat_stat[32];
static uint32_t scat_xms_bound;
static mem_mapping_t scat_remap_mapping[6];
static mem_mapping_t scat_4000_EFFF_mapping[44];
static mem_mapping_t scat_low_ROMCS_mapping[8];

static int scat_max_map[32] = { 0, 1,  1,  1,  2,  3,  4,  8,
                                4, 8, 12, 16, 20, 24, 28, 32,
                                0, 5,  9, 13,  6, 10,  0,  0,
                                0, 0,  0,  0,  0,  0,  0,  0 };
static int scatsx_max_map[32] = {  0,  1,  2,  1,  3,  4,  6, 10,
                                   5,  9, 13,  4,  8, 12, 16, 14,
                                  18, 22, 26, 20, 24, 28, 32, 18,
                                  20, 32,  0,  0,  0,  0,  0,  0 };

static int external_is_RAS = 0;

static int scatsx_external_is_RAS[33] = { 0, 0, 0, 0, 0, 0, 0, 0,
                                          0, 0, 0, 1, 0, 0, 1, 1,
                                          0, 0, 0, 0, 0, 0, 1, 1,
                                          1, 1, 1, 1, 1, 1, 1, 1,
                                          0 };

uint8_t scat_read(uint16_t port, void *priv);
void scat_write(uint16_t port, uint8_t val, void *priv);

void scat_romcs_state_update(uint8_t val)
{
        int i;
        for(i = 0; i < 4; i++)
        {
                if (val & 1)
                {
                        mem_mapping_enable(&scat_low_ROMCS_mapping[i << 1]);
                        mem_mapping_enable(&scat_low_ROMCS_mapping[(i << 1) + 1]);
                }
                else
                {
                        mem_mapping_disable(&scat_low_ROMCS_mapping[i << 1]);
                        mem_mapping_disable(&scat_low_ROMCS_mapping[(i << 1) + 1]);
                }
                val >>= 1;
        }

        for(i = 0; i < 4; i++)
        {
                if (val & 1)
                {
                        mem_mapping_enable(&bios_mapping[i << 1]);
                        mem_mapping_enable(&bios_mapping[(i << 1) + 1]);
                }
                else
                {
                        mem_mapping_disable(&bios_mapping[i << 1]);
                        mem_mapping_disable(&bios_mapping[(i << 1) + 1]);
                }
                val >>= 1;
        }
}

void scat_shadow_state_update()
{
        int i, val;

        for (i = 0; i < 24; i++)
        {
                val = ((scat_regs[SCAT_SHADOW_RAM_ENABLE_1 + (i >> 3)] >> (i & 7)) & 1) ? MEM_READ_INTERNAL | MEM_WRITE_INTERNAL : MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL;
                mem_set_mem_state((i + 40) << 14, 0x4000, val);
        }

        flushmmucache();
}

void scat_set_xms_bound(uint8_t val)
{
        uint32_t max_xms_size = ((scat_regs[SCAT_VERSION] & 0xF0) != 0 && ((val & 0x10) != 0)) || (scat_regs[SCAT_VERSION] >= 4) ? 0xFE0000 : 0xFC0000;

        switch (val & 0x0F)
        {
                case 1:
                scat_xms_bound = 0x100000;
                break;
                case 2:
                scat_xms_bound = 0x140000;
                break;
                case 3:
                scat_xms_bound = 0x180000;
                break;
                case 4:
                scat_xms_bound = 0x200000;
                break;
                case 5:
                scat_xms_bound = 0x300000;
                break;
                case 6:
                scat_xms_bound = 0x400000;
                break;
                case 7:
                scat_xms_bound = (scat_regs[SCAT_VERSION] & 0xF0) == 0 ? 0x600000 : 0x500000;
                break;
                case 8:
                scat_xms_bound = (scat_regs[SCAT_VERSION] & 0xF0) == 0 ? 0x800000 : 0x700000;
                break;
                case 9:
                scat_xms_bound = (scat_regs[SCAT_VERSION] & 0xF0) == 0 ? 0xA00000 : 0x800000;
                break;
                case 10:
                scat_xms_bound = (scat_regs[SCAT_VERSION] & 0xF0) == 0 ? 0xC00000 : 0x900000;
                break;
                case 11:
                scat_xms_bound = (scat_regs[SCAT_VERSION] & 0xF0) == 0 ? 0xE00000 : 0xA00000;
                break;
                case 12:
                scat_xms_bound = (scat_regs[SCAT_VERSION] & 0xF0) == 0 ? max_xms_size : 0xB00000;
                break;
                case 13:
                scat_xms_bound = (scat_regs[SCAT_VERSION] & 0xF0) == 0 ? max_xms_size : 0xC00000;
                break;
                case 14:
                scat_xms_bound = (scat_regs[SCAT_VERSION] & 0xF0) == 0 ? max_xms_size : 0xD00000;
                break;
                case 15:
                scat_xms_bound = (scat_regs[SCAT_VERSION] & 0xF0) == 0 ? max_xms_size : 0xF00000;
                break;
                default:
                scat_xms_bound = max_xms_size;
                break;
        }

        if ((((scat_regs[SCAT_VERSION] & 0xF0) == 0) && (val & 0x40) == 0 && (scat_regs[SCAT_DRAM_CONFIGURATION] & 0x0F) == 3) || (((scat_regs[SCAT_VERSION] & 0xF0) != 0) && (scat_regs[SCAT_DRAM_CONFIGURATION] & 0x1F) == 3))
        {
                if((val & 0x0F) == 0 || scat_xms_bound > 0x160000)
                        scat_xms_bound = 0x160000;
//                pclog("Set XMS bound(%02X) = %06X(%dKbytes for EMS access)\n", val, scat_xms_bound, (0x160000 - scat_xms_bound) >> 10);
                if (scat_xms_bound > 0x100000)
                        mem_set_mem_state(0x100000, scat_xms_bound - 0x100000, MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
                if (scat_xms_bound < 0x160000)
                        mem_set_mem_state(scat_xms_bound, 0x160000 - scat_xms_bound, MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
        }
        else
        {
                if (scat_xms_bound > max_xms_size)
                        scat_xms_bound = max_xms_size;
//                pclog("Set XMS bound(%02X) = %06X(%dKbytes for EMS access)\n", val, scat_xms_bound, ((mem_size << 10) - scat_xms_bound) >> 10);
                if (scat_xms_bound > 0x100000)
                        mem_set_mem_state(0x100000, scat_xms_bound - 0x100000, MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
                if (scat_xms_bound < (mem_size << 10))
                        mem_set_mem_state(scat_xms_bound, (mem_size << 10) - scat_xms_bound, MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
        }

        mem_mapping_set_addr(&scat_low_mapping[31], 0xF80000, ((scat_regs[SCAT_VERSION] & 0xF0) != 0 && ((val & 0x10) != 0)) || (scat_regs[SCAT_VERSION] >= 4) ? 0x60000 : 0x40000);
        if(scat_regs[SCAT_VERSION] & 0xF0)
        {
                int i;
                for(i=0;i<8;i++)
                {
                        if(val & 0x10)
                                mem_mapping_disable(&scat_high_mapping[i]);
                        else
                                mem_mapping_enable(&scat_high_mapping[i]);
                }
        }
}

uint32_t get_scat_addr(uint32_t addr, scat_t *p)
{
        int nbank;

        if (p && (scat_regs[SCAT_EMS_CONTROL] & 0x80) && (p->regs_2x9 & 0x80))
                addr = (addr & 0x3fff) | (((p->regs_2x9 & 3) << 8) | p->regs_2x8) << 14;

        if ((scat_regs[SCAT_VERSION] & 0xF0) == 0)
        {
                switch((scat_regs[SCAT_EXTENDED_BOUNDARY] & ((scat_regs[SCAT_VERSION] & 0x0F) > 3 ? 0x40 : 0)) | (scat_regs[SCAT_DRAM_CONFIGURATION] & 0x0F))
                {
                        case 0x41:
                        nbank = addr >> 19;
                        if(nbank < 4)
                                nbank = 1;
                        else if(nbank == 4)
                                nbank = 0;
                        else
                                nbank -= 3;
                        break;
                        case 0x42:
                        nbank = addr >> 19;
                        if(nbank < 8)
                                nbank = 1 + (nbank >> 2);
                        else if(nbank == 8)
                                nbank = 0;
                        else
                                nbank -= 6;
                        break;
                        case 0x43:
                        nbank = addr >> 19;
                        if(nbank < 12)
                                nbank = 1 + (nbank >> 2);
                        else if(nbank == 12)
                                nbank = 0;
                        else
                                nbank -= 9;
                        break;
                        case 0x44:
                        nbank = addr >> 19;
                        if(nbank < 4)
                                nbank = 2;
                        else if(nbank < 6)
                                nbank -= 4;
                        else
                                nbank -= 3;
                        break;
                        case 0x45:
                        nbank = addr >> 19;
                        if(nbank < 8)
                                nbank = 2 + (nbank >> 2);
                        else if(nbank < 10)
                                nbank -= 8;
                        else
                                nbank -= 6;
                        break;
                        default:
                        nbank = addr >> (((scat_regs[SCAT_DRAM_CONFIGURATION] & 0x0F) < 8 && (scat_regs[SCAT_EXTENDED_BOUNDARY] & 0x40) == 0) ? 19 : 21);
                        break;
                }
                nbank &= (scat_regs[SCAT_EXTENDED_BOUNDARY] & 0x80) ? 7 : 3;
                if ((scat_regs[SCAT_EXTENDED_BOUNDARY] & 0x40) == 0 && (scat_regs[SCAT_DRAM_CONFIGURATION] & 0x0F) == 3 && nbank == 2 && (addr & 0x7FFFF) < 0x60000 && mem_size > 640)
                {
                        nbank = 1;
                        addr ^= 0x70000;
                }

                if(external_is_RAS && (scat_regs[SCAT_EXTENDED_BOUNDARY] & 0x80) == 0)
                {
                        if(nbank == 3)
                                nbank = 7;
                        else
                                return 0xFFFFFFFF;
                }
                else if(!external_is_RAS && scat_regs[SCAT_EXTENDED_BOUNDARY] & 0x80)
                {
                        switch(nbank)
                        {
                                case 7:
                                nbank = 3;
                                break;
                                // Note - In the following cases, the chipset accesses multiple memory banks at the same time, so it's impossible to predict which memory bank is actually accessed.
                                case 5:
                                case 1:
                                nbank = 1;
                                break;
                                case 3:
                                nbank = 2;
                                break;
                                default:
                                nbank = 0;
                                break;
                        }
                }

                if((scat_regs[SCAT_VERSION] & 0x0F) > 3 && (mem_size > 2048) && (mem_size & 1536))
                {
                        if((mem_size & 1536) == 512)
                        {
                                if(nbank == 0)
                                        addr &= 0x7FFFF;
                                else
                                        addr = 0x80000 + ((addr & 0x1FFFFF) | ((nbank - 1) << 21));
                        }
                        else
                        {
                                if(nbank < 2)
                                        addr = (addr & 0x7FFFF) | (nbank << 19);
                                else
                                        addr = 0x100000 + ((addr & 0x1FFFFF) | ((nbank - 2) << 21));
                        }
                }
                else
                {
                        int nbanks_2048k, nbanks_512k;
                        if (mem_size <= ((scat_regs[SCAT_VERSION] & 0x0F) > 3 ? 2048 : 4096) && (((scat_regs[SCAT_DRAM_CONFIGURATION] & 0x0F) < 8) || external_is_RAS))
                        {
                                nbanks_2048k = 0;
                                nbanks_512k = mem_size >> 9;
                        }
                        else
                        { 
                                nbanks_2048k = mem_size >> 11;
                                nbanks_512k = (mem_size & 1536) >> 9;
                        }
                        if(nbank < nbanks_2048k || (nbanks_2048k > 0 && nbank >= nbanks_2048k + nbanks_512k + ((mem_size & 511) >> 7)))
                        {
                                  addr &= 0x1FFFFF;
                                  addr |= (nbank << 21);
                        }
                        else if(nbank < nbanks_2048k + nbanks_512k || nbank >= nbanks_2048k + nbanks_512k + ((mem_size & 511) >> 7))
                        {
                                  addr &= 0x7FFFF;
                                  addr |= (nbanks_2048k << 21) | ((nbank - nbanks_2048k) << 19);
                        }
                        else
                        {
                                  addr &= 0x1FFFF;
                                  addr |= (nbanks_2048k << 21) | (nbanks_512k << 19) | ((nbank - nbanks_2048k - nbanks_512k) << 17);
                        }
                }
        }
        else
        {
                uint32_t addr2;
                switch(scat_regs[SCAT_DRAM_CONFIGURATION] & 0x1F)
                {
                        case 2:
                        case 4:
                        nbank = addr >> 19;
                        if((nbank & (scat_regs[SCAT_EXTENDED_BOUNDARY] & 0x80 ? 7 : 3)) < 2)
                        {
                                nbank = (addr >> 10) & 1;
                                addr2 = addr >> 11;
                        }
                        else
                                addr2 = addr >> 10;
                        break;
                        case 3:
                        nbank = addr >> 19;
                        if((nbank & (scat_regs[SCAT_EXTENDED_BOUNDARY] & 0x80 ? 7 : 3)) < 2)
                        {
                                nbank = (addr >> 10) & 1;
                                addr2 = addr >> 11;
                        }
                        else if((nbank & (scat_regs[SCAT_EXTENDED_BOUNDARY] & 0x80 ? 7 : 3)) == 2 && (addr & 0x7FFFF) < 0x60000)
                        {
                                addr ^= 0x1F0000;
                                nbank = (addr >> 10) & 1;
                                addr2 = addr >> 11;
                        }
                        else
                                addr2 = addr >> 10;
                        break;
                        case 5:
                        nbank = addr >> 19;
                        if((nbank & (scat_regs[SCAT_EXTENDED_BOUNDARY] & 0x80 ? 7 : 3)) < 4)
                        {
                                nbank = (addr >> 10) & 3;
                                addr2 = addr >> 12;
                        }
                        else
                                addr2 = addr >> 10;
                        break;
                        case 6:
                        nbank = addr >> 19;
                        if(nbank < 2)
                        {
                                nbank = (addr >> 10) & 1;
                                addr2 = addr >> 11;
                        }
                        else
                        {
                                nbank = 2 + ((addr - 0x100000) >> 21);
                                addr2 = (addr - 0x100000) >> 11;
                        }
                        break;
                        case 7:
                        case 0x0F:
                        nbank = addr >> 19;
                        if(nbank < 2)
                        {
                                nbank = (addr >> 10) & 1;
                                addr2 = addr >> 11;
                        }
                        else if(nbank < 10)
                        {
                                nbank = 2 + (((addr - 0x100000) >> 11) & 1);
                                addr2 = (addr - 0x100000) >> 12;
                        }
                        else
                        {
                                nbank = 4 + ((addr - 0x500000) >> 21);
                                addr2 = (addr - 0x500000) >> 11;
                        }
                        break;
                        case 8:
                        nbank = addr >> 19;
                        if(nbank < 4)
                        {
                                nbank = 1;
                                addr2 = addr >> 11;
                        }
                        else if(nbank == 4)
                        {
                                nbank = 0;
                                addr2 = addr >> 10;
                        }
                        else
                        {
                                nbank -= 3;
                                addr2 = addr >> 10;
                        }
                        break;
                        case 9:
                        nbank = addr >> 19;
                        if(nbank < 8)
                        {
                                nbank = 1 + ((addr >> 11) & 1);
                                addr2 = addr >> 12;
                        }
                        else if(nbank == 8)
                        {
                                nbank = 0;
                                addr2 = addr >> 10;
                        }
                        else
                        {
                                nbank -= 6;
                                addr2 = addr >> 10;
                        }
                        break;
                        case 0x0A:
                        nbank = addr >> 19;
                        if(nbank < 8)
                        {
                                nbank = 1 + ((addr >> 11) & 1);
                                addr2 = addr >> 12;
                        }
                        else if(nbank < 12)
                        {
                                nbank = 3;
                                addr2 = addr >> 11;
                        }
                        else if(nbank == 12)
                        {
                                nbank = 0;
                                addr2 = addr >> 10;
                        }
                        else
                        {
                                nbank -= 9;
                                addr2 = addr >> 10;
                        }
                        break;
                        case 0x0B:
                        nbank = addr >> 21;
                        addr2 = addr >> 11;
                        break;
                        case 0x0C:
                        case 0x0D:
                        nbank = addr >> 21;
                        if((nbank & (scat_regs[SCAT_EXTENDED_BOUNDARY] & 0x80 ? 7 : 3)) < 2)
                        {
                                nbank = (addr >> 11) & 1;
                                addr2 = addr >> 12;
                        }
                        else
                                addr2 = addr >> 11;
                        break;
                        case 0x0E:
                        case 0x13:
                        nbank = addr >> 21;
                        if((nbank & (scat_regs[SCAT_EXTENDED_BOUNDARY] & 0x80 ? 7 : 3)) < 4)
                        {
                                nbank = (addr >> 11) & 3;
                                addr2 = addr >> 13;
                        }
                        else
                                addr2 = addr >> 11;
                        break;
                        case 0x10:
                        case 0x11:
                        nbank = addr >> 19;
                        if(nbank < 2)
                        {
                                nbank = (addr >> 10) & 1;
                                addr2 = addr >> 11;
                        }
                        else if(nbank < 10)
                        {
                                nbank = 2 + (((addr - 0x100000) >> 11) & 1);
                                addr2 = (addr - 0x100000) >> 12;
                        }
                        else if(nbank < 18)
                        {
                                nbank = 4 + (((addr - 0x500000) >> 11) & 1);
                                addr2 = (addr - 0x500000) >> 12;
                        }
                        else
                        {
                                nbank = 6 + ((addr - 0x900000) >> 21);
                                addr2 = (addr - 0x900000) >> 11;
                        }
                        break;
                        case 0x12:
                        nbank = addr >> 19;
                        if(nbank < 2)
                        {
                                nbank = (addr >> 10) & 1;
                                addr2 = addr >> 11;
                        }
                        else if(nbank < 10)
                        {
                                nbank = 2 + (((addr - 0x100000) >> 11) & 1);
                                addr2 = (addr - 0x100000) >> 12;
                        }
                        else
                        {
                                nbank = 4 + (((addr - 0x500000) >> 11) & 3);
                                addr2 = (addr - 0x500000) >> 13;
                        }
                        break;
                        case 0x14:
                        case 0x15:
                        nbank = addr >> 21;
                        if((nbank & 7) < 4)
                        {
                                nbank = (addr >> 11) & 3;
                                addr2 = addr >> 13;
                        }
                        else if((nbank & 7) < 6)
                        {
                                nbank = 4 + (((addr - 0x800000) >> 11) & 1);
                                addr2 = (addr - 0x800000) >> 12;
                        }
                        else
                        {
                                nbank = 6 + (((addr - 0xC00000) >> 11) & 3);
                                addr2 = (addr - 0xC00000) >> 13;
                        }
                        break;
                        case 0x16:
                        nbank = ((addr >> 21) & 4) | ((addr >> 11) & 3);
                        addr2 = addr >> 13;
                        break;
                        case 0x17:
                        if(external_is_RAS && (addr & 0x800) == 0)
                                return 0xFFFFFFFF;
                        nbank = addr >> 19;
                        if(nbank < 2)
                        {
                                nbank = (addr >> 10) & 1;
                                addr2 = addr >> 11;
                        }
                        else
                        {
                                nbank = 2 + ((addr - 0x100000) >> 23);
                                addr2 = (addr - 0x100000) >> 12;
                        }
                        break;
                        case 0x18:
                        if(external_is_RAS && (addr & 0x800) == 0)
                                return 0xFFFFFFFF;
                        nbank = addr >> 21;
                        if(nbank < 4)
                        {
                                nbank = 1;
                                addr2 = addr >> 12;
                        }
                        else if(nbank == 4)
                        {
                                nbank = 0;
                                addr2 = addr >> 11;
                        }
                        else
                        {
                                nbank -= 3;
                                addr2 = addr >> 11;
                        }
                        break;
                        case 0x19:
                        if(external_is_RAS && (addr & 0x800) == 0)
                                return 0xFFFFFFFF;
                        nbank = addr >> 23;
                        if((nbank & 3) < 2)
                        {
                                nbank = (addr >> 12) & 1;
                                addr2 = addr >> 13;
                        }
                        else
                                addr2 = addr >> 12;
                        break;
                        default:
                        if((scat_regs[SCAT_DRAM_CONFIGURATION] & 0x1F) < 6)
                        {
                                nbank = addr >> 19;
                                addr2 = (addr >> 10) & 0x1FF;
                        }
                        else if((scat_regs[SCAT_DRAM_CONFIGURATION] & 0x1F) < 0x17)
                        {
                                nbank = addr >> 21;
                                addr2 = (addr >> 11) & 0x3FF;
                        }
                        else
                        {
                                nbank = addr >> 23;
                                addr2 = (addr >> 12) & 0x7FF;
                        }
                        break;
                }

                nbank &= (scat_regs[SCAT_EXTENDED_BOUNDARY] & 0x80) ? 7 : 3;
                if ((scat_regs[SCAT_DRAM_CONFIGURATION] & 0x1F) > 0x16 && nbank == 3)
                        return 0xFFFFFFFF;

                if(external_is_RAS && (scat_regs[SCAT_EXTENDED_BOUNDARY] & 0x80) == 0)
                {
                        if(nbank == 3)
                                nbank = 7;
                        else
                                return 0xFFFFFFFF;
                }
                else if(!external_is_RAS && scat_regs[SCAT_EXTENDED_BOUNDARY] & 0x80)
                {
                        switch(nbank)
                        {
                                case 7:
                                nbank = 3;
                                break;
                                // Note - In the following cases, the chipset accesses multiple memory banks at the same time, so it's impossible to predict which memory bank is actually accessed.
                                case 5:
                                case 1:
                                nbank = 1;
                                break;
                                case 3:
                                nbank = 2;
                                break;
                                default:
                                nbank = 0;
                                break;
                        }
                }

                switch(mem_size & ~511)
                {
                        case 1024:
                        case 1536:
                        addr &= 0x3FF;
                        if(nbank < 2)
                                addr |= (nbank << 10) | ((addr2 & 0x1FF) << 11);
                        else
                                addr |= ((addr2 & 0x1FF) << 10) | (nbank << 19);
                        break;
                        case 2048:
                        if((scat_regs[SCAT_DRAM_CONFIGURATION] & 0x1F) == 5)
                        {
                                addr &= 0x3FF;
                                if(nbank < 4)
                                        addr |= (nbank << 10) | ((addr2 & 0x1FF) << 12);
                                else
                                        addr |= ((addr2 & 0x1FF) << 10) | (nbank << 19);
                        }
                        else
                        {
                                addr &= 0x7FF;
                                addr |= ((addr2 & 0x3FF) << 11) | (nbank << 21);
                        }
                        break;
                        case 2560:
                        if(nbank == 0)
                                addr = (addr & 0x3FF) | ((addr2 & 0x1FF) << 10);
                        else
                        {
                                addr &= 0x7FF;
                                addr2 &= 0x3FF;
                                addr = addr + 0x80000 + ((addr2 << 11) | ((nbank - 1) << 21));
                        }
                        break;
                        case 3072:
                        if(nbank < 2)
                                addr = (addr & 0x3FF) | (nbank << 10) | ((addr2 & 0x1FF) << 11);
                        else
                                addr = 0x100000 + ((addr & 0x7FF) | ((addr2 & 0x3FF) << 11) | ((nbank - 2) << 21));
                        break;
                        case 4096:
                        case 6144:
                        addr &= 0x7FF;
                        if(nbank < 2)
                                addr |= (nbank << 11) | ((addr2 & 0x3FF) << 12);
                        else
                                addr |= ((addr2 & 0x3FF) << 11) | (nbank << 21);
                        break;
                        case 4608:
                        if(((scat_regs[SCAT_DRAM_CONFIGURATION] & 0x1F) >= 8 && (scat_regs[SCAT_DRAM_CONFIGURATION] & 0x1F) <= 0x0A) || ((scat_regs[SCAT_DRAM_CONFIGURATION] & 0x1F) == 0x18))
                        {
                                if(nbank == 0)
                                        addr = (addr & 0x3FF) | ((addr2 & 0x1FF) << 10);
                                else if(nbank < 3)
                                        addr = 0x80000 + ((addr & 0x7FF) | ((nbank - 1) << 11) | ((addr2 & 0x3FF) << 12));
                                else
                                        addr = 0x480000 + ((addr & 0x3FF) | ((addr2 & 0x1FF) << 10) | ((nbank - 3) << 19));
                        }
                        else if(nbank == 0)
                                addr = (addr & 0x3FF) | ((addr2 & 0x1FF) << 10);
                        else
                        {
                                addr &= 0x7FF;
                                addr2 &= 0x3FF;
                                addr = addr + 0x80000 + ((addr2 << 11) | ((nbank - 1) << 21));
                        }
                        break;
                        case 5120:
                        case 7168:
                        if(nbank < 2)
                                addr = (addr & 0x3FF) | (nbank << 10) | ((addr2 & 0x1FF) << 11);
                        else if(nbank < 4)
                                addr = 0x100000 + ((addr & 0x7FF) | ((addr2 & 0x3FF) << 12) | ((nbank & 1) << 11));
                        else
                                addr = 0x100000 + ((addr & 0x7FF) | ((addr2 & 0x3FF) << 11) | ((nbank - 2) << 21));
                        break;
                        case 6656:
                        if(((scat_regs[SCAT_DRAM_CONFIGURATION] & 0x1F) >= 8 && (scat_regs[SCAT_DRAM_CONFIGURATION] & 0x1F) <= 0x0A) || ((scat_regs[SCAT_DRAM_CONFIGURATION] & 0x1F) == 0x18))
                        {
                                if(nbank == 0)
                                        addr = (addr & 0x3FF) | ((addr2 & 0x1FF) << 10);
                                else if(nbank < 3)
                                        addr = 0x80000 + ((addr & 0x7FF) | ((nbank - 1) << 11) | ((addr2 & 0x3FF) << 12));
                                else if(nbank == 3)
                                        addr = 0x480000 + ((addr & 0x7FF) | ((addr2 & 0x3FF) << 11));
                                else
                                        addr = 0x680000 + ((addr & 0x3FF) | ((addr2 & 0x1FF) << 10) | ((nbank - 4) << 19));
                        }
                        else if(nbank == 0)
                                addr = (addr & 0x3FF) | ((addr2 & 0x1FF) << 10);
                        else if(nbank == 1)
                        {
                                addr &= 0x7FF;
                                addr2 &= 0x3FF;
                                addr = addr + 0x80000 + (addr2 << 11);
                        }
                        else
                        {
                                addr &= 0x7FF;
                                addr2 &= 0x3FF;
                                addr = addr + 0x280000 + ((addr2 << 12) | ((nbank & 1) << 11) | (((nbank - 2) & 6) << 21));
                        }
                        break;
                        case 8192:
                        addr &= 0x7FF;
                        if(nbank < 4)
                                addr |= (nbank << 11) | ((addr2 & 0x3FF) << 13);
                        else
                                addr |= ((addr2 & 0x3FF) << 11) | (nbank << 21);
                        break;
                        case 9216:
                        if(nbank < 2)
                                addr = (addr & 0x3FF) | (nbank << 10) | ((addr2 & 0x1FF) << 11);
                        else if(external_is_RAS)
                        {
                                if(nbank < 6)
                                        addr = 0x100000 + ((addr & 0x7FF) | ((addr2 & 0x3FF) << 12) | ((nbank & 1) << 11));
                                else
                                        addr = 0x100000 + ((addr & 0x7FF) | ((addr2 & 0x3FF) << 11) | ((nbank - 2) << 21));
                        }
                        else
                                addr = 0x100000 + ((addr & 0xFFF) | ((addr2 & 0x7FF) << 12) | ((nbank - 2) << 23));
                        break;
                        case 10240:
                        if(external_is_RAS)
                        {
                                addr &= 0x7FF;
                                if(nbank < 4)
                                        addr |= (nbank << 11) | ((addr2 & 0x3FF) << 13);
                                else
                                        addr |= ((addr2 & 0x3FF) << 11) | (nbank << 21);
                        }
                        else if(nbank == 0)
                                addr = (addr & 0x7FF) | ((addr2 & 0x3FF) << 11);
                        else
                        {
                                addr &= 0xFFF;
                                addr2 &= 0x7FF;
                                addr = addr + 0x200000 + ((addr2 << 12) | ((nbank - 1) << 23));
                        }
                        break;
                        case 11264:
                        if(nbank < 2)
                                addr = (addr & 0x3FF) | (nbank << 10) | ((addr2 & 0x1FF) << 11);
                        else if(nbank < 6)
                                addr = 0x100000 + ((addr & 0x7FF) | ((addr2 & 0x3FF) << 12) | ((nbank & 1) << 11));
                        else
                                addr = 0x100000 + ((addr & 0x7FF) | ((addr2 & 0x3FF) << 11) | ((nbank - 2) << 21));
                        break;
                        case 12288:
                        if(external_is_RAS)
                        {
                                addr &= 0x7FF;
                                if(nbank < 4)
                                        addr |= (nbank << 11) | ((addr2 & 0x3FF) << 13);
                                else if(nbank < 6)
                                        addr |= ((nbank & 1) << 11) | ((addr2 & 0x3FF) << 12) | ((nbank & 4) << 21);
                                else
                                        addr |= ((addr2 & 0x3FF) << 11) | (nbank << 21);
                        }
                        else
                        {
                                if(nbank < 2)
                                        addr = (addr & 0x7FF) | (nbank << 11) | ((addr2 & 0x3FF) << 12);
                                else
                                        addr = 0x400000 + ((addr & 0xFFF) | ((addr2 & 0x7FF) << 12) | ((nbank - 2) << 23));
                        }
                        break;
                        case 13312:
                        if(nbank < 2)
                                addr = (addr & 0x3FF) | (nbank << 10) | ((addr2 & 0x1FF) << 11);
                        else if(nbank < 4)
                                addr = 0x100000 + ((addr & 0x7FF) | ((addr2 & 0x3FF) << 12) | ((nbank & 1) << 11));
                        else
                                addr = 0x500000 + ((addr & 0x7FF) | ((addr2 & 0x3FF) << 13) | ((nbank & 3) << 11));
                        break;
                        case 14336:
                        addr &= 0x7FF;
                        if(nbank < 4)
                                addr |= (nbank << 11) | ((addr2 & 0x3FF) << 13);
                        else if(nbank < 6)
                                addr |= ((nbank & 1) << 11) | ((addr2 & 0x3FF) << 12) | ((nbank & 4) << 21);
                        else
                                addr |= ((addr2 & 0x3FF) << 11) | (nbank << 21);
                        break;
                        case 16384:
                        if(external_is_RAS)
                        {
                                addr &= 0x7FF;
                                addr2 &= 0x3FF;
                                addr |= ((nbank & 3) << 11) | (addr2 << 13) | ((nbank & 4) << 21);
                        }
                        else
                        {
                                addr &= 0xFFF;
                                addr2 &= 0x7FF;
                                if(nbank < 2)
                                        addr |= (addr2 << 13) | (nbank << 12);
                                else
                                        addr |= (addr2 << 12) | (nbank << 23);
                        }
                        break;
                        default:
                        if(mem_size < 2048 || ((mem_size & 1536) == 512) || (mem_size == 2048 && (scat_regs[SCAT_DRAM_CONFIGURATION] & 0x1F) < 6))
                        {
                                addr &= 0x3FF;
                                addr2 &= 0x1FF;
                                addr |= (addr2 << 10) | (nbank << 19);
                        }
                        else if(mem_size < 8192 || (scat_regs[SCAT_DRAM_CONFIGURATION] & 0x1F) < 0x17)
                        {
                                addr &= 0x7FF;
                                addr2 &= 0x3FF;
                                addr |= (addr2 << 11) | (nbank << 21);
                        }
                        else
                        {
                                addr &= 0xFFF;
                                addr2 &= 0x7FF;
                                addr |= (addr2 << 12) | (nbank << 23);
                        }
                        break;
                }
        }
        return addr;
}

void scat_set_global_EMS_state(int state)
{
        int i;
        uint32_t base_addr, virt_addr;

        for(i=((scat_regs[SCAT_VERSION] & 0xF0) == 0) ? 0 : 24; i<32; i++)
        {
                base_addr = (i + 16) << 14;
                if(i >= 24)
                        base_addr += 0x30000;
                if(state && (scat_stat[i].regs_2x9 & 0x80))
                {
                        virt_addr = get_scat_addr(base_addr, &scat_stat[i]);
                        if(i < 24) mem_mapping_disable(&scat_4000_EFFF_mapping[i]);
                        else mem_mapping_disable(&scat_4000_EFFF_mapping[i + 12]);
                        mem_mapping_enable(&scat_ems_mapping[i]);
                        if(virt_addr < (mem_size << 10)) mem_mapping_set_exec(&scat_ems_mapping[i], ram + virt_addr);
                        else mem_mapping_set_exec(&scat_ems_mapping[i], NULL);
                }
                else
                {
                        mem_mapping_set_exec(&scat_ems_mapping[i], ram + base_addr);
                        mem_mapping_disable(&scat_ems_mapping[i]);

                        int conf = (scat_regs[SCAT_VERSION] & 0xF0) ? (scat_regs[SCAT_DRAM_CONFIGURATION] & 0x1F) : (scat_regs[SCAT_DRAM_CONFIGURATION] & 0xF) | ((scat_regs[SCAT_EXTENDED_BOUNDARY] & 0x40) >> 2);
                        if(i < 24)
                        {
                                if(conf > 1 || (conf == 1 && i < 16))
                                        mem_mapping_enable(&scat_4000_EFFF_mapping[i]);
                                else
                                        mem_mapping_disable(&scat_4000_EFFF_mapping[i]);
                        }
                        else if(conf > 3 || ((scat_regs[SCAT_VERSION] & 0xF0) != 0 && conf == 2))
                                mem_mapping_enable(&scat_4000_EFFF_mapping[i + 12]);
                        else
                                mem_mapping_disable(&scat_4000_EFFF_mapping[i + 12]);
                }
        }
        flushmmucache();
}

void scat_memmap_state_update()
{
        int i;
        uint32_t addr;

        for(i=(((scat_regs[SCAT_VERSION] & 0xF0) == 0) ? 0 : 16);i<44;i++)
        {
                addr = get_scat_addr(0x40000 + (i << 14), NULL);
                mem_mapping_set_exec(&scat_4000_EFFF_mapping[i], addr < (mem_size << 10) ? ram + addr : NULL);
//                pclog("scat_memmap_state_update : Address %06X to %06X\n", 0x40000 + (i << 14), addr);
        }
        addr = get_scat_addr(0, NULL);
//        pclog("scat_memmap_state_update : Address 000000 to %06X\n", addr);
        mem_mapping_set_exec(&scat_low_mapping[0], addr < (mem_size << 10) ? ram + addr : NULL);
        addr = get_scat_addr(0xF0000, NULL);
//        pclog("scat_memmap_state_update : Address 0F0000 to %06X\n", addr);
        mem_mapping_set_exec(&scat_low_mapping[1], addr < (mem_size << 10) ? ram + addr : NULL);
        for(i=2;i<32;i++)
        {
                addr = get_scat_addr(i << 19, NULL);
                mem_mapping_set_exec(&scat_low_mapping[i], addr < (mem_size << 10) ? ram + addr : NULL);
//                pclog("scat_memmap_state_update : Address %06X to %06X\n", i << 19, addr);
        }

        if((scat_regs[SCAT_VERSION] & 0xF0) == 0)
        {
                for(i=0;i < scat_max_map[(scat_regs[SCAT_DRAM_CONFIGURATION] & 0xF) | ((scat_regs[SCAT_EXTENDED_BOUNDARY] & 0x40) >> 2)];i++)
                        mem_mapping_enable(&scat_low_mapping[i]);
                for(;i<32;i++) mem_mapping_disable(&scat_low_mapping[i]);

                for(i=24;i<36;i++)
                {
                        if(((scat_regs[SCAT_DRAM_CONFIGURATION] & 0xF) | (scat_regs[SCAT_EXTENDED_BOUNDARY] & 0x40)) < 4)
                                mem_mapping_disable(&scat_4000_EFFF_mapping[i]);
                        else
                                mem_mapping_enable(&scat_4000_EFFF_mapping[i]);
                }
        }
        else
        {
                for(i=0;i < scatsx_max_map[scat_regs[SCAT_DRAM_CONFIGURATION] & 0x1F];i++)
                        mem_mapping_enable(&scat_low_mapping[i]);
                for(;i<32;i++) mem_mapping_disable(&scat_low_mapping[i]);

                for(i=24;i<36;i++)
                {
                        if((scat_regs[SCAT_DRAM_CONFIGURATION] & 0x1F) < 2 || (scat_regs[SCAT_DRAM_CONFIGURATION] & 0x1F) == 3)
                                mem_mapping_disable(&scat_4000_EFFF_mapping[i]);
                        else
                                mem_mapping_enable(&scat_4000_EFFF_mapping[i]);
                }
        }

        if((((scat_regs[SCAT_VERSION] & 0xF0) == 0) && (scat_regs[SCAT_EXTENDED_BOUNDARY] & 0x40) == 0) || ((scat_regs[SCAT_VERSION] & 0xF0) != 0))
        {
                if((((scat_regs[SCAT_VERSION] & 0xF0) == 0) && (scat_regs[SCAT_DRAM_CONFIGURATION] & 0x0F) == 3) || (((scat_regs[SCAT_VERSION] & 0xF0) != 0) && (scat_regs[SCAT_DRAM_CONFIGURATION] & 0x1F) == 3))
                {
                        mem_mapping_disable(&scat_low_mapping[2]);
                        for(i=0;i<6;i++)
                        {
                                addr = get_scat_addr(0x100000 + (i << 16), NULL);
                                mem_mapping_set_exec(&scat_remap_mapping[i], addr < (mem_size << 10) ? ram + addr : NULL);
                                mem_mapping_enable(&scat_remap_mapping[i]);
//                                pclog("scat_memmap_state_update : Address %06X to %06X\n", 0x100000 + (i << 16), addr);
                        }
                }
                else
                {
                        for(i=0;i<6;i++)
                                mem_mapping_disable(&scat_remap_mapping[i]);
                        if((((scat_regs[SCAT_VERSION] & 0xF0) == 0) && (scat_regs[SCAT_DRAM_CONFIGURATION] & 0x0F) > 4) || (((scat_regs[SCAT_VERSION] & 0xF0) != 0) && (scat_regs[SCAT_DRAM_CONFIGURATION] & 0x1F) > 3))
                                mem_mapping_enable(&scat_low_mapping[2]);
                }
        }
        else
        {
                for(i=0;i<6;i++)
                        mem_mapping_disable(&scat_remap_mapping[i]);
                mem_mapping_enable(&scat_low_mapping[2]);
        }

        scat_set_global_EMS_state(scat_regs[SCAT_EMS_CONTROL] & 0x80);
}

void scat_write(uint16_t port, uint8_t val, void *priv)
{
        uint8_t scat_reg_valid = 0, scat_shadow_update = 0, scat_map_update = 0, index;
        uint32_t base_addr, virt_addr;
 
        switch (port)
        {
                case 0x22:
                scat_index = val;
                break;
                
                case 0x23:
                switch (scat_index)
                {
                        case SCAT_DMA_WAIT_STATE_CONTROL:
                        case SCAT_CLOCK_CONTROL:
                        case SCAT_PERIPHERAL_CONTROL:
                        scat_reg_valid = 1;
                        break;
                        case SCAT_EMS_CONTROL:
                        if(val & 0x40)
                        {
                                if(val & 1)
                                {
                                        io_sethandler(0x0218, 0x0003, scat_read, NULL, NULL, scat_write, NULL, NULL,  NULL);
                                        io_removehandler(0x0208, 0x0003, scat_read, NULL, NULL, scat_write, NULL, NULL,  NULL);
                                }
                                else
                                {
                                        io_sethandler(0x0208, 0x0003, scat_read, NULL, NULL, scat_write, NULL, NULL,  NULL);
                                        io_removehandler(0x0218, 0x0003, scat_read, NULL, NULL, scat_write, NULL, NULL,  NULL);
                                }
                        }
                        else
                        {
                                io_removehandler(0x0208, 0x0003, scat_read, NULL, NULL, scat_write, NULL, NULL,  NULL);
                                io_removehandler(0x0218, 0x0003, scat_read, NULL, NULL, scat_write, NULL, NULL,  NULL);
                        }
                        scat_set_global_EMS_state(val & 0x80);
                        scat_reg_valid = 1;
                        break;
                        case SCAT_POWER_MANAGEMENT:
                        // TODO - Only use AUX parity disable bit for this version. Other bits should be implemented later.
                        val &= (scat_regs[SCAT_VERSION] & 0xF0) == 0 ? 0x40 : 0x60;
                        scat_reg_valid = 1;
                        break;
                        case SCAT_DRAM_CONFIGURATION:
                        scat_map_update = 1;

                        if((scat_regs[SCAT_VERSION] & 0xF0) == 0)
                        {
                                cpu_waitstates = (val & 0x70) == 0 ? 1 : 2;
                                cpu_update_waitstates();
                        }

                        scat_reg_valid = 1;
                        break;
                        case SCAT_EXTENDED_BOUNDARY:
                        if((scat_regs[SCAT_VERSION] & 0xF0) == 0)
                        {
                                if(scat_regs[SCAT_VERSION] < 4)
                                {
                                        val &= 0xBF;
                                        scat_set_xms_bound(val & 0x0f);
                                }
                                else
                                {
                                        val = (val & 0x7F) | 0x80;
                                        scat_set_xms_bound(val & 0x4f);
                                }
                        }
                        else
                                scat_set_xms_bound(val & 0x1f);
                        mem_set_mem_state(0x40000, 0x60000, (val & 0x20) ? MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL : MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
                        if((val ^ scat_regs[SCAT_EXTENDED_BOUNDARY]) & 0xC0) scat_map_update = 1;
                        scat_reg_valid = 1;
                        break;
                        case SCAT_ROM_ENABLE:
                        scat_reg_valid = 1;
                        scat_romcs_state_update(val);
                        break;
                        case SCAT_RAM_WRITE_PROTECT:
                        scat_reg_valid = 1;
                        flushmmucache_cr3();
                        break;
                        case SCAT_SHADOW_RAM_ENABLE_1:
                        case SCAT_SHADOW_RAM_ENABLE_2:
                        case SCAT_SHADOW_RAM_ENABLE_3:
                        scat_reg_valid = 1;
                        scat_shadow_update = 1;
                        break;
                        case SCATSX_LAPTOP_FEATURES:
                        if((scat_regs[SCAT_VERSION] & 0xF0) != 0)
                        {
                                val = (val & ~8) | (scat_regs[SCATSX_LAPTOP_FEATURES] & 8);
                                scat_reg_valid = 1;
                        }
                        break;
                        case SCATSX_FAST_VIDEO_CONTROL:
                        case SCATSX_FAST_VIDEORAM_ENABLE:
                        case SCATSX_HIGH_PERFORMANCE_REFRESH:
                        case SCATSX_CAS_TIMING_FOR_DMA:
                        if((scat_regs[SCAT_VERSION] & 0xF0) != 0) scat_reg_valid = 1;
                        break;
                        default:
                        break;
                }
                if (scat_reg_valid)
                        scat_regs[scat_index] = val;
                else pclog("Attemped to write unimplemented SCAT register %02X at %04X:%04X\n", scat_index, val, CS, cpu_state.oldpc);
                if (scat_shadow_update)
                        scat_shadow_state_update();
                if (scat_map_update)
                        scat_memmap_state_update();
//                pclog("Write SCAT Register %02X to %02X at %04X:%04X\n", scat_index, val, CS, cpu_state.pc);
                break;

                case 0x92:
                if ((mem_a20_alt ^ val) & 2)
                {
                         mem_a20_alt = val & 2;
                         mem_a20_recalc();
                }
                if ((~scat_port_92 & val) & 1)
                {
                         softresetx86();
                        cpu_set_edx();
                }
                scat_port_92 = val;
                break;

                case 0x208:
                case 0x218:
                if ((scat_regs[SCAT_EMS_CONTROL] & 0x41) == (0x40 | ((port & 0x10) >> 4)))
                {
//                        pclog("Write SCAT EMS Control Port %04X to %02X at %04X:%04X\n", port, val, CS, cpu_state.pc);
                        if((scat_regs[SCAT_VERSION] & 0xF0) == 0) index = scat_ems_reg_2xA & 0x1F;
                        else index = ((scat_ems_reg_2xA & 0x40) >> 4) + (scat_ems_reg_2xA & 0x3) + 24;
                        scat_stat[index].regs_2x8 = val;
                        base_addr = (index + 16) << 14;
                        if(index >= 24)
                                base_addr += 0x30000;

                        if((scat_regs[SCAT_EMS_CONTROL] & 0x80) && (scat_stat[index].regs_2x9 & 0x80))
                        {
                                virt_addr = get_scat_addr(base_addr, &scat_stat[index]);
                                if(virt_addr < (mem_size << 10)) mem_mapping_set_exec(&scat_ems_mapping[index], ram + virt_addr);
                                else mem_mapping_set_exec(&scat_ems_mapping[index], NULL);
                                flushmmucache();
                        }
                }
                break;
                case 0x209:
                case 0x219:
                if ((scat_regs[SCAT_EMS_CONTROL] & 0x41) == (0x40 | ((port & 0x10) >> 4)))
                {
//                        pclog("Write SCAT EMS Control Port %04X to %02X at %04X:%04X\n", port, val, CS, cpu_state.pc);
                        if((scat_regs[SCAT_VERSION] & 0xF0) == 0) index = scat_ems_reg_2xA & 0x1F;
                        else index = ((scat_ems_reg_2xA & 0x40) >> 4) + (scat_ems_reg_2xA & 0x3) + 24;
                        scat_stat[index].regs_2x9 = val;
                        base_addr = (index + 16) << 14;
                        if(index >= 24)
                                base_addr += 0x30000;

                        if (scat_regs[SCAT_EMS_CONTROL] & 0x80)
                        {
                                if (val & 0x80)
                                {
                                        virt_addr = get_scat_addr(base_addr, &scat_stat[index]);
                                        if(index < 24) mem_mapping_disable(&scat_4000_EFFF_mapping[index]);
                                        else mem_mapping_disable(&scat_4000_EFFF_mapping[index + 12]);
                                        if(virt_addr < (mem_size << 10)) mem_mapping_set_exec(&scat_ems_mapping[index], ram + virt_addr);
                                        else mem_mapping_set_exec(&scat_ems_mapping[index], NULL);
                                        mem_mapping_enable(&scat_ems_mapping[index]);
//                                        pclog("Map page %d(address %05X) to address %06X\n", index, base_addr, virt_addr);
                                }
                                else
                                {
                                        mem_mapping_set_exec(&scat_ems_mapping[index], ram + base_addr);
                                        mem_mapping_disable(&scat_ems_mapping[index]);
                                        if(index < 24) mem_mapping_enable(&scat_4000_EFFF_mapping[index]);
                                        else mem_mapping_enable(&scat_4000_EFFF_mapping[index + 12]);
//                                        pclog("Unmap page %d(address %06X)\n", index, base_addr);
                                }
                                flushmmucache();
                        }

                        if (scat_ems_reg_2xA & 0x80)
                        {
                                scat_ems_reg_2xA = (scat_ems_reg_2xA & 0xe0) | ((scat_ems_reg_2xA + 1) & (((scat_regs[SCAT_VERSION] & 0xF0) == 0) ? 0x1f : 3));
                        }
                }
                break;
                case 0x20A:
                case 0x21A:
                if ((scat_regs[SCAT_EMS_CONTROL] & 0x41) == (0x40 | ((port & 0x10) >> 4)))
                {
//                        pclog("Write SCAT EMS Control Port %04X to %02X at %04X:%04X\n", port, val, CS, cpu_state.pc);
                        scat_ems_reg_2xA = ((scat_regs[SCAT_VERSION] & 0xF0) == 0) ? val : val & 0xc3;
                }
                break;
        }
}

uint8_t scat_read(uint16_t port, void *priv)
{
        uint8_t val = 0xff, index;
        switch (port)
        {
                case 0x23:
                switch (scat_index)
                {
                        case SCAT_MISCELLANEOUS_STATUS:
                        val = (scat_regs[scat_index] & 0x3f) | (~nmi_mask & 0x80) | ((mem_a20_key & 2) << 5);
                        break;
                        case SCAT_DRAM_CONFIGURATION:
                        if ((scat_regs[SCAT_VERSION] & 0xF0) == 0) val = (scat_regs[scat_index] & 0x8f) | (cpu_waitstates == 1 ? 0 : 0x10);
                        else val = scat_regs[scat_index];
                        break;
                        case SCAT_EXTENDED_BOUNDARY:
                        val = scat_regs[scat_index];
                        if ((scat_regs[SCAT_VERSION] & 0xF0) == 0)
                        {
                                if((scat_regs[SCAT_VERSION] & 0x0F) >= 4)
                                        val |= 0x80;
                                else
                                        val &= 0xAF;
                        }
                        break;
                        default:
                        val = scat_regs[scat_index];
                        break;
                }
//                pclog("Read SCAT Register %02X at %04X:%04X\n", scat_index, CS, cpu_state.pc);
                break;

                case 0x92:
                val = scat_port_92;
                break;

                case 0x208:
                case 0x218:
                if ((scat_regs[SCAT_EMS_CONTROL] & 0x41) == (0x40 | ((port & 0x10) >> 4)))
                {
//                        pclog("Read SCAT EMS Control Port %04X at %04X:%04X\n", port, CS, cpu_state.pc);
                        if((scat_regs[SCAT_VERSION] & 0xF0) == 0) index = scat_ems_reg_2xA & 0x1F;
                        else index = ((scat_ems_reg_2xA & 0x40) >> 4) + (scat_ems_reg_2xA & 0x3) + 24;
                        val = scat_stat[index].regs_2x8;
                }
                break;
                case 0x209:
                case 0x219:
                if ((scat_regs[SCAT_EMS_CONTROL] & 0x41) == (0x40 | ((port & 0x10) >> 4)))
                {
//                        pclog("Read SCAT EMS Control Port %04X at %04X:%04X\n", port, CS, cpu_state.pc);
                        if((scat_regs[SCAT_VERSION] & 0xF0) == 0) index = scat_ems_reg_2xA & 0x1F;
                        else index = ((scat_ems_reg_2xA & 0x40) >> 4) + (scat_ems_reg_2xA & 0x3) + 24;
                        val = scat_stat[index].regs_2x9;
                }
                break;
                case 0x20A:
                case 0x21A:
                if ((scat_regs[SCAT_EMS_CONTROL] & 0x41) == (0x40 | ((port & 0x10) >> 4)))
                {
//                        pclog("Read SCAT EMS Control Port %04X at %04X:%04X\n", port, CS, cpu_state.pc);
                        val = scat_ems_reg_2xA;
                }
                break;
        }
        return val;
}

uint8_t mem_read_scatb(uint32_t addr, void *priv)
{
        uint8_t val = 0xff;
        scat_t *stat = (scat_t *)priv;

        addr = get_scat_addr(addr, stat);
        if (addr < (mem_size << 10))
                val = ram[addr];

        return val;
}

void mem_write_scatb(uint32_t addr, uint8_t val, void *priv)
{
        scat_t *stat = (scat_t *)priv;
        uint32_t oldaddr = addr, chkaddr;

        addr = get_scat_addr(addr, stat);
        chkaddr = stat ? addr : oldaddr;
        if (chkaddr >= 0xC0000 && chkaddr < 0x100000)
        {
                if(scat_regs[SCAT_RAM_WRITE_PROTECT] & (1 << ((chkaddr - 0xC0000) >> 15))) return;
        }
        if (addr < (mem_size << 10))
                  ram[addr] = val;
}

uint16_t mem_read_scatw(uint32_t addr, void *priv)
{
        uint16_t val = 0xffff;
        scat_t *stat = (scat_t *)priv;

        if((addr & 0x3FFF) == 0x3FFF) pclog("mem_read_scatw(%08X, %p) called.\n", addr, priv);
        addr = get_scat_addr(addr, stat);
        if (addr < (mem_size << 10))
                val = *(uint16_t *)&ram[addr];

        return val;
}

void mem_write_scatw(uint32_t addr, uint16_t val, void *priv)
{
        scat_t *stat = (scat_t *)priv;
        uint32_t oldaddr = addr, chkaddr;

        if((addr & 0x3FFF) == 0x3FFF) pclog("mem_write_scatw(%08X, %04X, %p) called.\n", addr, val, priv);
        addr = get_scat_addr(addr, stat);
        chkaddr = stat ? addr : oldaddr;
        if (chkaddr >= 0xC0000 && chkaddr < 0x100000)
        {
                if(scat_regs[SCAT_RAM_WRITE_PROTECT] & (1 << ((chkaddr - 0xC0000) >> 15))) return;
        }
        if (addr < (mem_size << 10))
                  *(uint16_t *)&ram[addr] = val;
}

uint32_t mem_read_scatl(uint32_t addr, void *priv)
{
        uint32_t val = 0xffffffff;
        scat_t *stat = (scat_t *)priv;

        if((addr & 0x3FFF) > 0x3FFC) pclog("mem_read_scatl(%08X, %p) called.\n", addr, priv);
        addr = get_scat_addr(addr, stat);
        if (addr < (mem_size << 10))
                val = *(uint32_t *)&ram[addr];

        return val;
}

void mem_write_scatl(uint32_t addr, uint32_t val, void *priv)
{
        scat_t *stat = (scat_t *)priv;
        uint32_t oldaddr = addr, chkaddr;

        if((addr & 0x3FFF) > 0x3FFC) pclog("mem_write_scatl(%08X, %08X, %p) called.\n", addr, val, priv);
        addr = get_scat_addr(addr, stat);
        chkaddr = stat ? addr : oldaddr;
        if (chkaddr >= 0xC0000 && chkaddr < 0x100000)
        {
                if(scat_regs[SCAT_RAM_WRITE_PROTECT] & (1 << ((chkaddr - 0xC0000) >> 15))) return;
        }
        if (addr < (mem_size << 10))
                  *(uint32_t *)&ram[addr] = val;
}

void scat_init()
{
        int i;

        io_sethandler(0x0022, 0x0002, scat_read, NULL, NULL, scat_write, NULL, NULL,  NULL);
        io_sethandler(0x0092, 0x0001, scat_read, NULL, NULL, scat_write, NULL, NULL,  NULL);

        for (i = 0; i < 256; i++)
        {
                scat_regs[i] = 0xff;
        }

        scat_regs[SCAT_DMA_WAIT_STATE_CONTROL] = 0;
        switch(romset)
        {
                case ROM_GW286CT:
                case ROM_SPC4216P:
                case ROM_SPC4620P:
                scat_regs[SCAT_VERSION] = 4;
                break;
                default:
                scat_regs[SCAT_VERSION] = 1;
                break;
        }
        scat_regs[SCAT_CLOCK_CONTROL] = 2;
        scat_regs[SCAT_PERIPHERAL_CONTROL] = 0x80;
        scat_regs[SCAT_MISCELLANEOUS_STATUS] = 0x37;
        scat_regs[SCAT_POWER_MANAGEMENT] = 0;
        scat_regs[SCAT_ROM_ENABLE] = 0xC0;
        scat_regs[SCAT_RAM_WRITE_PROTECT] = 0;
        scat_regs[SCAT_SHADOW_RAM_ENABLE_1] = 0;
        scat_regs[SCAT_SHADOW_RAM_ENABLE_2] = 0;
        scat_regs[SCAT_SHADOW_RAM_ENABLE_3] = 0;
        scat_regs[SCAT_DRAM_CONFIGURATION] = cpu_waitstates == 1 ? 2 : 0x12;
        scat_regs[SCAT_EXTENDED_BOUNDARY] = 0;
        scat_regs[SCAT_EMS_CONTROL] = 0;
        scat_port_92 = 0;

        mem_mapping_disable(&ram_low_mapping);
        mem_mapping_disable(&ram_mid_mapping);
        mem_mapping_disable(&ram_high_mapping);
        for (i = 0; i < 4; i++)
                mem_mapping_disable(&bios_mapping[i]);
        mem_mapping_add(&scat_low_mapping[0], 0, 0x40000, mem_read_scatb, mem_read_scatw, mem_read_scatl, mem_write_scatb, mem_write_scatw, mem_write_scatl, ram, MEM_MAPPING_INTERNAL, NULL);
        mem_mapping_add(&scat_low_mapping[1], 0xF0000, 0x10000, mem_read_scatb, mem_read_scatw, mem_read_scatl, mem_write_scatb, mem_write_scatw, mem_write_scatl, ram + 0xF0000, MEM_MAPPING_INTERNAL, NULL);
        for(i=2;i<32;i++)
        {
                mem_mapping_add(&scat_low_mapping[i], (i << 19), 0x80000, mem_read_scatb, mem_read_scatw, mem_read_scatl, mem_write_scatb, mem_write_scatw, mem_write_scatl, ram + (i << 19), MEM_MAPPING_INTERNAL, NULL);
        }
        mem_mapping_set_addr(&scat_low_mapping[31], 0xF80000, scat_regs[SCAT_VERSION] < 4 ? 0x40000 : 0x60000);

        for (i = 0; i < 44; i++)
                mem_mapping_add(&scat_4000_EFFF_mapping[i], 0x40000 + (i << 14), 0x4000, mem_read_scatb, mem_read_scatw, mem_read_scatl, mem_write_scatb, mem_write_scatw, mem_write_scatl, mem_size > 256 + (i << 4) ? ram + 0x40000 + (i << 14) : NULL, MEM_MAPPING_INTERNAL, NULL);
        for (i = 0; i < 8; i++)
        {
                mem_mapping_add(&scat_low_ROMCS_mapping[i], 0xC0000 + (i << 14), 0x4000, mem_read_bios, mem_read_biosw, mem_read_biosl, mem_write_null, mem_write_nullw, mem_write_nulll, rom + ((i << 14) & biosmask), MEM_MAPPING_EXTERNAL|MEM_MAPPING_ROM, NULL);
                mem_mapping_disable(&scat_low_ROMCS_mapping[i]);
        }

        for (i = 0; i < 32; i++)
        {
                scat_stat[i].regs_2x8 = 0xff;
                scat_stat[i].regs_2x9 = 0x03;
                mem_mapping_add(&scat_ems_mapping[i], (i + (i >= 24 ? 28 : 16)) << 14, 0x04000, mem_read_scatb, mem_read_scatw, mem_read_scatl, mem_write_scatb, mem_write_scatw, mem_write_scatl, ram + ((i + (i >= 24 ? 28 : 16)) << 14), 0, &scat_stat[i]);
        }

        for (i = (scat_regs[SCAT_VERSION] < 4 ? 0 : 8); i < 16; i++)
        {
                mem_mapping_add(&scat_high_mapping[i], (i << 14) + 0xFC0000, 0x04000, mem_read_bios, mem_read_biosw, mem_read_biosl, mem_write_null, mem_write_nullw, mem_write_nulll, rom + ((i << 14) & biosmask), 0, NULL);
                mem_mapping_enable(&scat_high_mapping[i]);
        }

        for(i=0;i<6;i++)
                mem_mapping_add(&scat_remap_mapping[i], 0x100000 + (i << 16), 0x10000, mem_read_scatb, mem_read_scatw, mem_read_scatl, mem_write_scatb, mem_write_scatw, mem_write_scatl, mem_size >= 1024 ? ram + get_scat_addr(0x100000 + (i << 16), NULL) : NULL, MEM_MAPPING_INTERNAL, NULL);

        external_is_RAS = (scat_regs[SCAT_VERSION] > 3) || (((mem_size & ~2047) >> 11) + ((mem_size & 1536) >> 9) + ((mem_size & 511) >> 7)) > 4;

        scat_set_xms_bound(0);
        scat_memmap_state_update();
        scat_shadow_state_update();
}

void scatsx_init()
{
        int i;

        io_sethandler(0x0022, 0x0002, scat_read, NULL, NULL, scat_write, NULL, NULL,  NULL);
        io_sethandler(0x0092, 0x0001, scat_read, NULL, NULL, scat_write, NULL, NULL,  NULL);

        for (i = 0; i < 256; i++)
        {
                scat_regs[i] = 0xff;
        }

        scat_regs[SCAT_DMA_WAIT_STATE_CONTROL] = 0;
        scat_regs[SCAT_VERSION] = 0x13;
        scat_regs[SCAT_CLOCK_CONTROL] = 6;
        scat_regs[SCAT_PERIPHERAL_CONTROL] = 0;
        scat_regs[SCAT_MISCELLANEOUS_STATUS] = 0x37;
        scat_regs[SCAT_POWER_MANAGEMENT] = 0;
        scat_regs[SCAT_ROM_ENABLE] = 0xC0;
        scat_regs[SCAT_RAM_WRITE_PROTECT] = 0;
        scat_regs[SCAT_SHADOW_RAM_ENABLE_1] = 0;
        scat_regs[SCAT_SHADOW_RAM_ENABLE_2] = 0;
        scat_regs[SCAT_SHADOW_RAM_ENABLE_3] = 0;
        scat_regs[SCAT_DRAM_CONFIGURATION] = 1;
        scat_regs[SCAT_EXTENDED_BOUNDARY] = 0;
        scat_regs[SCAT_EMS_CONTROL] = 0;
        scat_regs[SCATSX_LAPTOP_FEATURES] = 0;
        scat_regs[SCATSX_FAST_VIDEO_CONTROL] = 0;
        scat_regs[SCATSX_FAST_VIDEORAM_ENABLE] = 0;
        scat_regs[SCATSX_HIGH_PERFORMANCE_REFRESH] = 8;
        scat_regs[SCATSX_CAS_TIMING_FOR_DMA] = 3;
        scat_port_92 = 0;

        mem_mapping_disable(&ram_low_mapping);
        mem_mapping_disable(&ram_mid_mapping);
        mem_mapping_disable(&ram_high_mapping);
        for (i = 0; i < 4; i++)
                mem_mapping_disable(&bios_mapping[i]);
        mem_mapping_add(&scat_low_mapping[0], 0, 0x80000, mem_read_scatb, mem_read_scatw, mem_read_scatl, mem_write_scatb, mem_write_scatw, mem_write_scatl, ram, MEM_MAPPING_INTERNAL, NULL);
        mem_mapping_add(&scat_low_mapping[1], 0xF0000, 0x10000, mem_read_scatb, mem_read_scatw, mem_read_scatl, mem_write_scatb, mem_write_scatw, mem_write_scatl, ram + 0xF0000, MEM_MAPPING_INTERNAL, NULL);
        for(i=2;i<32;i++)
        {
                mem_mapping_add(&scat_low_mapping[i], (i << 19), 0x80000, mem_read_scatb, mem_read_scatw, mem_read_scatl, mem_write_scatb, mem_write_scatw, mem_write_scatl, ram + (i << 19), MEM_MAPPING_INTERNAL, NULL);
        }
        mem_mapping_set_addr(&scat_low_mapping[31], 0xF80000, 0x40000);

        for (i = 16; i < 44; i++)
        {
                mem_mapping_add(&scat_4000_EFFF_mapping[i], 0x40000 + (i << 14), 0x4000, mem_read_scatb, mem_read_scatw, mem_read_scatl, mem_write_scatb, mem_write_scatw, mem_write_scatl, mem_size > 256 + (i << 4) ? ram + 0x40000 + (i << 14) : NULL, MEM_MAPPING_INTERNAL, NULL);
                mem_mapping_enable(&scat_4000_EFFF_mapping[i]);
        }
        for (i = 0; i < 8; i++)
        {
                mem_mapping_add(&scat_low_ROMCS_mapping[i], 0xC0000 + (i << 14), 0x4000, mem_read_bios, mem_read_biosw, mem_read_biosl, mem_write_null, mem_write_nullw, mem_write_nulll, rom + ((i << 14) & biosmask), MEM_MAPPING_EXTERNAL|MEM_MAPPING_ROM, NULL);
                mem_mapping_disable(&scat_low_ROMCS_mapping[i]);
        }

        for (i = 24; i < 32; i++)
        {
                scat_stat[i].regs_2x8 = 0xff;
                scat_stat[i].regs_2x9 = 0x03;
                mem_mapping_add(&scat_ems_mapping[i], (i + 28) << 14, 0x04000, mem_read_scatb, mem_read_scatw, mem_read_scatl, mem_write_scatb, mem_write_scatw, mem_write_scatl, ram + ((i + 28) << 14), 0, &scat_stat[i]);
                mem_mapping_disable(&scat_ems_mapping[i]);
        }

        for (i = 0; i < 16; i++)
        {
                mem_mapping_add(&scat_high_mapping[i], (i << 14) + 0xFC0000, 0x04000, mem_read_bios, mem_read_biosw, mem_read_biosl, mem_write_null, mem_write_nullw, mem_write_nulll, rom + ((i << 14) & biosmask), 0, NULL);
                mem_mapping_enable(&scat_high_mapping[i]);
        }

        for(i=0;i<6;i++)
                mem_mapping_add(&scat_remap_mapping[i], 0x100000 + (i << 16), 0x10000, mem_read_scatb, mem_read_scatw, mem_read_scatl, mem_write_scatb, mem_write_scatw, mem_write_scatl, mem_size >= 1024 ? ram + get_scat_addr(0x100000 + (i << 16), NULL) : NULL, MEM_MAPPING_INTERNAL, NULL);

        external_is_RAS = scatsx_external_is_RAS[mem_size >> 9];

        scat_set_xms_bound(0);
        scat_memmap_state_update();
        scat_shadow_state_update();
}
