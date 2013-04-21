/*ET4000 emulation*/
#include "ibm.h"
#include "video.h"
#include "vid_svga.h"
#include "vid_unk_ramdac.h"

int et4k_b8000;


void et4000_out(uint16_t addr, uint8_t val)
{
        uint8_t old;
        
        if (((addr&0xFFF0) == 0x3D0 || (addr&0xFFF0) == 0x3B0) && !(svga_miscout&1)) addr ^= 0x60;

        pclog("ET4000 out %04X %02X\n", addr, val);
                
        switch (addr)
        {
                case 0x3C6: case 0x3C7: case 0x3C8: case 0x3C9:
                unk_ramdac_out(addr,val);
                return;
                
                case 0x3CD: /*Banking*/
                svgawbank=(val&0xF)*65536;
                svgarbank=((val>>4)&0xF)*65536;
                svgaseg=val;
                pclog("Banking write %08X %08X %02X\n", svgawbank, svgarbank, val);
                return;
                case 0x3CF:
                switch (gdcaddr&15)
                {
                        case 6:
                        et4k_b8000=((crtc[0x36]&0x38)==0x28) && ((gdcreg[6]&0xC)==4);
                        break;
                }
                break;
                case 0x3D4:
                crtcreg = val & 0x3f;
                return;
                case 0x3D5:
                if (crtcreg <= 7 && crtc[0x11] & 0x80) return;
                old=crtc[crtcreg];
                crtc[crtcreg]=val;
                et4k_b8000=((crtc[0x36]&0x38)==0x28) && ((gdcreg[6]&0xC)==4);
                if (old!=val)
                {
                        if (crtcreg<0xE || crtcreg>0x10)
                        {
                                fullchange=changeframecount;
                                svga_recalctimings();
                        }
                }
                break;
                case 0x3D8:
                if (val==0xA0) svgaon=1;
                if (val==0x29) svgaon=0;
                break;
        }
        svga_out(addr,val);
}

uint8_t et4000_in(uint16_t addr)
{
        uint8_t temp;
                
        if (((addr&0xFFF0) == 0x3D0 || (addr&0xFFF0) == 0x3B0) && !(svga_miscout&1)) addr ^= 0x60;
        
        if (addr != 0x3da) pclog("IN ET4000 %04X\n", addr);
        
        switch (addr)
        {
                case 0x3C5:
                if ((seqaddr&0xF)==7) return seqregs[seqaddr&0xF]|4;
                break;

                case 0x3C6: case 0x3C7: case 0x3C8: case 0x3C9:
                return unk_ramdac_in(addr);
                
                case 0x3CD: /*Banking*/
                return svgaseg;
                case 0x3D4:
                return crtcreg;
                case 0x3D5:
                return crtc[crtcreg];
        }
        return svga_in(addr);
}

void et4000_recalctimings()
{
        svga_ma|=(crtc[0x33]&3)<<16;
        if (crtc[0x35]&2)    svga_vtotal+=0x400;
        if (crtc[0x35]&4)    svga_dispend+=0x400;
        if (crtc[0x35]&8)    svga_vsyncstart+=0x400;
        if (crtc[0x35]&0x10) svga_split+=0x400;
        if (!svga_rowoffset) svga_rowoffset=0x100;
//        if (crtc[0x3F]&0x80) svga_rowoffset+=0x100;
        if (crtc[0x3F]&1)    svga_htotal+=256;
        if (attrregs[0x16]&0x20) svga_hdisp<<=1;
//        pclog("Rowoffset %i\n",svga_rowoffset);

        switch (((svga_miscout >> 2) & 3) | ((crtc[0x34] << 1) & 4))
        {
                case 0: case 1: break;
                case 3: svga_clock = cpuclock/40000000.0; break;
                case 5: svga_clock = cpuclock/65000000.0; break;
                default: svga_clock = cpuclock/36000000.0; break;
        }

}

int et4000_init()
{
        svga_recalctimings_ex = et4000_recalctimings;
        svga_vram_limit = 1 << 20; /*1mb*/
        vrammask = 0xfffff;
        return svga_init();
}

GFXCARD vid_et4000 =
{
        et4000_init,
        /*IO at 3Cx/3Dx*/
        et4000_out,
        et4000_in,
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
