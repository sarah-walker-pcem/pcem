#include "ibm.h"
#include "device.h"
#include "io.h"
#include "cpu.h"
#include "lpt.h"
#include "mem.h"
#include "serial.h"
#include "vid_oti067.h"

#include "acer386sx.h"

static int acer_index = 0;
static uint8_t acer_regs[256];
static uint8_t acer_last_write;
static int acer_locked;
static void *acer_oti067;

void acer386sx_set_oti067(void *oti067)
{
        acer_oti067 = oti067;
}

void acer386sx_write(uint16_t addr, uint8_t val, void *priv)
{
        if (addr & 1)
        {
                if (acer_locked)
                {
                        if (acer_last_write == 0x14 && val == 0x09)
                                acer_locked = 0;
                        
                        acer_last_write = val;
                        return;
                }

                if (acer_index == 0xff && val == 0xff)
                        acer_locked = 1;
                else
                {
                        pclog("Write ACER reg %02x %02x %08x\n", acer_index, val, cs);
                        acer_regs[acer_index] = val;

                        switch (acer_index)
                        {
                                case 0xa:
                                switch ((val >> 4) & 3)
                                {
                                        case 0:
                                        mem_set_mem_state(0xe0000, 0x10000, MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
                                        break;
                                        case 1:
                                        mem_set_mem_state(0xe0000, 0x10000, MEM_READ_INTERNAL | MEM_WRITE_EXTERNAL);
                                        break;
                                        case 2:
                                        mem_set_mem_state(0xe0000, 0x10000, MEM_READ_EXTERNAL | MEM_WRITE_INTERNAL);
                                        break;
                                        case 3:
                                        mem_set_mem_state(0xe0000, 0x10000, MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
                                        break;
                                }
                                break;
                                case 0xb:
                                switch ((val >> 4) & 3)
                                {
                                        case 0:
                                        mem_set_mem_state(0xf0000, 0x10000, MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
                                        break;
                                        case 1:
                                        mem_set_mem_state(0xf0000, 0x10000, MEM_READ_INTERNAL | MEM_WRITE_EXTERNAL);
                                        break;
                                        case 2:
                                        mem_set_mem_state(0xf0000, 0x10000, MEM_READ_EXTERNAL | MEM_WRITE_INTERNAL);
                                        break;
                                        case 3:
                                        mem_set_mem_state(0xf0000, 0x10000, MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
                                        break;
                                }
                                break;
                        }
                }
        }
        else
                acer_index = val;
}

uint8_t acer386sx_read(uint16_t addr, void *priv)
{
        if (acer_locked)
                return 0xff;
        if (addr & 1)
        {
                if ((acer_index >= 0xc0 || acer_index == 0x20) && cpu_iscyrix)
                   return 0xff; /*Don't conflict with Cyrix config registers*/
                return acer_regs[acer_index];
        }
        else
           return acer_index;
}

static uint8_t acer386sx_37f = 0, acer386sx_io = 0;
void acer386sx_io_write(uint16_t addr, uint8_t val, void *priv)
{
        switch (addr)
        {
                case 0x37f:
                acer386sx_37f = val;
                if (val & 4)
                        oti067_enable_disable(acer_oti067, 0);
                else
                        oti067_enable_disable(acer_oti067, 1);
                break;
                
                case 0x3f3:
                acer386sx_io = val;
                serial1_remove();
                serial2_remove();
                lpt1_remove();
                lpt2_remove();

                switch (val & 3)
                {
                        case 0: lpt1_init(0x378); break;
                        case 1: lpt1_init(0x3bc); break;
                        case 2: lpt1_init(0x278); break;
                }
                if (!(val & 0x04))
                        serial1_set(0x3f8, 4);
                if (!(val & 0x08))
                        serial2_set(0x2f8, 3);
                break;
        }
}

uint8_t acer386sx_io_read(uint16_t addr, void *priv)
{
        switch (addr)
        {
                case 0x37f:
                return acer386sx_37f;
                
                case 0x3f3:
                return acer386sx_io;
        }
        
        return 0xff;
}

void acer386sx_init()
{
        io_sethandler(0x0022, 0x0002, acer386sx_read, NULL, NULL, acer386sx_write, NULL, NULL,  NULL);
        io_sethandler(0x037f, 0x0001, acer386sx_io_read, NULL, NULL, acer386sx_io_write, NULL, NULL,  NULL);
        io_sethandler(0x03f3, 0x0001, acer386sx_io_read, NULL, NULL, acer386sx_io_write, NULL, NULL,  NULL);
 
        acer386sx_io = 0;
        acer386sx_37f = 0;
}
