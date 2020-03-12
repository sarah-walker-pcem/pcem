#include <stdio.h>
#include <stdlib.h>
#include "ibm.h"
#include "device.h"
#include "io.h"
#include "nvr.h"
#include "nvr_tc8521.h"
#include "pic.h"
#include "timer.h"
#include "rtc.h"
#include "paths.h"
#include "config.h"
#include "model.h"
#include "nmi.h"
#include "t1000.h"
#include "x86.h"

int oldromset;
int nvrmask=63;
uint8_t nvrram[128];
int nvraddr;

int nvr_dosave = 0;

typedef struct nvr_t
{
        pc_timer_t rtc_timer;
        pc_timer_t onesec_timer;
        pc_timer_t update_end_timer;

        int onesec_cnt;
} nvr_t;

FILE *nvrfopen(char *fn, char *mode)
{
        char s[512];
        FILE *f;
                
        strcpy(s, nvr_path);
        put_backslash(s);
        strcat(s, config_name);
        strcat(s, ".");
        strcat(s, fn);
        pclog("NVR try opening %s\n", s);
        f = fopen(s, mode);
        if (f)
                return f;

        if (mode[0] == 'r')
        {
                strcpy(s, nvr_path);
                put_backslash(s);
                strcat(s, "default");
                put_backslash(s);
                strcat(s, fn);
                return fopen(s, mode);
        }
        else
        {
                pclog("Failed to open file '%s' for write\n", s);
                return NULL;
        }
}

void getnvrtime()
{
	time_get(nvrram);
}

static void nvr_speed_changed(void *p)
{
        nvr_t *nvr = (nvr_t *)p;

        if (!(nvrram[RTC_REGA] & RTC_RS))
        {
		timer_disable(&nvr->rtc_timer);
                return;
        }
        else
        {
                int c = 1 << ((nvrram[RTC_REGA] & RTC_RS) - 1);
        	timer_set_delay_u64(&nvr->rtc_timer, (uint64_t)(RTCCONST * c));
        }
}

static void nvr_rtc(void *p)
{
        nvr_t *nvr = (nvr_t *)p;

        if (!(nvrram[RTC_REGA] & RTC_RS))
        {
		timer_disable(&nvr->rtc_timer);
                return;
        }
        else
        {
                int c = 1 << ((nvrram[RTC_REGA] & RTC_RS) - 1);
                timer_advance_u64(&nvr->rtc_timer, (uint64_t)(RTCCONST * c));
//                pclog("RTCtime now %f\n",rtctime);
                nvrram[RTC_REGC] |= RTC_PF;
                if (nvrram[RTC_REGB] & RTC_PIE)
                {
                        nvrram[RTC_REGC] |= RTC_IRQF;
                        if (AMSTRAD) picint(2);
                        else         picint(0x100);
//                        pclog("RTC int\n");
                }
        }
}

int nvr_update_status = 0;

#define ALARM_DONTCARE	0xc0

int nvr_check_alarm(int nvraddr)
{
        return (nvrram[nvraddr + 1] == nvrram[nvraddr] || (nvrram[nvraddr + 1] & ALARM_DONTCARE) == ALARM_DONTCARE);
}


static void nvr_update_end(void *p)
{
//        nvr_t *nvr = (nvr_t *)p;

        if (!(nvrram[RTC_REGB] & RTC_SET))
        {
                getnvrtime();
                /* Clear update status. */
                nvr_update_status = 0;

                if (nvr_check_alarm(RTC_SECONDS) && nvr_check_alarm(RTC_MINUTES) && nvr_check_alarm(RTC_HOURS))
                {
                        nvrram[RTC_REGC] |= RTC_AF;
                        if (nvrram[RTC_REGB] & RTC_AIE)
                        {
                                nvrram[RTC_REGC] |= RTC_IRQF;
                                if (AMSTRAD) picint(2);
                                else         picint(0x100);
                        }
                }

                /* The flag and interrupt should be issued on update ended, not started. */
                nvrram[RTC_REGC] |= RTC_UF;
                if (nvrram[RTC_REGB] & RTC_UIE)
                {
                        nvrram[RTC_REGC] |= RTC_IRQF;
                        if (AMSTRAD) picint(2);
                        else         picint(0x100);
                }
        }
        
//                pclog("RTC onesec\n");
}

static void nvr_onesec(void *p)
{
        nvr_t *nvr = (nvr_t *)p;

        nvr->onesec_cnt++;
        if (nvr->onesec_cnt >= 100)
        {
                if (!(nvrram[RTC_REGB] & RTC_SET))
                {
                        nvr_update_status = RTC_UIP;
                        rtc_tick();

                        timer_set_delay_u64(&nvr->update_end_timer, (uint64_t)((244.0 + 1984.0) * TIMER_USEC));
                }
                nvr->onesec_cnt = 0;
        }
        timer_advance_u64(&nvr->onesec_timer, (uint64_t)(10000 * TIMER_USEC));
}

static void writenvr(uint16_t addr, uint8_t val, void *p)
{
        nvr_t *nvr = (nvr_t *)p;
        int c, old;
        
        cycles -= ISA_CYCLES(8);
//        printf("Write NVR %03X %02X %02X %04X:%04X %i\n",addr,nvraddr,val,cs>>4,pc,ins);
        if (addr&1)
        {
                if (nvraddr==RTC_REGC || nvraddr==RTC_REGD)
                        return; /* Registers C and D are read-only. There's no reason to continue. */
//                if (nvraddr == 0x33) pclog("NVRWRITE33 %02X %04X:%04X %i\n",val,CS,pc,ins);
                if (nvraddr > RTC_REGD && nvrram[nvraddr] != val)
                   nvr_dosave = 1;
                
		old = nvrram[nvraddr];
                nvrram[nvraddr]=val;

                if (nvraddr == RTC_REGA)
                {
//                        pclog("NVR rate %i\n",val&0xF);
                        if (val & RTC_RS)
                        {
                                c = 1 << ((val & RTC_RS) - 1);
				timer_set_delay_u64(&nvr->rtc_timer, (uint64_t)(RTCCONST * c));
                        }
                        else
				timer_disable(&nvr->rtc_timer);
                }
		else
		{
                        if (nvraddr == RTC_REGB)
                        {
                                if (((old ^ val) & RTC_SET) && (val & RTC_SET))
                                {
                                        nvrram[RTC_REGA] &= ~RTC_UIP;             /* This has to be done according to the datasheet. */
                                        nvrram[RTC_REGB] &= ~RTC_UIE;             /* This also has to happen per the specification. */
                                }
                        }

                        if ((nvraddr < RTC_REGA) || (nvraddr == RTC_CENTURY))
                        {
                                if ((nvraddr != 1) && (nvraddr != 3) && (nvraddr != 5))
                                {
                                        if ((old != val) && !enable_sync)
                                        {
                                                time_update(nvrram, nvraddr);
                                                nvr_dosave = 1;
                                        }
                                }
                        }
                }
        }
        else
        {
                nvraddr=val&nvrmask;
                /*PS/2 BIOSes will disable NMIs and expect the watchdog timer to still be able
                  to fire them. I suspect the watchdog is exempt from NMI masking. Currently NMIs
                  are always enabled for PS/2 machines - this would mean that other peripherals
                  could fire NMIs regardless of the mask state, but as there aren't any emulated
                  MCA peripherals that do this it's currently a moot point.*/

		/* Also don't update the NMI mask on Amstrad PCs - actually
		 * ought not to do it for any XT because their NMI mask 
		 * register is at 0xA0. But particularly important on the 
		 * PC200 and PPC because their video subsystem issues NMIs */
                if (!(models[model].flags & (MODEL_MCA | MODEL_AMSTRAD)))
		{
                        nmi_mask = ~val & 0x80;
		}
        }
}

uint8_t readnvr(uint16_t addr, void *p)
{
//        nvr_t *nvr = (nvr_t *)p;
        uint8_t temp;
//        printf("Read NVR %03X %02X %02X %04X:%04X\n",addr,nvraddr,nvrram[nvraddr],cs>>4,pc);
        cycles -= ISA_CYCLES(8);
        if (addr&1)
        {
                if (nvraddr == RTC_REGA)
                        return ((nvrram[RTC_REGA] & 0x7F) | nvr_update_status);
                if (nvraddr == RTC_REGD)
                        nvrram[RTC_REGD] |= RTC_VRT;
                if (nvraddr == RTC_REGC)
                {
                        if (AMSTRAD) picintc(2);
                        else         picintc(0x100);
                        temp = nvrram[RTC_REGC];
                        nvrram[RTC_REGC] = 0;
                        return temp;
                }
//                if (AMIBIOS && nvraddr==0x36) return 0;
//                if (nvraddr==0xA) nvrram[0xA]^=0x80;
                return nvrram[nvraddr];
        }
        return nvraddr;
}

void loadnvr()
{
        FILE *f;

        nvrmask=63;
        oldromset=romset;
        switch (romset)
        {
                case ROM_PC1512:      f = nvrfopen("pc1512.nvr",      "rb"); break;
                case ROM_PC1640:      f = nvrfopen("pc1640.nvr",      "rb"); break;
                case ROM_PC200:       f = nvrfopen("pc200.nvr",       "rb"); break;
                case ROM_PC2086:      f = nvrfopen("pc2086.nvr",      "rb"); break;
                case ROM_PC3086:      f = nvrfopen("pc3086.nvr",      "rb"); break;
                case ROM_PPC512:      f = nvrfopen("ppc512.nvr",      "rb"); break;
                case ROM_IBMAT:       f = nvrfopen("at.nvr",          "rb"); break;
                case ROM_IBMXT286:    f = nvrfopen("ibmxt286.nvr",    "rb"); break;
                case ROM_IBMPS1_2011: f = nvrfopen("ibmps1_2011.nvr", "rb"); /*nvrmask = 127; */break;
                case ROM_IBMPS1_2121: f = nvrfopen("ibmps1_2121.nvr", "rb"); nvrmask = 127; break;
                case ROM_IBMPS2_M30_286:   f = nvrfopen("ibmps2_m30_286.nvr",   "rb"); /*nvrmask = 127; */break;
                case ROM_IBMPS2_M50:  f = nvrfopen("ibmps2_m50.nvr",  "rb"); break;
                case ROM_IBMPS2_M55SX: f = nvrfopen("ibmps2_m55sx.nvr",  "rb"); break;
                case ROM_IBMPS2_M80:  f = nvrfopen("ibmps2_m80.nvr",  "rb"); break;
                case ROM_CMDPC30:     f = nvrfopen("cmdpc30.nvr",     "rb"); nvrmask = 127; break;
                case ROM_AMI286:      f = nvrfopen("ami286.nvr",      "rb"); nvrmask = 127; break;
                case ROM_TG286M:      f = nvrfopen("tg286m.nvr",      "rb"); nvrmask = 127; break;
                case ROM_AWARD286:    f = nvrfopen("award286.nvr",    "rb"); nvrmask = 127; break;
                case ROM_GDC212M:     f = nvrfopen("gdc212m.nvr",     "rb"); nvrmask = 127; break;
                case ROM_HYUNDAI_SUPER286TR: f = nvrfopen("super286tr.nvr", "rb"); nvrmask = 127; break;
                case ROM_GW286CT:     f = nvrfopen("gw286ct.nvr",     "rb"); nvrmask = 127; break;
                case ROM_SPC4200P:    f = nvrfopen("spc4200p.nvr",    "rb"); nvrmask = 127; break;
                case ROM_SPC4216P:    f = nvrfopen("spc4216p.nvr",    "rb"); nvrmask = 127; break;
                case ROM_SPC4620P:    f = nvrfopen("spc4620p.nvr",    "rb"); nvrmask = 127; break;
                case ROM_DELL200:     f = nvrfopen("dell200.nvr",     "rb"); nvrmask = 127; break;
                case ROM_IBMAT386:    f = nvrfopen("at386.nvr",       "rb"); nvrmask = 127; break;
                case ROM_DESKPRO_386: f = nvrfopen("deskpro386.nvr",  "rb"); break;
                case ROM_ACER386:     f = nvrfopen("acer386.nvr",     "rb"); nvrmask = 127; break;
                case ROM_KMXC02:      f = nvrfopen("kmxc02.nvr",      "rb"); nvrmask = 127; break;
                case ROM_MEGAPC:      f = nvrfopen("megapc.nvr",      "rb"); nvrmask = 127; break;
                case ROM_AMA932J:     f = nvrfopen("ama932j.nvr",     "rb"); nvrmask = 127; break;
                case ROM_AMI386SX:    f = nvrfopen("ami386.nvr",      "rb"); nvrmask = 127; break;
                case ROM_AMI486:      f = nvrfopen("ami486.nvr",      "rb"); nvrmask = 127; break;
                case ROM_WIN486:      f = nvrfopen("win486.nvr",      "rb"); nvrmask = 127; break;
                case ROM_PCI486:      f = nvrfopen("hot-433.nvr",     "rb"); nvrmask = 127; break;
                case ROM_SIS496:      f = nvrfopen("sis496.nvr",      "rb"); nvrmask = 127; break;
                case ROM_P55VA:       f = nvrfopen("p55va.nvr",       "rb"); nvrmask = 127; break;
                case ROM_P55TVP4:     f = nvrfopen("p55tvp4.nvr",     "rb"); nvrmask = 127; break;
                case ROM_430VX:       f = nvrfopen("430vx.nvr",       "rb"); nvrmask = 127; break;
                case ROM_REVENGE:     f = nvrfopen("revenge.nvr",     "rb"); nvrmask = 127; break;
                case ROM_ENDEAVOR:    f = nvrfopen("endeavor.nvr",    "rb"); nvrmask = 127; break;
                case ROM_P55T2P4:     f = nvrfopen("p55t2p4.nvr",     "rb"); nvrmask = 127; break;
                case ROM_PX386:       f = nvrfopen("px386.nvr",       "rb"); nvrmask = 127; break;
                case ROM_DTK386:      f = nvrfopen("dtk386.nvr",      "rb"); nvrmask = 127; break;
                case ROM_MR386DX_OPTI495:  f = nvrfopen("mr386dx_opti495.nvr",  "rb"); nvrmask = 127; break;
                case ROM_AMI386DX_OPTI495: f = nvrfopen("ami386dx_opti495.nvr", "rb"); nvrmask = 127; break;
                case ROM_EPSON_PCAX:       f = nvrfopen("epson_pcax.nvr",       "rb"); nvrmask = 127; break;
                case ROM_EPSON_PCAX2E:     f = nvrfopen("epson_pcax2e.nvr",     "rb"); nvrmask = 127; break;
                case ROM_PB_L300SX:        f = nvrfopen("pb_l300sx.nvr",        "rb"); nvrmask = 127; break;
                case ROM_EPSON_PCAX3:      f = nvrfopen("epson_pcax3.nvr",      "rb"); nvrmask = 127; break;
                case ROM_COMPAQ_PII:       f = nvrfopen("compaq_pii.nvr",       "rb"); break;
                case ROM_T3100E:           f = nvrfopen("t3100e.nvr",           "rb"); break;
                case ROM_T1000:            tc8521_loadnvr();
					   t1000_configsys_loadnvr();
					   t1000_emsboard_loadnvr();
					   return;
		case ROM_T1200:		   tc8521_loadnvr();
					   t1200_state_loadnvr();
					   t1000_emsboard_loadnvr();
					   return;
		case ROM_ELX_PC425X:       f = nvrfopen("elx_pc425.nvr",       "rb"); nvrmask = 127; break;
		case ROM_PB570:            f = nvrfopen("pb570.nvr",           "rb"); nvrmask = 127; break;
		case ROM_ZAPPA:            f = nvrfopen("zappa.nvr",           "rb"); nvrmask = 127; break;
		case ROM_ITAUTEC_INFOWAYM: f = nvrfopen("infowaym.nvr",        "rb"); nvrmask = 127; break;
		case ROM_PB520R:           f = nvrfopen("pb520r.nvr",          "rb"); nvrmask = 127; break;
		case ROM_XI8088:           f = nvrfopen("xi8088.nvr",          "rb"); nvrmask = 127; break;
                case ROM_IBMPS2_M70_TYPE3: f = nvrfopen("ibmps2_m70_type3.nvr","rb"); break;
                case ROM_IBMPS2_M70_TYPE4: f = nvrfopen("ibmps2_m70_type4.nvr","rb"); break;
		case ROM_TULIP_TC7:        f = nvrfopen("tulip_tc7.nvr",       "rb"); break;
		case ROM_PB410A:           f = nvrfopen("pb410a.nvr",          "rb"); nvrmask = 127; break;
		case ROM_BULL_MICRAL_45:   f = nvrfopen("bull_micral_45.nvr",  "rb"); break;
                case ROM_FIC_VA503P:       f = nvrfopen("fic_va503p.nvr",      "rb"); nvrmask = 127; break;
                case ROM_CBM_SL386SX25:    f = nvrfopen("cbm_sl386sx25.nvr",   "rb"); nvrmask = 127; break;
                case ROM_IBMPS1_2133_451:  f = nvrfopen("ibmps1_2133.nvr",     "rb"); nvrmask = 127; break;
                case ROM_ECS_386_32:       f = nvrfopen("ecs_386_32.nvr",      "rb"); nvrmask = 127; break;
                
                default: return;
        }
        if (!f)
        {
                memset(nvrram,0xFF,128);
                if (!enable_sync)
                {
                        nvrram[RTC_SECONDS] = nvrram[RTC_MINUTES] = nvrram[RTC_HOURS] = 0;
                        nvrram[RTC_DOM] = nvrram[RTC_MONTH] = 1;
                        nvrram[RTC_YEAR] = BCD(80);
                        nvrram[RTC_CENTURY] = BCD(19);
                        nvrram[RTC_REGB] = RTC_2412;
                }
                return;
        }
        fread(nvrram,128,1,f);
        if (enable_sync)
                time_internal_sync(nvrram);
        else
                time_internal_set_nvrram(nvrram); /* Update the internal clock state based on the NVR registers. */
        fclose(f);
        nvrram[RTC_REGA] = 6;
        nvrram[RTC_REGB] = RTC_2412;
}
void savenvr()
{
        FILE *f;
        switch (oldromset)
        {
                case ROM_PC1512:      f = nvrfopen("pc1512.nvr",      "wb"); break;
                case ROM_PC1640:      f = nvrfopen("pc1640.nvr",      "wb"); break;
                case ROM_PC200:       f = nvrfopen("pc200.nvr",       "wb"); break;
                case ROM_PC2086:      f = nvrfopen("pc2086.nvr",      "wb"); break;
                case ROM_PC3086:      f = nvrfopen("pc3086.nvr",      "wb"); break;
                case ROM_PPC512:      f = nvrfopen("ppc512.nvr",      "wb"); break;
                case ROM_IBMAT:       f = nvrfopen("at.nvr",          "wb"); break;
                case ROM_IBMXT286:    f = nvrfopen("ibmxt286.nvr",    "wb"); break;
                case ROM_IBMPS1_2011: f = nvrfopen("ibmps1_2011.nvr", "wb"); break;
                case ROM_IBMPS1_2121: f = nvrfopen("ibmps1_2121.nvr", "wb"); break;
                case ROM_IBMPS2_M30_286:   f = nvrfopen("ibmps2_m30_286.nvr",   "wb"); break;
                case ROM_IBMPS2_M50:  f = nvrfopen("ibmps2_m50.nvr",  "wb"); break;
                case ROM_IBMPS2_M55SX: f = nvrfopen("ibmps2_m55sx.nvr",  "wb"); break;
                case ROM_IBMPS2_M80:  f = nvrfopen("ibmps2_m80.nvr",  "wb"); break;
                case ROM_CMDPC30:     f = nvrfopen("cmdpc30.nvr",     "wb"); break;
                case ROM_AMI286:      f = nvrfopen("ami286.nvr",      "wb"); break;
                case ROM_TG286M:      f = nvrfopen("tg286m.nvr",      "wb"); break;
                case ROM_AWARD286:    f = nvrfopen("award286.nvr",    "wb"); break;
                case ROM_GDC212M:     f = nvrfopen("gdc212m.nvr",     "wb"); break;
                case ROM_HYUNDAI_SUPER286TR: f = nvrfopen("super286tr.nvr", "wb"); break;
                case ROM_GW286CT:     f = nvrfopen("gw286ct.nvr",     "wb"); break;
                case ROM_SPC4200P:    f = nvrfopen("spc4200p.nvr",    "wb"); break;
                case ROM_SPC4216P:    f = nvrfopen("spc4216p.nvr",    "wb"); break;
                case ROM_SPC4620P:    f = nvrfopen("spc4620p.nvr",    "wb"); break;
                case ROM_DELL200:     f = nvrfopen("dell200.nvr",     "wb"); break;
                case ROM_IBMAT386:    f = nvrfopen("at386.nvr",       "wb"); break;
                case ROM_DESKPRO_386: f = nvrfopen("deskpro386.nvr",  "wb"); break;
                case ROM_ACER386:     f = nvrfopen("acer386.nvr",     "wb"); break;
                case ROM_KMXC02:      f = nvrfopen("kmxc02.nvr",      "wb"); break;
                case ROM_MEGAPC:      f = nvrfopen("megapc.nvr",      "wb"); break;
                case ROM_AMA932J:     f = nvrfopen("ama932j.nvr",     "wb"); break;
                case ROM_AMI386SX:    f = nvrfopen("ami386.nvr",      "wb"); break;
                case ROM_AMI486:      f = nvrfopen("ami486.nvr",      "wb"); break;
                case ROM_WIN486:      f = nvrfopen("win486.nvr",      "wb"); break;
                case ROM_PCI486:      f = nvrfopen("hot-433.nvr",     "wb"); break;
                case ROM_SIS496:      f = nvrfopen("sis496.nvr",      "wb"); break;
                case ROM_P55VA:       f = nvrfopen("p55va.nvr",       "wb"); break;
                case ROM_P55TVP4:     f = nvrfopen("p55tvp4.nvr",     "wb"); break;
                case ROM_430VX:       f = nvrfopen("430vx.nvr",       "wb"); break;
                case ROM_REVENGE:     f = nvrfopen("revenge.nvr",     "wb"); break;
                case ROM_ENDEAVOR:    f = nvrfopen("endeavor.nvr",    "wb"); break;
                case ROM_P55T2P4:     f = nvrfopen("p55t2p4.nvr",     "wb"); break;
                case ROM_PX386:       f = nvrfopen("px386.nvr",       "wb"); break;
                case ROM_DTK386:      f = nvrfopen("dtk386.nvr",      "wb"); break;
                case ROM_MR386DX_OPTI495:  f = nvrfopen("mr386dx_opti495.nvr",  "wb"); break;
                case ROM_AMI386DX_OPTI495: f = nvrfopen("ami386dx_opti495.nvr", "wb"); break;
                case ROM_EPSON_PCAX:       f = nvrfopen("epson_pcax.nvr",       "wb"); break;
                case ROM_EPSON_PCAX2E:     f = nvrfopen("epson_pcax2e.nvr",     "wb"); break;
                case ROM_EPSON_PCAX3:      f = nvrfopen("epson_pcax3.nvr",      "wb"); break;
                case ROM_COMPAQ_PII:       f = nvrfopen("compaq_pii.nvr",       "wb"); break;
                case ROM_PB_L300SX:        f = nvrfopen("pb_l300sx.nvr",        "wb"); break;
                case ROM_T3100E:           f = nvrfopen("t3100e.nvr",           "wb"); break;
		case ROM_T1000:		   tc8521_savenvr();
					   t1000_configsys_savenvr();
					   t1000_emsboard_savenvr();
					   return;
		case ROM_T1200:		   tc8521_savenvr();
					   t1200_state_savenvr();
					   t1000_emsboard_savenvr();
					   return;
		case ROM_ELX_PC425X:       f = nvrfopen("elx_pc425.nvr",       "wb"); break;
		case ROM_PB570:            f = nvrfopen("pb570.nvr",           "wb"); break;
		case ROM_ZAPPA:            f = nvrfopen("zappa.nvr",           "wb"); break;
		case ROM_ITAUTEC_INFOWAYM:  f = nvrfopen("infowaym.nvr",        "wb"); break;
		case ROM_PB520R:           f = nvrfopen("pb520r.nvr",          "wb"); break;
		case ROM_XI8088:           f = nvrfopen("xi8088.nvr",          "wb"); break;
                case ROM_IBMPS2_M70_TYPE3: f = nvrfopen("ibmps2_m70_type3.nvr","wb"); break;
                case ROM_IBMPS2_M70_TYPE4: f = nvrfopen("ibmps2_m70_type4.nvr","wb"); break;
		case ROM_TULIP_TC7:    	   f = nvrfopen("tulip_tc7.nvr",       "wb"); break;
 		case ROM_PB410A:       	   f = nvrfopen("pb410a.nvr",          "wb"); break;
		case ROM_BULL_MICRAL_45:   f = nvrfopen("bull_micral_45.nvr",  "wb"); break;
                case ROM_FIC_VA503P:       f = nvrfopen("fic_va503p.nvr",      "wb"); break;
                case ROM_CBM_SL386SX25:    f = nvrfopen("cbm_sl386sx25.nvr",   "wb"); break;
                case ROM_IBMPS1_2133_451:  f = nvrfopen("ibmps1_2133.nvr",     "wb"); break;
                case ROM_ECS_386_32:       f = nvrfopen("ecs_386_32.nvr",      "wb"); break;
                
                default: return;
        }
        fwrite(nvrram,128,1,f);
        fclose(f);
}

static void *nvr_init()
{
        nvr_t *nvr = (nvr_t *)malloc(sizeof(nvr_t));
        memset(nvr, 0, sizeof(nvr_t));

        io_sethandler(0x0070, 0x0002, readnvr, NULL, NULL, writenvr, NULL, NULL,  nvr);
        timer_add(&nvr->rtc_timer, nvr_rtc, nvr, 1);
        timer_add(&nvr->onesec_timer, nvr_onesec, nvr, 1);
        timer_add(&nvr->update_end_timer, nvr_update_end, nvr, 0);

        return nvr;
}

static void nvr_close(void *p)
{
        nvr_t *nvr = (nvr_t *)p;
        
        free(nvr);
}

device_t nvr_device =
{
        "Motorola MC146818 RTC",
        0,
        nvr_init,
        nvr_close,
        NULL,
        nvr_speed_changed,
        NULL,
        NULL,
        NULL
};
