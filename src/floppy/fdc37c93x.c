#include "ibm.h"

#include "fdc.h"
#include "fdd.h"
#include "io.h"
#include "lpt.h"
#include "serial.h"
#include "fdc37c93x.h"

typedef struct fdc37c93x_t
{
        int index;
        
        uint8_t global_regs[0x30];
        uint8_t fdc_regs[0x100];
        uint8_t ide1_regs[0x100];
        uint8_t ide2_regs[0x100];
        uint8_t lpt_regs[0x100];
        uint8_t com1_regs[0x100];
        uint8_t com2_regs[0x100];
        uint8_t rtc_regs[0x100];
        uint8_t kbd_regs[0x100];
        uint8_t auxio_regs[0x100];
        uint8_t accessbus_regs[0x100];
        
        uint8_t lock[2];
} fdc37c93x_t;

static fdc37c93x_t fdc37c93x_global;

static void write_lock(fdc37c93x_t *fdc37c93x, uint8_t val)
{
        if (val == 0x55 && fdc37c93x->lock[1] == 0x55)
                fdc_3f1_enable(0);
        if (fdc37c93x->lock[0] == 0x55 && fdc37c93x->lock[1] == 0x55 && val != 0x55)
                fdc_3f1_enable(1);

        fdc37c93x->lock[0] = fdc37c93x->lock[1];
        fdc37c93x->lock[1] = val;
}

void fdc37c93x_write(uint16_t port, uint8_t val, void *p)
{
        fdc37c93x_t *fdc37c93x = (fdc37c93x_t *)p;

//        pclog("Write SuperIO %04x %02x\n", port, val);
        if (fdc37c93x->lock[0] == 0x55 && fdc37c93x->lock[1] == 0x55)
        {
                if (port == 0x3f0)
                {
                        if (val == 0xaa)
                                write_lock(fdc37c93x, val);
                        else
                                fdc37c93x->index = val;
                }
                else
                {
                        uint16_t addr;
                        int irq;
                        
//                        pclog("fdc37c93x_write: index=%02x dev=%i val=%02x\n", fdc37c93x->index, fdc37c93x->global_regs[7], val);
                        if (fdc37c93x->index < 0x30)
                        {
                                if (fdc37c93x->index != 0x20 && fdc37c93x->index != 0x21)
                                        fdc37c93x->global_regs[fdc37c93x->index] = val;
                        }
                        else switch (fdc37c93x->global_regs[7])
                        {
                                case 0:
                                fdc37c93x->fdc_regs[fdc37c93x->index] = val;
                                break;
                                case 1:
                                fdc37c93x->ide1_regs[fdc37c93x->index] = val;
                                break;
                                case 2:
                                fdc37c93x->ide2_regs[fdc37c93x->index] = val;
                                break;
                                case 3:
                                fdc37c93x->lpt_regs[fdc37c93x->index] = val;
                                break;
                                case 4:
                                fdc37c93x->com1_regs[fdc37c93x->index] = val;
                                break;
                                case 5:
                                fdc37c93x->com2_regs[fdc37c93x->index] = val;
                                break;
                                case 6:
                                fdc37c93x->rtc_regs[fdc37c93x->index] = val;
                                break;
                                case 7:
                                fdc37c93x->kbd_regs[fdc37c93x->index] = val;
                                break;
                                case 8:
                                fdc37c93x->auxio_regs[fdc37c93x->index] = val;
                                break;
                                case 9:
                                fdc37c93x->accessbus_regs[fdc37c93x->index] = val;
                                break;
                        }

                        fdc_remove();
                        if ((fdc37c93x->global_regs[0x22] & 0x01) && (fdc37c93x->fdc_regs[0x30] & 0x01))
                        {
//                                pclog("FDC enabled\n");
                                fdc_add();
                        }
/*                        else
                                pclog("FDC disabled\n");*/

                        lpt1_remove();
                        lpt2_remove();
                        addr = (fdc37c93x->lpt_regs[0x61] & 0xfc) | ((fdc37c93x->lpt_regs[0x60] & 3) << 8);
                        if ((fdc37c93x->global_regs[0x22] & (1 << 3)) && (fdc37c93x->lpt_regs[0x30] & 0x01))
                        {
//                                pclog("LPT addr = %04x\n", addr);
                                lpt1_init(addr);
                        }
/*                        else
                                pclog("LPT disabled\n");*/
                        
                        serial1_remove();
                        addr = (fdc37c93x->com1_regs[0x61] & 0xf8) | ((fdc37c93x->com1_regs[0x60] & 3) << 8);
                        irq = fdc37c93x->com1_regs[0x70] & 0xf;
                        if ((fdc37c93x->global_regs[0x22] & (1 << 4)) && (fdc37c93x->com1_regs[0x30] & 0x01))
                        {
//                                pclog("COM1 addr = %04x, IRQ = %i\n", addr, irq);
                                serial1_set(addr, irq);
                        }
/*                        else
                                pclog("COM1 disabled\n");*/

                        serial2_remove();
                        addr = (fdc37c93x->com2_regs[0x61] & 0xf8) | ((fdc37c93x->com2_regs[0x60] & 3) << 8);
                        irq = fdc37c93x->com2_regs[0x70] & 0xf;
                        if ((fdc37c93x->global_regs[0x22] & (1 << 5)) && (fdc37c93x->com2_regs[0x30] & 0x01))
                        {
//                                pclog("COM2 addr = %04x, IRQ = %i\n", addr, irq);
                                serial2_set(addr, irq);
                        }
/*                        else
                                pclog("COM2 disabled\n");*/


			fdc_update_enh_mode(fdc37c93x->fdc_regs[0xf0] & 1);

			fdc_update_densel_force((fdc37c93x->fdc_regs[0xf1] & 0x0c) >> 2);
			fdd_swap = (fdc37c93x->fdc_regs[0xf0] & (1 << 4)) ? 1 : 0;
                }
        }
        else
        {
                if (port == 0x3f0)
                        write_lock(fdc37c93x, val);
        }
}

uint8_t fdc37c93x_read(uint16_t port, void *p)
{
        fdc37c93x_t *fdc37c93x = (fdc37c93x_t *)p;
        
//        pclog("Read SuperIO %04x %02x\n", port, fdc37c665_curreg);
        if (fdc37c93x->lock[0] == 0x55 && fdc37c93x->lock[1] == 0x55)
        {
                if (port == 0x3f1)
                {
                        if (fdc37c93x->index < 0x30)
                                return fdc37c93x->global_regs[fdc37c93x->index];
                        else switch (fdc37c93x->global_regs[7])
                        {
                                case 0:
                                return fdc37c93x->fdc_regs[fdc37c93x->index];
                                case 1:
                                return fdc37c93x->ide1_regs[fdc37c93x->index];
                                case 2:
                                return fdc37c93x->ide2_regs[fdc37c93x->index];
                                case 3:
                                return fdc37c93x->lpt_regs[fdc37c93x->index];
                                case 4:
                                return fdc37c93x->com1_regs[fdc37c93x->index];
                                case 5:
                                return fdc37c93x->com2_regs[fdc37c93x->index];
                                case 6:
                                return fdc37c93x->rtc_regs[fdc37c93x->index];
                                case 7:
                                return fdc37c93x->kbd_regs[fdc37c93x->index];
                                case 8:
                                return fdc37c93x->auxio_regs[fdc37c93x->index];
                                case 9:
                                return fdc37c93x->accessbus_regs[fdc37c93x->index];
                        }
                }
        }
        return 0xff;
}

void fdc37c932fr_init()
{
        fdc37c93x_t *fdc37c93x = &fdc37c93x_global;
        int c;
        
        io_sethandler(0x03f0, 0x0002, fdc37c93x_read, NULL, NULL, fdc37c93x_write, NULL, NULL,  fdc37c93x);

        fdc_update_is_nsc(0);

        memset(&fdc37c93x_global, 0, sizeof(fdc37c93x_global));

        fdc37c93x->global_regs[0x03] = 0x03;
        fdc37c93x->global_regs[0x20] = 0x03;
        fdc37c93x->global_regs[0x21] = 0x01;
        fdc37c93x->global_regs[0x24] = 0x04;
        fdc37c93x->global_regs[0x26] = 0xf0;
        fdc37c93x->global_regs[0x27] = 0x03;
        
        fdc37c93x->fdc_regs[0x60] = 0x03;
        fdc37c93x->fdc_regs[0x61] = 0xf0;
        fdc37c93x->fdc_regs[0x70] = 0x06;
        fdc37c93x->fdc_regs[0x74] = 0x02;
        fdc37c93x->fdc_regs[0xf0] = 0x0e;
        fdc37c93x->fdc_regs[0xf2] = 0xff;
        
        fdc37c93x->ide1_regs[0x60] = 0x01;
        fdc37c93x->ide1_regs[0x61] = 0xf0;
        fdc37c93x->ide1_regs[0x62] = 0x03;
        fdc37c93x->ide1_regs[0x63] = 0xf6;
        fdc37c93x->ide1_regs[0x70] = 0x0e;
        fdc37c93x->ide1_regs[0xf0] = 0x0c;
        fdc37c93x->ide1_regs[0xf1] = 0x0d;

        fdc37c93x->lpt_regs[0x74] = 0x04;
        fdc37c93x->lpt_regs[0xf0] = 0x3c;

        fdc37c93x->com2_regs[0x74] = 0x04;
        fdc37c93x->com2_regs[0xf1] = 0x02;
        fdc37c93x->com2_regs[0xf2] = 0x03;

        fdc37c93x->rtc_regs[0x63] = 0x70;
        fdc37c93x->rtc_regs[0xf4] = 0x03;

        fdc37c93x->auxio_regs[0xb1] = 0x80;
        fdc37c93x->auxio_regs[0xc0] = 0x01;
        fdc37c93x->auxio_regs[0xc1] = 0x01;
        fdc37c93x->auxio_regs[0xc5] = 0x01;
        fdc37c93x->auxio_regs[0xc6] = 0x01;
        fdc37c93x->auxio_regs[0xc7] = 0x01;
        fdc37c93x->auxio_regs[0xc8] = 0x01;
        fdc37c93x->auxio_regs[0xc9] = 0x80;
        for (c = 0xcb; c < 0xee; c++)
                fdc37c93x->auxio_regs[c] = 0x01;
        
	fdc_update_densel_polarity(1);
	fdc_update_densel_force(0);
	fdd_swap = 0;
}
