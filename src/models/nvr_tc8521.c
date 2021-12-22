#include <stdio.h>
#include "ibm.h"
#include "io.h"
#include "nvr.h"
#include "nvr_tc8521.h"
#include "rtc_tc8521.h"
#include "pic.h"
#include "timer.h"
#include "rtc.h"
#include "paths.h"
#include "config.h"
#include "nmi.h"

static pc_timer_t nvr_onesec_timer;
static int nvr_onesec_cnt = 0;

static void tc8521_onesec(void *p)
{
        nvr_onesec_cnt++;
        if (nvr_onesec_cnt >= 100)
        {
		tc8521_tick();
                nvr_onesec_cnt = 0;
        }
        timer_advance_u64(&nvr_onesec_timer, 10000 * TIMER_USEC);
}

void write_tc8521(uint16_t addr, uint8_t val, void *priv)
{
	uint8_t page = nvrram[0x0D] & 3;

	addr &= 0x0F;
	if (addr < 0x0D) addr += 16 * page;

        if (addr >= 0x10 && nvrram[addr] != val)
        	nvr_dosave = 1;
               
	nvrram[addr] = val;
}


uint8_t read_tc8521(uint16_t addr, void *priv)
{
	uint8_t page = nvrram[0x0D] & 3;

	addr &= 0x0F;
	if (addr < 0x0D) addr += 16 * page;

	return nvrram[addr];
}


void tc8521_loadnvr()
{
        FILE *f;

        nvrmask=63;
        oldromset=romset;
        switch (romset)
        {
                case ROM_T1000: f = nvrfopen("t1000.nvr", "rb"); break;
                case ROM_T1200: f = nvrfopen("t1200.nvr", "rb"); break;
                default: return;
        }
        if (!f)
        {
                memset(nvrram,0xFF,64);
		nvrram[0x0E] = 0;	/* Test register */
                if (!enable_sync)
                {
			memset(nvrram, 0, 16);
                }
                return;
        }
        fread(nvrram,64,1,f);
        if (enable_sync)
                tc8521_internal_sync(nvrram);
        else
                tc8521_internal_set_nvrram(nvrram); /* Update the internal clock state based on the NVR registers. */
        fclose(f);
}


void tc8521_savenvr()
{
        FILE *f;
        switch (oldromset)
        {
                case ROM_T1000: f = nvrfopen("t1000.nvr", "wb"); break;
                case ROM_T1200: f = nvrfopen("t1200.nvr", "wb"); break;
                default: return;
        }
        fwrite(nvrram,64,1,f);
        fclose(f);
}

void nvr_tc8521_init()
{
        io_sethandler(0x2C0, 0x10, read_tc8521, NULL, NULL, write_tc8521, NULL, NULL,  NULL);
        timer_add(&nvr_onesec_timer, tc8521_onesec, NULL, 1);
}
