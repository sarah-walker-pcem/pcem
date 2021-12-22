#include <stdlib.h>
#include "ibm.h"
#include "device.h"
#include "mem.h"
#include "zenith.h"

static uint8_t *zenith_scratchpad_ram;
static mem_mapping_t zenith_scratchpad_mapping;

static uint8_t zenith_scratchpad_read(uint32_t addr, void *p)
{
        return zenith_scratchpad_ram[addr & 0x3fff];
}
static void zenith_scratchpad_write(uint32_t addr, uint8_t val, void *p)
{
        zenith_scratchpad_ram[addr & 0x3fff] = val;
}

static void *zenith_scratchpad_init()
{
        zenith_scratchpad_ram = malloc(0x4000);

        mem_mapping_disable(&bios_mapping[4]);
        mem_mapping_disable(&bios_mapping[5]);
        
        mem_mapping_add(&zenith_scratchpad_mapping, 0xf0000, 0x4000,
                        zenith_scratchpad_read, NULL, NULL,
                        zenith_scratchpad_write, NULL, NULL,
                        zenith_scratchpad_ram,  MEM_MAPPING_EXTERNAL, NULL);

        return zenith_scratchpad_ram;
}

static void zenith_scratchpad_close(void *p)
{
        free(p);
}

device_t zenith_scratchpad_device =
{
        "Zenith scratchpad RAM",
        0,
        zenith_scratchpad_init,
        zenith_scratchpad_close,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
};
