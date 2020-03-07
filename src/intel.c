#include "ibm.h"
#include "cpu.h"
#include "io.h"
#include "mem.h"
#include "model.h"
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
static pc_timer_t batman_timer;

static void batman_timer_over(void *p)
{
}

static void batman_timer_write(uint16_t addr, uint8_t val, void *p)
{
        if (addr & 1)
                batman_timer_latch = (batman_timer_latch & 0xff) | (val << 8);
        else
                batman_timer_latch = (batman_timer_latch & 0xff00) | val;
        timer_set_delay_u64(&batman_timer, batman_timer_latch * TIMER_USEC);
}

static uint8_t batman_timer_read(uint16_t addr, void *p)
{
        uint16_t batman_timer_latch;
        
        cycles -= (int)(PITCONST >> 32);
        
        batman_timer_latch = timer_get_remaining_us(&batman_timer);

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
        timer_add(&batman_timer, batman_timer_over, NULL, 0);
}


uint8_t endeavor_brdconfig(uint16_t port, void *p)
{
        uint8_t temp;
        CPU *cpu_s = &models[model].cpu[cpu_manufacturer].cpus[cpu];
//        pclog("endeavor_brdconfig read port=%04x\n", port);
        switch (port)
        {
                case 0x79:
                // Bit # | Description                         | Bit = 1       | Bit = 0
                // 0     | Internal CPU Clock Freq. (Switch 6) | 3/2x          | 2x              (Manual has these swapped in one place?)
                // 1     | Soft Off capable power supply       | No            | Yes
                // 2     | On-board Audio present              | Yes           | No
                // 3     | External CPU clock (Switch 7)       |               |
                // 4     | External CPU clock (Switch 8)       |               |
                // 5     | Setup Disable (Switch 5)            | Enable access | Disable access
                // 6     | Clear CMOS (Switch 4)               | Keep values   | Clear values
                // 7     | Password Clear (Switch 3)           | Keep password | Clear password

                // Bus Speed | Switch 7 | Switch 8
                // 50 MHz    | Off      | Off
                // 60 Mhz    | On       | Off
                // 66 MHz    | Off      | On

                // Multiplier for each supported processor-
                // -The 3/2 setting is used for 75 MHz, 90 MHz, and 100 MHz processors
                // -The 2 times setting is used for 120 MHz processors.

                if (cpu_s->rspeed == 75000000 && cpu_s->pci_speed == 25000000)
                        temp = 0x01;
                else if (cpu_s->rspeed == 90000000 && cpu_s->pci_speed == 30000000)
                        temp = 0x01 | 0x08;
                else if (cpu_s->rspeed == 100000000 && cpu_s->pci_speed == 25000000)
                        temp = 0x00;
                else if (cpu_s->rspeed == 100000000 && cpu_s->pci_speed == 33333333)
                        temp = 0x01 | 0x10;
                else if (cpu_s->rspeed == 120000000 && cpu_s->pci_speed == 30000000)
                        temp = 0x08;
                else if (cpu_s->rspeed == 133333333 && cpu_s->pci_speed == 33333333)
                        temp = 0x10;
                else
                        temp = 0x10; // TODO: how are the overdrive processors configured?
                return 0xe2 | temp;
        }
        return 0;
}

void intel_endeavor_init()
{
        io_sethandler(0x0079, 0x0001, endeavor_brdconfig, NULL, NULL, NULL, NULL, NULL, NULL);
}

static uint8_t zappa_brdconfig(uint16_t port, void *p)
{
        uint8_t temp;
        CPU *cpu_s = &models[model].cpu[cpu_manufacturer].cpus[cpu];
//        pclog("zappa_brdconfig read port=%04x\n", port);
        switch (port)
        {
                case 0x79:
                // Bit # | Description                         | Bit = 1       | Bit = 0
                // 0     | Internal CPU Clock Freq. (Switch 6) | 3/2x          | 2x              (Manual has these swapped in one place?)
                // 1     | No Connect                          |               |
                // 2     | No Connect                          |               |
                // 3     | External CPU clock (Switch 7)       |               |
                // 4     | External CPU clock (Switch 8)       |               |
                // 5     | Setup Disable (Switch 5)            | Enable access | Disable access
                // 6     | Clear CMOS (Switch 4)               | Keep values   | Clear values
                // 7     | Password Clear (Switch 3)           | Keep password | Clear password

                // Bus Speed | Switch 7 | Switch 8
                // 50 MHz    | Off      | Off
                // 60 Mhz    | On       | Off
                // 66 MHz    | Off      | On

                // Multiplier for each supported processor-
                // -The 3/2 setting is used for 75 MHz, 90 MHz, and 100 MHz processors
                // -The 2 times setting is used for 120 MHz processors.

                if (cpu_s->rspeed == 75000000 && cpu_s->pci_speed == 25000000)
                        temp = 0x01;
                else if (cpu_s->rspeed == 90000000 && cpu_s->pci_speed == 30000000)
                        temp = 0x01 | 0x08;
                else if (cpu_s->rspeed == 100000000 && cpu_s->pci_speed == 25000000)
                        temp = 0x00;
                else if (cpu_s->rspeed == 100000000 && cpu_s->pci_speed == 33333333)
                        temp = 0x01 | 0x10;
                else if (cpu_s->rspeed == 120000000 && cpu_s->pci_speed == 30000000)
                        temp = 0x08;
                else if (cpu_s->rspeed == 133333333 && cpu_s->pci_speed == 33333333)
                        temp = 0x10;
                else
                        temp = 0x10; // TODO: how are the overdrive processors configured?
                return 0xe0 | temp;
        }
        return 0;
}

void intel_zappa_init()
{
        io_sethandler(0x0079, 0x0001, zappa_brdconfig, NULL, NULL, NULL, NULL, NULL, NULL);
}
