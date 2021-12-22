#include "ibm.h"
#include "82091aa.h"
#include "fdc.h"
#include "io.h"
#include "lpt.h"
#include "serial.h"

#define AIP_AIPID   0x00
#define AIP_AIPREV  0x01
#define AIP_AIPCFG1 0x02
#define AIP_AIPCFG2 0x03
#define AIP_FCFG1   0x10
#define AIP_FCFG2   0x11
#define AIP_PCFG1   0x20
#define AIP_PCFG2   0x21
#define AIP_SACFG1  0x30
#define AIP_SACFG2  0x31
#define AIP_SBCFG1  0x40
#define AIP_SBCFG2  0x41
#define AIP_IDECFG  0x50

#define FCFG1_FDC_ENA  (1 << 0)
#define FCFG1_FDC_ADDR (1 << 1)

#define PCFG1_PP_ENA   (1 << 0)
#define PCFG1_PP_ADDR  (3 << 1)
#define PCFG1_PP_ADDR_378 (0 << 1)
#define PCFG1_PP_ADDR_278 (1 << 1)
#define PCFG1_PP_ADDR_3BC (2 << 1)

#define SxCFG1_ENA     (1 << 0)
#define SxCFG1_ADDR    (7 << 1)
#define SxCFG1_ADDR_3F8 (0 << 1)
#define SxCFG1_ADDR_2F8 (1 << 1)
#define SxCFG1_ADDR_220 (2 << 1)
#define SxCFG1_ADDR_228 (3 << 1)
#define SxCFG1_ADDR_238 (4 << 1)
#define SxCFG1_ADDR_2E8 (5 << 1)
#define SxCFG1_ADDR_338 (6 << 1)
#define SxCFG1_ADDR_3E8 (7 << 1)
#define SxCFG1_IRQ     (1 << 4)
#define SxCFG1_IRQ_3    (0 << 4)
#define SxCFG1_IRQ_4    (1 << 4)

static struct
{
        int idx;
        uint8_t regs[256];
} aip;

static uint8_t aip_read(uint16_t addr, void *p)
{
        uint8_t temp;

        if (!(addr & 1))
                temp = aip.idx;
        else
                temp = aip.regs[aip.idx];

//        pclog("aip_read: addr=%04x val=%02x\n", addr, temp);
        return temp;
}

static void aip_write(uint16_t addr, uint8_t val, void *p)
{
//        pclog("aip_write: addr=%04x val=%02x\n", addr, val);
        if (!(addr & 1))
                aip.idx = val;
        else
        {
//                pclog("AIP register write: idx=%02x val=%02x\n", aip.idx, val);
                switch (aip.idx)
                {
                        case AIP_AIPCFG1:
                        aip.regs[AIP_AIPCFG1] = (val & ~0x4e) | (aip.regs[AIP_AIPCFG1] & 0x48);
                        break;
                        case AIP_AIPCFG2:
                        aip.regs[AIP_AIPCFG2] = val & 0xf8;
                        break;
                        case AIP_FCFG1:
                        aip.regs[AIP_FCFG1] = val & 0x83;
                        fdc_remove();
                        if ((aip.regs[AIP_FCFG1] & FCFG1_FDC_ENA) && !(aip.regs[AIP_FCFG1] & FCFG1_FDC_ADDR))
                                fdc_add();
                        break;
                        case AIP_FCFG2:
                        aip.regs[AIP_FCFG2] = (val & 0x0d) | (aip.regs[AIP_FCFG2] & 0x02);
                        break;
                        case AIP_PCFG1:
                        aip.regs[AIP_PCFG1] = val & 0xef;
                        lpt1_remove();
                        if (aip.regs[AIP_PCFG1] & PCFG1_PP_ENA)
                        {
                                switch (aip.regs[AIP_PCFG1] & PCFG1_PP_ADDR)
                                {
                                        case PCFG1_PP_ADDR_378:
                                        lpt1_init(0x378);
                                        break;
                                        case PCFG1_PP_ADDR_278:
                                        lpt1_init(0x278);
                                        break;
                                        case PCFG1_PP_ADDR_3BC:
                                        lpt1_init(0x3bc);
                                        break;
                                }
                        }
                        break;
                        case AIP_PCFG2:
                        aip.regs[AIP_PCFG2] = (val & 0x0d) | (aip.regs[AIP_PCFG2] & 0x22);
                        break;
                        case AIP_SACFG1:
                        aip.regs[AIP_SACFG1] = val & 0x9f;
                        serial1_remove();
                        if (aip.regs[AIP_SACFG1] & SxCFG1_ENA)
                        {
                                uint16_t base = 0;
                                int irq = 0;
                                
                                switch (aip.regs[AIP_SACFG1] & SxCFG1_ADDR)
                                {
                                        case SxCFG1_ADDR_3F8: base = 0x3f8; break;
                                        case SxCFG1_ADDR_2F8: base = 0x2f8; break;
                                        case SxCFG1_ADDR_220: base = 0x220; break;
                                        case SxCFG1_ADDR_228: base = 0x228; break;
                                        case SxCFG1_ADDR_238: base = 0x238; break;
                                        case SxCFG1_ADDR_2E8: base = 0x2e8; break;
                                        case SxCFG1_ADDR_338: base = 0x338; break;
                                        case SxCFG1_ADDR_3E8: base = 0x3e8; break;
                                }
                                switch (aip.regs[AIP_SACFG1] & SxCFG1_IRQ)
                                {
                                        case SxCFG1_IRQ_3: irq = 3; break;
                                        case SxCFG1_IRQ_4: irq = 4; break;
                                }
                                serial1_set(base, irq);
                        }
                        break;
                        case AIP_SACFG2:
                        aip.regs[AIP_SACFG2] = (val & 0x01d) | (aip.regs[AIP_SACFG2] & 0x02);
                        break;
                        case AIP_SBCFG1:
                        aip.regs[AIP_SBCFG1] = val & 0x9f;
                        serial2_remove();
                        if (aip.regs[AIP_SBCFG1] & SxCFG1_ENA)
                        {
                                uint16_t base = 0;
                                int irq = 0;

                                switch (aip.regs[AIP_SBCFG1] & SxCFG1_ADDR)
                                {
                                        case SxCFG1_ADDR_3F8: base = 0x3f8; break;
                                        case SxCFG1_ADDR_2F8: base = 0x2f8; break;
                                        case SxCFG1_ADDR_220: base = 0x220; break;
                                        case SxCFG1_ADDR_228: base = 0x228; break;
                                        case SxCFG1_ADDR_238: base = 0x238; break;
                                        case SxCFG1_ADDR_2E8: base = 0x2e8; break;
                                        case SxCFG1_ADDR_338: base = 0x338; break;
                                        case SxCFG1_ADDR_3E8: base = 0x3e8; break;
                                }
                                switch (aip.regs[AIP_SBCFG1] & SxCFG1_IRQ)
                                {
                                        case SxCFG1_IRQ_3: irq = 3; break;
                                        case SxCFG1_IRQ_4: irq = 4; break;
                                }
                                serial2_set(base, irq);
                        }
                        break;
                        case AIP_SBCFG2:
                        aip.regs[AIP_SACFG2] = (val & 0x01d) | (aip.regs[AIP_SACFG2] & 0x02);
                        break;
                        case AIP_IDECFG:
                        aip.regs[AIP_IDECFG] = val & 0x07;
                        break;
                }
        }
}

void aip_82091aa_init(uint16_t base)
{
        io_sethandler(base, 0x0002, aip_read, NULL, NULL, aip_write, NULL, NULL,  NULL);
        
        memset(&aip.regs, 0xff, sizeof(aip.regs));
        aip.regs[AIP_AIPID]   = 0xa0;
        aip.regs[AIP_AIPREV]  = 0x00;
        aip.regs[AIP_AIPCFG1] = 0x40;
        aip.regs[AIP_AIPCFG2] = 0x00;
        aip.regs[AIP_FCFG1]   = 0x01;
        aip.regs[AIP_FCFG2]   = 0x00;
        aip.regs[AIP_PCFG1]   = 0x00;
        aip.regs[AIP_PCFG2]   = 0x00;
        aip.regs[AIP_SACFG1]  = 0x00;
        aip.regs[AIP_SACFG2]  = 0x02;
        aip.regs[AIP_SBCFG1]  = 0x00;
        aip.regs[AIP_SBCFG2]  = 0x02;
        aip.regs[AIP_IDECFG]  = 0x01;
}
