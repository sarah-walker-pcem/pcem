#include "ibm.h"

#include "dma.h"
#include "fdc.h"
#include "io.h"
#include "mem.h"
#include "ps2_mca.h"
#include "video.h"
#include "x86.h"

static uint8_t dmaregs[16];
static uint8_t dma16regs[16];
static uint8_t dmapages[16];

static int dma_wp, dma16_wp;
static uint8_t dma_m;
static uint8_t dma_stat;
static uint8_t dma_stat_rq;
static uint8_t dma_command, dma16_command;

static struct
{        
        int xfr_command, xfr_channel;
        int byte_ptr;
        
        int is_ps2;
} dma_ps2;

#define DMA_PS2_IOA            (1 << 0)
#define DMA_PS2_XFER_MEM_TO_IO (1 << 2)
#define DMA_PS2_XFER_IO_TO_MEM (3 << 2)
#define DMA_PS2_XFER_MASK      (3 << 2)
#define DMA_PS2_DEC2           (1 << 4)
#define DMA_PS2_SIZE16         (1 << 6)

static void dma_ps2_run(int channel);

void dma_reset()
{
        int c;

        dma_wp = dma16_wp = 0;
        dma_m = 0;
        
        for (c = 0; c < 16; c++) 
                dmaregs[c] = 0;
        for (c = 0; c < 8; c++)
        {
                dma[c].mode = 0;
                dma[c].ac = 0;
                dma[c].cc = 0;
                dma[c].ab = 0;
                dma[c].cb = 0;
                dma[c].size = (c & 4) ? 1 : 0;
        }
}

uint8_t dma_read(uint16_t addr, void *priv)
{
        int channel = (addr >> 1) & 3;
        uint8_t temp;
//        printf("Read DMA %04X %04X:%04X %i %02X\n",addr,CS,pc, pic_intpending, pic.pend);
        switch (addr & 0xf)
        {
                case 0: case 2: case 4: case 6: /*Address registers*/
                dma_wp ^= 1;
                if (dma_wp) 
                        return dma[channel].ac & 0xff;
                return (dma[channel].ac >> 8) & 0xff;
                
                case 1: case 3: case 5: case 7: /*Count registers*/
                dma_wp ^= 1;
                if (dma_wp)
                        temp = dma[channel].cc & 0xff;
                else
                        temp = dma[channel].cc >> 8;
                return temp;
                
                case 8: /*Status register*/
                temp = dma_stat & 0xf;
                dma_stat &= ~0xf;
                return temp;
                
                case 0xd:
                return 0;
        }
//        printf("Bad DMA read %04X %04X:%04X\n",addr,CS,pc);
        return dmaregs[addr & 0xf];
}

void dma_write(uint16_t addr, uint8_t val, void *priv)
{
        int channel = (addr >> 1) & 3;
//        printf("Write DMA %04X %02X %04X:%04X\n",addr,val,CS,pc);
        dmaregs[addr & 0xf] = val;
        switch (addr & 0xf)
        {
                case 0: case 2: case 4: case 6: /*Address registers*/
                dma_wp ^= 1;
                if (dma_wp)
                        dma[channel].ab = (dma[channel].ab & 0xffff00) | val;
                else
                        dma[channel].ab = (dma[channel].ab & 0xff00ff) | (val << 8);
                dma[channel].ac = dma[channel].ab;
//                pclog("Addr = %04x\n", dma.ab[(addr >> 1) & 3]);
                return;
                
                case 1: case 3: case 5: case 7: /*Count registers*/
                dma_wp ^= 1;
                if (dma_wp)
                        dma[channel].cb = (dma[channel].cb & 0xff00) | val;
                else
                        dma[channel].cb = (dma[channel].cb & 0x00ff) | (val << 8);
                dma[channel].cc = dma[channel].cb;
                return;
                
                case 8: /*Control register*/
                dma_command = val;
                return;
                
                case 0xa: /*Mask*/
                if (val & 4)
                        dma_m |=  (1 << (val & 3));
                else
                        dma_m &= ~(1 << (val & 3));
                return;
                
                case 0xb: /*Mode*/
                channel = (val & 3);
                dma[channel].mode = val;
                if (dma_ps2.is_ps2)
                {
                        dma[channel].ps2_mode &= ~0x1c;
                        if (val & 0x20)
                                dma[channel].ps2_mode |= 0x10;
                        if ((val & 0xc) == 8)
                                dma[channel].ps2_mode |= 4;
                        else if ((val & 0xc) == 4)
                                dma[channel].ps2_mode |= 0xc;
                }
                return;
                
                case 0xc: /*Clear FF*/
                dma_wp = 0;
                return;
                
                case 0xd: /*Master clear*/
                dma_wp = 0;
                dma_m |= 0xf;
                return;
                
                case 0xf: /*Mask write*/
                dma_m = (dma_m & 0xf0) | (val & 0xf);
                return;
        }
}

static uint8_t dma_ps2_read(uint16_t addr, void *priv)
{
        dma_t *dma_c = &dma[dma_ps2.xfr_channel];
        uint8_t temp = 0xff;
        
        switch (addr)
        {
                case 0x1a:
                switch (dma_ps2.xfr_command)
                {
                        case 2: /*Address*/
                        case 3:
                        switch (dma_ps2.byte_ptr)
                        {
                                case 0:
                                temp = dma_c->ac & 0xff;
                                dma_ps2.byte_ptr = 1;
                                break;
                                case 1:
                                temp = (dma_c->ac >> 8) & 0xff;
                                dma_ps2.byte_ptr = 2;
                                break;
                                case 2:
                                temp = (dma_c->ac >> 16) & 0xff;
                                dma_ps2.byte_ptr = 0;
                                break;
                        }
                        break;
                        case 4: /*Count*/
                        case 5:
                        if (dma_ps2.byte_ptr)
                                temp = dma_c->cc >> 8;
                        else
                                temp = dma_c->cc & 0xff;
                        dma_ps2.byte_ptr = (dma_ps2.byte_ptr + 1) & 1;
                        break;
                        case 6: /*Read DMA status*/
                        if (dma_ps2.byte_ptr)
                        {
                                temp = ((dma_stat_rq & 0xf0) >> 4) | (dma_stat & 0xf0);
                                dma_stat &= ~0xf0;
                                dma_stat_rq &= ~0xf0;
                        }
                        else
                        {
                                temp = (dma_stat_rq & 0xf) | ((dma_stat & 0xf) << 4);
                                dma_stat &= ~0xf;
                                dma_stat_rq &= ~0xf;
                        }
                        dma_ps2.byte_ptr = (dma_ps2.byte_ptr + 1) & 1;
                        break;                        
                        case 7: /*Mode*/
                        temp = dma_c->ps2_mode;
                        break;
                        case 8: /*Arbitration Level*/
                        temp = dma_c->arb_level;
                        break;
                        
                        default:
                        fatal("Bad XFR Read command %i channel %i\n", dma_ps2.xfr_command, dma_ps2.xfr_channel);
                }
                break;
        }

//        pclog("Read DMA %04X %04X:%04X %02X\n",addr,CS,pc, temp);
        
        return temp;
}

static void dma_ps2_write(uint16_t addr, uint8_t val, void *priv)
{
        dma_t *dma_c = &dma[dma_ps2.xfr_channel];
        uint8_t mode;
        
//        pclog("Write PS2 DMA %04X %02X %04X:%04X\n",addr,val,CS,cpu_state.pc);
        
        switch (addr)
        {
                case 0x18:
                dma_ps2.xfr_channel = val & 0x7;
                dma_ps2.xfr_command = val >> 4;
                dma_ps2.byte_ptr = 0;
                switch (dma_ps2.xfr_command)
                {
                        case 9: /*Set DMA mask*/
                        dma_m |= (1 << dma_ps2.xfr_channel);
                        break;
                        case 0xa: /*Reset DMA mask*/
                        dma_m &= ~(1 << dma_ps2.xfr_channel);
                        break;
                        case 0xb:
                        if (!(dma_m & (1 << dma_ps2.xfr_channel)))
                                dma_ps2_run(dma_ps2.xfr_channel);
                        break;
                        
//                        case 0xb: output = 3; break;
                }                        
                break;
                case 0x1a:
                switch (dma_ps2.xfr_command)
                {
                        case 0: /*I/O address*/
                        if (dma_ps2.byte_ptr)
                                dma_c->io_addr = (dma_c->io_addr & 0x00ff) | (val << 8);
                        else
                                dma_c->io_addr = (dma_c->io_addr & 0xff00) | val;
                        dma_ps2.byte_ptr = (dma_ps2.byte_ptr + 1) & 1;
                        break;

                        case 2: /*Address*/
                        switch (dma_ps2.byte_ptr)
                        {
                                case 0:
                                dma_c->ac = (dma_c->ac & 0xffff00) | val;
                                dma_ps2.byte_ptr = 1;
                                break;
                                case 1:
                                dma_c->ac = (dma_c->ac & 0xff00ff) | (val << 8);
                                dma_ps2.byte_ptr = 2;
                                break;
                                case 2:
                                dma_c->ac = (dma_c->ac & 0x00ffff) | (val << 16);
                                dma_ps2.byte_ptr = 0;
                                break;
                        }
                        dma_c->ab = dma_c->ac;
                        break;

                        case 4: /*Count*/
                        if (dma_ps2.byte_ptr)
                                dma_c->cc = (dma_c->cc & 0xff) | (val << 8);
                        else
                                dma_c->cc = (dma_c->cc & 0xff00) | val;
                        dma_ps2.byte_ptr = (dma_ps2.byte_ptr + 1) & 1;
                        dma_c->cb = dma_c->cc;
                        break;

                        case 7: /*Mode register*/
                        mode = 0;
                        if (val & DMA_PS2_DEC2)
                                mode |= 0x20;
                        if ((val & DMA_PS2_XFER_MASK) == DMA_PS2_XFER_MEM_TO_IO)
                                mode |= 8;
                        else if ((val & DMA_PS2_XFER_MASK) == DMA_PS2_XFER_IO_TO_MEM)
                                mode |= 4;
                        dma_c->mode = (dma_c->mode & ~0x2c) | mode;
                        dma_c->ps2_mode = val;
                        dma_c->size = val & DMA_PS2_SIZE16;
                        break;

                        case 8: /*Arbitration Level*/
                        dma_c->arb_level = val;
                        break;

                        default:
                        fatal("Bad XFR command %i channel %i val %02x\n", dma_ps2.xfr_command, dma_ps2.xfr_channel, val);
                }
                break;
        }
}

uint8_t dma16_read(uint16_t addr, void *priv)
{
        int channel = ((addr >> 2) & 3) + 4;
        uint8_t temp;
//        printf("Read DMA %04X %04X:%04X\n",addr,cs>>4,pc);
        addr >>= 1;
        switch (addr & 0xf)
        {
                case 0: case 2: case 4: case 6: /*Address registers*/
                dma16_wp ^= 1;
                if (dma_ps2.is_ps2)
                {
                        if (dma16_wp) 
                                return dma[channel].ac;
                        return (dma[channel].ac >> 8) & 0xff;
                }
                if (dma16_wp) 
                        return (dma[channel].ac >> 1) & 0xff;
                return (dma[channel].ac >> 9) & 0xff;
                
                case 1: case 3: case 5: case 7: /*Count registers*/
                dma16_wp ^= 1;
                if (dma16_wp)
                        temp = dma[channel].cc & 0xff;
                else
                        temp = dma[channel].cc >> 8;
                return temp;
                
                case 8: /*Status register*/
                temp = dma_stat >> 4;
                dma_stat &= ~0xf0;
                return temp;
        }
        return dma16regs[addr & 0xf];
}

void dma16_write(uint16_t addr, uint8_t val, void *priv)
{
        int channel = ((addr >> 2) & 3) + 4;
//        printf("Write dma16 %04X %02X %04X:%04X\n",addr,val,CS,pc);
        addr >>= 1;
        dma16regs[addr & 0xf] = val;
        switch (addr & 0xf)
        {
                case 0: case 2: case 4: case 6: /*Address registers*/
                dma16_wp ^= 1;
                if (dma_ps2.is_ps2)
                {
                        if (dma16_wp)
                                dma[channel].ab = (dma[channel].ab & 0xffff00) | val;
                        else
                                dma[channel].ab = (dma[channel].ab & 0xff00ff) | (val << 8);
                }
                else
                {
                        if (dma16_wp)
                                dma[channel].ab = (dma[channel].ab & 0xfffe00) | (val << 1);
                        else
                                dma[channel].ab = (dma[channel].ab & 0xfe01ff) | (val << 9);
                }
                dma[channel].ac = dma[channel].ab;
                return;
                
                case 1: case 3: case 5: case 7: /*Count registers*/
                dma16_wp ^= 1;
                if (dma16_wp)
                        dma[channel].cb = (dma[channel].cb & 0xff00) | val;
                else
                        dma[channel].cb = (dma[channel].cb & 0x00ff) | (val << 8);
                dma[channel].cc = dma[channel].cb;
                return;
                
                case 8: /*Control register*/
                return;
                
                case 0xa: /*Mask*/
                if (val & 4)
                        dma_m |=  (0x10 << (val & 3));
                else
                        dma_m &= ~(0x10 << (val & 3));
                return;
                
                case 0xb: /*Mode*/
                channel = (val & 3) + 4;
                dma[channel].mode = val;
                if (dma_ps2.is_ps2)
                {
                        dma[channel].ps2_mode &= ~0x1c;
                        if (val & 0x20)
                                dma[channel].ps2_mode |= 0x10;
                        if ((val & 0xc) == 8)
                                dma[channel].ps2_mode |= 4;
                        else if ((val & 0xc) == 4)
                                dma[channel].ps2_mode |= 0xc;
                }
                return;
                
                case 0xc: /*Clear FF*/
                dma16_wp = 0;
                return;
                
                case 0xd: /*Master clear*/
                dma16_wp = 0;
                dma_m |= 0xf0;
                return;
                
                case 0xf: /*Mask write*/
                dma_m = (dma_m & 0x0f) | ((val & 0xf) << 4);
                return;
        }
}


void dma_page_write(uint16_t addr, uint8_t val, void *priv)
{
        dmapages[addr & 0xf] = val;
        switch (addr & 0xf)
        {
                case 1:
                dma[2].page = (AT) ? val : val & 0xf;
                dma[2].ab = (dma[2].ab & 0xffff) | (dma[2].page << 16);
                dma[2].ac = (dma[2].ac & 0xffff) | (dma[2].page << 16);
                break;
                case 2:
                dma[3].page = (AT) ? val : val & 0xf;
                dma[3].ab = (dma[3].ab & 0xffff) | (dma[3].page << 16);
                dma[3].ac = (dma[3].ac & 0xffff) | (dma[3].page << 16);
                break;
                case 3:
                dma[1].page = (AT) ? val : val & 0xf;
                dma[1].ab = (dma[1].ab & 0xffff) | (dma[1].page << 16);
                dma[1].ac = (dma[1].ac & 0xffff) | (dma[1].page << 16);
                break;
                case 7:
                dma[0].page = (AT) ? val : val & 0xf;
                dma[0].ab = (dma[0].ab & 0xffff) | (dma[0].page << 16);
                dma[0].ac = (dma[0].ac & 0xffff) | (dma[0].page << 16);
                break;
                case 0x9:
                dma[6].page = val & 0xfe;
                dma[6].ab = (dma[6].ab & 0x1ffff) | (dma[6].page << 16);
                dma[6].ac = (dma[6].ac & 0x1ffff) | (dma[6].page << 16);
                break;
                case 0xa:
                dma[7].page = val & 0xfe;
                dma[7].ab = (dma[7].ab & 0x1ffff) | (dma[7].page << 16);
                dma[7].ac = (dma[7].ac & 0x1ffff) | (dma[7].page << 16);
                break;
                case 0xb:
                dma[5].page = val & 0xfe;
                dma[5].ab = (dma[5].ab & 0x1ffff) | (dma[5].page << 16);
                dma[5].ac = (dma[5].ac & 0x1ffff) | (dma[5].page << 16);
                break;
        }
}

uint8_t dma_page_read(uint16_t addr, void *priv)
{
        return dmapages[addr & 0xf];
}

void dma_init()
{
        io_sethandler(0x0000, 0x0010, dma_read,      NULL, NULL, dma_write,      NULL, NULL,  NULL);
        io_sethandler(0x0080, 0x0008, dma_page_read, NULL, NULL, dma_page_write, NULL, NULL,  NULL);
        dma_ps2.is_ps2 = 0;
}

void dma16_init()
{
        io_sethandler(0x00C0, 0x0020, dma16_read,    NULL, NULL, dma16_write,    NULL, NULL,  NULL);
        io_sethandler(0x0088, 0x0008, dma_page_read, NULL, NULL, dma_page_write, NULL, NULL,  NULL);
}

void ps2_dma_init()
{
        io_sethandler(0x0018, 0x0001, dma_ps2_read,  NULL, NULL, dma_ps2_write,  NULL, NULL,  NULL);
        io_sethandler(0x001a, 0x0001, dma_ps2_read,  NULL, NULL, dma_ps2_write,  NULL, NULL,  NULL);
        dma_ps2.is_ps2 = 1;
}


uint8_t _dma_read(uint32_t addr)
{
        uint8_t temp = mem_readb_phys(addr);
        return temp;
}

void _dma_write(uint32_t addr, uint8_t val)
{
        mem_writeb_phys(addr, val);
        mem_invalidate_range(addr, addr);
}

int dma_channel_read(int channel)
{
        dma_t *dma_c = &dma[channel];
        uint16_t temp;
        int tc = 0;
        
        if (channel < 4)
        {
        	if (dma_command & 0x04)
        		return DMA_NODATA;
        }
        else
        {
        	if (dma16_command & 0x04)
        		return DMA_NODATA;
        }
		
        if (!AT)
                refreshread();

	if (dma_m & (1 << channel))
                return DMA_NODATA;
        if ((dma_c->mode & 0xC) != 8)
                return DMA_NODATA;
        
        if (!dma_c->size)
        {
                temp = _dma_read(dma_c->ac);

                if (dma_c->mode & 0x20)
                {
                        if (dma_ps2.is_ps2)
                                dma_c->ac--;
                        else
                                dma_c->ac = (dma_c->ac & 0xff0000) | ((dma_c->ac - 1) & 0xffff);
                }
                else
                {
                        if (dma_ps2.is_ps2)
                                dma_c->ac++;
                        else
                                dma_c->ac = (dma_c->ac & 0xff0000) | ((dma_c->ac + 1) & 0xffff);
                }
        }
        else
        {
                temp =  _dma_read(dma_c->ac) |
                       (_dma_read(dma_c->ac + 1) << 8);

                if (dma_c->mode & 0x20)
                {
                        if (dma_ps2.is_ps2)
                                dma_c->ac -= 2;
                        else
                                dma_c->ac = (dma_c->ac & 0xfe0000) | ((dma_c->ac - 2) & 0x1ffff);
                }
                else
                {
                        if (dma_ps2.is_ps2)
                                dma_c->ac += 2;
                        else
                                dma_c->ac = (dma_c->ac & 0xfe0000) | ((dma_c->ac + 2) & 0x1ffff);
                }
        }

        dma_stat_rq |= (1 << channel);
                
        dma_c->cc--;
        if (dma_c->cc < 0)
        {
                tc = 1;
                if (dma_c->mode & 0x10) /*Auto-init*/
                {
                        dma_c->cc = dma_c->cb;
                        dma_c->ac = dma_c->ab;
                }
                else
                        dma_m |= (1 << channel);
                dma_stat |= (1 << channel);
        }

        if (tc)
                return temp | DMA_OVER;
        return temp;
}

int dma_channel_write(int channel, uint16_t val)
{
        dma_t *dma_c = &dma[channel];

        if (channel < 4)
        {
        	if (dma_command & 0x04)
        		return DMA_NODATA;
        }
        else
        {
        	if (dma16_command & 0x04)
        		return DMA_NODATA;
        }

        if (!AT)
                refreshread();

        if (dma_m & (1 << channel))
                return DMA_NODATA;
        if ((dma_c->mode & 0xC) != 4)
                return DMA_NODATA;

        if (!dma_c->size)
        {
                _dma_write(dma_c->ac, val);
                
                if (dma_c->mode & 0x20)
                {
                        if (dma_ps2.is_ps2)
                                dma_c->ac--;
                        else
                                dma_c->ac = (dma_c->ac & 0xff0000) | ((dma_c->ac - 1) & 0xffff);
                }
                else
                {
                        if (dma_ps2.is_ps2)
                                dma_c->ac++;
                        else
                                dma_c->ac = (dma_c->ac & 0xff0000) | ((dma_c->ac + 1) & 0xffff);
                }
        }
        else
        {
                _dma_write(dma_c->ac,     val);
                _dma_write(dma_c->ac + 1, val >> 8);                

                if (dma_c->mode & 0x20)
                {
                        if (dma_ps2.is_ps2)
                                dma_c->ac -= 2;
                        else
                                dma_c->ac = (dma_c->ac & 0xfe0000) | ((dma_c->ac - 2) & 0x1ffff);
                }
                else
                {
                        if (dma_ps2.is_ps2)
                                dma_c->ac += 2;
                        else
                                dma_c->ac = (dma_c->ac & 0xfe0000) | ((dma_c->ac + 2) & 0x1ffff);
                }
        }

        dma_stat_rq |= (1 << channel);

        dma_c->cc--;
        if (dma_c->cc < 0)
        {
                if (dma_c->mode & 0x10) /*Auto-init*/
                {
                        dma_c->cc = dma_c->cb;
                        dma_c->ac = dma_c->ab;
                }
                else
                        dma_m |= (1 << channel);
                dma_stat |= (1 << channel);
        }

        if (dma_m & (1 << channel))
                return DMA_OVER;

        return 0;
}

static void dma_ps2_run(int channel)
{
        dma_t *dma_c = &dma[channel];
        
        switch (dma_c->ps2_mode & DMA_PS2_XFER_MASK)
        {
                case DMA_PS2_XFER_MEM_TO_IO:
                do
                {
                        if (!dma_c->size)
                        {
                                uint8_t temp = _dma_read(dma_c->ac);
                                outb(dma_c->io_addr, temp);
                                
                                if (dma_c->ps2_mode & DMA_PS2_DEC2)
                                        dma_c->ac--;
                                else
                                        dma_c->ac++;
                        }
                        else
                        {
                                uint16_t temp = _dma_read(dma_c->ac) | (_dma_read(dma_c->ac + 1) << 8);
                                outw(dma_c->io_addr, temp);
                                
                                if (dma_c->ps2_mode & DMA_PS2_DEC2)
                                        dma_c->ac -= 2;
                                else
                                        dma_c->ac += 2;
                        }

                        dma_stat_rq |= (1 << channel);
                        dma_c->cc--;
                } while (dma_c->cc > 0);
                
                dma_stat |= (1 << channel);
                break;

                case DMA_PS2_XFER_IO_TO_MEM:
                do
                {
                        if (!dma_c->size)
                        {
                                uint8_t temp = inb(dma_c->io_addr);
                                _dma_write(dma_c->ac, temp);
                                
                                if (dma_c->ps2_mode & DMA_PS2_DEC2)
                                        dma_c->ac--;
                                else
                                        dma_c->ac++;
                        }
                        else
                        {
                                uint16_t temp = inw(dma_c->io_addr);
                                _dma_write(dma_c->ac, temp & 0xff);
                                _dma_write(dma_c->ac + 1, temp >> 8);
                                
                                if (dma_c->ps2_mode & DMA_PS2_DEC2)
                                        dma_c->ac -= 2;
                                else
                                        dma_c->ac += 2;
                        }

                        dma_stat_rq |= (1 << channel);
                        dma_c->cc--;
                } while (dma_c->cc > 0);
                
                ps2_cache_clean();
                dma_stat |= (1 << channel);
                break;
                
                default: /*Memory verify*/
                do
                {
                        if (!dma_c->size)
                        {
                                if (dma_c->ps2_mode & DMA_PS2_DEC2)
                                        dma_c->ac--;
                                else
                                        dma_c->ac++;
                        }
                        else
                        {
                                if (dma_c->ps2_mode & DMA_PS2_DEC2)
                                        dma_c->ac -= 2;
                                else
                                        dma_c->ac += 2;
                        }

                        dma_stat_rq |= (1 << channel);
                        dma->cc--;
                } while (dma->cc > 0);
                
                dma_stat |= (1 << channel);
                break;
        }
}
