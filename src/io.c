#include "ibm.h"
#include "ide.h"
#include "video.h"
#include "cpu.h"

uint8_t  (*port_inb[0x10000][2])(uint16_t addr);
uint16_t (*port_inw[0x10000][2])(uint16_t addr);
uint32_t (*port_inl[0x10000][2])(uint16_t addr);

void (*port_outb[0x10000][2])(uint16_t addr, uint8_t  val);
void (*port_outw[0x10000][2])(uint16_t addr, uint16_t val);
void (*port_outl[0x10000][2])(uint16_t addr, uint32_t val);

void io_init()
{
        int c;
        for (c = 0; c < 0x10000; c++)
        {
                port_inb[c][0]  = port_inw[c][0]  = port_inl[c][0]  = NULL;
                port_outb[c][0] = port_outw[c][0] = port_outl[c][0] = NULL;
                port_inb[c][1]  = port_inw[c][1]  = port_inl[c][1]  = NULL;
                port_outb[c][1] = port_outw[c][1] = port_outl[c][1] = NULL;
        }
}

void io_sethandler(uint16_t base, int size, 
                   uint8_t  (*inb)(uint16_t addr), 
                   uint16_t (*inw)(uint16_t addr), 
                   uint32_t (*inl)(uint16_t addr), 
                   void (*outb)(uint16_t addr, uint8_t  val),
                   void (*outw)(uint16_t addr, uint16_t val),
                   void (*outl)(uint16_t addr, uint32_t val))
{
        int c;
        for (c = 0; c < size; c++)
        {
                if      (!port_inb[ base + c][0]) port_inb[ base + c][0] = inb;
                else if (!port_inb[ base + c][1]) port_inb[ base + c][1] = inb;                
                if      (!port_inw[ base + c][0]) port_inw[ base + c][0] = inw;
                else if (!port_inw[ base + c][1]) port_inw[ base + c][1] = inw;
                if      (!port_inl[ base + c][0]) port_inl[ base + c][0] = inl;
                else if (!port_inl[ base + c][1]) port_inl[ base + c][1] = inl;
                if      (!port_outb[base + c][0]) port_outb[base + c][0] = outb;
                else if (!port_outb[base + c][1]) port_outb[base + c][1] = outb;                
                if      (!port_outw[base + c][0]) port_outw[base + c][0] = outw;
                else if (!port_outw[base + c][1]) port_outw[base + c][1] = outw;
                if      (!port_outl[base + c][0]) port_outl[base + c][0] = outl;
                else if (!port_outl[base + c][1]) port_outl[base + c][1] = outl;
        }
}

void io_removehandler(uint16_t base, int size, 
                   uint8_t  (*inb)(uint16_t addr), 
                   uint16_t (*inw)(uint16_t addr), 
                   uint32_t (*inl)(uint16_t addr), 
                   void (*outb)(uint16_t addr, uint8_t  val),
                   void (*outw)(uint16_t addr, uint16_t val),
                   void (*outl)(uint16_t addr, uint32_t val))
{
        int c;
        for (c = 0; c < size; c++)
        {
                if (port_inb[ base + c][0] == inb)
                   port_inb[ base + c][0] = NULL;
                if (port_inb[ base + c][1] == inb)
                   port_inb[ base + c][1] = NULL;
                if (port_inw[ base + c][0] == inw)
                   port_inw[ base + c][0] = NULL;
                if (port_inw[ base + c][1] == inw)
                   port_inw[ base + c][1] = NULL;
                if (port_inl[ base + c][0] == inl)
                   port_inl[ base + c][0] = NULL;
                if (port_inl[ base + c][1] == inl)
                   port_inl[ base + c][1] = NULL;
                if (port_outb[ base + c][0] == outb)
                   port_outb[ base + c][0] = NULL;
                if (port_outb[ base + c][1] == outb)
                   port_outb[ base + c][1] = NULL;
                if (port_outw[ base + c][0] == outw)
                   port_outw[ base + c][0] = NULL;
                if (port_outw[ base + c][1] == outw)
                   port_outw[ base + c][1] = NULL;
                if (port_outl[ base + c][0] == outl)
                   port_outl[ base + c][0] = NULL;
                if (port_outl[ base + c][1] == outl)
                   port_outl[ base + c][1] = NULL;
        }
}

uint8_t cgamode,cgastat=0,cgacol;
int hsync;
uint8_t lpt2dat;
int sw9;
int t237=0;
uint8_t inb(uint16_t port)
{
        uint8_t temp = 0xff;
        int tempi;
        if (port_inb[port][0])
           temp &= port_inb[port][0](port);
        if (port_inb[port][1])
           temp &= port_inb[port][1](port);
        return temp;
        
        if (port&0x80) sw9=2;
//        if ((port&0x3F0)==0x3D0) printf("Video access read %03X %04X:%04X\n",port,cs>>4,pc);
//        if (cs<0xF0000 || cs>0x100000) printf("IN %04X %04X(%06X):%08X\n",port,CS,cs,pc);
//        /*if (output==3) */printf("IN %04X %04X:%04X\n",port,CS,pc);
//        if (port == 0x23) pclog("IN %04X %04X:%08X\n", port, CS, pc);
        switch (port)
        {
                case 0x220: case 0x221: case 0x222: case 0x223: /*Gameblaster*/
                if (sbtype>=SBPRO) return readsb(port);
                if (GAMEBLASTER) return readcms(port);
                return 0xFF;
        }
//        printf("Bad IN port %04X %04X:%04X\n",port,cs>>4,pc);
        return 0xff;
        /*dumpregs();
        exit(-1);*/
}

/*uint8_t inb(uint16_t port)
{
        uint8_t temp = _inb(port);
//        if (port != 0x61) pclog("IN %04X %02X %04X(%08X):%08X %f   %04X %i %i\n", port, temp, CS, cs, pc, pit.c[1], CX, keybsenddelay, GetTickCount());
        return temp;
}*/

uint8_t cpu_readport(uint32_t port) { return inb(port); }

void outb(uint16_t port, uint8_t val)
{
//        /*if (output==3) */printf("OUT %04X %02X %04X(%08X):%08X %i %i\n", port, val, CS, cs, pc, ins, GetTickCount());
        if (port_outb[port][0])
           port_outb[port][0](port, val);
        if (port_outb[port][1])
           port_outb[port][1](port, val);
        return;
        switch (port)
        {
                case 0x220: case 0x221: case 0x222: case 0x223: /*Gameblaster*/
                if (GAMEBLASTER) writecms(port,val);
                return;
        }
        pclog("OUT %04X %02X %04X:%08X\n",port,val,CS,pc);
}

uint16_t inw(uint16_t port)
{
        if (port_inw[port][0])
           return port_inw[port][0](port);
        if (port_inw[port][1])
           return port_inw[port][1](port);
           
        return inb(port) | (inb(port + 1) << 8);
}

void outw(uint16_t port, uint16_t val)
{
//        printf("OUTW %04X %04X %04X:%08X\n",port,val, CS, pc);
        if (port_outw[port][0])
           port_outw[port][0](port, val);
        if (port_outw[port][1])
           port_outw[port][1](port, val);

        if (port_outw[port][0] || port_outw[port][1])
           return;

        outb(port,val);
        outb(port+1,val>>8);
}

uint32_t inl(uint16_t port)
{
        if (port_inl[port][0])
           return port_inl[port][0](port);
        if (port_inl[port][1])
           return port_inl[port][1](port);
           
        return inw(port) | (inw(port + 2) << 16);
}

void outl(uint16_t port, uint32_t val)
{
        if (port_outl[port][0])
           port_outl[port][0](port, val);
        if (port_outl[port][1])
           port_outl[port][1](port, val);

        if (port_outl[port][0] || port_outl[port][1])
           return;
                
        outw(port, val);
        outw(port + 2, val >> 16);
}
