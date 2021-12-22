/*This is the chipset used in the LaserXT series model*/
#include "ibm.h"
#include "cpu.h"
#include "io.h"
#include "mem.h"

static int laserxt_emspage[4];
static int laserxt_emscontrol[4];
static mem_mapping_t laserxt_ems_mapping[4];
static int laserxt_ems_baseaddr_index = 0;
static uint8_t laserxt_turbo = 0x80;

static uint32_t get_laserxt_ems_addr(uint32_t addr)
{
        if (laserxt_emspage[(addr >> 14) & 3] & 0x80)
        {
                addr = (romset == ROM_LTXT ? 0x70000 + (((mem_size + 64) & 255) << 10) : 0x30000 + (((mem_size + 320) & 511) << 10)) + ((laserxt_emspage[(addr >> 14) & 3] & 0x0F) << 14) + ((laserxt_emspage[(addr >> 14) & 3] & 0x40) << 12) + (addr & 0x3FFF);
        }

        return addr;
}

static void laserxt_write(uint16_t port, uint8_t val, void *priv)
{
        int i;
        uint32_t paddr, vaddr;
        
        switch (port)
        {
                case 0x01F0:
                laserxt_turbo = val;
                cpu_set_turbo((val & 0x80) >> 7);
                //pclog("Turbo %s at %04X:%04X\n", (val & 0x80) ? "Enabled" : "Disabled", CS, cpu_state.oldpc);
                break;
                case 0x0208: case 0x4208: case 0x8208: case 0xC208:
                laserxt_emspage[port >> 14] = val;
                paddr = 0xC0000 + (port & 0xC000) + (((laserxt_ems_baseaddr_index + (4 - (port >> 14))) & 0x0C) << 14);

                if (val & 0x80)
                {
                        mem_mapping_enable(&laserxt_ems_mapping[port >> 14]);
                        vaddr = get_laserxt_ems_addr(paddr);
                        mem_mapping_set_exec(&laserxt_ems_mapping[port >> 14], ram + vaddr);
                }
                else
                {
                        mem_mapping_disable(&laserxt_ems_mapping[port >> 14]);
                }

                flushmmucache();
                break;

                case 0x0209: case 0x4209: case 0x8209: case 0xC209:
                laserxt_emscontrol[port >> 14] = val;
                laserxt_ems_baseaddr_index = 0;

                for (i = 0; i < 4; i++)
                        laserxt_ems_baseaddr_index |= (laserxt_emscontrol[i] & 0x80) >> (7 - i);

                mem_mapping_set_addr(&laserxt_ems_mapping[0], 0xC0000 + (((laserxt_ems_baseaddr_index + 4) & 0x0C) << 14), 0x4000);
                mem_mapping_set_addr(&laserxt_ems_mapping[1], 0xC4000 + (((laserxt_ems_baseaddr_index + 3) & 0x0C) << 14), 0x4000);
                mem_mapping_set_addr(&laserxt_ems_mapping[2], 0xC8000 + (((laserxt_ems_baseaddr_index + 2) & 0x0C) << 14), 0x4000);
                mem_mapping_set_addr(&laserxt_ems_mapping[3], 0xCC000 + (((laserxt_ems_baseaddr_index + 1) & 0x0C) << 14), 0x4000);

                flushmmucache();
                break;
        }
}

static uint8_t laserxt_read(uint16_t port, void *priv)
{
        switch (port)
        {
                case 0x01F0:
                return laserxt_turbo;

                case 0x0208: case 0x4208: case 0x8208: case 0xC208:
                return laserxt_emspage[port >> 14];

                case 0x0209: case 0x4209: case 0x8209: case 0xC209:
                return laserxt_emscontrol[port >> 14];
        }
        return 0xff;
}

static void mem_write_laserxtems(uint32_t addr, uint8_t val, void *priv)
{
        addr = get_laserxt_ems_addr(addr);
        if (addr < (mem_size << 10))
                ram[addr] = val;
}

static uint8_t mem_read_laserxtems(uint32_t addr, void *priv)
{
        uint8_t val = 0xFF;

        addr = get_laserxt_ems_addr(addr);
        if (addr < (mem_size << 10))
                val = ram[addr];

        return val;
}

void laserxt_init()
{
        int i;

        io_sethandler(0x01F0, 0x0001, laserxt_read, NULL, NULL, laserxt_write, NULL, NULL,  NULL);
        laserxt_turbo = 0x80;
        cpu_set_turbo(1);

        if (mem_size > 640)
        {
                io_sethandler(0x0208, 0x0002, laserxt_read, NULL, NULL, laserxt_write, NULL, NULL,  NULL);
                io_sethandler(0x4208, 0x0002, laserxt_read, NULL, NULL, laserxt_write, NULL, NULL,  NULL);
                io_sethandler(0x8208, 0x0002, laserxt_read, NULL, NULL, laserxt_write, NULL, NULL,  NULL);
                io_sethandler(0xc208, 0x0002, laserxt_read, NULL, NULL, laserxt_write, NULL, NULL,  NULL);
                mem_mapping_set_addr(&ram_low_mapping, 0, romset == ROM_LTXT ? 0x70000 + (((mem_size + 64) & 255) << 10) : 0x30000 + (((mem_size + 320) & 511) << 10));
        }

        for (i = 0; i < 4; i++)
        {
                laserxt_emspage[i] = 0x7F;
                laserxt_emscontrol[i] = (i == 3) ? 0x00 : 0x80;
                mem_mapping_add(&laserxt_ems_mapping[i], 0xE0000 + (i << 14), 0x4000, mem_read_laserxtems, NULL, NULL, mem_write_laserxtems, NULL, NULL, ram + 0xA0000 + (i << 14), 0, NULL);
                mem_mapping_disable(&laserxt_ems_mapping[i]);
        }
        mem_set_mem_state(0x0a0000, 0x60000, MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
}
