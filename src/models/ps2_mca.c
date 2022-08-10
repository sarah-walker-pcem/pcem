#include "ibm.h"
#include "cpu.h"
#include "device.h"
#include "io.h"
#include "lpt.h"
#include "mca.h"
#include "mem.h"
#include "ps2_mca.h"
#include "ps2_nvr.h"
#include "rom.h"
#include "serial.h"
#include "vid_vga.h"
#include "x86.h"

// static uint8_t ps2_92, ps2_94, ps2_102, ps2_103, ps2_104, ps2_105, ps2_190;

/*static struct
{
        uint8_t status, int_status;
        uint8_t attention, ctrl;
} ps2_hd;*/

static struct {
        uint8_t adapter_setup;
        uint8_t option[4];
        uint8_t pos_vga;
        uint8_t setup;
        uint8_t sys_ctrl_port_a;
        uint8_t subaddr_lo, subaddr_hi;

        uint8_t memory_bank[8];

        uint8_t io_id;

        mem_mapping_t shadow_mapping;
        mem_mapping_t split_mapping;
        mem_mapping_t expansion_mapping;
        mem_mapping_t cache_mapping;

        uint8_t (*planar_read)(uint16_t port);
        void (*planar_write)(uint16_t port, uint8_t val);

        uint8_t mem_regs[3];

        uint32_t split_addr;

        uint8_t mem_pos_regs[8];

        int pending_cache_miss;
} ps2;

/*The model 70 type 3/4 BIOS performs cache testing. Since PCem doesn't have any
  proper cache emulation, it's faked a bit here.

  Port E2 is used for cache diagnostics. Bit 7 seems to be set on a cache miss,
  toggling bit 2 seems to clear this. The BIOS performs at least the following
  tests :

  - Disable RAM, access low 64kb (386) / 8kb (486), execute code from cache to
    access low memory and verify that there are no cache misses.
  - Write to low memory using DMA, read low memory and verify that all accesses
    cause cache misses.
  - Read low memory, verify that first access is cache miss. Read again and
    verify that second access is cache hit.

  These tests are also performed on the 486 model 70, despite there being no
  external cache on this system. Port E2 seems to control the internal cache on
  these systems. Presumably this port is connected to KEN#/FLUSH# on the 486.
  This behaviour is required to pass the timer interrupt test on the 486 version
  - the BIOS uses a fixed length loop that will terminate too early on a 486/25
  if it executes from internal cache.

  To handle this, PCem uses some basic heuristics :
  - If cache is enabled but RAM is disabled, accesses to low memory go directly
    to cache memory.
  - Reads to cache addresses not 'valid' will set the cache miss flag, and mark
    that line as valid.
  - Cache flushes will clear the valid array.
  - DMA via the undocumented PS/2 command 0xb will clear the valid array.
  - Disabling the cache will clear the valid array.
  - Disabling the cache will also mark shadowed ROM areas as using ROM timings.
    This works around the timing loop mentioned above.
*/
static uint8_t ps2_cache[65536];
static int ps2_cache_valid[65536 / 8];

static uint8_t ps2_read_cache_ram(uint32_t addr, void *priv) {
        //        pclog("ps2_read_cache_ram: addr=%08x %i %04x:%04x\n", addr, ps2_cache_valid[addr >> 3], CS,cpu_state.pc);
        if (!ps2_cache_valid[addr >> 3]) {
                ps2_cache_valid[addr >> 3] = 1;
                ps2.mem_regs[2] |= 0x80;
        } else
                ps2.pending_cache_miss = 0;

        return ps2_cache[addr];
}
static uint16_t ps2_read_cache_ramw(uint32_t addr, void *priv) {
        //        pclog("ps2_read_cache_ramw: addr=%08x %i %04x:%04x\n", addr, ps2_cache_valid[addr >> 3], CS,cpu_state.pc);
        if (!ps2_cache_valid[addr >> 3]) {
                ps2_cache_valid[addr >> 3] = 1;
                ps2.mem_regs[2] |= 0x80;
        } else
                ps2.pending_cache_miss = 0;

        return *(uint16_t *)&ps2_cache[addr];
}
static uint32_t ps2_read_cache_raml(uint32_t addr, void *priv) {
        //        pclog("ps2_read_cache_raml: addr=%08x %i %04x:%04x\n", addr, ps2_cache_valid[addr >> 3], CS,cpu_state.pc);
        if (!ps2_cache_valid[addr >> 3]) {
                ps2_cache_valid[addr >> 3] = 1;
                ps2.mem_regs[2] |= 0x80;
        } else
                ps2.pending_cache_miss = 0;

        return *(uint32_t *)&ps2_cache[addr];
}
static void ps2_write_cache_ram(uint32_t addr, uint8_t val, void *priv) {
        //        pclog("ps2_write_cache_ram: addr=%08x val=%02x %04x:%04x %i\n", addr, val, CS,cpu_state.pc, ins);
        ps2_cache[addr] = val;
}

void ps2_cache_clean() { memset(ps2_cache_valid, 0, sizeof(ps2_cache_valid)); }

static uint8_t ps2_read_shadow_ram(uint32_t addr, void *priv) {
        addr = (addr & 0x1ffff) + 0xe0000;
        return mem_read_ram(addr, priv);
}
static uint16_t ps2_read_shadow_ramw(uint32_t addr, void *priv) {
        addr = (addr & 0x1ffff) + 0xe0000;
        return mem_read_ramw(addr, priv);
}
static uint32_t ps2_read_shadow_raml(uint32_t addr, void *priv) {
        addr = (addr & 0x1ffff) + 0xe0000;
        return mem_read_raml(addr, priv);
}
static void ps2_write_shadow_ram(uint32_t addr, uint8_t val, void *priv) {
        addr = (addr & 0x1ffff) + 0xe0000;
        mem_write_ram(addr, val, priv);
}
static void ps2_write_shadow_ramw(uint32_t addr, uint16_t val, void *priv) {
        addr = (addr & 0x1ffff) + 0xe0000;
        mem_write_ramw(addr, val, priv);
}
static void ps2_write_shadow_raml(uint32_t addr, uint32_t val, void *priv) {
        addr = (addr & 0x1ffff) + 0xe0000;
        mem_write_raml(addr, val, priv);
}

static uint8_t ps2_read_split_ram(uint32_t addr, void *priv) {
        addr = (addr & 0x3ffff) + 0xa0000;
        return mem_read_ram(addr, priv);
}
static uint16_t ps2_read_split_ramw(uint32_t addr, void *priv) {
        addr = (addr & 0x3ffff) + 0xa0000;
        return mem_read_ramw(addr, priv);
}
static uint32_t ps2_read_split_raml(uint32_t addr, void *priv) {
        addr = (addr & 0x3ffff) + 0xa0000;
        return mem_read_raml(addr, priv);
}
static void ps2_write_split_ram(uint32_t addr, uint8_t val, void *priv) {
        addr = (addr & 0x3ffff) + 0xa0000;
        mem_write_ram(addr, val, priv);
}
static void ps2_write_split_ramw(uint32_t addr, uint16_t val, void *priv) {
        addr = (addr & 0x3ffff) + 0xa0000;
        mem_write_ramw(addr, val, priv);
}
static void ps2_write_split_raml(uint32_t addr, uint32_t val, void *priv) {
        addr = (addr & 0x3ffff) + 0xa0000;
        mem_write_raml(addr, val, priv);
}

#define PS2_SETUP_IO 0x80
#define PS2_SETUP_VGA 0x20

#define PS2_ADAPTER_SETUP 0x08

static uint8_t model_50_read(uint16_t port) {
        switch (port) {
        case 0x100:
                return 0xff;
        case 0x101:
                return 0xfb;
        case 0x102:
                return ps2.option[0];
        case 0x103:
                return ps2.option[1];
        case 0x104:
                return ps2.option[2];
        case 0x105:
                return ps2.option[3];
        case 0x106:
                return ps2.subaddr_lo;
        case 0x107:
                return ps2.subaddr_hi;
        }
        return 0xff;
}

static uint8_t model_55sx_read(uint16_t port) {
        switch (port) {
        case 0x100:
                return 0xff;
        case 0x101:
                return 0xfb;
        case 0x102:
                return ps2.option[0];
        case 0x103:
                return ps2.option[1];
        case 0x104:
                return ps2.memory_bank[ps2.option[3] & 7]; // ps2.option[2];
        case 0x105:
                return ps2.option[3];
        case 0x106:
                return ps2.subaddr_lo;
        case 0x107:
                return ps2.subaddr_hi;
        }
        return 0xff;
}

static uint8_t model_70_type3_read(uint16_t port) {
        switch (port) {
        case 0x100:
                return 0xff;
        case 0x101:
                return 0xf9;
        case 0x102:
                return ps2.option[0];
        case 0x103:
                return ps2.option[1];
        case 0x104:
                return ps2.option[2];
        case 0x105:
                return ps2.option[3];
        case 0x106:
                return ps2.subaddr_lo;
        case 0x107:
                return ps2.subaddr_hi;
        }
        return 0xff;
}

static uint8_t model_80_read(uint16_t port) {
        switch (port) {
        case 0x100:
                return 0xff;
        case 0x101:
                return 0xfd;
        case 0x102:
                return ps2.option[0];
        case 0x103:
                return ps2.option[1];
        case 0x104:
                return ps2.option[2];
        case 0x105:
                return ps2.option[3];
        case 0x106:
                return ps2.subaddr_lo;
        case 0x107:
                return ps2.subaddr_hi;
        }
        return 0xff;
}

static void model_50_write(uint16_t port, uint8_t val) {
        switch (port) {
        case 0x100:
                ps2.io_id = val;
                break;
        case 0x101:
                break;
        case 0x102:
                lpt1_remove();
                serial1_remove();
                if (val & 0x04) {
                        if (val & 0x08)
                                serial1_init(0x3f8, 4, 1);
                        else
                                serial1_init(0x2f8, 3, 1);
                } else
                        serial1_remove();
                if (val & 0x10) {
                        switch ((val >> 5) & 3) {
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
                break;
        case 0x103:
                ps2.option[1] = val;
                break;
        case 0x104:
                ps2.option[2] = val;
                break;
        case 0x105:
                ps2.option[3] = val;
                break;
        case 0x106:
                ps2.subaddr_lo = val;
                break;
        case 0x107:
                ps2.subaddr_hi = val;
                break;
        }
}

static void model_55sx_write(uint16_t port, uint8_t val) {
        switch (port) {
        case 0x100:
                ps2.io_id = val;
                break;
        case 0x101:
                break;
        case 0x102:
                lpt1_remove();
                serial1_remove();
                if (val & 0x04) {
                        if (val & 0x08)
                                serial1_init(0x3f8, 4, 1);
                        else
                                serial1_init(0x2f8, 3, 1);
                } else
                        serial1_remove();
                if (val & 0x10) {
                        switch ((val >> 5) & 3) {
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
                break;
        case 0x103:
                ps2.option[1] = val;
                break;
        case 0x104:
                ps2.memory_bank[ps2.option[3] & 7] &= ~0xf;
                ps2.memory_bank[ps2.option[3] & 7] |= (val & 0xf);
                pclog("Write memory bank %i %02x\n", ps2.option[3] & 7, val);
                break;
        case 0x105:
                pclog("Write POS3 %02x\n", val);
                ps2.option[3] = val;

                if (!(val & 0x10)) {
                        mem_set_mem_state(0xe0000, 0x20000, MEM_READ_INTERNAL | MEM_WRITE_DISABLED);
                        mem_mapping_disable(&ps2.shadow_mapping);
                } else {
                        mem_set_mem_state(0xe0000, 0x20000, MEM_READ_EXTERNAL | MEM_WRITE_INTERNAL);
                        mem_mapping_enable(&ps2.shadow_mapping);
                }

                if ((ps2.option[1] & 1) && !(ps2.option[3] & 0x20))
                        mem_set_mem_state(mem_size * 1024, 256 * 1024, MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
                else
                        mem_set_mem_state(mem_size * 1024, 256 * 1024, MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
                break;
        case 0x106:
                ps2.subaddr_lo = val;
                break;
        case 0x107:
                ps2.subaddr_hi = val;
                break;
        }
}

static void model_70_type3_write(uint16_t port, uint8_t val) {
        switch (port) {
        case 0x100:
                break;
        case 0x101:
                break;
        case 0x102:
                lpt1_remove();
                serial1_remove();
                if (val & 0x04) {
                        if (val & 0x08)
                                serial1_init(0x3f8, 4, 1);
                        else
                                serial1_init(0x2f8, 3, 1);
                } else
                        serial1_remove();
                if (val & 0x10) {
                        switch ((val >> 5) & 3) {
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
                break;
        case 0x105:
                ps2.option[3] = val;
                break;
        case 0x106:
                ps2.subaddr_lo = val;
                break;
        case 0x107:
                ps2.subaddr_hi = val;
                break;
        }
}

static void model_80_write(uint16_t port, uint8_t val) {
        switch (port) {
        case 0x100:
                break;
        case 0x101:
                break;
        case 0x102:
                lpt1_remove();
                serial1_remove();
                if (val & 0x04) {
                        if (val & 0x08)
                                serial1_init(0x3f8, 4, 1);
                        else
                                serial1_init(0x2f8, 3, 1);
                } else
                        serial1_remove();
                if (val & 0x10) {
                        switch ((val >> 5) & 3) {
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
                break;
        case 0x103:
                ps2.option[1] = (ps2.option[1] & 0x0f) | (val & 0xf0);
                break;
        case 0x104:
                ps2.option[2] = val;
                break;
        case 0x105:
                ps2.option[3] = val;
                break;
        case 0x106:
                ps2.subaddr_lo = val;
                break;
        case 0x107:
                ps2.subaddr_hi = val;
                break;
        }
}

uint8_t ps2_mca_read(uint16_t port, void *p) {
        uint8_t temp;

        switch (port) {
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
                        temp = ps2.planar_read(port);
                else if (!(ps2.setup & PS2_SETUP_VGA))
                        temp = 0xfd;
                else if (ps2.adapter_setup & PS2_ADAPTER_SETUP)
                        temp = mca_read(port);
                else
                        temp = 0xff;
                break;
        case 0x101:
                if (!(ps2.setup & PS2_SETUP_IO))
                        temp = ps2.planar_read(port);
                else if (!(ps2.setup & PS2_SETUP_VGA))
                        temp = 0xef;
                else if (ps2.adapter_setup & PS2_ADAPTER_SETUP)
                        temp = mca_read(port);
                else
                        temp = 0xff;
                break;
        case 0x102:
                if (!(ps2.setup & PS2_SETUP_IO))
                        temp = ps2.planar_read(port);
                else if (!(ps2.setup & PS2_SETUP_VGA))
                        temp = ps2.pos_vga;
                else if (ps2.adapter_setup & PS2_ADAPTER_SETUP)
                        temp = mca_read(port);
                else
                        temp = 0xff;
                break;
        case 0x103:
                if (!(ps2.setup & PS2_SETUP_IO))
                        temp = ps2.planar_read(port);
                else if ((ps2.setup & PS2_SETUP_VGA) && (ps2.adapter_setup & PS2_ADAPTER_SETUP))
                        temp = mca_read(port);
                else
                        temp = 0xff;
                break;
        case 0x104:
                if (!(ps2.setup & PS2_SETUP_IO))
                        temp = ps2.planar_read(port);
                else if ((ps2.setup & PS2_SETUP_VGA) && (ps2.adapter_setup & PS2_ADAPTER_SETUP))
                        temp = mca_read(port);
                else
                        temp = 0xff;
                break;
        case 0x105:
                if (!(ps2.setup & PS2_SETUP_IO))
                        temp = ps2.planar_read(port);
                else if ((ps2.setup & PS2_SETUP_VGA) && (ps2.adapter_setup & PS2_ADAPTER_SETUP))
                        temp = mca_read(port);
                else
                        temp = 0xff;
                break;
        case 0x106:
                if (!(ps2.setup & PS2_SETUP_IO))
                        temp = ps2.planar_read(port);
                else if ((ps2.setup & PS2_SETUP_VGA) && (ps2.adapter_setup & PS2_ADAPTER_SETUP))
                        temp = mca_read(port);
                else
                        temp = 0xff;
                break;
        case 0x107:
                if (!(ps2.setup & PS2_SETUP_IO))
                        temp = ps2.planar_read(port);
                else if ((ps2.setup & PS2_SETUP_VGA) && (ps2.adapter_setup & PS2_ADAPTER_SETUP))
                        temp = mca_read(port);
                else
                        temp = 0xff;
                break;

        default:
                temp = 0xff;
                break;
        }

        //        pclog("ps2_read: port=%04x temp=%02x %04x:%04x %i\n", port, temp, CS, cpu_state.pc, ins);

        return temp;
}

static void ps2_mca_write(uint16_t port, uint8_t val, void *p) {
        //        pclog("ps2_write: port=%04x val=%02x %04x:%04x\n", port, val, CS,cpu_state.pc);

        switch (port) {
        case 0x0092:
                if ((val & 1) && !(ps2.sys_ctrl_port_a & 1)) {
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
                if ((val & 0x80) && !(ps2.adapter_setup & 0x80))
                        mca_reset();
                ps2.adapter_setup = val;
                mca_set_index(val & 7);
                break;
        case 0x100:
                if (!(ps2.setup & PS2_SETUP_IO))
                        ps2.planar_write(port, val);
                else if ((ps2.setup & PS2_SETUP_VGA) && (ps2.adapter_setup & PS2_ADAPTER_SETUP))
                        mca_write(port, val);
                break;
        case 0x101:
                if (!(ps2.setup & PS2_SETUP_IO))
                        ps2.planar_write(port, val);
                else if ((ps2.setup & PS2_SETUP_VGA) && (ps2.setup & PS2_SETUP_VGA) && (ps2.adapter_setup & PS2_ADAPTER_SETUP))
                        mca_write(port, val);
                break;
        case 0x102:
                if (!(ps2.setup & PS2_SETUP_IO))
                        ps2.planar_write(port, val);
                else if (!(ps2.setup & PS2_SETUP_VGA)) {
                        if (mb_vga) {
                                vga_disable(mb_vga);
                                if (val & 1)
                                        vga_enable(mb_vga);
                        }
                        ps2.pos_vga = val;
                } else if (ps2.adapter_setup & PS2_ADAPTER_SETUP)
                        mca_write(port, val);
                break;
        case 0x103:
                if (!(ps2.setup & PS2_SETUP_IO))
                        ps2.planar_write(port, val);
                else if (ps2.adapter_setup & PS2_ADAPTER_SETUP)
                        mca_write(port, val);
                break;
        case 0x104:
                if (!(ps2.setup & PS2_SETUP_IO))
                        ps2.planar_write(port, val);
                else if (ps2.adapter_setup & PS2_ADAPTER_SETUP)
                        mca_write(port, val);
                break;
        case 0x105:
                if (!(ps2.setup & PS2_SETUP_IO))
                        ps2.planar_write(port, val);
                else if (ps2.adapter_setup & PS2_ADAPTER_SETUP)
                        mca_write(port, val);
                break;
        case 0x106:
                if (!(ps2.setup & PS2_SETUP_IO))
                        ps2.planar_write(port, val);
                else if (ps2.adapter_setup & PS2_ADAPTER_SETUP)
                        mca_write(port, val);
                break;
        case 0x107:
                if (!(ps2.setup & PS2_SETUP_IO))
                        ps2.planar_write(port, val);
                else if (ps2.adapter_setup & PS2_ADAPTER_SETUP)
                        mca_write(port, val);
                break;
        }
}

static void ps2_mca_board_common_init() {
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
}

void ps2_mca_board_model_50_init() {
        ps2_mca_board_common_init();

        mem_remap_top_384k();
        mca_init(4);

        ps2.planar_read = model_50_read;
        ps2.planar_write = model_50_write;
}

void ps2_mca_board_model_55sx_init() {
        ps2_mca_board_common_init();

        mem_mapping_add(&ps2.shadow_mapping, (mem_size + 256) * 1024, 128 * 1024, ps2_read_shadow_ram, ps2_read_shadow_ramw,
                        ps2_read_shadow_raml, ps2_write_shadow_ram, ps2_write_shadow_ramw, ps2_write_shadow_raml, &ram[0xe0000],
                        MEM_MAPPING_INTERNAL, NULL);

        //        mem_remap_top_384k();
        mem_remap_top_256k();
        ps2.option[3] = 0x10;

        memset(ps2.memory_bank, 0xf0, 8);
        switch (mem_size / 1024) {
        case 1:
                ps2.memory_bank[0] = 0x61;
                break;
        case 2:
                ps2.memory_bank[0] = 0x51;
                break;
        case 3:
                ps2.memory_bank[0] = 0x51;
                ps2.memory_bank[1] = 0x61;
                break;
        case 4:
                ps2.memory_bank[0] = 0x51;
                ps2.memory_bank[1] = 0x51;
                break;
        case 5:
                ps2.memory_bank[0] = 0x01;
                ps2.memory_bank[1] = 0x61;
                break;
        case 6:
                ps2.memory_bank[0] = 0x01;
                ps2.memory_bank[1] = 0x51;
                break;
        case 7: /*Not supported*/
                ps2.memory_bank[0] = 0x01;
                ps2.memory_bank[1] = 0x51;
                break;
        case 8:
                ps2.memory_bank[0] = 0x01;
                ps2.memory_bank[1] = 0x01;
                break;
        }

        mca_init(4);

        ps2.planar_read = model_55sx_read;
        ps2.planar_write = model_55sx_write;
}

static void mem_encoding_update() {
        if (ps2.split_addr >= mem_size * 1024)
                mem_mapping_disable(&ps2.split_mapping);

        ps2.split_addr = (ps2.mem_regs[0] & 0xf) << 20;

        if (ps2.mem_regs[1] & 2)
                mem_set_mem_state(0xe0000, 0x20000, MEM_READ_EXTERNAL | MEM_WRITE_INTERNAL);
        else
                mem_set_mem_state(0xe0000, 0x20000, MEM_READ_INTERNAL | MEM_WRITE_DISABLED);

        if (!(ps2.mem_regs[1] & 8)) {
                if (ps2.split_addr >= mem_size * 1024)
                        mem_mapping_set_addr(&ps2.split_mapping, ps2.split_addr, 256 * 1024);
        }
}

static uint8_t mem_encoding_read(uint16_t addr, void *p) {
        switch (addr) {
        case 0xe0:
                return ps2.mem_regs[0];
        case 0xe1:
                return ps2.mem_regs[1];
        }
        return 0xff;
}
static void mem_encoding_write(uint16_t addr, uint8_t val, void *p) {
        switch (addr) {
        case 0xe0:
                ps2.mem_regs[0] = val;
                break;
        case 0xe1:
                ps2.mem_regs[1] = val;
                break;
        }
        mem_encoding_update();
}

static uint8_t mem_encoding_read_cached(uint16_t addr, void *p) {
        switch (addr) {
        case 0xe0:
                return ps2.mem_regs[0];
        case 0xe1:
                return ps2.mem_regs[1];
        case 0xe2:
                return ps2.mem_regs[2];
        }
        return 0xff;
}
static void mem_encoding_write_cached(uint16_t addr, uint8_t val, void *p) {
        uint8_t old;

        switch (addr) {
        case 0xe0:
                ps2.mem_regs[0] = val;
                break;
        case 0xe1:
                ps2.mem_regs[1] = val;
                break;
        case 0xe2:
                old = ps2.mem_regs[2];
                ps2.mem_regs[2] = (ps2.mem_regs[2] & 0x80) | (val & ~0x88);
                if (val & 2) {
                        //                        pclog("Clear latch - %i\n", ps2.pending_cache_miss);
                        if (ps2.pending_cache_miss)
                                ps2.mem_regs[2] |= 0x80;
                        else
                                ps2.mem_regs[2] &= ~0x80;
                        ps2.pending_cache_miss = 0;
                }

                if ((val & 0x21) == 0x20 && (old & 0x21) != 0x20)
                        ps2.pending_cache_miss = 1;
                if ((val & 0x21) == 0x01 && (old & 0x21) != 0x01)
                        ps2_cache_clean();
                if (val & 0x01)
                        ram_mid_mapping.flags |= MEM_MAPPING_ROM;
                else
                        ram_mid_mapping.flags &= ~MEM_MAPPING_ROM;
                break;
        }
        //        pclog("mem_encoding_write: addr=%02x val=%02x %04x:%04x  %02x %02x\n", addr, val, CS,cpu_state.pc,
        //        ps2.mem_regs[1],ps2.mem_regs[2]);
        mem_encoding_update();
        if ((ps2.mem_regs[1] & 0x10) && (ps2.mem_regs[2] & 0x21) == 0x20) {
                mem_mapping_disable(&ram_low_mapping);
                mem_mapping_enable(&ps2.cache_mapping);
                flushmmucache();
        } else {
                mem_mapping_disable(&ps2.cache_mapping);
                mem_mapping_enable(&ram_low_mapping);
                flushmmucache();
        }
}

static uint8_t ps2_mem_expansion_read(int port, void *p) { return ps2.mem_pos_regs[port & 7]; }

static void ps2_mem_expansion_write(int port, uint8_t val, void *p) {
        if (port < 0x102 || port == 0x104)
                return;

        ps2.mem_pos_regs[port & 7] = val;

        if (ps2.mem_pos_regs[2] & 1)
                mem_mapping_enable(&ps2.expansion_mapping);
        else
                mem_mapping_disable(&ps2.expansion_mapping);
}

void ps2_mca_board_model_70_type34_init(int is_type4) {
        ps2_mca_board_common_init();

        mem_remap_top_256k();
        ps2.split_addr = mem_size * 1024;
        mca_init(4);

        ps2.planar_read = model_70_type3_read;
        ps2.planar_write = model_70_type3_write;

        device_add(&ps2_nvr_device);

        io_sethandler(0x00e0, 0x0003, mem_encoding_read_cached, NULL, NULL, mem_encoding_write_cached, NULL, NULL, NULL);

        ps2.mem_regs[1] = 2;

        switch (mem_size / 1024) {
        case 2:
                ps2.option[1] = 0xa6;
                ps2.option[2] = 0x01;
                break;
        case 4:
                ps2.option[1] = 0xaa;
                ps2.option[2] = 0x01;
                break;
        case 6:
                ps2.option[1] = 0xca;
                ps2.option[2] = 0x01;
                break;
        case 8:
        default:
                ps2.option[1] = 0xca;
                ps2.option[2] = 0x02;
                break;
        }

        if (is_type4)
                ps2.option[2] |= 0x04; /*486 CPU*/

        mem_mapping_add(&ps2.split_mapping, (mem_size + 256) * 1024, 256 * 1024, ps2_read_split_ram, ps2_read_split_ramw,
                        ps2_read_split_raml, ps2_write_split_ram, ps2_write_split_ramw, ps2_write_split_raml, &ram[0xa0000],
                        MEM_MAPPING_INTERNAL, NULL);
        mem_mapping_disable(&ps2.split_mapping);

        mem_mapping_add(&ps2.cache_mapping, 0, is_type4 ? (8 * 1024) : (64 * 1024), ps2_read_cache_ram, ps2_read_cache_ramw,
                        ps2_read_cache_raml, ps2_write_cache_ram, NULL, NULL, ps2_cache, MEM_MAPPING_INTERNAL, NULL);
        mem_mapping_disable(&ps2.cache_mapping);

        if (mem_size > 8192) {
                /* Only 8 MB supported on planar, create a memory expansion card for the rest */
                mem_mapping_set_addr(&ram_high_mapping, 0x100000, 0x700000);

                ps2.mem_pos_regs[0] = 0xff;
                ps2.mem_pos_regs[1] = 0xfc;

                switch (mem_size / 1024) {
                case 10:
                        ps2.mem_pos_regs[4] = 0xfe;
                        break;
                case 12:
                        ps2.mem_pos_regs[4] = 0xfa;
                        break;
                case 14:
                        ps2.mem_pos_regs[4] = 0xea;
                        break;
                case 16:
                        ps2.mem_pos_regs[4] = 0xaa;
                        break;
                }

                mca_add(ps2_mem_expansion_read, ps2_mem_expansion_write, NULL, NULL);
                mem_mapping_add(&ps2.expansion_mapping, 0x800000, (mem_size - 8192) * 1024, mem_read_ram, mem_read_ramw,
                                mem_read_raml, mem_write_ram, mem_write_ramw, mem_write_raml, &ram[0x800000],
                                MEM_MAPPING_INTERNAL, NULL);
                mem_mapping_disable(&ps2.expansion_mapping);
        }
}

void ps2_mca_board_model_80_type2_init() {
        ps2_mca_board_common_init();

        mem_remap_top_256k();
        ps2.split_addr = mem_size * 1024;
        mca_init(8);

        ps2.planar_read = model_80_read;
        ps2.planar_write = model_80_write;

        device_add(&ps2_nvr_device);

        io_sethandler(0x00e0, 0x0002, mem_encoding_read, NULL, NULL, mem_encoding_write, NULL, NULL, NULL);

        ps2.mem_regs[1] = 2;

        switch (mem_size / 1024) {
        case 1:
                ps2.option[1] = 0x0c;
                break;
        case 2:
                ps2.option[1] = 0x0e;
                break;
        case 3:
                ps2.option[1] = 0x02;
                break;
        case 4:
        default:
                ps2.option[1] = 0x0a;
                break;
        }

        mem_mapping_add(&ps2.split_mapping, (mem_size + 256) * 1024, 256 * 1024, ps2_read_split_ram, ps2_read_split_ramw,
                        ps2_read_split_raml, ps2_write_split_ram, ps2_write_split_ramw, ps2_write_split_raml, &ram[0xa0000],
                        MEM_MAPPING_INTERNAL, NULL);
        mem_mapping_disable(&ps2.split_mapping);

        if (mem_size > 4096) {
                /* Only 4 MB supported on planar, create a memory expansion card for the rest */
                mem_mapping_set_addr(&ram_high_mapping, 0x100000, 0x300000);

                ps2.mem_pos_regs[0] = 0xff;
                ps2.mem_pos_regs[1] = 0xfc;

                switch (mem_size / 1024) {
                case 5:
                        ps2.mem_pos_regs[4] = 0xfc;
                        break;
                case 6:
                        ps2.mem_pos_regs[4] = 0xfe;
                        break;
                case 7:
                        ps2.mem_pos_regs[4] = 0xf2;
                        break;
                case 8:
                        ps2.mem_pos_regs[4] = 0xfa;
                        break;
                case 9:
                        ps2.mem_pos_regs[4] = 0xca;
                        break;
                case 10:
                        ps2.mem_pos_regs[4] = 0xea;
                        break;
                case 11:
                        ps2.mem_pos_regs[4] = 0x2a;
                        break;
                case 12:
                        ps2.mem_pos_regs[4] = 0xaa;
                        break;
                }

                mca_add(ps2_mem_expansion_read, ps2_mem_expansion_write, NULL, NULL);
                mem_mapping_add(&ps2.expansion_mapping, 0x400000, (mem_size - 4096) * 1024, mem_read_ram, mem_read_ramw,
                                mem_read_raml, mem_write_ram, mem_write_ramw, mem_write_raml, &ram[0x400000],
                                MEM_MAPPING_INTERNAL, NULL);
                mem_mapping_disable(&ps2.expansion_mapping);
        }
}
