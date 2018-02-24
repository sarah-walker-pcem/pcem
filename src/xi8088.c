#include "ibm.h"
#include "io.h"
#include "cpu.h"
#include "keyboard_at.h"

#include "xi8088.h"

static uint8_t xi8088_turbo;

uint8_t xi8088_turbo_get()
{
        return xi8088_turbo;
}

void xi8088_turbo_set(uint8_t value)
{
        xi8088_turbo = value;
        if (!value)
        {
                pclog("Xi8088 turbo off\n");
                int c = cpu;
                cpu = 0;        /* 8088/4.77 */
                cpu_set();
                cpu = c;
        }
        else
        {
                pclog("Xi8088 turbo on\n");
                cpu_set();
        }
}

void xi8088_init()
{
        xi8088_turbo = 1;
}
