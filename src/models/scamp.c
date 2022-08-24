#include "ibm.h"
#include "io.h"
#include "mem.h"
#include "scamp.h"

static struct {
        int cfg_index;
        uint8_t cfg_regs[256];
        int cfg_enable;

        int ems_index;
        int ems_autoinc;
        uint16_t ems[0x24];
        mem_mapping_t ems_mappings[20]; /*a0000-effff*/
        uint32_t mappings[20];

        int ram_config;

        mem_mapping_t ram_mapping[2];
        uint32_t ram_virt_base[2], ram_phys_base[2];
        uint32_t ram_mask[2];
        int row_virt_shift[2], row_phys_shift[2];
        int ram_interleaved[2];
        int ibank_shift[2];

        uint8_t port_92;
} scamp;

#define CFG_ID 0x00
#define CFG_SLTPTR 0x02
#define CFG_RAMMAP 0x03
#define CFG_EMSEN1 0x0b
#define CFG_EMSEN2 0x0c
#define CFG_ABAXS 0x0e
#define CFG_CAXS 0x0f
#define CFG_DAXS 0x10
#define CFG_FEAXS 0x11

#define ID_VL82C311 0xd6

#define RAMMAP_REMP386 (1 << 4)

#define EMSEN1_EMSMAP (1 << 4)
#define EMSEN1_EMSENAB (1 << 7)

/*Commodore SL386SX requires proper memory slot decoding to detect memory size.
  Therefore we emulate the SCAMP memory address decoding, and therefore are
  limited to the DRAM combinations supported by the actual chip*/
enum { BANK_NONE, BANK_256K, BANK_256K_INTERLEAVED, BANK_1M, BANK_1M_INTERLEAVED, BANK_4M, BANK_4M_INTERLEAVED };

static const struct {
        int size_kb;
        int rammap;
        int bank[2];
} ram_configs[] = {
        {512, 0x0, {BANK_256K, BANK_NONE}},
        {1024, 0x1, {BANK_256K_INTERLEAVED, BANK_NONE}},
        {1536, 0x2, {BANK_256K_INTERLEAVED, BANK_256K}},
        {2048, 0x3, {BANK_256K_INTERLEAVED, BANK_256K_INTERLEAVED}},
        {3072, 0xc, {BANK_256K_INTERLEAVED, BANK_1M}},
        {4096, 0x5, {BANK_1M_INTERLEAVED, BANK_NONE}},
        {5120, 0xd, {BANK_256K_INTERLEAVED, BANK_1M_INTERLEAVED}},
        {6144, 0x6, {BANK_1M_INTERLEAVED, BANK_1M}},
        {8192, 0x7, {BANK_1M_INTERLEAVED, BANK_1M_INTERLEAVED}},
        {12288, 0xe, {BANK_1M_INTERLEAVED, BANK_4M}},
        {16384, 0x9, {BANK_4M_INTERLEAVED, BANK_NONE}},
};

static const struct {
        int bank[2];
        int remapped;
} rammap[16] = {
        {{BANK_256K, BANK_NONE}, 0},
        {{BANK_256K_INTERLEAVED, BANK_NONE}, 0},
        {{BANK_256K_INTERLEAVED, BANK_256K}, 0},
        {{BANK_256K_INTERLEAVED, BANK_256K_INTERLEAVED}, 0},

        {{BANK_1M, BANK_NONE}, 0},
        {{BANK_1M_INTERLEAVED, BANK_NONE}, 0},
        {{BANK_1M_INTERLEAVED, BANK_1M}, 0},
        {{BANK_1M_INTERLEAVED, BANK_1M_INTERLEAVED}, 0},

        {{BANK_4M, BANK_NONE}, 0},
        {{BANK_4M_INTERLEAVED, BANK_NONE}, 0},
        {{BANK_NONE, BANK_4M}, 1},             /*Bank 2 remapped to 0*/
        {{BANK_NONE, BANK_4M_INTERLEAVED}, 1}, /*Banks 2/3 remapped to 0/1*/

        {{BANK_256K_INTERLEAVED, BANK_1M}, 0},
        {{BANK_256K_INTERLEAVED, BANK_1M_INTERLEAVED}, 0},
        {{BANK_1M_INTERLEAVED, BANK_4M}, 0},
        {{BANK_1M_INTERLEAVED, BANK_4M_INTERLEAVED}, 0}, /*Undocumented - probably wrong!*/
};

/*The column bits masked when using 256kbit DRAMs in 4Mbit mode aren't contiguous,
  so we use separate routines for that special case*/
static uint8_t ram_mirrored_256k_in_4mi_read(uint32_t addr, void *p) {
        int bank = (intptr_t)p;
        int row, column, byte;

        addr -= scamp.ram_virt_base[bank];
        byte = addr & 1;
        if (!scamp.ram_interleaved[bank]) {
                if (addr & 0x400)
                        return 0xff;

                addr = (addr & 0x3ff) | ((addr & ~0x7ff) >> 1);
                column = (addr >> 1) & scamp.ram_mask[bank];
                row = ((addr & 0xff000) >> 13) | (((addr & 0x200000) >> 22) << 9);

                addr = byte | (column << 1) | (row << scamp.row_phys_shift[bank]);
        } else {
                column = (addr >> 1) & ((scamp.ram_mask[bank] << 1) | 1);
                row = ((addr & 0x1fe000) >> 13) | (((addr & 0x400000) >> 22) << 9);

                addr = byte | (column << 1) | (row << (scamp.row_phys_shift[bank] + 1));
        }

        return ram[addr + scamp.ram_phys_base[bank]];
}
static void ram_mirrored_256k_in_4mi_write(uint32_t addr, uint8_t val, void *p) {
        int bank = (intptr_t)p;
        int row, column, byte;

        addr -= scamp.ram_virt_base[bank];
        byte = addr & 1;
        if (!scamp.ram_interleaved[bank]) {
                if (addr & 0x400)
                        return;

                addr = (addr & 0x3ff) | ((addr & ~0x7ff) >> 1);
                column = (addr >> 1) & scamp.ram_mask[bank];
                row = ((addr & 0xff000) >> 13) | (((addr & 0x200000) >> 22) << 9);

                addr = byte | (column << 1) | (row << scamp.row_phys_shift[bank]);
        } else {
                column = (addr >> 1) & ((scamp.ram_mask[bank] << 1) | 1);
                row = ((addr & 0x1fe000) >> 13) | (((addr & 0x400000) >> 22) << 9);

                addr = byte | (column << 1) | (row << (scamp.row_phys_shift[bank] + 1));
        }

        ram[addr + scamp.ram_phys_base[bank]] = val;
}

/*Read/write handlers for interleaved memory banks. We must keep CPU and ram array
  mapping linear, otherwise we won't be able to execute code from interleaved banks*/
static uint8_t ram_mirrored_interleaved_read(uint32_t addr, void *p) {
        int bank = (intptr_t)p;
        int row, column, byte;

        addr -= scamp.ram_virt_base[bank];
        byte = addr & 1;
        if (!scamp.ram_interleaved[bank]) {
                if (addr & 0x400)
                        return 0xff;

                addr = (addr & 0x3ff) | ((addr & ~0x7ff) >> 1);
                column = (addr >> 1) & scamp.ram_mask[bank];
                row = (addr >> scamp.row_virt_shift[bank]) & scamp.ram_mask[bank];

                addr = byte | (column << 1) | (row << scamp.row_phys_shift[bank]);
        } else {
                column = (addr >> 1) & ((scamp.ram_mask[bank] << 1) | 1);
                row = (addr >> (scamp.row_virt_shift[bank] + 1)) & scamp.ram_mask[bank];

                addr = byte | (column << 1) | (row << (scamp.row_phys_shift[bank] + 1));
        }

        return ram[addr + scamp.ram_phys_base[bank]];
}
static void ram_mirrored_interleaved_write(uint32_t addr, uint8_t val, void *p) {
        int bank = (intptr_t)p;
        int row, column, byte;

        addr -= scamp.ram_virt_base[bank];
        byte = addr & 1;
        if (!scamp.ram_interleaved[bank]) {
                if (addr & 0x400)
                        return;

                addr = (addr & 0x3ff) | ((addr & ~0x7ff) >> 1);
                column = (addr >> 1) & scamp.ram_mask[bank];
                row = (addr >> scamp.row_virt_shift[bank]) & scamp.ram_mask[bank];

                addr = byte | (column << 1) | (row << scamp.row_phys_shift[bank]);
        } else {
                column = (addr >> 1) & ((scamp.ram_mask[bank] << 1) | 1);
                row = (addr >> (scamp.row_virt_shift[bank] + 1)) & scamp.ram_mask[bank];

                addr = byte | (column << 1) | (row << (scamp.row_phys_shift[bank] + 1));
        }

        ram[addr + scamp.ram_phys_base[bank]] = val;
}

static uint8_t ram_mirrored_read(uint32_t addr, void *p) {
        int bank = (intptr_t)p;
        int row, column, byte;

        addr -= scamp.ram_virt_base[bank];
        byte = addr & 1;
        column = (addr >> 1) & scamp.ram_mask[bank];
        row = (addr >> scamp.row_virt_shift[bank]) & scamp.ram_mask[bank];
        addr = byte | (column << 1) | (row << scamp.row_phys_shift[bank]);

        return ram[addr + scamp.ram_phys_base[bank]];
}
static void ram_mirrored_write(uint32_t addr, uint8_t val, void *p) {
        int bank = (intptr_t)p;
        int row, column, byte;

        addr -= scamp.ram_virt_base[bank];
        byte = addr & 1;
        column = (addr >> 1) & scamp.ram_mask[bank];
        row = (addr >> scamp.row_virt_shift[bank]) & scamp.ram_mask[bank];
        addr = byte | (column << 1) | (row << scamp.row_phys_shift[bank]);

        ram[addr + scamp.ram_phys_base[bank]] = val;
}

static void recalc_mappings(void) {
        int c;
        uint32_t virt_base = 0;
        uint8_t cur_rammap = scamp.cfg_regs[CFG_RAMMAP] & 0xf;
        intptr_t bank_nr = 0;

        for (c = 0; c < 2; c++)
                mem_mapping_disable(&scamp.ram_mapping[c]);

        /*Once the BIOS programs the correct DRAM configuration, switch to regular
          linear memory mapping*/
        if (cur_rammap == ram_configs[scamp.ram_config].rammap) {
                mem_mapping_set_handler(&ram_low_mapping, mem_read_ram, mem_read_ramw, mem_read_raml, mem_write_ram,
                                        mem_write_ramw, mem_write_raml);
                mem_mapping_set_addr(&ram_low_mapping, 0, 0xa0000);
                mem_mapping_enable(&ram_high_mapping);
                return;
        } else {
                mem_mapping_set_handler(&ram_low_mapping, ram_mirrored_read, NULL, NULL, ram_mirrored_write, NULL, NULL);
                mem_mapping_disable(&ram_low_mapping);
        }

        if (rammap[cur_rammap].bank[0] == BANK_NONE)
                bank_nr = 1;

        pclog("Bank remap, cur_rammap=%x\n", cur_rammap);

        for (; bank_nr < 2; bank_nr++) {
                uint32_t old_virt_base = virt_base;
                int phys_bank = ram_configs[scamp.ram_config].bank[bank_nr];

                pclog("  Bank %i: phys_bank=%i rammap_bank=%i virt_base=%08x phys_base=%08x\n", bank_nr, phys_bank,
                      rammap[cur_rammap].bank[bank_nr], virt_base, scamp.ram_phys_base[bank_nr]);
                scamp.ram_virt_base[bank_nr] = virt_base;

                if (virt_base == 0) {
                        switch (rammap[cur_rammap].bank[bank_nr]) {
                        case BANK_NONE:
                                fatal("Bank 0 is empty!\n");
                                break;

                        case BANK_256K:
                                if (phys_bank != BANK_NONE) {
                                        mem_mapping_set_addr(&ram_low_mapping, 0, 0x80000);
                                        mem_mapping_set_p(&ram_low_mapping, (void *)bank_nr);
                                }
                                virt_base += 512 * 1024;
                                scamp.row_virt_shift[bank_nr] = 10;
                                break;

                        case BANK_256K_INTERLEAVED:
                                if (phys_bank != BANK_NONE) {
                                        mem_mapping_set_addr(&ram_low_mapping, 0, 0xa0000);
                                        mem_mapping_set_p(&ram_low_mapping, (void *)bank_nr);
                                }
                                virt_base += 512 * 1024 * 2;
                                scamp.row_virt_shift[bank_nr] = 10;
                                break;

                        case BANK_1M:
                                if (phys_bank != BANK_NONE) {
                                        mem_mapping_set_addr(&ram_low_mapping, 0, 0xa0000);
                                        mem_mapping_set_p(&ram_low_mapping, (void *)bank_nr);
                                        mem_mapping_set_addr(&scamp.ram_mapping[bank_nr], 0x100000, 0x100000);
                                        mem_mapping_set_exec(&scamp.ram_mapping[bank_nr],
                                                             &ram[scamp.ram_phys_base[bank_nr] + 0x100000]);
                                }
                                virt_base += 2048 * 1024;
                                scamp.row_virt_shift[bank_nr] = 11;
                                break;

                        case BANK_1M_INTERLEAVED:
                                if (phys_bank != BANK_NONE) {
                                        mem_mapping_set_addr(&ram_low_mapping, 0, 0xa0000);
                                        mem_mapping_set_p(&ram_low_mapping, (void *)bank_nr);
                                        mem_mapping_set_addr(&scamp.ram_mapping[bank_nr], 0x100000, 0x300000);
                                        mem_mapping_set_exec(&scamp.ram_mapping[bank_nr],
                                                             &ram[scamp.ram_phys_base[bank_nr] + 0x100000]);
                                }
                                virt_base += 2048 * 1024 * 2;
                                scamp.row_virt_shift[bank_nr] = 11;
                                break;

                        case BANK_4M:
                                if (phys_bank != BANK_NONE) {
                                        mem_mapping_set_addr(&ram_low_mapping, 0, 0xa0000);
                                        mem_mapping_set_p(&ram_low_mapping, (void *)bank_nr);
                                        mem_mapping_set_addr(&scamp.ram_mapping[bank_nr], 0x100000, 0x700000);
                                        mem_mapping_set_exec(&scamp.ram_mapping[bank_nr],
                                                             &ram[scamp.ram_phys_base[bank_nr] + 0x100000]);
                                }
                                virt_base += 8192 * 1024;
                                scamp.row_virt_shift[bank_nr] = 12;
                                break;

                        case BANK_4M_INTERLEAVED:
                                if (phys_bank != BANK_NONE) {
                                        mem_mapping_set_addr(&ram_low_mapping, 0, 0xa0000);
                                        mem_mapping_set_p(&ram_low_mapping, (void *)bank_nr);
                                        mem_mapping_set_addr(&scamp.ram_mapping[bank_nr], 0x100000, 0xf00000);
                                        mem_mapping_set_exec(&scamp.ram_mapping[bank_nr],
                                                             &ram[scamp.ram_phys_base[bank_nr] + 0x100000]);
                                }
                                virt_base += 8192 * 1024 * 2;
                                scamp.row_virt_shift[bank_nr] = 12;
                                break;
                        }
                } else {
                        switch (rammap[cur_rammap].bank[bank_nr]) {
                        case BANK_NONE:
                                break;

                        case BANK_256K:
                                if (phys_bank != BANK_NONE) {
                                        mem_mapping_set_addr(&scamp.ram_mapping[bank_nr], virt_base, 0x80000);
                                        mem_mapping_set_exec(&scamp.ram_mapping[bank_nr], &ram[scamp.ram_phys_base[bank_nr]]);
                                }
                                virt_base += 512 * 1024;
                                scamp.row_virt_shift[bank_nr] = 10;
                                break;

                        case BANK_256K_INTERLEAVED:
                                if (phys_bank != BANK_NONE) {
                                        mem_mapping_set_addr(&scamp.ram_mapping[bank_nr], virt_base, 0x100000);
                                        mem_mapping_set_exec(&scamp.ram_mapping[bank_nr], &ram[scamp.ram_phys_base[bank_nr]]);
                                }
                                virt_base += 512 * 1024 * 2;
                                scamp.row_virt_shift[bank_nr] = 10;
                                break;

                        case BANK_1M:
                                if (phys_bank != BANK_NONE) {
                                        mem_mapping_set_addr(&scamp.ram_mapping[bank_nr], virt_base, 0x200000);
                                        mem_mapping_set_exec(&scamp.ram_mapping[bank_nr], &ram[scamp.ram_phys_base[bank_nr]]);
                                }
                                virt_base += 2048 * 1024;
                                scamp.row_virt_shift[bank_nr] = 11;
                                break;

                        case BANK_1M_INTERLEAVED:
                                if (phys_bank != BANK_NONE) {
                                        mem_mapping_set_addr(&scamp.ram_mapping[bank_nr], virt_base, 0x400000);
                                        mem_mapping_set_exec(&scamp.ram_mapping[bank_nr], &ram[scamp.ram_phys_base[bank_nr]]);
                                }
                                virt_base += 2048 * 1024 * 2;
                                scamp.row_virt_shift[bank_nr] = 11;
                                break;

                        case BANK_4M:
                                if (phys_bank != BANK_NONE) {
                                        mem_mapping_set_addr(&scamp.ram_mapping[bank_nr], virt_base, 0x800000);
                                        mem_mapping_set_exec(&scamp.ram_mapping[bank_nr], &ram[scamp.ram_phys_base[bank_nr]]);
                                }
                                virt_base += 8192 * 1024;
                                scamp.row_virt_shift[bank_nr] = 12;
                                break;

                        case BANK_4M_INTERLEAVED:
                                if (phys_bank != BANK_NONE) {
                                        mem_mapping_set_addr(&scamp.ram_mapping[bank_nr], virt_base, 0x1000000);
                                        mem_mapping_set_exec(&scamp.ram_mapping[bank_nr], &ram[scamp.ram_phys_base[bank_nr]]);
                                }
                                virt_base += 8192 * 1024 * 2;
                                scamp.row_virt_shift[bank_nr] = 12;
                                break;
                        }
                }
                switch (rammap[cur_rammap].bank[bank_nr]) {
                case BANK_256K:
                case BANK_1M:
                case BANK_4M:
                        mem_mapping_set_handler(&scamp.ram_mapping[bank_nr], ram_mirrored_read, NULL, NULL, ram_mirrored_write,
                                                NULL, NULL);
                        if (!old_virt_base)
                                mem_mapping_set_handler(&ram_low_mapping, ram_mirrored_read, NULL, NULL, ram_mirrored_write, NULL,
                                                        NULL);
                        pclog("   not interleaved\n");
                        break;

                case BANK_256K_INTERLEAVED:
                case BANK_1M_INTERLEAVED:
                        mem_mapping_set_handler(&scamp.ram_mapping[bank_nr], ram_mirrored_interleaved_read, NULL, NULL,
                                                ram_mirrored_interleaved_write, NULL, NULL);
                        if (!old_virt_base)
                                mem_mapping_set_handler(&ram_low_mapping, ram_mirrored_interleaved_read, NULL, NULL,
                                                        ram_mirrored_interleaved_write, NULL, NULL);
                        pclog("   interleaved\n");
                        break;

                case BANK_4M_INTERLEAVED:
                        if (phys_bank == BANK_256K || phys_bank == BANK_256K_INTERLEAVED) {
                                mem_mapping_set_handler(&scamp.ram_mapping[bank_nr], ram_mirrored_256k_in_4mi_read, NULL, NULL,
                                                        ram_mirrored_256k_in_4mi_write, NULL, NULL);
                                if (!old_virt_base)
                                        mem_mapping_set_handler(&ram_low_mapping, ram_mirrored_256k_in_4mi_read, NULL, NULL,
                                                                ram_mirrored_256k_in_4mi_write, NULL, NULL);
                                pclog("   256k in 4mi\n");
                        } else {
                                mem_mapping_set_handler(&scamp.ram_mapping[bank_nr], ram_mirrored_interleaved_read, NULL, NULL,
                                                        ram_mirrored_interleaved_write, NULL, NULL);
                                if (!old_virt_base)
                                        mem_mapping_set_handler(&ram_low_mapping, ram_mirrored_interleaved_read, NULL, NULL,
                                                                ram_mirrored_interleaved_write, NULL, NULL);
                                pclog("   interleaved\n");
                        }
                        break;
                }
        }
}

static void recalc_sltptr(void) {
        uint32_t sltptr = scamp.cfg_regs[CFG_SLTPTR] << 16;

        if (sltptr >= 0xa0000 && sltptr < 0x100000)
                sltptr = 0x100000;
        if (sltptr > 0xfe0000)
                sltptr = 0xfe0000;

        //        pclog("sltptr=%06x\n", sltptr);
        if (sltptr >= 0xa0000) {
                mem_set_mem_state(0, 0xa0000, MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
                mem_set_mem_state(0x100000, sltptr - 0x100000, MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
                mem_set_mem_state(sltptr, 0x1000000 - sltptr, MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
        } else {
                mem_set_mem_state(0, sltptr, MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
                mem_set_mem_state(sltptr, 0xa0000 - sltptr, MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
                mem_set_mem_state(0x100000, 0xf00000, MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
        }
}

static uint8_t scamp_ems_read(uint32_t addr, void *p) {
        int segment = (intptr_t)p;

        addr = (addr & 0x3fff) | scamp.mappings[segment];
        return ram[addr];
}
static void scamp_ems_write(uint32_t addr, uint8_t val, void *p) {
        int segment = (intptr_t)p;

        addr = (addr & 0x3fff) | scamp.mappings[segment];
        ram[addr] = val;
}

static void recalc_ems(void) {
        const uint32_t ems_base[12] = {0xc0000, 0xc4000, 0xc8000, 0xcc000, 0xd0000, 0xd4000,
                                       0xd8000, 0xdc000, 0xe0000, 0xe4000, 0xe8000, 0xec000};
        uint32_t new_mappings[20];
        uint16_t ems_enable;
        int segment;

        for (segment = 0; segment < 20; segment++)
                new_mappings[segment] = 0xa0000 + segment * 0x4000;

        if (scamp.cfg_regs[CFG_EMSEN1] & EMSEN1_EMSENAB)
                ems_enable = scamp.cfg_regs[CFG_EMSEN2] | ((scamp.cfg_regs[CFG_EMSEN1] & 0xf) << 8);
        else
                ems_enable = 0;

        for (segment = 0; segment < 12; segment++) {
                if (ems_enable & (1 << segment)) {
                        uint32_t phys_addr = scamp.ems[segment] << 14;

                        /*If physical address is in remapped memory then adjust down to a0000-fffff range*/
                        if ((scamp.cfg_regs[CFG_RAMMAP] & RAMMAP_REMP386) && phys_addr >= (mem_size * 1024) &&
                            phys_addr < ((mem_size + 384) * 1024))
                                phys_addr = (phys_addr - mem_size * 1024) + 0xa0000;
                        new_mappings[(ems_base[segment] - 0xa0000) >> 14] = phys_addr;
                }
        }

        for (segment = 0; segment < 20; segment++) {
                if (new_mappings[segment] != scamp.mappings[segment]) {
                        scamp.mappings[segment] = new_mappings[segment];
                        if (new_mappings[segment] < (mem_size * 1024)) {
                                mem_mapping_set_exec(&scamp.ems_mappings[segment], ram + scamp.mappings[segment]);
                                mem_mapping_enable(&scamp.ems_mappings[segment]);
                        } else
                                mem_mapping_disable(&scamp.ems_mappings[segment]);
                }
        }
}

#define NR_ELEMS(x) (sizeof(x) / sizeof(x[0]))

static void shadow_control(uint32_t addr, uint32_t size, int state, int ems_enable) {
        //        pclog("shadow_control: addr=%08x size=%04x state=%i\n", addr, size, state);
        if (ems_enable)
                mem_set_mem_state(addr, size, MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
        else
                switch (state) {
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

static void shadow_recalc(void) {
        uint8_t abaxs = (scamp.cfg_regs[CFG_RAMMAP] & RAMMAP_REMP386) ? 0 : scamp.cfg_regs[CFG_ABAXS];
        uint8_t caxs = (scamp.cfg_regs[CFG_RAMMAP] & RAMMAP_REMP386) ? 0 : scamp.cfg_regs[CFG_CAXS];
        uint8_t daxs = (scamp.cfg_regs[CFG_RAMMAP] & RAMMAP_REMP386) ? 0 : scamp.cfg_regs[CFG_DAXS];
        uint8_t feaxs = (scamp.cfg_regs[CFG_RAMMAP] & RAMMAP_REMP386) ? 0 : scamp.cfg_regs[CFG_FEAXS];
        uint32_t ems_enable;

        if (scamp.cfg_regs[CFG_EMSEN1] & EMSEN1_EMSENAB) {
                if (scamp.cfg_regs[CFG_EMSEN1] & EMSEN1_EMSMAP) /*Axxx/Bxxx/Dxxx*/
                        ems_enable = (scamp.cfg_regs[CFG_EMSEN2] & 0xf) | ((scamp.cfg_regs[CFG_EMSEN1] & 0xf) << 4) |
                                     ((scamp.cfg_regs[CFG_EMSEN2] & 0xf0) << 8);
                else /*Cxxx/Dxxx/Exxx*/
                        ems_enable = (scamp.cfg_regs[CFG_EMSEN2] << 8) | ((scamp.cfg_regs[CFG_EMSEN1] & 0xf) << 16);
        } else
                ems_enable = 0;

        /*Enabling remapping will disable all shadowing*/
        if (scamp.cfg_regs[CFG_RAMMAP] & RAMMAP_REMP386)
                mem_remap_top_384k();

        shadow_control(0xa0000, 0x4000, abaxs & 3, ems_enable & 0x00001);
        shadow_control(0xa0000, 0x4000, abaxs & 3, ems_enable & 0x00002);
        shadow_control(0xa8000, 0x4000, (abaxs >> 2) & 3, ems_enable & 0x00004);
        shadow_control(0xa8000, 0x4000, (abaxs >> 2) & 3, ems_enable & 0x00008);

        shadow_control(0xb0000, 0x4000, (abaxs >> 4) & 3, ems_enable & 0x00010);
        shadow_control(0xb0000, 0x4000, (abaxs >> 4) & 3, ems_enable & 0x00020);
        shadow_control(0xb8000, 0x4000, (abaxs >> 6) & 3, ems_enable & 0x00040);
        shadow_control(0xb8000, 0x4000, (abaxs >> 6) & 3, ems_enable & 0x00080);

        shadow_control(0xc0000, 0x4000, caxs & 3, ems_enable & 0x00100);
        shadow_control(0xc4000, 0x4000, (caxs >> 2) & 3, ems_enable & 0x00200);
        shadow_control(0xc8000, 0x4000, (caxs >> 4) & 3, ems_enable & 0x00400);
        shadow_control(0xcc000, 0x4000, (caxs >> 6) & 3, ems_enable & 0x00800);

        shadow_control(0xd0000, 0x4000, daxs & 3, ems_enable & 0x01000);
        shadow_control(0xd4000, 0x4000, (daxs >> 2) & 3, ems_enable & 0x02000);
        shadow_control(0xd8000, 0x4000, (daxs >> 4) & 3, ems_enable & 0x04000);
        shadow_control(0xdc000, 0x4000, (daxs >> 6) & 3, ems_enable & 0x08000);

        shadow_control(0xe0000, 0x4000, feaxs & 3, ems_enable & 0x10000);
        shadow_control(0xe4000, 0x4000, feaxs & 3, ems_enable & 0x20000);
        shadow_control(0xe8000, 0x4000, (feaxs >> 2) & 3, ems_enable & 0x40000);
        shadow_control(0xec000, 0x4000, (feaxs >> 2) & 3, ems_enable & 0x80000);

        shadow_control(0xf0000, 0x8000, (feaxs >> 4) & 3, 0);
        shadow_control(0xf8000, 0x8000, (feaxs >> 6) & 3, 0);
}

void scamp_write(uint16_t addr, uint8_t val, void *p) {
        //        pclog("scamp_write: addr=%04x val=%02x\n", addr, val);
        switch (addr) {
        case 0x92:
                if ((mem_a20_alt ^ val) & 2) {
                        mem_a20_alt = val & 2;
                        mem_a20_recalc();
                }
                if ((~scamp.port_92 & val) & 1) {
                        softresetx86();
                        cpu_set_edx();
                }
                scamp.port_92 = val;
                break;

        case 0xe8:
                scamp.ems_index = val & 0x1f;
                scamp.ems_autoinc = val & 0x40;
                break;

        case 0xea:
                if (scamp.ems_index < 0x24) {
                        scamp.ems[scamp.ems_index] = (scamp.ems[scamp.ems_index] & 0x300) | val;
                        recalc_ems();
                }
                //                pclog("EMS[%02x]=%03x (l)\n", scamp.ems_index, scamp.ems[scamp.ems_index]);
                break;
        case 0xeb:
                if (scamp.ems_index < 0x24) {
                        scamp.ems[scamp.ems_index] = (scamp.ems[scamp.ems_index] & 0x0ff) | ((val & 3) << 8);
                        recalc_ems();
                }
                //                pclog("EMS[%02x]=%03x (h)\n", scamp.ems_index, scamp.ems[scamp.ems_index]);
                if (scamp.ems_autoinc)
                        scamp.ems_index = (scamp.ems_index + 1) & 0x3f;
                break;

        case 0xec:
                if (scamp.cfg_enable)
                        scamp.cfg_index = val;
                break;

        case 0xed:
                if (scamp.cfg_enable) {
                        if (scamp.cfg_index >= 0x02 && scamp.cfg_index <= 0x16) {
                                scamp.cfg_regs[scamp.cfg_index] = val;
                                //                                pclog("SCAMP CFG[%02x]=%02x\n", scamp.cfg_index, val);
                                switch (scamp.cfg_index) {
                                case CFG_SLTPTR:
                                        recalc_sltptr();
                                        break;

                                case CFG_RAMMAP:
                                        recalc_mappings();
                                        mem_mapping_disable(&ram_remapped_mapping);
                                        shadow_recalc();
                                        break;

                                case CFG_EMSEN1:
                                case CFG_EMSEN2:
                                        shadow_recalc();
                                        recalc_ems();
                                        break;

                                case CFG_ABAXS:
                                case CFG_CAXS:
                                case CFG_DAXS:
                                case CFG_FEAXS:
                                        shadow_recalc();
                                        break;
                                }
                        }
                }
                break;

        case 0xee:
                if (scamp.cfg_enable && mem_a20_alt) {
                        scamp.port_92 &= ~2;
                        mem_a20_alt = 0;
                        mem_a20_recalc();
                }
                break;
        }
}

uint8_t scamp_read(uint16_t addr, void *p) {
        uint8_t ret = 0xff;

        switch (addr) {
        case 0x92:
                ret = scamp.port_92;
                break;

        case 0xe8:
                ret = scamp.ems_index | scamp.ems_autoinc;
                break;

        case 0xea:
                if (scamp.ems_index < 0x24)
                        ret = scamp.ems[scamp.ems_index] & 0xff;
                break;
        case 0xeb:
                if (scamp.ems_index < 0x24)
                        ret = (scamp.ems[scamp.ems_index] >> 8) | 0xfc;
                if (scamp.ems_autoinc)
                        scamp.ems_index = (scamp.ems_index + 1) & 0x3f;
                break;

        case 0xed:
                if (scamp.cfg_enable) {
                        if (scamp.cfg_index >= 0x00 && scamp.cfg_index <= 0x16)
                                ret = scamp.cfg_regs[scamp.cfg_index];
                }
                break;

        case 0xee:
                if (!mem_a20_alt) {
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

void scamp_init(void) {
        uint32_t addr;
        intptr_t c;

        memset(&scamp, 0, sizeof(scamp));
        scamp.cfg_regs[CFG_ID] = ID_VL82C311;
        scamp.cfg_enable = 1;

        io_sethandler(0x0092, 0x0001, scamp_read, NULL, NULL, scamp_write, NULL, NULL, NULL);
        io_sethandler(0x00e8, 0x0001, scamp_read, NULL, NULL, scamp_write, NULL, NULL, NULL);
        io_sethandler(0x00ea, 0x0006, scamp_read, NULL, NULL, scamp_write, NULL, NULL, NULL);
        io_sethandler(0x00f4, 0x0002, scamp_read, NULL, NULL, scamp_write, NULL, NULL, NULL);
        io_sethandler(0x00f9, 0x0001, scamp_read, NULL, NULL, scamp_write, NULL, NULL, NULL);
        io_sethandler(0x00fb, 0x0001, scamp_read, NULL, NULL, scamp_write, NULL, NULL, NULL);

        scamp.ram_config = 0;

        /*Find best fit configuration for the requested memory size*/
        for (c = 0; c < NR_ELEMS(ram_configs); c++) {
                if (mem_size < ram_configs[c].size_kb)
                        break;

                scamp.ram_config = c;
        }

        mem_mapping_set_handler(&ram_low_mapping, ram_mirrored_read, NULL, NULL, ram_mirrored_write, NULL, NULL);
        mem_mapping_disable(&ram_high_mapping);
        mem_mapping_set_addr(&ram_mid_mapping, 0xf0000, 0x10000);
        mem_mapping_set_exec(&ram_mid_mapping, ram + 0xf0000);

        mem_mapping_disable(&bios_mapping[0]);
        mem_mapping_disable(&bios_mapping[1]);
        mem_mapping_disable(&bios_mapping[2]);
        mem_mapping_disable(&bios_mapping[3]);

        addr = 0;
        for (c = 0; c < 2; c++) {
                mem_mapping_add(&scamp.ram_mapping[c], 0, 0, ram_mirrored_read, NULL, NULL, ram_mirrored_write, NULL, NULL,
                                &ram[addr], MEM_MAPPING_INTERNAL, (void *)c);
                mem_mapping_disable(&scamp.ram_mapping[c]);

                scamp.ram_phys_base[c] = addr;
                //                pclog("Bank calc : %i = %08x\n", c ,addr);

                switch (ram_configs[scamp.ram_config].bank[c]) {
                case BANK_NONE:
                        scamp.ram_mask[c] = 0;
                        scamp.ram_interleaved[c] = 0;
                        break;

                case BANK_256K:
                        addr += 512 * 1024;
                        scamp.ram_mask[c] = 0x1ff;
                        scamp.row_phys_shift[c] = 10;
                        scamp.ram_interleaved[c] = 0;
                        break;

                case BANK_256K_INTERLEAVED:
                        addr += 512 * 1024 * 2;
                        scamp.ram_mask[c] = 0x1ff;
                        scamp.row_phys_shift[c] = 10;
                        scamp.ibank_shift[c] = 19;
                        scamp.ram_interleaved[c] = 1;
                        break;

                case BANK_1M:
                        addr += 2048 * 1024;
                        scamp.ram_mask[c] = 0x3ff;
                        scamp.row_phys_shift[c] = 11;
                        scamp.ram_interleaved[c] = 0;
                        break;

                case BANK_1M_INTERLEAVED:
                        addr += 2048 * 1024 * 2;
                        scamp.ram_mask[c] = 0x3ff;
                        scamp.row_phys_shift[c] = 11;
                        scamp.ibank_shift[c] = 21;
                        scamp.ram_interleaved[c] = 1;
                        break;

                case BANK_4M:
                        addr += 8192 * 1024;
                        scamp.ram_mask[c] = 0x7ff;
                        scamp.row_phys_shift[c] = 12;
                        scamp.ram_interleaved[c] = 0;
                        break;

                case BANK_4M_INTERLEAVED:
                        addr += 8192 * 1024 * 2;
                        scamp.ram_mask[c] = 0x7ff;
                        scamp.row_phys_shift[c] = 12;
                        scamp.ibank_shift[c] = 23;
                        scamp.ram_interleaved[c] = 1;
                        break;
                }
        }

        mem_set_mem_state(0xfe0000, 0x20000, MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);

        for (c = 0; c < 20; c++) {
                mem_mapping_add(&scamp.ems_mappings[c], 0xa0000 + c * 0x4000, 0x4000, scamp_ems_read, NULL, NULL, scamp_ems_write,
                                NULL, NULL, ram + 0xa0000 + c * 0x4000, MEM_MAPPING_INTERNAL, (void *)c);
                scamp.mappings[c] = 0xa0000 + c * 0x4000;
        }
}
