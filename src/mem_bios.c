#include <stdlib.h>
#include "ibm.h"
#include "mem.h"
#include "mem_bios.h"
#include "rom.h"
#include "video.h"
#include "xi8088.h"

static void romfread(uint8_t *buf, size_t size, size_t count, FILE *fp)
{
	int result = fread(buf,size,count,fp);
	if (result < count)
	{
		pclog("ROM read failed: Expected %d, read %d\n", count, result);
	}
}

static int mem_load_basic(char *path)
{
        char s[256];
        FILE *f;
        
        sprintf(s, "%s/ibm-basic-1.10.rom", path);
        f = romfopen(s, "rb");
        if (!f)
        {
                sprintf(s, "%s/basicc11.f6", path);
                f = romfopen(s, "rb");
                if (!f) return 1; /*I don't really care if BASIC is there or not*/
                romfread(rom + 0x6000, 8192, 1, f);
                fclose(f);
                sprintf(s, "%s/basicc11.f8", path);
                f = romfopen(s, "rb");
                if (!f) return 0; /*But if some of it is there, then all of it must be*/
                romfread(rom + 0x8000, 8192, 1, f);
                fclose(f);
                sprintf(s, "%s/basicc11.fa", path);
                f = romfopen(s, "rb");
                if (!f) return 0;
                romfread(rom + 0xA000, 8192, 1, f);
                fclose(f);
                sprintf(s, "%s/basicc11.fc", path);
                f = romfopen(s, "rb");
                if (!f) return 0;
                romfread(rom + 0xC000, 8192, 1, f);
                fclose(f);
        }
        else
        {
                romfread(rom + 0x6000, 32768, 1, f);
                fclose(f);
        }

        return 1;
}
        
int loadbios()
{
        FILE *f=NULL,*ff=NULL;
        int c;
       
        loadfont("mda.rom", FONT_MDA);
	loadfont("wy700.rom", FONT_WY700);
	loadfont("8x12.bin", FONT_MDSI);
	loadfont("im1024font.bin", FONT_IM1024);
        
        biosmask = 0xffff;
        
        if (!rom)
                rom = malloc(0x20000);
        memset(romext,0x63,0x4000);
        memset(rom, 0xff, 0x20000);
        
        pclog("Starting with romset %i\n", romset);
        
        switch (romset)
        {
                case ROM_PC1512:
                f=romfopen("pc1512/40043.v1","rb");
                ff=romfopen("pc1512/40044.v1","rb");
                if (!f || !ff) break;
                for (c=0xC000;c<0x10000;c+=2)
                {
                        rom[c]=getc(f);
                        rom[c+1]=getc(ff);
                }
                fclose(ff);
                fclose(f);
                loadfont("pc1512/40078.ic127", FONT_CGA);
                return 1;
                case ROM_PC1640:
                f=romfopen("pc1640/40044.v3","rb");
                ff=romfopen("pc1640/40043.v3","rb");
                if (!f || !ff) break;
                for (c=0xC000;c<0x10000;c+=2)
                {
                        rom[c]=getc(f);
                        rom[c+1]=getc(ff);
                }
                fclose(ff);
                fclose(f);
                f=romfopen("pc1640/40100","rb");
                if (!f) break;
                fclose(f);
                return 1;
                case ROM_PC200:
                f=romfopen("pc200/pc20v2.1","rb");
                ff=romfopen("pc200/pc20v2.0","rb");
                if (!f || !ff) break;
                for (c=0xC000;c<0x10000;c+=2)
                {
                        rom[c]=getc(f);
                        rom[c+1]=getc(ff);
                }
                fclose(ff);
                fclose(f);
                loadfont("pc200/40109.bin", FONT_PC200);
                return 1;
                case ROM_PPC512:
                f=romfopen("ppc512/40107.v2","rb");
                ff=romfopen("ppc512/40108.v2","rb");
                if (!f || !ff) break;
                for (c=0xC000;c<0x10000;c+=2)
                {
                        rom[c]=getc(f);
                        rom[c+1]=getc(ff);
                }
                fclose(ff);
                fclose(f);
                loadfont("ppc512/40109.bin", FONT_PC200);
                return 1;
                case ROM_TANDY:
                f=romfopen("tandy/tandy1t1.020","rb");
                if (!f) break;
                romfread(rom,65536,1,f);
                fclose(f);
                return 1;
                case ROM_TANDY1000HX:
                f = romfopen("tandy1000hx/v020000.u12", "rb");
                if (!f) break;
                romfread(rom, 0x20000, 1, f);
                fclose(f);
                biosmask = 0x1ffff;
                return 1;
                case ROM_TANDY1000SL2:
                f  = romfopen("tandy1000sl2/8079047.hu1" ,"rb");
                ff = romfopen("tandy1000sl2/8079048.hu2","rb");
                if (!f || !ff) break;
                fseek(f,  0x30000/2, SEEK_SET);
                fseek(ff, 0x30000/2, SEEK_SET);
                for (c = 0x0000; c < 0x10000; c += 2)
                {
                        rom[c] = getc(f);
                        rom[c + 1] = getc(ff);
                }
                fclose(ff);
                fclose(f);
                return 1;
/*                case ROM_IBMPCJR:
                f=fopen("pcjr/bios.rom","rb");
                romfread(rom+0xE000,8192,1,f);
                fclose(f);
                f=fopen("pcjr/basic.rom","rb");
                romfread(rom+0x6000,32768,1,f);
                fclose(f);
                break;*/
                case ROM_IBMXT:
                f=romfopen("ibmxt/xt.rom","rb");
                if (!f)
                {
                        f = romfopen("ibmxt/5000027.u19", "rb");
                        ff = romfopen("ibmxt/1501512.u18","rb");
                        if (!f || !ff) break;
                        romfread(rom, 0x8000, 1, f);
                        romfread(rom + 0x8000, 0x8000, 1, ff);
                        fclose(ff);
                        fclose(f);
                        return 1;
                }
                else
                {
                        romfread(rom,65536,1,f);
                        fclose(f);
                        return 1;
                }
                break;
                
                case ROM_IBMPCJR:
                f = romfopen("ibmpcjr/bios.rom","rb");
                if (!f) break;
                romfread(rom, 0x10000, 1, f);
                fclose(f);
                return 1;
                
                case ROM_GENXT:
                f=romfopen("genxt/pcxt.rom","rb");
                if (!f) break;
                romfread(rom+0xE000,8192,1,f);
                fclose(f);
                if (!mem_load_basic("genxt"))
                        break;
                return 1;
                case ROM_DTKXT:
                f=romfopen("dtk/dtk_erso_2.42_2764.bin","rb");
                if (!f) break;
                romfread(rom+0xE000,8192,1,f);
                fclose(f);
                return 1;
                case ROM_OLIM24:
                f  = romfopen("olivetti_m24/olivetti_m24_version_1.43_low.bin" ,"rb");
                ff = romfopen("olivetti_m24/olivetti_m24_version_1.43_high.bin","rb");
                if (!f || !ff) break;
                for (c = 0x0000; c < 0x4000; c += 2)
                {
                        rom[c + 0xc000] = getc(f);
                        rom[c + 0xc001] = getc(ff);
                }
                fclose(ff);
                fclose(f);
                return 1;
                        
                case ROM_PC2086:
                f  = romfopen("pc2086/40179.ic129" ,"rb");
                ff = romfopen("pc2086/40180.ic132","rb");
                if (!f || !ff) break;
                pclog("Loading BIOS\n");
                for (c = 0x0000; c < 0x4000; c += 2)
                {
                        rom[c + 0x0000] = getc(f);
                        rom[c + 0x0001] = getc(ff);
                }
                pclog("%02X %02X %02X\n", rom[0xfff0], rom[0xfff1], rom[0xfff2]);
                fclose(ff);
                fclose(f);
                f = romfopen("pc2086/40186.ic171", "rb");
                if (!f) break;
                fclose(f);
                biosmask = 0x3fff;
                return 1;

                case ROM_PC3086:
                f  = romfopen("pc3086/fc00.bin" ,"rb");
                if (!f) break;
                romfread(rom, 0x4000, 1, f);
                fclose(f);
                f = romfopen("pc3086/c000.bin", "rb");
                if (!f) break;
                fclose(f);
                biosmask = 0x3fff;                
                return 1;

                case ROM_IBMAT:
/*                f=romfopen("amic206.bin","rb");
                if (!f) break;
                romfread(rom,65536,1,f);
                fclose(f);
                return 1;*/
                case ROM_IBMAT386:
                f = romfopen("ibmat/62x0820.u27", "rb");
                ff  =romfopen("ibmat/62x0821.u47", "rb");
                if (!f || !ff) break;
                for (c=0x0000;c<0x10000;c+=2)
                {
                        rom[c]=getc(f);
                        rom[c+1]=getc(ff);
                }
                fclose(ff);
                fclose(f);
                return 1;
                case ROM_CMDPC30:
                f  = romfopen("cmdpc30/commodore pc 30 iii even.bin", "rb");
                ff = romfopen("cmdpc30/commodore pc 30 iii odd.bin",  "rb");
                if (!f || !ff) break;
                for (c = 0x0000; c < 0x8000; c += 2)
                {
                        rom[c]     = getc(f);
                        rom[c + 1] = getc(ff);
                }
                fclose(ff);
                fclose(f);
                biosmask = 0x7fff;
                return 1;
                case ROM_DELL200:
                f=romfopen("dells200/dell0.bin","rb");
                ff=romfopen("dells200/dell1.bin","rb");
                if (!f || !ff) break;
                for (c=0x0000;c<0x10000;c+=2)
                {
                        rom[c]=getc(f);
                        rom[c+1]=getc(ff);
                }
                fclose(ff);
                fclose(f);
                return 1;
/*                case ROM_IBMAT386:
                f=romfopen("at386/at386.bin","rb");
                if (!f) break;
                romfread(rom,65536,1,f);
                fclose(f);
                return 1;*/
                case ROM_AMA932J:
//                f=romfopen("at386/at386.bin","rb");
                f=romfopen("ama932j/ami.bin","rb");
                if (!f) break;
                romfread(rom,65536,1,f);
                fclose(f);
                f=romfopen("ama932j/oti067.bin","rb");
                if (!f) break;
                fclose(f);
                return 1;

                case ROM_AMI386SX:
//                f=romfopen("at386/at386.bin","rb");
                f=romfopen("ami386/ami386.bin","rb");
                if (!f) break;
                romfread(rom,65536,1,f);
                fclose(f);
                return 1;

                case ROM_AMI386DX_OPTI495: /*This uses the OPTi 82C495 chipset*/
                f=romfopen("ami386dx/opt495sx.ami","rb");
                if (!f) break;
                romfread(rom,65536,1,f);
                fclose(f);
                return 1;
                case ROM_MR386DX_OPTI495: /*This uses the OPTi 82C495 chipset*/
                f=romfopen("mr386dx/opt495sx.mr","rb");
                if (!f) break;
                romfread(rom,65536,1,f);
                fclose(f);
                return 1;

                case ROM_ACER386:
                f=romfopen("acer386/acer386.bin","rb");
                if (!f) break;
                romfread(rom,65536,1,f);
                fclose(f);
                rom[0xB0]=0xB0-0x51;
                rom[0x40d4]=0x51; /*PUSH CX*/
                f=romfopen("acer386/oti067.bin","rb");
                if (!f) break;
                fclose(f);
                return 1;

                case ROM_KMXC02:
                f=romfopen("kmxc02/3ctm005.bin","rb");
                if (!f) break;
                romfread(rom,65536,1,f);
                fclose(f);
                return 1;

                case ROM_AMI286:
                f=romfopen("ami286/amic206.bin","rb");
                if (!f) break;
                romfread(rom,65536,1,f);
                fclose(f);
//                memset(romext,0x63,0x8000);
                return 1;

                case ROM_TG286M:
                f=romfopen("tg286m/ami.bin","rb");
                if (!f) break;
                romfread(rom,65536,1,f);
                fclose(f);
//                memset(romext,0x63,0x8000);
                return 1;

                case ROM_AWARD286:
                f=romfopen("award286/award.bin","rb");
                if (!f) break;
                romfread(rom,65536,1,f);
                fclose(f);
                return 1;

                case ROM_GDC212M:
                f=romfopen("gdc212m/gdc212m_72h.bin","rb");
                if (!f) break;
                romfread(rom,65536,1,f);
                fclose(f);
                return 1;

                case ROM_HYUNDAI_SUPER286TR:
                f=romfopen("super286tr/award.bin","rb");
                if (!f) break;
                romfread(rom,65536,1,f);
                fclose(f);
                return 1;

                case ROM_GW286CT:
                f=romfopen("gw286ct/2ctc001.bin","rb");
                if (!f) break;
                romfread(rom,65536,1,f);
                fclose(f);
                return 1;

                case ROM_SPC4200P:
                f=romfopen("spc4200p/u8.01","rb");
                if (!f) break;
                romfread(rom,65536,1,f);
                fclose(f);
                return 1;

                case ROM_SPC4216P:
                f=romfopen("spc4216p/phoenix.bin","rb");
                if (!f)
                {
                        f = romfopen("spc4216p/7101.u8", "rb");
                        ff = romfopen("spc4216p/ac64.u10","rb");
                        if (!f || !ff) break;
                        for (c = 0x0000; c < 0x10000; c += 2)
                        {
                                rom[c]     = getc(f);
                                rom[c + 1] = getc(ff);
                        }
                        fclose(ff);
                        fclose(f);
                        return 1;
                }
                else
                {
                        romfread(rom,65536,1,f);

                        fclose(f);
                        return 1;
                }
                break;

                case ROM_SPC4620P:
                f=romfopen("spc4620p/svb6120a_font.rom","rb");
                if (!f) break;
                fclose(f);
                f = romfopen("spc4620p/31005h.u8", "rb");
                ff = romfopen("spc4620p/31005h.u10","rb");
                if (f && ff)
                {
                        fseek(f,  0x8000, SEEK_SET);
                        fseek(ff, 0x8000, SEEK_SET);                
                        for (c = 0x0000; c < 0x10000; c += 2)
                        {
                                rom[c]     = getc(f);
                                rom[c + 1] = getc(ff);
                        }
                        fclose(ff);
                        fclose(f);
                        return 1;
                }
                break;

                case ROM_EUROPC:
//                return 0;
                f=romfopen("europc/50145","rb");
                if (!f) break;
                romfread(rom+0x8000,32768,1,f);
                fclose(f);
//                memset(romext,0x63,0x8000);
                return 1;

                case ROM_IBMPC:
                f=romfopen("ibmpc/pc102782.bin","rb");
                if (!f) break;
//                f=fopen("pc081682.bin","rb");
                romfread(rom+0xE000,8192,1,f);
                fclose(f);
                if (!mem_load_basic("ibmpc"))
                        break;
                return 1;

                case ROM_MEGAPC:
                f  = romfopen("megapc/41651-bios lo.u18",  "rb");
                ff = romfopen("megapc/211253-bios hi.u19", "rb");
                if (!f || !ff) break;
                fseek(f,  0x8000, SEEK_SET);
                fseek(ff, 0x8000, SEEK_SET);                
                for (c = 0x0000; c < 0x10000; c+=2)
                {
                        rom[c]=getc(f);
                        rom[c+1]=getc(ff);
                }
                fclose(ff);
                fclose(f);
                return 1;
                        
                case ROM_AMI486:
                f=romfopen("ami486/ami486.bin","rb");
                if (!f) break;
                romfread(rom,65536,1,f);
                fclose(f);
                //is486=1;
                return 1;
                
                case ROM_WIN486:
//                f=romfopen("win486/win486.bin","rb");
                f=romfopen("win486/ali1429g.amw","rb");
                if (!f) break;
                romfread(rom,65536,1,f);
                fclose(f);
                //is486=1;
                return 1;

                case ROM_PCI486:
                f=romfopen("hot-433/hot-433.ami","rb");               
                if (!f) break;
                romfread(rom,           0x20000, 1, f);                
                fclose(f);
                biosmask = 0x1ffff;
                //is486=1;
                return 1;

                case ROM_SIS496:
                f = romfopen("sis496/sis496-1.awa", "rb");
                if (!f) break;
                romfread(rom,           0x20000, 1, f);                
                fclose(f);
                biosmask = 0x1ffff;
                pclog("Load SIS496 %x %x\n", rom[0x1fff0], rom[0xfff0]);
                return 1;
                
                case ROM_P55VA:
                f = romfopen("p55va/va021297.bin", "rb");
                if (!f) break;
                romfread(rom, 0x20000, 1, f);                
                fclose(f);
                biosmask = 0x1ffff;
                return 1;

                case ROM_P55TVP4:
                f = romfopen("p55tvp4/tv5i0204.awd", "rb");
                if (!f) break;
                romfread(rom, 0x20000, 1, f);                
                fclose(f);
                biosmask = 0x1ffff;
                return 1;

                case ROM_430VX:
//                f = romfopen("430vx/ga586atv.bin", "rb");
//                f = fopen("430vx/vx29.bin", "rb");
                f = romfopen("430vx/55xwuq0e.bin", "rb");
//                f=romfopen("430vx/430vx","rb");               
                if (!f) break;
                romfread(rom,           0x20000, 1, f);                
                fclose(f);
                biosmask = 0x1ffff;
                //is486=1;
                return 1;

                case ROM_P55T2P4:
                f = romfopen("p55t2p4/0207_j2.bin", "rb");
                if (!f) break;
                romfread(rom, 0x20000, 1, f);                
                fclose(f);
                biosmask = 0x1ffff;
                return 1;

                case ROM_REVENGE:
                f = romfopen("revenge/1009af2_.bio", "rb");
                if (!f) break;
                fseek(f, 0x80, SEEK_SET);
                romfread(rom + 0x10000, 0x10000, 1, f);                
                fclose(f);
                f = romfopen("revenge/1009af2_.bi1", "rb");
                if (!f) break;
                fseek(f, 0x80, SEEK_SET);
                romfread(rom, 0xc000, 1, f);                
                fclose(f);
                biosmask = 0x1ffff;
                //is486=1;
                return 1;
                case ROM_ENDEAVOR:
                f = romfopen("endeavor/1006cb0_.bio", "rb");
                if (!f) break;
                fseek(f, 0x80, SEEK_SET);
                romfread(rom + 0x10000, 0x10000, 1, f);                
                fclose(f);
                f = romfopen("endeavor/1006cb0_.bi1", "rb");
                if (!f) break;
                fseek(f, 0x80, SEEK_SET);
                romfread(rom, 0xd000, 1, f);
                fclose(f);
                biosmask = 0x1ffff;
                //is486=1;
                return 1;

                case ROM_IBMPS1_2011:
#if 0
                f=romfopen("ibmps1es/ibm_1057757_24-05-90.bin","rb");
                ff=romfopen("ibmps1es/ibm_1057757_29-15-90.bin","rb");
                fseek(f, 0x10000, SEEK_SET);
                fseek(ff, 0x10000, SEEK_SET);
                if (!f || !ff) break;
                for (c = 0x0000; c < 0x20000; c += 2)
                {
                        rom[c] = getc(f);
                        rom[c+1] = getc(ff);
                }
                fclose(ff);
                fclose(f);
#endif
//#if 0
                f = romfopen("ibmps1es/f80000.bin", "rb");
                if (!f) break;
                fseek(f, 0x60000, SEEK_SET);
                romfread(rom, 0x20000, 1, f);                
                fclose(f);
//#endif
                biosmask = 0x1ffff;
                return 1;

                case ROM_IBMPS1_2121:
                f = romfopen("ibmps1_2121/fc0000.bin", "rb");
                if (!f) break;
                fseek(f, 0x20000, SEEK_SET);
                romfread(rom, 0x20000, 1, f);                
                fclose(f);
                biosmask = 0x1ffff;
                return 1;

                case ROM_DESKPRO_386:
                f=romfopen("deskpro386/109592-005.u11.bin","rb");
                ff=romfopen("deskpro386/109591-005.u13.bin","rb");
                if (!f || !ff) break;
                for (c = 0x0000; c < 0x8000; c += 2)
                {
                        rom[c] = getc(f);
                        rom[c+1] = getc(ff);
                }
                fclose(ff);
                fclose(f);
                biosmask = 0x7fff;
                return 1;

                case ROM_AMIXT:
                f = romfopen("amixt/ami_8088_bios_31jan89.bin", "rb");
                if (!f) break;
                romfread(rom + 0xE000, 8192, 1, f);
                fclose(f);
                return 1;
                
                case ROM_LTXT:
                f = romfopen("ltxt/27c64.bin", "rb");
                if (!f) break;
                romfread(rom + 0xE000, 8192, 1, f);
                fclose(f);
                if (!mem_load_basic("ltxt"))
                        break;
                return 1;

                case ROM_LXT3:
                f = romfopen("lxt3/27c64d.bin", "rb");
                if (!f) break;
                romfread(rom + 0xE000, 8192, 1, f);
                fclose(f);
                if (!mem_load_basic("lxt3"))
                        break;
                return 1;

                case ROM_PX386: /*Phoenix 80386 BIOS*/
                f=romfopen("px386/3iip001l.bin","rb");
                ff=romfopen("px386/3iip001h.bin","rb");
                if (!f || !ff) break;
                for (c = 0x0000; c < 0x10000; c += 2)
                {
                        rom[c] = getc(f);
                        rom[c+1] = getc(ff);
                }
                fclose(ff);
                fclose(f);
                return 1;

                case ROM_DTK386: /*Uses NEAT chipset*/
                f = romfopen("dtk386/3cto001.bin", "rb");
                if (!f) break;
                romfread(rom, 65536, 1, f);
                fclose(f);
                return 1;

                case ROM_PXXT:
                f = romfopen("pxxt/000p001.bin", "rb");
                if (!f) break;
                romfread(rom + 0xE000, 8192, 1, f);
                fclose(f);
                return 1;

                case ROM_JUKOPC:
                f = romfopen("jukopc/000o001.bin", "rb");
                if (!f) break;
                romfread(rom + 0xE000, 8192, 1, f);
                fclose(f);
                return 1;
				
		case ROM_IBMPS2_M30_286:
                f = romfopen("ibmps2_m30_286/33f5381a.bin", "rb");
                if (!f) break;
                romfread(rom, 0x20000, 1, f);                
                fclose(f);
                biosmask = 0x1ffff;
                return 1;

                case ROM_IBMPS2_M50:
                f=romfopen("i8550021/90x7423.zm14","rb");
                ff=romfopen("i8550021/90x7426.zm16","rb");
                if (!f || !ff) break;
                for (c = 0x0000; c < 0x10000; c += 2)
                {
                        rom[c] = getc(f);
                        rom[c+1] = getc(ff);
                }
                fclose(ff);
                fclose(f);
                f=romfopen("i8550021/90x7420.zm13","rb");
                ff=romfopen("i8550021/90x7429.zm18","rb");
                if (!f || !ff) break;
                for (c = 0x10000; c < 0x20000; c += 2)
                {
                        rom[c] = getc(f);
                        rom[c+1] = getc(ff);
                }
                fclose(ff);
                fclose(f);
                biosmask = 0x1ffff;
                return 1;

                case ROM_IBMPS2_M55SX:
                f=romfopen("i8555081/33f8146.zm41","rb");
                ff=romfopen("i8555081/33f8145.zm40","rb");
                if (!f || !ff) break;
                for (c = 0x0000; c < 0x20000; c += 2)
                {
                        rom[c] = getc(f);
                        rom[c+1] = getc(ff);
                }
                fclose(ff);
                fclose(f);
                biosmask = 0x1ffff;
                return 1;

                case ROM_IBMPS2_M80:
                f=romfopen("i8580111/15f6637.bin","rb");
                ff=romfopen("i8580111/15f6639.bin","rb");
                if (!f || !ff) break;
                for (c = 0x0000; c < 0x20000; c += 2)
                {
                        rom[c] = getc(f);
                        rom[c+1] = getc(ff);
                }
                fclose(ff);
                fclose(f);
                biosmask = 0x1ffff;
                return 1;
				
                case ROM_ATARIPC3:
                f=romfopen("ataripc3/AWARD_ATARI_PC_BIOS_3.08.BIN","rb");
                if (!f) break;
                romfread(rom+0x8000,32768,1,f);
                fclose(f);
                return 1;

                case ROM_IBMXT286:
                f = romfopen("ibmxt286/BIOS_5162_21APR86_U34_78X7460_27256.BIN", "rb");
                ff  =romfopen("ibmxt286/BIOS_5162_21APR86_U35_78X7461_27256.BIN", "rb");
                if (!f || !ff) break;
                for (c=0x0000;c<0x10000;c+=2)
                {
                        rom[c]=getc(f);
                        rom[c+1]=getc(ff);
                }
                fclose(ff);
                fclose(f);
                return 1;
				
                case ROM_EPSON_PCAX:
                f  = romfopen("epson_pcax/EVAX", "rb");
                ff = romfopen("epson_pcax/ODAX",  "rb");
                if (!f || !ff) break;
                for (c = 0x0000; c < 0x8000; c += 2)
                {
                        rom[c]     = getc(f);
                        rom[c + 1] = getc(ff);
                }
                fclose(ff);
                fclose(f);
                biosmask = 0x7fff;
                return 1;

                case ROM_EPSON_PCAX2E:
                f = romfopen("epson_pcax2e/EVAXE", "rb");
                ff = romfopen("epson_pcax2e/ODAXE", "rb");
                if (!f || !ff) break;
                for (c = 0x0000; c < 0x10000; c += 2)
                {
                        rom[c] = getc(f);
                        rom[c+1] = getc(ff);
                }
                fclose(ff);
                fclose(f);
                return 1;
				
                case ROM_EPSON_PCAX3:
                f = romfopen("epson_pcax3/EVAX3", "rb");
                ff = romfopen("epson_pcax3/ODAX3", "rb");
                if (!f || !ff) break;
                for (c = 0x0000; c < 0x10000;c += 2)
                {
                        rom[c] = getc(f);
                        rom[c+1] = getc(ff);
                }
                fclose(ff);
                fclose(f);
                return 1;
				
                case ROM_T3100E:
                loadfont("t3100e/t3100e_font.bin", FONT_T3100E);
                f=romfopen("t3100e/t3100e.rom","rb");
                if (!f) break;
                romfread(rom,65536,1,f);
                fclose(f);
                return 1;

                case ROM_T1000:
                loadfont("t1000/t1000font.rom", FONT_CGA);
                f=romfopen("t1000/t1000.rom","rb");
                if (!f) break;
                romfread(rom, 0x8000,1,f);
		memcpy(rom + 0x8000, rom, 0x8000);
                biosmask = 0x7fff;
                fclose(f);
                return 1;

                case ROM_T1200:
                loadfont("t1200/t1000font.rom", FONT_CGA);
                f=romfopen("t1200/t1200_019e.ic15.bin","rb");
                if (!f) break;
                romfread(rom, 0x8000,1,f);
		memcpy(rom + 0x8000, rom, 0x8000);
                biosmask = 0x7fff;
                fclose(f);
                return 1;
				
		case ROM_PB_L300SX:
                f = romfopen("pb_l300sx/pb_l300sx.bin", "rb");
                if (!f) break;
                romfread(rom, 65536, 1, f);
                fclose(f);
                return 1;
				
                case ROM_NCR_PC4I:
                f=romfopen("ncr_pc4i/NCR_PC4i_BIOSROM_1985.BIN" ,"rb");
                if (!f) break;
                romfread(rom, 0x4000, 1, f);
                fclose(f);
                biosmask = 0x3fff;
                return 1;
				
                case ROM_TO16_PC:
                f=romfopen("to16_pc/TO16_103.bin","rb");
                if (!f) break;
                romfread(rom+0x8000,32768,1,f);
                fclose(f);
                return 1;
                
                case ROM_COMPAQ_PII:
                f  = romfopen("compaq_pii/109740-001.rom", "rb");
                ff = romfopen("compaq_pii/109739-001.rom", "rb");
                if (!f || !ff) break;
                for (c = 0x0000; c < 0x8000; c += 2)
                {
                         rom[c]     = getc(f);
                         rom[c + 1] = getc(ff);
                }
                fclose(ff);
                fclose(f);
                biosmask = 0x7fff;
                return 1;
                
                case ROM_ELX_PC425X:
                /*PC-425X uses a single ROM chip containing both main and video BIOSes.
                  First 32kb is video BIOS, next 32kb is blank, last 64kb is main BIOS.
                  Alternatively, seperate BIOS + video BIOS dumps are supported.*/
                f = romfopen("elx_pc425x/elx_pc425x.bin", "rb");
                if (!f)
                {
                        if (!rom_present("elx_pc425x/elx_pc425x_vbios.bin"))
                                break;
                        f = romfopen("elx_pc425x/elx_pc425x_bios.bin", "rb");
                        if (!f)
                                break;
                }
                else
                        fseek(f, 0x10000, SEEK_SET);
                romfread(rom, 65536, 1, f);
                fclose(f);
                return 1;

                case ROM_PB570:
                if (!rom_present("pb570/gd5430.bin"))
                        break;
                f = romfopen("pb570/1007by0r.bio", "rb");
                if (!f) break;
                fseek(f, 0x80, SEEK_SET);
                romfread(rom + 0x10000, 0x10000, 1, f);                
                fclose(f);
                f = romfopen("pb570/1007by0r.bi1", "rb");
                if (!f) break;
                fseek(f, 0x80, SEEK_SET);
                romfread(rom, 0xc000, 1, f);
                romfread(rom+0xc000, 0x1000, 1, f);
                fclose(f);
                biosmask = 0x1ffff;
                return 1;
                
                case ROM_ZAPPA:
                f = romfopen("zappa/1006bs0_.bio", "rb");
                if (!f) break;
                fseek(f, 0x80, SEEK_SET);
                romfread(rom + 0x10000, 0x10000, 1, f);
                fclose(f);
                f = romfopen("zappa/1006bs0_.bi1", "rb");
                if (!f) break;
                fseek(f, 0x80, SEEK_SET);
                romfread(rom, 0xd000, 1, f);
                fclose(f);
                biosmask = 0x1ffff;
                return 1;

                // keep separate from generic Zappa because there is a BIOS logo
                // in flash.bin
                case ROM_ITAUTEC_INFOWAYM:
//                if (!rom_present("infowaym/gd5434.bin"))
//                        break;
                f = romfopen("infowaym/1006bs0_.bio", "rb");
                if (!f) break;
                fseek(f, 0x80, SEEK_SET);
                romfread(rom + 0x10000, 0x10000, 1, f);
                fclose(f);
                f = romfopen("infowaym/1006bs0_.bi1", "rb");
                if (!f) break;
                fseek(f, 0x80, SEEK_SET);
                romfread(rom, 0xd000, 1, f);
                fclose(f);
                biosmask = 0x1ffff;
                return 1;

                case ROM_PB520R:
                if (!rom_present("pb520r/gd5434.bin"))
                        break;
                f = romfopen("pb520r/1009bc0r.bio", "rb");
                if (!f) break;
                fseek(f, 0x80, SEEK_SET);
                romfread(rom + 0x10000, 0x10000, 1, f);                
                fclose(f);
                f = romfopen("pb520r/1009bc0r.bi1", "rb");
                if (!f) break;
                fseek(f, 0x80, SEEK_SET);
                romfread(rom, 0xc000, 1, f);
                romfread(rom+0xc000, 0x1000, 1, f);
                fclose(f);
                biosmask = 0x1ffff;
                return 1;
				
                case ROM_COMPAQ_PIP:
                f=romfopen("compaq_pip/Compaq Portable Plus 100666-001 Rev C.bin","rb");
                if (!f) break;
                romfread(rom + 0xE000, 8192, 1, f);
                fclose(f);
                return 1;

                case ROM_XI8088:
                f = romfopen("xi8088/bios-xi8088.bin", "rb"); /* use the bios without xt-ide because it's configurable in pcem */
                if (!f) break;
                if (xi8088_bios_128kb())
                {
                        /* high bit is flipped in xi8088 */
                        romfread(rom + 0x10000, 0x10000, 1, f);
                        romfread(rom, 0x10000, 1, f);
                        biosmask = 0x1ffff;
                }
                else
                {
                        /* smaller bios, more UMBs */
                        romfread(rom, 0x10000, 1, f);
                }
                fclose(f);
                return 1;

                case ROM_IBMPS2_M70_TYPE3:
                f = romfopen("ibmps2_m70_type3/70-a_even.bin","rb");
                ff = romfopen("ibmps2_m70_type3/70-a_odd.bin","rb");
                if (!f || !ff)
                        break;
                for (c = 0x0000; c < 0x20000; c += 2)
                {
                        rom[c] = getc(f);
                        rom[c+1] = getc(ff);
                }
                fclose(ff);
                fclose(f);
                biosmask = 0x1ffff;
                return 1;

                case ROM_IBMPS2_M70_TYPE4:
                f = romfopen("ibmps2_m70_type4/70-b_even.bin","rb");
                ff = romfopen("ibmps2_m70_type4/70-b_odd.bin","rb");
                if (!f || !ff)
                        break;
                for (c = 0x0000; c < 0x20000; c += 2)
                {
                        rom[c] = getc(f);
                        rom[c+1] = getc(ff);
                }
                fclose(ff);
                fclose(f);
                biosmask = 0x1ffff;
                return 1;
								
                case ROM_TULIP_TC7:
                f  = romfopen("tulip_tc7/tc7be.bin", "rb");
                ff = romfopen("tulip_tc7/tc7bo.bin", "rb");
                if (!f || !ff) break;
                for (c = 0x0000; c < 0x8000; c += 2)
                {
                         rom[c]     = getc(f);
                         rom[c + 1] = getc(ff);
                }
                fclose(ff);
                fclose(f);
                biosmask = 0x7fff;
                return 1;
				
                case ROM_ZD_SUPERS:  /* [8088] Zenith Data Systems SupersPort */
                f=romfopen("zdsupers/z184m v3.1d.10d","rb");
                if (!f) break;
                romfread(rom, 32768, 1, f);
                fclose(f);
                biosmask = 0x7fff;
                return 1;

                case ROM_PB410A:
                f = romfopen("pb410a/PB410A.080337.4ABF.U25.bin","rb");
                if (!f) break;
                romfread(rom, 0x20000, 1, f);
                fclose(f);
                biosmask = 0x1ffff;
                return 1;
				
		case ROM_BULL_MICRAL_45:
		f = romfopen("bull_micral_45/even.fil", "rb");
                ff = romfopen("bull_micral_45/odd.fil", "rb");
                if (!f || !ff) break;
                for (c = 0x0000; c < 0x10000; c += 2)
                {
                        rom[c] = getc(f);
                        rom[c+1] = getc(ff);
                }
                fclose(ff);
                fclose(f);
                return 1;

                case ROM_FIC_VA503P:
                f = romfopen("fic_va503p/je4333.bin","rb");
                if (!f) break;
                romfread(rom, 0x20000, 1, f);
                fclose(f);
                biosmask = 0x1ffff;
                return 1;

                case ROM_CBM_SL386SX25:
                f = romfopen("cbm_sl386sx25/f000.rom", "rb");
                if (!f) break;
                romfread(rom, 0x10000, 1, f);
                fclose(f);
                return 1;

                case ROM_IBMPS1_2133_451:
                f = romfopen("ibmps1_2133/PS1_2133_52G2974_ROM.bin", "rb");
                if (!f) break;
                romfread(rom, 0x20000, 1, f);
                fclose(f);
                biosmask = 0x1ffff;
                return 1;

                case ROM_ECS_386_32:
		f = romfopen("ecs386_32/386_32_even.bin", "rb");
                ff = romfopen("ecs386_32/386_32_odd.bin", "rb");
                if (!f || !ff) break;
                for (c = 0x0000; c < 0x10000; c += 2)
                {
                        rom[c] = getc(f);
                        rom[c+1] = getc(ff);
                }
                fclose(ff);
                fclose(f);
                return 1;

                case ROM_LEDGE_MODELM:
                f = romfopen("leadingedge_modelm/Leading Edge - Model M - BIOS ROM - Version 4.71.bin","rb");
                if (!f) break;
                romfread(rom, 0x4000, 1, f);
                fclose(f);
                biosmask = 0x3fff;
                return 1;
        }
        printf("Failed to load ROM!\n");
        if (f) fclose(f);
        if (ff) fclose(ff);
        return 0;
}
