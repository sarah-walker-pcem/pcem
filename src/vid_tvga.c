/*Trident TVGA (8900D) emulation*/
#include "ibm.h"
#include "video.h"
#include "vid_svga.h"
#include "vid_tkd8001_ramdac.h"

uint8_t trident3d8,trident3d9;
int tridentoldmode;
uint8_t tridentoldctrl2,tridentnewctrl2;
uint8_t tridentdac;

void tvga_out(uint16_t addr, uint8_t val)
{
        uint8_t old;

        if (((addr&0xFFF0) == 0x3D0 || (addr&0xFFF0) == 0x3B0) && !(svga_miscout&1)) addr ^= 0x60;

        switch (addr)
        {
                case 0x3C5:
                switch (seqaddr&0xF)
                {
                        case 0xB: tridentoldmode=1; break;
                        case 0xC: if (seqregs[0xE]&0x80) seqregs[0xC]=val; break;
                        case 0xD: if (tridentoldmode) { tridentoldctrl2=val; rowdbl=val&0x10; } else tridentnewctrl2=val; break;
                        case 0xE:
                        seqregs[0xE]=val^2;
                        svgawbank=(seqregs[0xE]&0xF)*65536;
                        if (!(gdcreg[0xF]&1)) svgarbank=svgawbank;
                        return;
                }
                break;

                case 0x3C6: case 0x3C7: case 0x3C8: case 0x3C9:
                tkd8001_ramdac_out(addr,val);
                return;

                case 0x3CF:
                switch (gdcaddr&15)
                {
                        case 0xE:
                        gdcreg[0xE]=val^2;
                        if ((gdcreg[0xF]&1)==1)
                           svgarbank=(gdcreg[0xE]&0xF)*65536;
                        break;
                        case 0xF:
                        if (val&1) svgarbank=(gdcreg[0xE] &0xF)*65536;
                        else       svgarbank=(seqregs[0xE]&0xF)*65536;
                        svgawbank=(seqregs[0xE]&0xF)*65536;
                        break;
                }
                break;
                case 0x3D4:
                crtcreg=val&63;
                return;
                case 0x3D5:
                if (crtcreg <= 7 && crtc[0x11] & 0x80) return;
                old=crtc[crtcreg];
                crtc[crtcreg]=val;
                //if (crtcreg!=0xE && crtcreg!=0xF) pclog("CRTC R%02X = %02X\n",crtcreg,val);
                if (old!=val)
                {
                        if (crtcreg<0xE || crtcreg>0x10)
                        {
                                fullchange=changeframecount;
                                svga_recalctimings();
                        }
                }
                return;
                case 0x3D8:
                trident3d8=val;
                if (gdcreg[0xF]&4)
                {
                        svgawbank=(val&0x1F)*65536;
//                                pclog("SVGAWBANK 3D8 %08X %04X:%04X\n",svgawbank,CS,pc);
                        if (!(gdcreg[0xF]&1))
                        {
                                svgarbank=(val&0x1F)*65536;
//                                        pclog("SVGARBANK 3D8 %08X %04X:%04X\n",svgarbank,CS,pc);
                        }
                }
                return;
                case 0x3D9:
                trident3d9=val;
                if ((gdcreg[0xF]&5)==5)
                {
                        svgarbank=(val&0x1F)*65536;
//                                pclog("SVGARBANK 3D9 %08X %04X:%04X\n",svgarbank,CS,pc);
                }
                return;
        }
        svga_out(addr,val);
}

uint8_t tvga_in(uint16_t addr)
{
        uint8_t temp;
        
        if (((addr&0xFFF0) == 0x3D0 || (addr&0xFFF0) == 0x3B0) && !(svga_miscout&1)) addr ^= 0x60;
        
        switch (addr)
        {
                case 0x3C5:
                if ((seqaddr&0xF)==0xB)
                {
//                        printf("Read Trident ID %04X:%04X %04X\n",CS,pc,readmemw(ss,SP));
                        tridentoldmode=0;
                        return 0x33; /*TVGA8900D*/
                }
                if ((seqaddr&0xF)==0xC)
                {
//                        printf("Read Trident Power Up 1 %04X:%04X %04X\n",CS,pc,readmemw(ss,SP));
//                        return 0x20; /*2 DRAM banks*/
                }
                if ((seqaddr&0xF)==0xD)
                {
                        if (tridentoldmode) return tridentoldctrl2;
                        return tridentnewctrl2;
                }
                break;
                case 0x3C6: case 0x3C7: case 0x3C8: case 0x3C9:
                return tkd8001_ramdac_in(addr);
                case 0x3CD: /*Banking*/
                return svgaseg;
                case 0x3D4:
                return crtcreg;
                case 0x3D5:
                return crtc[crtcreg];
        }
        return svga_in(addr);
}

void tvga_recalctimings()
{
        if (!svga_rowoffset) svga_rowoffset=0x100; /*This is the only sensible way I can see this being handled,
                                                     given that TVGA8900D has no overflow bits.
                                                     Some sort of overflow is required for 320x200x24 and 1024x768x16*/

        if ((crtc[0x1E]&0xA0)==0xA0) svga_ma|=0x10000;
        if ((crtc[0x27]&0x01)==0x01) svga_ma|=0x20000;
        if ((crtc[0x27]&0x02)==0x02) svga_ma|=0x40000;
        
        if (tridentoldctrl2 & 0x10)
        {
                svga_rowoffset<<=1;
                svga_ma<<=1;
        }
        if (tridentoldctrl2 & 0x10) /*I'm not convinced this is the right register for this function*/
           svga_lowres=0;
           
        if (gdcreg[0xF] & 8)
        {
                svga_htotal<<=1;
                svga_hdisp<<=1;
        }
        svga_interlace = crtc[0x1E] & 4;
        if (svga_interlace)
           svga_rowoffset >>= 1;
        
        switch (((svga_miscout>>2)&3) | ((tridentnewctrl2<<2)&4))
        {
                case 2: svga_clock = cpuclock/44900000.0; break;
                case 3: svga_clock = cpuclock/36000000.0; break;
                case 4: svga_clock = cpuclock/57272000.0; break;
                case 5: svga_clock = cpuclock/65000000.0; break;
                case 6: svga_clock = cpuclock/50350000.0; break;
                case 7: svga_clock = cpuclock/40000000.0; break;
        }
}

int tvga_init()
{
        svga_recalctimings_ex = tvga_recalctimings;
        svga_vram_limit = 1 << 20; /*1mb - chip supports 2mb, but drivers are buggy*/
        vrammask = 0xfffff;
        return svga_init();
}

GFXCARD vid_tvga =
{
        tvga_init,
        /*IO at 3Cx/3Dx*/
        tvga_out,
        tvga_in,
        /*IO at 3Ax/3Bx*/
        video_out_null,
        video_in_null,
        
        svga_poll,
        svga_recalctimings,
        
        svga_write,
        video_write_null,
        video_write_null,
        
        svga_read,
        video_read_null,
        video_read_null
};
