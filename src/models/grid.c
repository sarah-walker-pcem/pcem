/*This is the chipset used in the LaserXT series model*/
#include "ibm.h"
#include "cpu.h"
#include "io.h"
#include "mem.h"

#define GRID_APPROM_SELECT 0x440
#define GRID_APPROM_ENABLE 0x405

/*
approm mapping regs?
XXX_7FA         equ     7FAh
XXX_7F8         equ     7F8h
XXX_7F9         equ     7F9h
XXX_BD0         equ     0BD0h
XXX_BD1         equ     0BD1h
*/

#define GRID_EMS_PAGE_0 0x0258
#define GRID_EMS_PAGE_1 0x4258
#define GRID_EMS_PAGE_2 0x8258
#define GRID_EMS_PAGE_3 0xC258
#define GRID_TURBO 0x416
#define GRID_UNUSED_424 0x424
#define GRID_426 0x426
#define GRID_HIGH_ENABLE 0xFFF
#define GRID_ROM_SUBSYSTEM 0x6F8

// EMS window
#define GRID_EMS_BASE 0xE0000
#define GRID_EMS_PAGE_SIZE 0x4000
#define GRID_EMS_PAGE_MASK 0x3FFF
#define GRID_EMS_PAGE_SHIFT 14
// physical base of extended memory
#define GRID_EXTENDED_BASE 0xA0000
#define GRID_1M 0x100000

static uint8_t grid_unknown = 0;
static uint8_t grid_unused_424 = 0;
static uint8_t grid_426 = 0;
static uint8_t grid_high_enable = 0;
static uint8_t grid_ems_page[4];
static mem_mapping_t grid_ems_mapping[4];
static mem_mapping_t grid_high_mapping;
static uint8_t grid_turbo = 0x01;
static uint8_t grid_rom_enable = 0;
static uint8_t grid_rom_select = 0;

static uint32_t get_grid_ems_paddr(uint32_t addr) {
        uint32_t slot = (addr >> GRID_EMS_PAGE_SHIFT) & 0x3;
        uint32_t paddr = addr;
        if (grid_ems_page[slot] & 0x80) {
                paddr = GRID_EXTENDED_BASE + ((uint32_t)(grid_ems_page[slot] & 0x7F) << GRID_EMS_PAGE_SHIFT) + (addr & GRID_EMS_PAGE_MASK);
                //pclog("EMSADDR %08X->%08X\n", addr, paddr);
        }

        return paddr;
}

static void grid_ems_mem_write8(uint32_t addr, uint8_t val, void *priv) {
        addr = get_grid_ems_paddr(addr);
        if (addr < (mem_size << 10)) {
                ram[addr] = val;                
        }
}

static uint8_t grid_ems_mem_read8(uint32_t addr, void *priv) {
        uint8_t val = 0xFF;

        addr = get_grid_ems_paddr(addr);
        if (addr < (mem_size << 10)) {
                val = ram[addr];                
        }

        return val;
}

static void grid_ems_mem_write16(uint32_t addr, uint16_t val, void *priv) {
        addr = get_grid_ems_paddr(addr);
        if (addr < (mem_size << 10)) {
                *(uint16_t *)&(ram[addr]) = val;                
        }
}

static uint16_t grid_ems_mem_read16(uint32_t addr, void *priv) {
        uint16_t val = 0xFFFF;

        addr = get_grid_ems_paddr(addr);
        if (addr < (mem_size << 10)) {
                val = *(uint16_t *)&(ram[addr]);
        }
        return val;
}

static uint32_t get_grid_high_paddr(uint32_t addr) {        
        return addr - 0x60000;
}

static void grid_high_mem_write8(uint32_t addr, uint8_t val, void *priv) {
        addr = get_grid_high_paddr(addr);
        if (addr < (mem_size << 10))
                ram[addr] = val;
}

static uint8_t grid_high_mem_read8(uint32_t addr, void *priv) {
        uint8_t val = 0xFF;

        addr = get_grid_high_paddr(addr);
        if (addr < (mem_size << 10))
                val = ram[addr];

        return val;
}

static void grid_high_mem_write16(uint32_t addr, uint16_t val, void *priv) {
        addr = get_grid_high_paddr(addr);
        if (addr < (mem_size << 10))
                *(uint16_t *)&ram[addr] = val;
}

static uint16_t grid_high_mem_read16(uint32_t addr, void *priv) {
        uint16_t val = 0xFFFF;

        addr = get_grid_high_paddr(addr);
        if (addr < (mem_size << 10))
                val = *(uint16_t *)&ram[addr];

        return val;
}

static void grid_ems_update_mapping(uint32_t slot) {
        uint32_t vaddr = GRID_EMS_BASE + (slot << GRID_EMS_PAGE_SHIFT);
        if (grid_ems_page[slot] & 0x80) {
                uint32_t paddr;
                mem_mapping_enable(&grid_ems_mapping[slot]);
                paddr = get_grid_ems_paddr(vaddr);
                mem_mapping_set_exec(&grid_ems_mapping[slot], ram + paddr);
        } else {
                mem_mapping_disable(&grid_ems_mapping[slot]);
        }
}

static void grid_io_write(uint16_t port, uint8_t val, void *priv) {
        int i;
        uint32_t paddr, vaddr;

        switch (port) {
        case GRID_426:
                grid_426 = val;
                break;
        case GRID_UNUSED_424:
                grid_unused_424 = val;
                break;
        case GRID_ROM_SUBSYSTEM:
        case GRID_ROM_SUBSYSTEM+1:
        case GRID_ROM_SUBSYSTEM+2:
        case GRID_ROM_SUBSYSTEM+3:
        case GRID_ROM_SUBSYSTEM+4:
        case GRID_ROM_SUBSYSTEM+5:
        case GRID_ROM_SUBSYSTEM+6:
        case GRID_ROM_SUBSYSTEM+7:
                break;
        case GRID_APPROM_SELECT:
                grid_rom_select = val;
                break;
        case GRID_APPROM_ENABLE:
                grid_rom_enable = val;
                break;
        case GRID_TURBO:   
                grid_turbo = val;
                cpu_set_turbo(val & 0x01);
                break;
        case GRID_EMS_PAGE_0:
        case GRID_EMS_PAGE_1:
        case GRID_EMS_PAGE_2:
        case GRID_EMS_PAGE_3: {
                uint8_t page = val & 0x7F;
                uint32_t slot = (port >> 14) & 0x3;
                if (grid_ems_page[slot] == val)
                        break; // no change

                grid_ems_page[slot] = val;
                if (grid_high_enable & 0x1)
                        break; // XMS is enabled
                grid_ems_update_mapping(slot);

                flushmmucache();
                break;
        }
        case GRID_HIGH_ENABLE: {
                uint32_t i;

                if (((val ^ grid_high_enable) & 0x1) == 0)
                        break; // no change
                grid_high_enable = val;
                if (grid_high_enable & 0x1) {
                        for (i = 0; i < 4; i++)
                                mem_mapping_disable(&grid_ems_mapping[i]);
                        mem_mapping_enable(&grid_high_mapping);
                } else {
                        mem_mapping_disable(&grid_high_mapping);
                        for (i = 0; i < 4; i++) {
                                grid_ems_update_mapping(i);
                        }
                }
                flushmmucache();
                break;
        }
        default:
                break;
        }
}

static uint8_t grid_io_read(uint16_t port, void *priv) {
        switch (port) {
        case GRID_426:
                return grid_426;
                break;
        case GRID_UNUSED_424:
                return grid_unused_424;
                break;
        case GRID_ROM_SUBSYSTEM:
                return 0x99;
                break;
        case GRID_ROM_SUBSYSTEM+1:
        case GRID_ROM_SUBSYSTEM+2:
        case GRID_ROM_SUBSYSTEM+3:
        case GRID_ROM_SUBSYSTEM+4:
        case GRID_ROM_SUBSYSTEM+5:
        case GRID_ROM_SUBSYSTEM+6:
        case GRID_ROM_SUBSYSTEM+7:
                break;
        case GRID_APPROM_SELECT:
                return grid_rom_select;
        case GRID_APPROM_ENABLE:
                return grid_rom_enable;
        case GRID_TURBO:   
                return grid_turbo;
        case GRID_HIGH_ENABLE:
                return grid_high_enable;
        case GRID_EMS_PAGE_0:
        case GRID_EMS_PAGE_1:
        case GRID_EMS_PAGE_2:
        case GRID_EMS_PAGE_3: {
                uint32_t slot = (port >> 14) & 0x3;
                return grid_ems_page[slot];
        }
        default:
                break;
        }

        return 0xff;
}

void grid_init() {
        uint32_t slot;

        // GRiD only has 32KB BIOS and segment E0000 is used for EMS
        for (slot = 0; slot < 6; slot++) {
                mem_mapping_disable(&bios_mapping[slot]);
        }

        io_sethandler(GRID_ROM_SUBSYSTEM, 0x0008, grid_io_read, NULL, NULL, grid_io_write, NULL, NULL, NULL);
        io_sethandler(GRID_UNUSED_424, 0x0001, grid_io_read, NULL, NULL, grid_io_write, NULL, NULL, NULL);
        io_sethandler(GRID_426, 0x0001, grid_io_read, NULL, NULL, grid_io_write, NULL, NULL, NULL);
        io_sethandler(GRID_APPROM_SELECT, 0x0001, grid_io_read, NULL, NULL, grid_io_write, NULL, NULL, NULL);
        io_sethandler(GRID_APPROM_ENABLE, 0x0001, grid_io_read, NULL, NULL, grid_io_write, NULL, NULL, NULL);
        io_sethandler(GRID_TURBO, 0x0001, grid_io_read, NULL, NULL, grid_io_write, NULL, NULL, NULL);
        grid_turbo = 0x1;
        cpu_set_turbo(1);

        io_sethandler(GRID_HIGH_ENABLE, 0x0001, grid_io_read, NULL, NULL, grid_io_write, NULL, NULL, NULL);
        io_sethandler(GRID_EMS_PAGE_0, 0x0001, grid_io_read, NULL, NULL, grid_io_write, NULL, NULL, NULL);
        io_sethandler(GRID_EMS_PAGE_1, 0x0001, grid_io_read, NULL, NULL, grid_io_write, NULL, NULL, NULL);
        io_sethandler(GRID_EMS_PAGE_2, 0x0001, grid_io_read, NULL, NULL, grid_io_write, NULL, NULL, NULL);
        io_sethandler(GRID_EMS_PAGE_3, 0x0001, grid_io_read, NULL, NULL, grid_io_write, NULL, NULL, NULL);
 
        grid_high_enable = 0;
        for (slot = 0; slot < 4; slot++) {
                grid_ems_page[slot] = 0;
                mem_mapping_add(&grid_ems_mapping[slot], GRID_EMS_BASE + (slot << GRID_EMS_PAGE_SHIFT), GRID_EMS_PAGE_SIZE, grid_ems_mem_read8, grid_ems_mem_read16, NULL,
                                grid_ems_mem_write8, grid_ems_mem_write16, NULL, ram + GRID_EXTENDED_BASE + (slot << GRID_EMS_PAGE_SHIFT), MEM_MAPPING_EXTERNAL, NULL);
                mem_mapping_disable(&grid_ems_mapping[slot]);
                //mem_set_mem_state(GRID_EMS_BASE + (i << GRID_EMS_PAGE_SHIFT), GRID_EMS_PAGE_SIZE, MEM_READ_ANY | MEM_WRITE_ANY);
                //mem_set_mem_state(GRID_EMS_BASE + (i << GRID_EMS_PAGE_SHIFT), GRID_EMS_PAGE_SIZE, MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
        }
        mem_mapping_add(&grid_high_mapping, GRID_1M, (mem_size-640) << 10, grid_high_mem_read8, grid_high_mem_read16, NULL,
                                grid_high_mem_write8, grid_high_mem_write16, NULL, ram + GRID_EXTENDED_BASE, 0, NULL);
        mem_mapping_enable(&grid_high_mapping);

        //mem_set_mem_state(GRID_EXTENDED_BASE, (mem_size-640) << 10, MEM_READ_ANY | MEM_WRITE_ANY);
        flushmmucache();
}

/*
"CP3022",5
"CP3024",5
"CP344",6,805,4,26, translated 980,5,17
"CP3044",9,980,5,17
"CP3042",9
"CP3104",7,776,8,33
*/



