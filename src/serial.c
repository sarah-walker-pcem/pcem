#include "ibm.h"
#include "io.h"
#include "mouse.h"
#include "pic.h"
#include "serial.h"
#include "timer.h"

SERIAL serial,serial2;

int mousepos=-1;
int mousedelay;
uint8_t serial_fifo[256];
int serial_fifo_read, serial_fifo_write;

void (*serial_rcr)();

void serial_reset()
{
        serial.iir=serial.ier=serial.lcr=0;
        serial2.iir=serial2.ier=serial2.lcr=0;
        mousedelay=0;
        serial_fifo_read = serial_fifo_write = 0;
}

void serial_write_fifo(uint8_t dat)
{
        serial_fifo[serial_fifo_write] = dat;
        serial_fifo_write = (serial_fifo_write + 1) & 0xFF;
        if (!(serial.linestat & 1))
        {
                serial.linestat|=1;
                if ((serial.mctrl & 8) && (serial.ier & 1))
                        picint(0x10);
                serial.iir=4;
        }
}

uint8_t serial_read_fifo()
{
        if (serial_fifo_read != serial_fifo_write)
        {
                serial.dat = serial_fifo[serial_fifo_read];
                serial_fifo_read = (serial_fifo_read + 1) & 0xFF;
        }
        return serial.dat;
}

void sendserial(uint8_t dat)
{
        serial.rcr=dat;
        serial.linestat|=1;
        if (serial.mctrl&8) picint(0x10);
        serial.iir=4;
}

void serial_write(uint16_t addr, uint8_t val, void *priv)
{
//        pclog("Write serial %03X %02X %04X:%04X\n",addr,val,CS,pc);
        switch (addr&7)
        {
                case 0:
                if (serial.lcr&0x80 && !AMSTRADIO)
                {
                        serial.dlab1=val;
                        return;
                }
                serial.thr=val;
                serial.linestat|=0x20;
                if (serial.mctrl&0x10)
                {
                        serial_write_fifo(val);
//                        serial.rcr=val;
//                        serial.linestat|=1;
                }
                break;
                case 1:
                if (serial.lcr&0x80 && !AMSTRADIO)
                {
                        serial.dlab2=val;
                        return;
                }
                serial.ier=val;
                break;
                case 3: serial.lcr=val; break;
                case 4:
                if ((val&2) && !(serial.mctrl&2))
                {
                        if (serial_rcr)
                           serial_rcr();
//                        pclog("RCR raised! sending M\n");
                }
                serial.mctrl=val;
                break;
        }
}

uint8_t serial_read(uint16_t addr, void *priv)
{
        uint8_t temp;
//        pclog("Read serial %03X %04X(%08X):%04X %i %i  ", addr, CS, cs, pc, mousedelay, ins);
        switch (addr&7)
        {
                case 0:
                if (serial.lcr&0x80 && !AMSTRADIO) return serial.dlab1;
//                picintc(0x10);
                serial.iir=1;
                serial.linestat&=~1;
                temp=serial_read_fifo();
                if (serial_fifo_read != serial_fifo_write)
                {
                        mousepos = 0;
                        mousedelay = 5000 * (1 << TIMER_SHIFT);
//                        pclog("Next FIFO\n");
                }
                break;
                case 1:
                if (serial.lcr&0x80 && !AMSTRADIO) temp = serial.dlab2;
                else                               temp = serial.ier;
                break;
                case 2: temp=serial.iir; break;
                case 3: temp=serial.lcr; break;
                case 4: temp=serial.mctrl; break;
                case 5: temp=serial.linestat; serial.linestat|=0x60; break;
                default: temp=0;
        }
//        pclog("%02X\n",temp);
        return temp;
}

void serial2_write(uint16_t addr, uint8_t val, void *priv)
{
//        pclog("Write serial2 %03X %02X %04X:%04X\n",addr,val,cs>>4,pc);
        switch (addr&7)
        {
                case 0:
                if (serial2.lcr&0x80 && !AMSTRADIO)
                {
                        serial2.dlab1=val;
                        return;
                }
                serial2.thr=val;
                serial2.linestat|=0x20;
                if (serial2.mctrl&0x10)
                {
                        serial2.rcr=val;
                        serial2.linestat|=1;
                }
                break;
                case 1:
                if (serial2.lcr&0x80 && !AMSTRADIO)
                {
                        serial2.dlab2=val;
                        return;
                }
                serial2.ier=val;
                break;
                case 3: serial2.lcr=val; break;
                case 4:
                serial2.mctrl=val;
                break;
        }
}

uint8_t serial2_read(uint16_t addr, void *priv)
{
        uint8_t temp;
//        pclog("Read serial2 %03X %04X:%04X\n",addr,cs>>4,pc);
        switch (addr&7)
        {
                case 0:
                if (serial2.lcr&0x80 && !AMSTRADIO) return serial2.dlab1;
                serial2.iir=1;
                serial2.linestat&=~1;
                temp=serial2.rcr;
                break;
                case 1:
                if (serial2.lcr&0x80 && !AMSTRADIO) return serial2.dlab2;
                temp=0;
                break;
                case 2: temp=serial2.iir; break;
                case 3: temp=serial2.lcr; break;
                case 4: temp=serial2.mctrl; break;
                case 5: temp=serial2.linestat; break;
                default: temp=0;
        }
//        pclog("%02X\n",temp);
        return temp;
}


/*Tandy might need COM1 at 2f8*/
void serial1_init(uint16_t addr)
{
        io_sethandler(addr, 0x0008, serial_read,  NULL, NULL, serial_write,  NULL, NULL, NULL);
        serial_rcr = NULL;
}
void serial1_remove()
{
        io_removehandler(0x2e8, 0x0008, serial_read,  NULL, NULL, serial_write,  NULL, NULL, NULL);
        io_removehandler(0x2f8, 0x0008, serial_read,  NULL, NULL, serial_write,  NULL, NULL, NULL);
        io_removehandler(0x3e8, 0x0008, serial_read,  NULL, NULL, serial_write,  NULL, NULL, NULL);
        io_removehandler(0x3f8, 0x0008, serial_read,  NULL, NULL, serial_write,  NULL, NULL, NULL);
}

void serial2_init(uint16_t addr)
{
        io_sethandler(addr, 0x0008, serial2_read, NULL, NULL, serial2_write, NULL, NULL, NULL);
}
void serial2_remove()
{
        io_removehandler(0x2e8, 0x0008, serial2_read, NULL, NULL, serial2_write, NULL, NULL, NULL);
        io_removehandler(0x2f8, 0x0008, serial2_read, NULL, NULL, serial2_write, NULL, NULL, NULL);
        io_removehandler(0x3e8, 0x0008, serial2_read, NULL, NULL, serial2_write, NULL, NULL, NULL);
        io_removehandler(0x3f8, 0x0008, serial2_read, NULL, NULL, serial2_write, NULL, NULL, NULL);
}
