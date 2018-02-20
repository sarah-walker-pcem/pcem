#include "ibm.h"
#include "cpu.h"
#include "io.h"
#include "mem.h"
#include "pit.h"
#include "timer.h"
#include "x86.h"

#include "intel.h"

static uint8_t batman_port_92;

uint8_t batman_brdconfig(uint16_t port, void *p)
{
//        pclog("batman_brdconfig read port=%04x\n", port);
        switch (port)
        {
                case 0x73:
                return 0xff;
                case 0x75:
                return 0xdf;
                case 0x92:
                return batman_port_92;
        }
        return 0;
}

void batman_brdconfig_write(uint16_t port, uint8_t val, void *p)
{
        switch (port)
        {
                case 0x92:
                if ((mem_a20_alt ^ val) & 2)
                {
                         mem_a20_alt = val & 2;
                         mem_a20_recalc();
                }
                if ((~batman_port_92 & val) & 1)
                {
                         softresetx86();
                        cpu_set_edx();
                }
                batman_port_92 = val;
        }
}

static uint16_t batman_timer_latch;
static int batman_timer = 0;

static void batman_timer_over(void *p)
{
        batman_timer = 0;
}

static void batman_timer_write(uint16_t addr, uint8_t val, void *p)
{
        if (addr & 1)
                batman_timer_latch = (batman_timer_latch & 0xff) | (val << 8);
        else
                batman_timer_latch = (batman_timer_latch & 0xff00) | val;
        batman_timer = batman_timer_latch * TIMER_USEC;
}

static uint8_t batman_timer_read(uint16_t addr, void *p)
{
        uint16_t batman_timer_latch;
        
        cycles -= (int)PITCONST;
        
        timer_clock();

        if (batman_timer < 0)
                return 0;
        
        batman_timer_latch = batman_timer / TIMER_USEC;

        if (addr & 1)
                return batman_timer_latch >> 8;
        return batman_timer_latch & 0xff;
}

void intel_batman_init()
{
        io_sethandler(0x0073, 0x0001, batman_brdconfig, NULL, NULL, NULL, NULL, NULL, NULL);
        io_sethandler(0x0075, 0x0001, batman_brdconfig, NULL, NULL, NULL, NULL, NULL, NULL);

        io_sethandler(0x0078, 0x0002, batman_timer_read, NULL, NULL, batman_timer_write, NULL, NULL, NULL);
        io_sethandler(0x0092, 0x0001, batman_brdconfig, NULL, NULL, batman_brdconfig_write, NULL, NULL, NULL);
        batman_port_92 = 0;
        timer_add(batman_timer_over, &batman_timer, &batman_timer, NULL);
}


uint8_t endeavor_brdconfig(uint16_t port, void *p)
{
//        pclog("endeavor_brdconfig read port=%04x\n", port);
        switch (port)
        {
                case 0x79:
                return 0xff;
        }
        return 0;
}

void intel_endeavor_init()
{
        io_sethandler(0x0079, 0x0001, endeavor_brdconfig, NULL, NULL, NULL, NULL, NULL, NULL);
}

static uint8_t zappa_brdconfig(uint16_t port, void *p)
{
//        pclog("zappa_brdconfig read port=%04x\n", port);
        switch (port)
        {
                case 0x79:
                return 0xff;
        }
        return 0;
}

void intel_zappa_init()
{
        io_sethandler(0x0079, 0x0001, zappa_brdconfig, NULL, NULL, NULL, NULL, NULL, NULL);
}
