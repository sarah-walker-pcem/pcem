#include "ibm.h"
#include "io.h"
#include "mem.h"
#include "cpu.h"
#include "x86.h"

#include "headland.h"

static int headland_index;
static uint8_t headland_regs[256];
static uint8_t headland_port_92 = 0xFC, headland_ems_mar = 0, headland_cri = 0;
static uint8_t headland_regs_cr[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static uint16_t headland_ems_mr[64];

static mem_mapping_t headland_low_mapping;
static mem_mapping_t headland_ems_mapping[64];
static mem_mapping_t headland_mid_mapping;
static mem_mapping_t headland_high_mapping;
static mem_mapping_t headland_4000_9FFF_mapping[24];

// TODO - Headland chipset's memory address mapping emulation isn't fully implemented yet, so memory configuration is hardcoded
// now.
static int headland_mem_conf_cr0[41] = {0x00, 0x00, 0x20, 0x40, 0x60, 0xA0, 0x40, 0xE0, 0xA0, 0xC0, 0xE0, 0xE0, 0xC0, 0xE0,
                                        0xE0, 0xE0, 0xE0, 0x20, 0x40, 0x40, 0xA0, 0xC0, 0xE0, 0xE0, 0xC0, 0xE0, 0xE0, 0xE0,
                                        0xE0, 0xE0, 0xE0, 0xE0, 0x20, 0x40, 0x60, 0x60, 0xC0, 0xE0, 0xE0, 0xE0, 0xE0};
static int headland_mem_conf_cr1[41] = {0x00, 0x40, 0x00, 0x00, 0x00, 0x40, 0x40, 0x40, 0x00, 0x40, 0x40, 0x40, 0x00, 0x00,
                                        0x00, 0x00, 0x00, 0x40, 0x40, 0x40, 0x00, 0x00, 0x00, 0x00, 0x40, 0x40, 0x40, 0x40,
                                        0x40, 0x40, 0x40, 0x40, 0x00, 0x00, 0x40, 0x40, 0x00, 0x00, 0x00, 0x00, 0x40};

uint32_t get_headland_addr(uint32_t addr, uint16_t *mr) {
        if (mr && (headland_regs_cr[0] & 2) && (*mr & 0x200)) {
                addr = (addr & 0x3fff) | ((*mr & 0x1F) << 14);

                if (headland_regs_cr[1] & 0x40) {
                        if ((headland_regs_cr[4] & 0x80) && (headland_regs_cr[6] & 1)) {
                                if (headland_regs_cr[0] & 0x80) {
                                        addr |= (*mr & 0x60) << 14;
                                        if (*mr & 0x100)
                                                addr += ((*mr & 0xC00) << 13) + (((*mr & 0x80) + 0x80) << 15);
                                        else
                                                addr += (*mr & 0x80) << 14;
                                } else if (*mr & 0x100)
                                        addr += ((*mr & 0xC00) << 13) + (((*mr & 0x80) + 0x20) << 15);
                                else
                                        addr += (*mr & 0x80) << 12;
                        } else if (headland_regs_cr[0] & 0x80)
                                addr |= (*mr & 0x100) ? ((*mr & 0x80) + 0x400) << 12 : (*mr & 0xE0) << 14;
                        else
                                addr |= (*mr & 0x100) ? ((*mr & 0xE0) + 0x40) << 14 : (*mr & 0x80) << 12;
                } else {
                        if ((headland_regs_cr[4] & 0x80) && (headland_regs_cr[6] & 1)) {
                                if (headland_regs_cr[0] & 0x80) {
                                        addr |= ((*mr & 0x60) << 14);
                                        if (*mr & 0x180)
                                                addr += ((*mr & 0xC00) << 13) + (((*mr & 0x180) - 0x60) << 16);
                                } else
                                        addr |= ((*mr & 0x60) << 14) | ((*mr & 0x180) << 16) | ((*mr & 0xC00) << 13);
                        } else if (headland_regs_cr[0] & 0x80)
                                addr |= (*mr & 0x1E0) << 14;
                        else
                                addr |= (*mr & 0x180) << 12;
                }
        } else if (mr == NULL && (headland_regs_cr[0] & 4) == 0 && mem_size >= 1024 && addr >= 0x100000)
                addr -= 0x60000;

        return addr;
}

void headland_set_global_EMS_state(int state) {
        int i;
        uint32_t base_addr, virt_addr;

        for (i = 0; i < 32; i++) {
                base_addr = (i + 16) << 14;
                if (i >= 24)
                        base_addr += 0x20000;
                if ((state & 2) && (headland_ems_mr[((state & 1) << 5) | i] & 0x200)) {
                        virt_addr = get_headland_addr(base_addr, &headland_ems_mr[((state & 1) << 5) | i]);
                        if (i < 24)
                                mem_mapping_disable(&headland_4000_9FFF_mapping[i]);
                        mem_mapping_disable(&headland_ems_mapping[(((state ^ 1) & 1) << 5) | i]);
                        mem_mapping_enable(&headland_ems_mapping[((state & 1) << 5) | i]);
                        if (virt_addr < (mem_size << 10))
                                mem_mapping_set_exec(&headland_ems_mapping[((state & 1) << 5) | i], ram + virt_addr);
                        else
                                mem_mapping_set_exec(&headland_ems_mapping[((state & 1) << 5) | i], NULL);
                } else {
                        mem_mapping_set_exec(&headland_ems_mapping[((state & 1) << 5) | i], ram + base_addr);
                        mem_mapping_disable(&headland_ems_mapping[(((state ^ 1) & 1) << 5) | i]);
                        mem_mapping_disable(&headland_ems_mapping[((state & 1) << 5) | i]);

                        if (i < 24)
                                mem_mapping_enable(&headland_4000_9FFF_mapping[i]);
                }
        }
        flushmmucache();
}

void headland_memmap_state_update() {
        int i;
        uint32_t addr;

        for (i = 0; i < 24; i++) {
                addr = get_headland_addr(0x40000 + (i << 14), NULL);
                mem_mapping_set_exec(&headland_4000_9FFF_mapping[i], addr < (mem_size << 10) ? ram + addr : NULL);
                // pclog("headland_memmap_state_update : Address %06X to %06X\n", 0x40000 + (i << 14), addr);
        }

        mem_set_mem_state(0xA0000, 0x40000, MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);

        if (mem_size > 640) {
                if ((headland_regs_cr[0] & 4) == 0) {
                        mem_mapping_set_addr(&headland_mid_mapping, 0x100000, mem_size > 1024 ? 0x60000 : (mem_size - 640) << 10);
                        mem_mapping_set_exec(&headland_mid_mapping, ram + 0xA0000);
                        if (mem_size > 1024) {
                                mem_mapping_set_addr(&headland_high_mapping, 0x160000, (mem_size - 1024) << 10);
                                mem_mapping_set_exec(&headland_high_mapping, ram + 0x100000);
                        }
                } else {
                        mem_mapping_set_addr(&headland_mid_mapping, 0xA0000, mem_size > 1024 ? 0x60000 : (mem_size - 640) << 10);
                        mem_mapping_set_exec(&headland_mid_mapping, ram + 0xA0000);
                        if (mem_size > 1024) {
                                mem_mapping_set_addr(&headland_high_mapping, 0x100000, (mem_size - 1024) << 10);
                                mem_mapping_set_exec(&headland_high_mapping, ram + 0x100000);
                        }
                }
        }

        headland_set_global_EMS_state(headland_regs_cr[0] & 3);
}

void headland_write(uint16_t addr, uint8_t val, void *priv) {
        uint8_t old_val, index;
        uint32_t base_addr, virt_addr;

        switch (addr) {
        case 0x22:
                headland_index = val;
                break;

        case 0x23:
                old_val = headland_regs[headland_index];

                if (headland_index == 0xc1 && !is486)
                        val = 0;
                headland_regs[headland_index] = val;
                // pclog("Headland write %02X %02X at %04X:%04X\n",headland_index,val,CS,cpu_state.oldpc);
                if (headland_index == 0x82) {
                        if (val & 0x10)
                                mem_set_mem_state(0xf0000, 0x10000, MEM_READ_INTERNAL | MEM_WRITE_DISABLED);
                        else
                                mem_set_mem_state(0xf0000, 0x10000, MEM_READ_EXTERNAL | MEM_WRITE_INTERNAL);
                } else if (headland_index == 0x87) {
                        if ((val & 1) && !(old_val & 1))
                                softresetx86();
                }
                break;

        case 0x92:
                if ((mem_a20_alt ^ val) & 2) {
                        mem_a20_alt = val & 2;
                        mem_a20_recalc();
                }
                if ((~headland_port_92 & val) & 1) {
                        softresetx86();
                        cpu_set_edx();
                }
                headland_port_92 = val | 0xFC;
                break;

        case 0x1EC:
                // pclog("Set EMS Map Register to %02X(actually FF%02X) at %04X:%04X\n", val, val, CS, cpu_state.oldpc);
                headland_ems_mr[headland_ems_mar & 0x3F] = val | 0xFF00;
                index = headland_ems_mar & 0x1F;
                base_addr = (index + 16) << 14;
                if (index >= 24)
                        base_addr += 0x20000;
                if ((headland_regs_cr[0] & 2) && ((headland_regs_cr[0] & 1) == ((headland_ems_mar & 0x20) >> 5))) {
                        virt_addr = get_headland_addr(base_addr, &headland_ems_mr[headland_ems_mar & 0x3F]);
                        if (index < 24)
                                mem_mapping_disable(&headland_4000_9FFF_mapping[index]);
                        if (virt_addr < (mem_size << 10))
                                mem_mapping_set_exec(&headland_ems_mapping[headland_ems_mar & 0x3F], ram + virt_addr);
                        else
                                mem_mapping_set_exec(&headland_ems_mapping[headland_ems_mar & 0x3F], NULL);
                        mem_mapping_enable(&headland_ems_mapping[headland_ems_mar & 0x3F]);
                        pclog("Map page %d(address %05X) to address %06X\n", index, base_addr, virt_addr);
                        flushmmucache();
                }
                if (headland_ems_mar & 0x80)
                        headland_ems_mar++;
                break;

        case 0x1ED:
                // pclog("Set Control Register Index to %02X at %04X:%04X\n", val, CS, cpu_state.oldpc);
                headland_cri = val;
                break;

        case 0x1EE:
                // pclog("Set EMS Map Address Register to %02X at %04X:%04X\n", val, CS, cpu_state.oldpc);
                headland_ems_mar = val;
                break;

        case 0x1EF:
                // pclog("Set Control Register %1X to %02X at %04X:%04X\n", headland_cri, val, CS, cpu_state.oldpc);
                old_val = headland_regs_cr[headland_cri];
                switch (headland_cri) {
                case 0:
                        headland_regs_cr[0] =
                                (val & 0x1F) | headland_mem_conf_cr0[(mem_size > 640 ? mem_size : mem_size - 128) >> 9];
                        mem_set_mem_state(0xE0000, 0x10000,
                                          (val & 8 ? MEM_READ_INTERNAL : MEM_READ_EXTERNAL) | MEM_WRITE_DISABLED);
                        mem_set_mem_state(0xF0000, 0x10000,
                                          (val & 0x10 ? MEM_READ_INTERNAL : MEM_READ_EXTERNAL) | MEM_WRITE_DISABLED);
                        headland_memmap_state_update();
                        break;
                case 1:
                        headland_regs_cr[1] =
                                (val & 0xBF) | headland_mem_conf_cr1[(mem_size > 640 ? mem_size : mem_size - 128) >> 9];
                        headland_memmap_state_update();
                        break;
                case 2:
                case 3:
                case 5:
                        headland_regs_cr[headland_cri] = val;
                        headland_memmap_state_update();
                        break;
                case 4:
                        headland_regs_cr[4] = (headland_regs_cr[4] & 0xF0) | (val & 0x0F);
                        if (val & 1) {
                                mem_mapping_disable(&bios_mapping[0]);
                                mem_mapping_disable(&bios_mapping[1]);
                                mem_mapping_disable(&bios_mapping[2]);
                                mem_mapping_disable(&bios_mapping[3]);
                        } else {
                                mem_mapping_enable(&bios_mapping[0]);
                                mem_mapping_enable(&bios_mapping[1]);
                                mem_mapping_enable(&bios_mapping[2]);
                                mem_mapping_enable(&bios_mapping[3]);
                        }
                        break;
                case 6:
                        if (headland_regs_cr[4] & 0x80) {
                                headland_regs_cr[headland_cri] = (val & 0xFE) | (mem_size > 8192 ? 1 : 0);
                                headland_memmap_state_update();
                        }
                        break;
                default:
                        break;
                }
                break;

        default:
                break;
        }
}

void headland_writew(uint16_t addr, uint16_t val, void *priv) {
        uint8_t index;
        uint32_t base_addr, virt_addr;

        switch (addr) {
        case 0x1EC:
                // pclog("Set EMS Map Register to %04X at %04X:%04X\n", val, CS, cpu_state.oldpc);
                headland_ems_mr[headland_ems_mar & 0x3F] = val;
                index = headland_ems_mar & 0x1F;
                base_addr = (index + 16) << 14;
                if (index >= 24)
                        base_addr += 0x20000;
                if ((headland_regs_cr[0] & 2) && (headland_regs_cr[0] & 1) == ((headland_ems_mar & 0x20) >> 5)) {
                        if (val & 0x200) {
                                virt_addr = get_headland_addr(base_addr, &headland_ems_mr[headland_ems_mar & 0x3F]);
                                if (index < 24)
                                        mem_mapping_disable(&headland_4000_9FFF_mapping[index]);
                                if (virt_addr < (mem_size << 10))
                                        mem_mapping_set_exec(&headland_ems_mapping[headland_ems_mar & 0x3F], ram + virt_addr);
                                else
                                        mem_mapping_set_exec(&headland_ems_mapping[headland_ems_mar & 0x3F], NULL);
                                mem_mapping_enable(&headland_ems_mapping[headland_ems_mar & 0x3F]);
                                // pclog("Map page %d(address %05X) to address %07X(val=%04X)\n", index, base_addr, virt_addr,
                                // val);
                        } else {
                                mem_mapping_set_exec(&headland_ems_mapping[headland_ems_mar & 0x3F], ram + base_addr);
                                mem_mapping_disable(&headland_ems_mapping[headland_ems_mar & 0x3F]);
                                if (index < 24)
                                        mem_mapping_enable(&headland_4000_9FFF_mapping[index]);
                                // pclog("Unmap page %d(address %05X)\n", index, base_addr);
                        }
                        flushmmucache();
                }
                if (headland_ems_mar & 0x80)
                        headland_ems_mar++;
                break;

        default:
                break;
        }
}

uint8_t headland_read(uint16_t addr, void *priv) {
        uint8_t val;

        switch (addr) {
        case 0x22:
                val = headland_index;
                break;

        case 0x23:
                if ((headland_index >= 0xc0 || headland_index == 0x20) && cpu_iscyrix)
                        val = 0xff; /*Don't conflict with Cyrix config registers*/
                else
                        val = headland_regs[headland_index];
                break;

        case 0x92:
                val = headland_port_92 | 0xFC;
                break;

        case 0x1EC:
                val = headland_ems_mr[headland_ems_mar & 0x3F];
                if (headland_ems_mar & 0x80)
                        headland_ems_mar++;
                break;

        case 0x1ED:
                val = headland_cri;
                break;

        case 0x1EE:
                val = headland_ems_mar;
                break;

        case 0x1EF:
                switch (headland_cri) {
                case 0:
                        val = (headland_regs_cr[0] & 0x1F) |
                              headland_mem_conf_cr0[(mem_size > 640 ? mem_size : mem_size - 128) >> 9];
                        break;
                case 1:
                        val = (headland_regs_cr[1] & 0xBF) |
                              headland_mem_conf_cr1[(mem_size > 640 ? mem_size : mem_size - 128) >> 9];
                        break;
                case 6:
                        if (headland_regs_cr[4] & 0x80)
                                val = (headland_regs_cr[6] & 0xFE) | (mem_size > 8192 ? 1 : 0);
                        else
                                val = 0;
                        break;
                default:
                        val = headland_regs_cr[headland_cri];
                        break;
                }
                break;

        default:
                val = 0xFF;
                break;
        }

        return val;
}

uint16_t headland_readw(uint16_t addr, void *priv) {
        uint16_t val;

        switch (addr) {
        case 0x1EC:
                val = headland_ems_mr[headland_ems_mar & 0x3F] | ((headland_regs_cr[4] & 0x80) ? 0xF000 : 0xFC00);
                if (headland_ems_mar & 0x80)
                        headland_ems_mar++;
                break;

        default:
                val = 0xFFFF;
                break;
        }

        return val;
}

uint8_t mem_read_headlandb(uint32_t addr, void *priv) {
        uint8_t val = 0xff;

        addr = get_headland_addr(addr, priv);
        if (addr < (mem_size << 10))
                val = ram[addr];

        return val;
}

void mem_write_headlandb(uint32_t addr, uint8_t val, void *priv) {
        addr = get_headland_addr(addr, priv);
        if (addr < (mem_size << 10))
                ram[addr] = val;
}

uint16_t mem_read_headlandw(uint32_t addr, void *priv) {
        uint16_t val = 0xffff;

        if ((addr & 0x3FFF) == 0x3FFF)
                pclog("mem_read_headlandw(%08X, %p) called.\n", addr, priv);
        addr = get_headland_addr(addr, priv);
        if (addr < (mem_size << 10))
                val = *(uint16_t *)&ram[addr];

        return val;
}

void mem_write_headlandw(uint32_t addr, uint16_t val, void *priv) {
        if ((addr & 0x3FFF) == 0x3FFF)
                pclog("mem_write_headlandw(%08X, %04X, %p) called.\n", addr, val, priv);
        addr = get_headland_addr(addr, priv);
        if (addr < (mem_size << 10))
                *(uint16_t *)&ram[addr] = val;
}

uint32_t mem_read_headlandl(uint32_t addr, void *priv) {
        uint32_t val = 0xffffffff;

        if ((addr & 0x3FFF) > 0x3FFC)
                pclog("mem_read_headlandl(%08X, %p) called.\n", addr, priv);
        addr = get_headland_addr(addr, priv);
        if (addr < (mem_size << 10))
                val = *(uint32_t *)&ram[addr];

        return val;
}

void mem_write_headlandl(uint32_t addr, uint32_t val, void *priv) {
        if ((addr & 0x3FFF) > 0x3FFC)
                pclog("mem_write_headland(%08X, %08X, %p) called.\n", addr, val, priv);
        addr = get_headland_addr(addr, priv);
        if (addr < (mem_size << 10))
                *(uint32_t *)&ram[addr] = val;
}

void headland_init() {
        int i;

        for (i = 0; i < 8; i++)
                headland_regs_cr[i] = 0;
        headland_regs_cr[0] = 4;

        switch (romset) {
        case ROM_AMI386SX:
                // Remark - Previously distributed AMI386 BIOS doesn't seem to be for the Headland chipset.
                io_sethandler(0x0022, 0x0002, headland_read, NULL, NULL, headland_write, NULL, NULL, NULL);
                break;
        case ROM_AMA932J:
                headland_regs_cr[4] = 0x20;
                io_sethandler(0x0092, 0x0001, headland_read, NULL, NULL, headland_write, NULL, NULL, NULL);
                break;
        default:
                headland_regs_cr[4] = 0;
                break;
        }

        io_sethandler(0x01EC, 0x0001, headland_read, headland_readw, NULL, headland_write, headland_writew, NULL, NULL);
        io_sethandler(0x01ED, 0x0003, headland_read, NULL, NULL, headland_write, NULL, NULL, NULL);

        for (i = 0; i < 64; i++)
                headland_ems_mr[i] = 0;

        mem_mapping_disable(&ram_low_mapping);
        mem_mapping_disable(&ram_mid_mapping);
        mem_mapping_disable(&ram_high_mapping);

        mem_mapping_add(&headland_low_mapping, 0, 0x40000, mem_read_headlandb, mem_read_headlandw, mem_read_headlandl,
                        mem_write_headlandb, mem_write_headlandw, mem_write_headlandl, ram, MEM_MAPPING_INTERNAL, NULL);

        if (mem_size > 640) {
                mem_mapping_add(&headland_mid_mapping, 0xA0000, 0x60000, mem_read_headlandb, mem_read_headlandw,
                                mem_read_headlandl, mem_write_headlandb, mem_write_headlandw, mem_write_headlandl, ram + 0xA0000,
                                MEM_MAPPING_INTERNAL, NULL);
                mem_mapping_enable(&headland_mid_mapping);
        }

        if (mem_size > 1024) {
                mem_mapping_add(&headland_high_mapping, 0x100000, ((mem_size - 1024) * 1024), mem_read_headlandb,
                                mem_read_headlandw, mem_read_headlandl, mem_write_headlandb, mem_write_headlandw,
                                mem_write_headlandl, ram + 0x100000, MEM_MAPPING_INTERNAL, NULL);
                mem_mapping_enable(&headland_high_mapping);
        }

        for (i = 0; i < 24; i++) {
                mem_mapping_add(&headland_4000_9FFF_mapping[i], 0x40000 + (i << 14), 0x4000, mem_read_headlandb,
                                mem_read_headlandw, mem_read_headlandl, mem_write_headlandb, mem_write_headlandw,
                                mem_write_headlandl, mem_size > 256 + (i << 4) ? ram + 0x40000 + (i << 14) : NULL,
                                MEM_MAPPING_INTERNAL, NULL);
                mem_mapping_enable(&headland_4000_9FFF_mapping[i]);
        }

        for (i = 0; i < 64; i++) {
                headland_ems_mr[i] = 0;
                mem_mapping_add(&headland_ems_mapping[i], ((i & 31) + ((i & 31) >= 24 ? 24 : 16)) << 14, 0x04000,
                                mem_read_headlandb, mem_read_headlandw, mem_read_headlandl, mem_write_headlandb,
                                mem_write_headlandw, mem_write_headlandl, ram + (((i & 31) + ((i & 31) >= 24 ? 24 : 16)) << 14),
                                0, &headland_ems_mr[i]);
                mem_mapping_disable(&headland_ems_mapping[i]);
        }

        headland_memmap_state_update();
}
