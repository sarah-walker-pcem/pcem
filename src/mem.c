/*MESS ROM notes :
        
        - pc2386 BIOS is corrupt (JMP at F000:FFF0 points to RAM)
        - pc2386 video BIOS is underdumped (16k instead of 24k)
        - c386sx16 BIOS fails checksum
*/

#include <stdlib.h>
#include <string.h>
#include "ibm.h"

#include "config.h"
#include "mem.h"
#include "video.h"
#include "x86.h"
#include "cpu.h"
#include "rom.h"

static uint8_t       (*_mem_read_b[0x40000])(uint32_t addr, void *priv);
static uint16_t      (*_mem_read_w[0x40000])(uint32_t addr, void *priv);
static uint32_t      (*_mem_read_l[0x40000])(uint32_t addr, void *priv);
static void         (*_mem_write_b[0x40000])(uint32_t addr, uint8_t  val, void *priv);
static void         (*_mem_write_w[0x40000])(uint32_t addr, uint16_t val, void *priv);
static void         (*_mem_write_l[0x40000])(uint32_t addr, uint32_t val, void *priv);
static void             *_mem_priv[0x40000];
static mem_mapping_t *_mem_mapping[0x40000];

static mem_mapping_t base_mapping;
static mem_mapping_t ram_low_mapping;
static mem_mapping_t ram_high_mapping;
static mem_mapping_t bios_mapping[8];
static mem_mapping_t vrom_mapping;
static mem_mapping_t romext_mapping;

int shadowbios,shadowbios_write;

static unsigned char isram[0x10000];

int mem_size;
int cache=4;
uint32_t biosmask;
int readlnum=0,writelnum=0;
int cachesize=256;

uint8_t *ram,*rom,*vram,*vrom;
uint8_t romext[32768];

static void mem_load_xtide_bios()
{
        FILE *f;
        f=romfopen("roms/ide_xt.bin","rb");

//        is486=0;
        if (f)
        {
                fread(romext,16384,1,f);
                fclose(f);
        }
}
        
static void mem_load_atide_bios()
{
        FILE *f;
        f=romfopen("roms/ide_at.bin","rb");

//        is486=0;
        if (f)
        {
                fread(romext,16384,1,f);
                fclose(f);
        }
}

int mem_load_video_bios()
{
        FILE *f = NULL;
        int c;
        switch (gfxcard)
        {
                case GFX_EGA:
                f=romfopen("roms/ibm_6277356_ega_card_u44_27128.bin","rb");
                if (f)
                {
                        pclog("Read EGA ROM!\n");
                        fread(vrom,16384,1,f);
                        if (vrom[0x3FFE]==0xAA && vrom[0x3FFF]==0x55)
                        {
                                pclog("Read EGA ROM in reverse\n");
                                fseek(f,0,SEEK_SET);
                                for (c=0x3FFF;c>=0;c--)
                                    vrom[c]=getc(f);
                        }
                        fclose(f);
                        return 1;
                }
                break;
                
                case GFX_TVGA:
                f=romfopen("roms/trident.bin","rb");
                if (f)
                {
                        fread(vrom,32768,1,f);
                        fclose(f);
                        return 1;
                }
                break;
                
                case GFX_ET4000:
                f=romfopen("roms/et4000.bin","rb");
                if (f)
                {
                        fread(vrom,32768,1,f);
                        fclose(f);
                        return 1;
                }
                break;

                case GFX_ET4000W32:
                f=romfopen("roms/et4000w32.bin","rb");
                if (f)
                {
                        fread(vrom,32768,1,f);
                        fclose(f);
                        return 1;
                }
                break;
                
                case GFX_BAHAMAS64:
                f=romfopen("roms/bahamas64.bin","rb");
                if (f)
                {
                        fread(vrom,32768,1,f);
                        fclose(f);
                        return 1;
                }
                break;
                
                case GFX_N9_9FX:
                f=romfopen("roms/s3_764.bin","rb");
                if (f)
                {
                        fread(vrom,32768,1,f);
                        fclose(f);
                        return 1;
                }
                break;
                
                case GFX_STEALTH64:
                f=romfopen("roms/TRIO64 (Ver. 1.5-07) [VGA] (S3 Incorporated).bin","rb");
                if (f)
                {
                        fread(vrom,32768,1,f);
                        fclose(f);
                        return 1;
                }
                break;
                
                case GFX_VIRGE:
                f=romfopen("roms/s3virge.bin","rb");
                if (f)
                {
                        fread(vrom,32768,1,f);
                        fclose(f);
                        return 1;
                }
                break;

                case GFX_TGUI9440:
                f=romfopen("roms/9440.vbi","rb");
                if (f)
                {
                        fread(vrom,32768,1,f);
                        fclose(f);
                        return 1;
                }
                break;
                
                case GFX_VGA:
                f=romfopen("roms/ibm_vga.bin","rb");
                if (f)
                {
                        fread(vrom,32768,1,f);
                        fclose(f);
                        return 1;
                }
                break;

                case GFX_VGAEDGE16:
                f=romfopen("roms/vgaedge16.vbi","rb");
                if (f)
                {
                        fread(vrom,32768,1,f);
                        fclose(f);
                        return 1;
                }
                break;

                case GFX_VGACHARGER:
                f=romfopen("roms/bios.bin","rb");
                if (f)
                {
                        fread(vrom,32768,1,f);
                        fclose(f);
                        return 1;
                }
                break;

                case GFX_OTI067:
                f=romfopen("roms/oti067/bios.bin","rb");
                if (f)
                {
                        fread(vrom,32768,1,f);
                        fclose(f);
                        return 1;
                }
                break;

                case GFX_MACH64GX:
                f=romfopen("roms/mach64gx/bios.bin","rb");
                if (f)
                {
                        fread(vrom,32768,1,f);
                        fclose(f);
                        return 1;
                }
                break;

                case GFX_CL_GD5429:
                f=romfopen("roms/5429.vbi","rb");
                if (f)
                {
                        fread(vrom,32768,1,f);
                        fclose(f);
                        return 1;
                }
                break;

                default:
                memset(vrom,0x63,0x8000);
                return 1;
        }
        pclog("mem_load_video_bios : failed to load video BIOS for gfxcard %i\n", gfxcard);
        memset(vrom,0x63,0x8000);
        return 0;
}
        
int loadbios()
{
        FILE *f=NULL,*ff=NULL;
        int c;
        
        loadfont("mda.rom", 0);
        
        biosmask = 0xffff;
        
        memset(romext,0x63,0x4000);
        
        pclog("Starting with romset %i\n", romset);
        
        switch (romset)
        {
                case ROM_PC1512:
                f=romfopen("roms/pc1512/40043.v1","rb");
                ff=romfopen("roms/pc1512/40044.v1","rb");
                if (!f || !ff) break;
                for (c=0xC000;c<0x10000;c+=2)
                {
                        rom[c]=getc(f);
                        rom[c+1]=getc(ff);
                }
                fclose(ff);
                fclose(f);
                memset(vrom,0x63,0x8000);
                mem_load_xtide_bios();
                loadfont("roms/pc1512/40078.ic127", 2);
                return 1;
                case ROM_PC1640:
                f=romfopen("roms/pc1640/40044.v3","rb");
                ff=romfopen("roms/pc1640/40043.v3","rb");
                if (!f || !ff) break;
                for (c=0xC000;c<0x10000;c+=2)
                {
                        rom[c]=getc(f);
                        rom[c+1]=getc(ff);
                }
                fclose(ff);
                fclose(f);
                f=romfopen("roms/pc1640/40100","rb");
                if (!f) break;
                fread(vrom,0x8000,1,f);
                fclose(f);
                mem_load_xtide_bios();
                return 1;
                case ROM_PC200:
                f=romfopen("roms/pc200/pc20v2.1","rb");
                ff=romfopen("roms/pc200/pc20v2.0","rb");
                if (!f || !ff) break;
                for (c=0xC000;c<0x10000;c+=2)
                {
                        rom[c]=getc(f);
                        rom[c+1]=getc(ff);
                }
                fclose(ff);
                fclose(f);
                memset(vrom,0x63,0x8000);
                mem_load_xtide_bios();
                loadfont("roms/pc200/40109.bin", 1);
                return 1;
                case ROM_TANDY:
                f=romfopen("roms/tandy/tandy1t1.020","rb");
                if (!f) break;
                fread(rom,65536,1,f);
                fclose(f);
                memset(vrom,0x63,0x8000);
                mem_load_xtide_bios();
                return 1;
/*                case ROM_IBMPCJR:
                f=fopen("pcjr/bios.rom","rb");
                fread(rom+0xE000,8192,1,f);
                fclose(f);
                f=fopen("pcjr/basic.rom","rb");
                fread(rom+0x6000,32768,1,f);
                fclose(f);
                break;*/
                case ROM_IBMXT:
                f=romfopen("roms/ibmxt/xt.rom","rb");
                if (!f)
                {
                        f=romfopen("roms/ibmxt/xt050986.0","rb");
                        ff=romfopen("roms/ibmxt/xt050986.1","rb");
                        if (!f || !ff) break;
                        for (c=0x0000;c<0x10000;c+=2)
                        {
                                rom[c]=getc(f);
                                rom[c+1]=getc(ff);
                        }
                        fclose(ff);
                        fclose(f);
                        mem_load_xtide_bios();
                        return 1;
                }
                else
                {
                        fread(rom,65536,1,f);
                        fclose(f);
                        mem_load_xtide_bios();
                        return 1;
                }
                break;
                case ROM_GENXT:
                f=romfopen("roms/genxt/pcxt.rom","rb");
                if (!f) break;
                fread(rom+0xE000,8192,1,f);
                fclose(f);
                mem_load_xtide_bios();
                return 1;
                case ROM_DTKXT:
                f=romfopen("roms/dtk/DTK_ERSO_2.42_2764.bin","rb");
                if (!f) break;
                fread(rom+0xE000,8192,1,f);
                fclose(f);
                mem_load_xtide_bios();
                return 1;
                case ROM_OLIM24:
                f  = romfopen("roms/olivetti_m24/olivetti_m24_version_1.43_low.bin" ,"rb");
                ff = romfopen("roms/olivetti_m24/olivetti_m24_version_1.43_high.bin","rb");
                if (!f || !ff) break;
                for (c = 0x0000; c < 0x4000; c += 2)
                {
                        rom[c + 0xc000] = getc(f);
                        rom[c + 0xc001] = getc(ff);
                }
                fclose(ff);
                fclose(f);
                mem_load_xtide_bios();
                return 1;
                        
                case ROM_PC2086:
                f  = romfopen("roms/pc2086/40179.ic129" ,"rb");
                ff = romfopen("roms/pc2086/40180.ic132","rb");
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
                f = romfopen("roms/pc2086/40186.ic171", "rb");
                if (!f) break;
                pclog("Loading VBIOS\n");
                fread(vrom, 32768, 1, f);
                fclose(f);
                mem_load_xtide_bios();
                pclog("%02X %02X %02X\n", rom[0xfff0], rom[0xfff1], rom[0xfff2]);
                biosmask = 0x3fff;
                return 1;

                case ROM_PC3086:
                f  = romfopen("roms/pc3086/fc00.bin" ,"rb");
                if (!f) break;
                fread(rom, 0x4000, 1, f);
                fclose(f);
                f = romfopen("roms/pc3086/c000.bin", "rb");
                if (!f) break;
                pclog("Loading VBIOS\n");
                fread(vrom, 32768, 1, f);
                fclose(f);
                mem_load_xtide_bios();
                pclog("%02X %02X %02X\n", rom[0xfff0], rom[0xfff1], rom[0xfff2]);
                biosmask = 0x3fff;                
                return 1;

                case ROM_IBMAT:
/*                f=romfopen("roms/AMIC206.BIN","rb");
                if (!f) break;
                fread(rom,65536,1,f);
                fclose(f);
                return 1;*/
                case ROM_IBMAT386:
                f=romfopen("roms/ibmat/at111585.0","rb");
                ff=romfopen("roms/ibmat/at111585.1","rb");
                if (!f || !ff) break;
                for (c=0x0000;c<0x10000;c+=2)
                {
                        rom[c]=getc(f);
                        rom[c+1]=getc(ff);
                }
                fclose(ff);
                fclose(f);
                mem_load_atide_bios();
                return 1;
                case ROM_CMDPC30:
                f  = romfopen("roms/cmdpc30/commodore pc 30 iii even.bin", "rb");
                ff = romfopen("roms/cmdpc30/commodore pc 30 iii odd.bin",  "rb");
                if (!f || !ff) break;
                for (c = 0x0000; c < 0x8000; c += 2)
                {
                        rom[c]     = getc(f);
                        rom[c + 1] = getc(ff);
                }
                fclose(ff);
                fclose(f);
                biosmask = 0x7fff;
                mem_load_atide_bios();
                return 1;
                case ROM_DELL200:
                f=romfopen("roms/dells200/dell0.bin","rb");
                ff=romfopen("roms/dells200/dell1.bin","rb");
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
                f=romfopen("roms/at386/at386.bin","rb");
                if (!f) break;
                fread(rom,65536,1,f);
                fclose(f);
                return 1;*/
                case ROM_AMI386: /*This uses the OPTi 82C495 chipset*/
//                f=romfopen("roms/at386/at386.bin","rb");
                f=romfopen("roms/ami386/ami386.bin","rb");
                if (!f) break;
                fread(rom,65536,1,f);
                fclose(f);
                return 1;


                case ROM_ACER386:
                f=romfopen("roms/acer386/acer386.bin","rb");
                if (!f) break;
                fread(rom,65536,1,f);
                fclose(f);
                rom[0xB0]=0xB0-0x51;
                rom[0x40d4]=0x51; /*PUSH CX*/
                f=romfopen("roms/acer386/oti067.bin","rb");
                if (!f) break;
                fread(vrom,0x8000,1,f);
                fclose(f);
                vrom[0x5D]=0x74;
                return 1;

                case ROM_AMI286:
                f=romfopen("roms/ami286/amic206.bin","rb");
                if (!f) break;
                fread(rom,65536,1,f);
                fclose(f);
//                memset(romext,0x63,0x8000);
                return 1;

                case ROM_EUROPC:
//                return 0;
                f=romfopen("roms/europc/50145","rb");
                if (!f) break;
                fread(rom+0x8000,32768,1,f);
                fclose(f);
//                memset(romext,0x63,0x8000);
                return 1;

                case ROM_IBMPC:
                f=romfopen("roms/ibmpc/pc102782.bin","rb");
                if (!f) break;
//                f=fopen("pc081682.bin","rb");
                fread(rom+0xE000,8192,1,f);
                fclose(f);
                f=romfopen("roms/ibmpc/basicc11.f6","rb");
                if (!f) return 1; /*I don't really care if BASIC is there or not*/
                fread(rom+0x6000,8192,1,f);
                fclose(f);
                f=romfopen("roms/ibmpc/basicc11.f8","rb");
                if (!f) break; /*But if some of it is there, then all of it must be*/
                fread(rom+0x8000,8192,1,f);
                fclose(f);
                f=romfopen("roms/ibmpc/basicc11.fa","rb");
                if (!f) break;
                fread(rom+0xA000,8192,1,f);
                fclose(f);
                f=romfopen("roms/ibmpc/basicc11.fc","rb");
                if (!f) break;
                fread(rom+0xC000,8192,1,f);
                fclose(f);
                mem_load_xtide_bios();
                return 1;

                case ROM_MEGAPC:
                        pclog("Loading MegaPC\n");
                f  = romfopen("roms/megapc/41651-bios lo.u18",  "rb");
                ff = romfopen("roms/megapc/211253-bios hi.u19", "rb");
                if (!f || !ff) break;
                for (c = 0x0000; c < 0x8000; c+=2)
                {
                        vrom[c]=getc(f);
                        vrom[c+1]=getc(ff);
                }
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
                f=romfopen("roms/ami486/ami486.BIN","rb");
                if (!f) break;
                fread(rom,65536,1,f);
                fclose(f);
                //is486=1;
                return 1;
                
                case ROM_WIN486:
//                f=romfopen("roms/win486/win486.bin","rb");
                f=romfopen("roms/win486/ALI1429G.AMW","rb");
                
                if (!f) break;
                fread(rom,65536,1,f);
                fclose(f);
                //is486=1;
                return 1;

                case ROM_PCI486:
                f=romfopen("roms/hot-433/hot-433.ami","rb");               
                if (!f) break;
                fread(rom,           0x20000, 1, f);                
                fclose(f);
                biosmask = 0x1ffff;
                //is486=1;
                return 1;

                case ROM_SIS496:
                f = romfopen("roms/sis496/SIS496-1.AWA", "rb");
                if (!f) break;
                fread(rom,           0x20000, 1, f);                
                fclose(f);
                biosmask = 0x1ffff;
                return 1;
                
                case ROM_430VX:
//                f = romfopen("roms/430vx/Ga586atv.bin", "rb");
//                f = fopen("roms/430vx/vx29.BIN", "rb");
                f = romfopen("roms/430vx/55XWUQ0E.BIN", "rb");
//                f=romfopen("roms/430vx/430vx","rb");               
                if (!f) break;
                fread(rom,           0x20000, 1, f);                
                fclose(f);
                biosmask = 0x1ffff;
                //is486=1;
                return 1;
        }
        printf("Failed to load ROM!\n");
        if (f) fclose(f);
        if (ff) fclose(ff);
        return 0;
}



uint32_t mmucache[0x100000];
int mmucaches[64];
int mmunext=0;
int abrt=0;

void initmmucache()
{
        int c;
//        if (output) pclog("initmmucache\n");
        memset(mmucache,0xFF,sizeof(mmucache));
        for (c=0;c<64;c++) mmucaches[c]=0xFFFFFFFF;
        mmunext=0;
}

void resetreadlookup()
{
        int c;
//        /*if (output) */pclog("resetreadlookup\n");
        memset(readlookup2,0xFF,1024*1024*4);
        for (c=0;c<256;c++) readlookup[c]=0xFFFFFFFF;
        readlnext=0;
        memset(writelookup2,0xFF,1024*1024*4);
        for (c=0;c<256;c++) writelookup[c]=0xFFFFFFFF;
        writelnext=0;
        pccache=0xFFFFFFFF;
//        readlnum=writelnum=0;

}

int mmuflush=0;
int mmu_perm=4;

void flushmmucache()
{
        int c;
//        /*if (output) */pclog("flushmmucache\n");
/*        for (c=0;c<16;c++)
        {
                if ( readlookup2[0xE0+c]!=0xFFFFFFFF) pclog("RL2 %02X = %08X\n",0xE0+c, readlookup2[0xE0+c]);
                if (writelookup2[0xE0+c]!=0xFFFFFFFF) pclog("WL2 %02X = %08X\n",0xE0+c,writelookup2[0xE0+c]);
        }*/
        for (c=0;c<256;c++)
        {
                if (readlookup[c]!=0xFFFFFFFF)
                {
                        readlookup2[readlookup[c]]=0xFFFFFFFF;
                        readlookup[c]=0xFFFFFFFF;
                }
                if (writelookup[c]!=0xFFFFFFFF)
                {
                        writelookup2[writelookup[c]]=0xFFFFFFFF;
                        writelookup[c]=0xFFFFFFFF;
                }
        }
        mmuflush++;
//        readlnum=writelnum=0;
        pccache=(uint32_t)0xFFFFFFFF;
        pccache2=(uint8_t *)0xFFFFFFFF;
        
//        memset(readlookup,0xFF,sizeof(readlookup));
//        memset(readlookup2,0xFF,1024*1024*4);
//        memset(writelookup,0xFF,sizeof(writelookup));
//        memset(writelookup2,0xFF,1024*1024*4);
/*        if (!(cr0>>31)) return;*/
        for (c=0;c<64;c++)
        {
                if (mmucaches[c]!=0xFFFFFFFF)
                {
                        mmucache[mmucaches[c]]=0xFFFFFFFF;
                        mmucaches[c]=0xFFFFFFFF;
                }
        }
//        pclog("Flush MMU cache\n");

/*        for (c = 0; c < 1024*1024; c++)
        {
                if (readlookup2[c] != 0xFFFFFFFF)
                {
                        pclog("Readlookup inconsistency - %05X %08X\n", c, readlookup2[c]);
                        dumpregs();
                        exit(-1);
                }
                if (writelookup2[c] != 0xFFFFFFFF)
                {
                        pclog("Readlookup inconsistency - %05X %08X\n", c, readlookup2[c]);
                        dumpregs();
                        exit(-1);
                }
        }*/
}

void flushmmucache_nopc()
{
        int c;
        for (c=0;c<256;c++)
        {
                if (readlookup[c]!=0xFFFFFFFF)
                {
                        readlookup2[readlookup[c]]=0xFFFFFFFF;
                        readlookup[c]=0xFFFFFFFF;
                }
                if (writelookup[c]!=0xFFFFFFFF)
                {
                        writelookup2[writelookup[c]]=0xFFFFFFFF;
                        writelookup[c]=0xFFFFFFFF;
                }
        }
        mmuflush++;
        for (c=0;c<64;c++)
        {
                if (mmucaches[c]!=0xFFFFFFFF)
                {
                        mmucache[mmucaches[c]]=0xFFFFFFFF;
                        mmucaches[c]=0xFFFFFFFF;
                }
        }
}

void flushmmucache_cr3()
{
        int c;
//        /*if (output) */pclog("flushmmucache_cr3\n");
        for (c=0;c<256;c++)
        {
                if (readlookup[c]!=0xFFFFFFFF)// && !readlookupp[c])
                {
                        readlookup2[readlookup[c]]=0xFFFFFFFF;
                        readlookup[c]=0xFFFFFFFF;
                }
                if (writelookup[c]!=0xFFFFFFFF)// && !writelookupp[c])
                {
                        writelookup2[writelookup[c]]=0xFFFFFFFF;
                        writelookup[c]=0xFFFFFFFF;
                }
        }
        for (c=0;c<64;c++)
        {
                if (mmucaches[c]!=0xFFFFFFFF)
                {
                        mmucache[mmucaches[c]]=0xFFFFFFFF;
                        mmucaches[c]=0xFFFFFFFF;
                }
        }

/*        for (c = 0; c < 1024*1024; c++)
        {
                if (readlookup2[c] != 0xFFFFFFFF)
                {
                        pclog("Readlookup inconsistency - %05X %08X\n", c, readlookup2[c]);
                        dumpregs();
                        exit(-1);
                }
                if (writelookup2[c] != 0xFFFFFFFF)
                {
                        pclog("Readlookup inconsistency - %05X %08X\n", c, readlookup2[c]);
                        dumpregs();
                        exit(-1);
                }
        }*/
}

extern int output;
#define mmutranslate(addr,rw) mmutranslatereal(addr,rw)
//#define mmutranslate(addr,rw) ((mmucache[addr>>12]!=0xFFFFFFFF)?(mmucache[addr>>12]+(addr&0xFFF)):mmutranslatereal(addr,rw))
int pctrans=0;

extern uint32_t testr[9];

uint32_t mmutranslatereal(uint32_t addr, int rw)
{
        uint32_t addr2;
        uint32_t temp,temp2,temp3;
                if (abrt) 
                {
//                        pclog("Translate recursive abort\n");
                        return -1;
                }
/*                if ((addr&~0xFFFFF)==0x77f00000) pclog("Do translate %08X %i  %08X  %08X\n",addr,rw,EAX,pc);
                if (addr==0x77f61000) output = 3;
                if (addr==0x77f62000) { dumpregs(); exit(-1); }
                if (addr==0x77f9a000) { dumpregs(); exit(-1); }*/
        addr2=(cr3+((addr>>20)&0xFFC));
        temp=temp2=((uint32_t *)ram)[addr2>>2];
//        if (output == 3) pclog("Do translate %08X %i %08X\n", addr, rw, temp);
        if (!(temp&1))// || (CPL==3 && !(temp&4) && !cpl_override) || (rw && !(temp&2) && (CPL==3 || cr0&WP_FLAG)))
        {
//                if (!nopageerrors) pclog("Section not present! %08X  %08X  %02X  %04X:%08X  %i %i\n",addr,temp,opcode,CS,pc,CPL,rw);

                cr2=addr;
                temp&=1;
                if (CPL==3) temp|=4;
                if (rw) temp|=2;
                abrt = ABRT_PF;
                abrt_error = temp;
/*                if (addr == 0x70046D)
                {
                        dumpregs();
                        exit(-1);
                }*/
                return -1;
        }
        temp=((uint32_t *)ram)[((temp&~0xFFF)+((addr>>10)&0xFFC))>>2];
        temp3=temp&temp2;
//        if (output == 3) pclog("Do translate %08X %08X\n", temp, temp3);
        if (!(temp&1) || (CPL==3 && !(temp3&4) && !cpl_override) || (rw && !(temp3&2) && (CPL==3 || cr0&WP_FLAG)))
        {
//                if (!nopageerrors) pclog("Page not present!  %08X   %08X   %02X %02X  %i  %08X  %04X:%08X  %04X:%08X %i  %i %i\n",addr,temp,opcode,opcode2,frame,rmdat32, CS,pc,SS,ESP,ins,CPL,rw);
               
//                dumpregs();
//                exit(-1);
//                if (addr == 0x815F6E90) output = 3;
/*                if (addr == 0x10ADE020) output = 3;*/
/*                if (addr == 0x10150010 && !nopageerrors)
                {
                        dumpregs();
                        exit(-1);
                }*/

                cr2=addr;
                temp&=1;
                if (CPL==3) temp|=4;
                if (rw) temp|=2;
                abrt = ABRT_PF;
                abrt_error = temp;
//                pclog("%04X\n",abrt);
                return -1;
        }
        mmu_perm=temp&4;
        ((uint32_t *)ram)[addr2>>2]|=0x20;
        ((uint32_t *)ram)[((temp2&~0xFFF)+((addr>>10)&0xFFC))>>2]|=(rw?0x60:0x20);
//        /*if (output) */pclog("Translate %08X %08X %08X  %08X:%08X  %08X\n",addr,(temp&~0xFFF)+(addr&0xFFF),temp,cs,pc,EDI);

        return (temp&~0xFFF)+(addr&0xFFF);
}

void mmu_invalidate(uint32_t addr)
{
//        readlookup2[addr >> 12] = writelookup2[addr >> 12] = 0xFFFFFFFF;
        flushmmucache_cr3();
}

int memspeed[11]={256,320,384,512,640,768,1024,1152,1280,1536,1920};
int memwaitstate;

static int cachelookup[256];
static uint8_t *cachelookup2;
static int cachelnext;

void addreadlookup(uint32_t virt, uint32_t phys)
{
//        return;
//        printf("Addreadlookup %08X %08X %08X %08X %08X %08X %02X %08X\n",virt,phys,cs,ds,es,ss,opcode,pc);
        if (virt == 0xffffffff)
                return;
                
        if (readlookup2[virt>>12]!=0xFFFFFFFF) 
        {
/*                if (readlookup2[virt>>12] != phys&~0xfff)
                {
                        pclog("addreadlookup mismatch - %05X000 %05X000\n", readlookup[readlnext], virt >> 12);
                        dumpregs();
                        exit(-1);
                }*/
                return;
        }
        
        
        if (!cachelookup2[phys >> 12])
        {
                readlnum++;
                cycles-=memwaitstate;
                if (cachelookup[cachelnext] != 0xffffffff)
                   cachelookup2[cachelookup[cachelnext]] = 0;
                cachelookup[cachelnext] = phys >> 12;
                cachelookup2[phys >> 12] = 1;
                cachelnext = (cachelnext + 1) & (cachesize - 1);
        }
        
        if (readlookup[readlnext]!=0xFFFFFFFF)
        {
                readlookup2[readlookup[readlnext]]=0xFFFFFFFF;
//                readlnum--;
        }
        readlookup2[virt>>12]=phys&~0xFFF;
        readlookupp[readlnext]=mmu_perm;
        readlookup[readlnext++]=virt>>12;
        readlnext&=(cachesize-1);
}

void addwritelookup(uint32_t virt, uint32_t phys)
{
//        return;
//        printf("Addwritelookup %08X %08X\n",virt,phys);
        if (virt == 0xffffffff)
                return;

        if (writelookup2[virt>>12]!=0xFFFFFFFF)
        {
/*                if (writelookup2[virt>>12] != phys&~0xfff)
                {
                        pclog("addwritelookup mismatch - %05X000 %05X000\n", readlookup[readlnext], virt >> 12);
                        dumpregs();
                        exit(-1);
                }*/
                return;
        }
        
        if (!cachelookup2[phys >> 12])
        {
                writelnum++;
                cycles-=memwaitstate;
                if (cachelookup[cachelnext] != 0xffffffff)
                   cachelookup2[cachelookup[cachelnext]] = 0;
                cachelookup[cachelnext] = phys >> 12;
                cachelookup2[phys >> 12] = 1;
                cachelnext = (cachelnext + 1) & (cachesize - 1);
        }
        
        cycles-=memwaitstate;
        if (writelookup[writelnext]!=0xFFFFFFFF)
        {
                writelookup2[writelookup[writelnext]]=0xFFFFFFFF;
//                writelnum--;
        }
        writelookup2[virt>>12]=phys&~0xFFF;
        writelookupp[writelnext]=mmu_perm;
        writelookup[writelnext++]=virt>>12;
        writelnext&=(cachesize-1);
}

#undef printf
uint8_t *getpccache(uint32_t a)
{
//        int logit=(a>0xFFFFF);
        uint32_t a2=a;
        //if (readlookup2[a>>12]!=0xFFFFFFFF) return &ram[(uint8_t *)readlookup2[a>>12] - (uint8_t *)(a&~0xFFF)];
        if (cr0>>31)
        {
//                if (output==3) pclog("Translate GetPCCache %08X\n",a);
pctrans=1;
                a=mmutranslate(a,0);
                pctrans=0;
//                if (output==3) pclog("GetPCCache output %08X\n",a);
                if (a==0xFFFFFFFF) return ram;
/*                {
                        printf("Bad getpccache %08X\n",a);
                        dumpregs();
                        exit(-1);
                }*/
        }
        a&=rammask;
        //if (output==3) pclog("Getpccache %08X %i\n",a,shadowbios);
        if (isram[a>>16])
        {
                //if (readlookup2[a>>12]!=0xFFFFFFFF) return &ram[readlookup2[a>>12]];
                if ((a>>16)!=0xF || shadowbios)
                   addreadlookup(a2,a);
//                if (a > 0xc0000000)
//                   printf("%016X %016X %016X\n", ((long)a&0xFFFFF000), ((long)a2&~0xFFF), (uint8_t *)(a&0xFFFFF000) - (uint8_t *)(a2&~0xFFF));
                return &ram[(uint8_t *)(a&0xFFFFF000) - (uint8_t *)(a2&~0xFFF)];
        }
//        if (logit) printf("PCCACHE - %08X -> %08X\n",a2,a);
        switch (a>>16)
        {
                case 0xC: if (a&0x8000) return &romext[(uint8_t *)(a&0x7000) - (uint8_t *)(a2&~0xFFF)];
                return &vrom[(uint8_t *)(a&0x7000) - (uint8_t *)(a2&~0xFFF)];
                case 0xE: if (shadowbios) return &ram[(uint8_t *)(a&0xFFF000) - (uint8_t *)(a2&~0xFFF)];
                return &rom[(uint8_t *)((a & biosmask) & ~ 0xfff) - (uint8_t *)(a2 & ~0xFFF)];
                case 0xF: if (shadowbios) return &ram[(uint8_t *)(a&0xFFF000) - (uint8_t *)(a2&~0xFFF)];
                return &rom[(uint8_t *)((a & biosmask) & ~ 0xfff) - (uint8_t *)(a2 & ~0xFFF)];
        }
        return &rom[(long)(a&0xF000) - (long)(a2&~0xFFF)];
                        /*printf("Bad getpccache %08X\n",a);
                        dumpregs();
                        exit(-1);*/
}
#define printf pclog

static uint32_t mem_logical_addr;
uint8_t readmembl(uint32_t addr)
{
        mem_logical_addr = addr;
        if (cr0 >> 31)
        {
                addr = mmutranslate(addr, 0);
                if (addr == 0xFFFFFFFF) return 0xFF;
        }
        addr &= rammask;

        if (_mem_read_b[addr >> 14]) return _mem_read_b[addr >> 14](addr, _mem_priv[addr >> 14]);
//        pclog("Bad readmembl %08X %04X:%08X\n", addr, CS, pc);
        return 0xFF;
}

void writemembl(uint32_t addr, uint8_t val)
{
        mem_logical_addr = addr;

        if (cr0 >> 31)
        {
                addr = mmutranslate(addr,1);
                if (addr == 0xFFFFFFFF) return;
        }
        addr &= rammask;

        if (_mem_write_b[addr >> 14]) _mem_write_b[addr >> 14](addr, val, _mem_priv[addr >> 14]);
//        else                          pclog("Bad write %08X %02X  %04X:%08X\n", addr, val, CS, pc);
}

uint8_t readmemb386l(uint32_t seg, uint32_t addr)
{
        if (seg==-1)
        {
                x86gpf("NULL segment", 0);
                printf("NULL segment! rb %04X(%08X):%08X %02X %08X\n",CS,cs,pc,opcode,addr);
                return -1;
        }
        mem_logical_addr = addr = addr + seg;
/*        if (readlookup2[mem_logical_addr >> 12] != 0xFFFFFFFF)
        {
                return ram[readlookup2[mem_logical_addr >> 12] + (mem_logical_addr & 0xFFF)];
        }*/
        
        if (cr0 >> 31)
        {
                addr = mmutranslate(addr, 0);
                if (addr == 0xFFFFFFFF) return 0xFF;
        }

        addr &= rammask;

        if (_mem_read_b[addr >> 14]) return _mem_read_b[addr >> 14](addr, _mem_priv[addr >> 14]);
//        pclog("Bad readmemb386l %08X %04X:%08X\n", addr, CS, pc);
        return 0xFF;
}

void writememb386l(uint32_t seg, uint32_t addr, uint8_t val)
{
        if (seg==-1)
        {
                x86gpf("NULL segment", 0);
                printf("NULL segment! wb %04X(%08X):%08X %02X %08X\n",CS,cs,pc,opcode,addr);
                return;
        }
        
        mem_logical_addr = addr = addr + seg;
        if (cr0 >> 31)
        {
                addr = mmutranslate(addr, 1);
                if (addr == 0xFFFFFFFF) return;
        }

        addr &= rammask;

/*        if (addr >= 0xa0000 && addr < 0xc0000)
           pclog("writemembl %08X %02X\n", addr, val);*/

        if (_mem_write_b[addr >> 14]) _mem_write_b[addr >> 14](addr, val, _mem_priv[addr >> 14]);
//        else                          pclog("Bad write %08X %02X %04X:%08X\n", addr, val, CS, pc);
}

uint16_t readmemwl(uint32_t seg, uint32_t addr)
{
        uint32_t addr2 = mem_logical_addr = seg + addr;
        if ((addr2&0xFFF)>0xFFE)
        {
                if (cr0>>31)
                {
                        if (mmutranslate(addr2,   0) == 0xffffffff) return 0xffff;
                        if (mmutranslate(addr2+1, 0) == 0xffffffff) return 0xffff;
                }
                if (is386) return readmemb386l(seg,addr)|(readmemb386l(seg,addr+1)<<8);
                else       return readmembl(seg+addr)|(readmembl(seg+addr+1)<<8);
        }
        if (seg==-1)
        {
                x86gpf("NULL segment", 0);
                printf("NULL segment! rw %04X(%08X):%08X %02X %08X\n",CS,cs,pc,opcode,addr);
                return -1;
        }
        if (cr0>>31)
        {
                addr2=mmutranslate(addr2,0);
                if (addr2==0xFFFFFFFF) return 0xFFFF;
        }

        addr2 &= rammask;

        if (_mem_read_w[addr2 >> 14]) return _mem_read_w[addr2 >> 14](addr2, _mem_priv[addr2 >> 14]);

        if (_mem_read_b[addr2 >> 14])
        {
                if (AT) return _mem_read_b[addr2 >> 14](addr2, _mem_priv[addr2 >> 14]) | (_mem_read_b[(addr2 + 1) >> 14](addr2 + 1, _mem_priv[addr2 >> 14]) << 8);
                else    return _mem_read_b[addr2 >> 14](addr2, _mem_priv[addr2 >> 14]) | (_mem_read_b[(seg + ((addr + 1) & 0xffff)) >> 14](seg + ((addr + 1) & 0xffff), _mem_priv[addr2 >> 14]) << 8);
        }
        return 0xffff;
}

void writememwl(uint32_t seg, uint32_t addr, uint16_t val)
{
        uint32_t addr2 = mem_logical_addr = seg + addr;
        if ((addr2&0xFFF)>0xFFE)
        {
                if (cr0>>31)
                {
                        if (mmutranslate(addr2,   1) == 0xffffffff) return;
                        if (mmutranslate(addr2+1, 1) == 0xffffffff) return;
                }
                if (is386)
                {
                        writememb386l(seg,addr,val);
                        writememb386l(seg,addr+1,val>>8);
                }
                else
                {
                        writemembl(seg+addr,val);
                        writemembl(seg+addr+1,val>>8);
                }
                return;
        }

        if (seg==-1)
        {
                x86gpf("NULL segment", 0);
                printf("NULL segment! ww %04X(%08X):%08X %02X %08X\n",CS,cs,pc,opcode,addr);
                return;
        }
        if (cr0>>31)
        {
                addr2=mmutranslate(addr2,1);
                if (addr2==0xFFFFFFFF) return;
        }
        
        addr2 &= rammask;

/*        if (addr2 >= 0xa0000 && addr2 < 0xc0000)
           pclog("writememwl %08X %02X\n", addr2, val);*/
        
        if (_mem_write_w[addr2 >> 14]) 
        {
                _mem_write_w[addr2 >> 14](addr2, val, _mem_priv[addr2 >> 14]);
                return;
        }

        if (_mem_write_b[addr2 >> 14]) 
        {
                _mem_write_b[addr2 >> 14](addr2, val, _mem_priv[addr2 >> 14]);
                _mem_write_b[(addr2 + 1) >> 14](addr2 + 1, val >> 8, _mem_priv[addr2 >> 14]);
                return;
        }
//        pclog("Bad write %08X %04X\n", addr2, val);
}

uint32_t readmemll(uint32_t seg, uint32_t addr)
{
        uint32_t addr2 = mem_logical_addr = seg + addr;
        if ((addr2&0xFFF)>0xFFC)
        {
                if (cr0>>31)
                {
                        if (mmutranslate(addr2,   0) == 0xffffffff) return 0xffffffff;
                        if (mmutranslate(addr2+3, 0) == 0xffffffff) return 0xffffffff;
                }
                return readmemwl(seg,addr)|(readmemwl(seg,addr+2)<<16);
        }

        if (seg==-1)
        {
                x86gpf("NULL segment", 0);
                printf("NULL segment! rl %04X(%08X):%08X %02X %08X\n",CS,cs,pc,opcode,addr);
                return -1;
        }
        
        if (cr0>>31)
        {
                addr2=mmutranslate(addr2,0);
                if (addr2==0xFFFFFFFF) return 0xFFFFFFFF;
        }

        addr2&=rammask;

        if (_mem_read_l[addr2 >> 14]) return _mem_read_l[addr2 >> 14](addr2, _mem_priv[addr2 >> 14]);

        if (_mem_read_w[addr2 >> 14]) return _mem_read_w[addr2 >> 14](addr2, _mem_priv[addr2 >> 14]) | (_mem_read_w[addr2 >> 14](addr2 + 2, _mem_priv[addr2 >> 14]) << 16);
        
        if (_mem_read_b[addr2 >> 14]) return _mem_read_b[addr2 >> 14](addr2, _mem_priv[addr2 >> 14]) | (_mem_read_b[addr2 >> 14](addr2 + 1, _mem_priv[addr2 >> 14]) << 8) | (_mem_read_b[addr2 >> 14](addr2 + 2, _mem_priv[addr2 >> 14]) << 16) | (_mem_read_b[addr2 >> 14](addr2 + 3, _mem_priv[addr2 >> 14]) << 24);

        return 0xffffffff;
}

void writememll(uint32_t seg, uint32_t addr, uint32_t val)
{
        uint32_t addr2 = mem_logical_addr = seg + addr;

        if ((addr2&0xFFF)>0xFFC)
        {
                if (cr0>>31)
                {
                        if (mmutranslate(addr2,   1) == 0xffffffff) return;
                        if (mmutranslate(addr2+3, 1) == 0xffffffff) return;
                }
                writememwl(seg,addr,val);
                writememwl(seg,addr+2,val>>16);
                return;
        }
        if (seg==-1)
        {
                x86gpf("NULL segment", 0);
                printf("NULL segment! wl %04X(%08X):%08X %02X %08X\n",CS,cs,pc,opcode,addr);
                return;
        }
        if (cr0>>31)
        {
                addr2=mmutranslate(addr2,1);
                if (addr2==0xFFFFFFFF) return;
        }
        
        addr2&=rammask;

/*        if (addr >= 0xa0000 && addr < 0xc0000)
           pclog("writememll %08X %08X\n", addr, val);*/

        if (_mem_write_l[addr2 >> 14]) 
        {
                _mem_write_l[addr2 >> 14](addr2, val,           _mem_priv[addr2 >> 14]);
                return;
        }
        if (_mem_write_w[addr2 >> 14]) 
        {
                _mem_write_w[addr2 >> 14](addr2,     val,       _mem_priv[addr2 >> 14]);
                _mem_write_w[addr2 >> 14](addr2 + 2, val >> 16, _mem_priv[addr2 >> 14]);
                return;
        }
        if (_mem_write_b[addr2 >> 14]) 
        {
                _mem_write_b[addr2 >> 14](addr2,     val,       _mem_priv[addr2 >> 14]);
                _mem_write_b[addr2 >> 14](addr2 + 1, val >> 8,  _mem_priv[addr2 >> 14]);
                _mem_write_b[addr2 >> 14](addr2 + 2, val >> 16, _mem_priv[addr2 >> 14]);
                _mem_write_b[addr2 >> 14](addr2 + 3, val >> 24, _mem_priv[addr2 >> 14]);
                return;
        }
//        pclog("Bad write %08X %08X\n", addr2, val);
}

uint8_t mem_readb_phys(uint32_t addr)
{
        mem_logical_addr = 0xffffffff;
        
        if (_mem_read_b[addr >> 14]) 
                return _mem_read_b[addr >> 14](addr, _mem_priv[addr >> 14]);
                
        return 0xff;
}

void mem_writeb_phys(uint32_t addr, uint8_t val)
{
        mem_logical_addr = 0xffffffff;
        
        if (_mem_write_b[addr >> 14]) 
                _mem_write_b[addr >> 14](addr, val, _mem_priv[addr >> 14]);
}

uint8_t mem_read_ram(uint32_t addr, void *priv)
{
//        if (addr >= 0xe0000 && addr < 0x100000) pclog("Read RAMb %08X\n", addr);
        addreadlookup(mem_logical_addr, addr);
        return ram[addr];
}
uint16_t mem_read_ramw(uint32_t addr, void *priv)
{
//        if (addr >= 0xe0000 && addr < 0x100000)  pclog("Read RAMw %08X\n", addr);
        addreadlookup(mem_logical_addr, addr);
        return *(uint16_t *)&ram[addr];
}
uint32_t mem_read_raml(uint32_t addr, void *priv)
{
//        if (addr >= 0xe0000 && addr < 0x100000) pclog("Read RAMl %08X\n", addr);
        addreadlookup(mem_logical_addr, addr);
        return *(uint32_t *)&ram[addr];
}

void mem_write_ram(uint32_t addr, uint8_t val, void *priv)
{
        addwritelookup(mem_logical_addr, addr);
        ram[addr] = val;
//        if (addr >= 0xe0000 && addr < 0x100000) pclog("Write RAMb %08X\n", addr);
}
void mem_write_ramw(uint32_t addr, uint16_t val, void *priv)
{
        addwritelookup(mem_logical_addr, addr);
        *(uint16_t *)&ram[addr] = val;
//        if (addr >= 0xe0000 && addr < 0x100000)  pclog("Write RAMw %08X %04X:%04X %i %04X  %04X:%04X  %04X:%04X\n", addr, CS, pc, ins, val, DS,SI, ES,DI);
}
void mem_write_raml(uint32_t addr, uint32_t val, void *priv)
{
        addwritelookup(mem_logical_addr, addr);
        *(uint32_t *)&ram[addr] = val;
//        if (addr >= 0xe0000 && addr < 0x100000) pclog("Write RAMl %08X  %04X:%04X  %04X:%04X  %04X:%04X\n", addr, CS, pc, DS,SI, ES,DI);
}

uint8_t mem_read_bios(uint32_t addr, void *priv)
{
                        if (AMIBIOS && (addr&0xFFFFF)==0xF8281) /*This is read constantly during AMIBIOS POST, but is never written to. It's clearly a status register of some kind, but for what?*/
                        {
//                                pclog("Read magic addr %04X(%06X):%04X\n",CS,cs,pc);
//                                if (pc==0x547D) output=3;
                                return 0x40;
                        }
//        pclog("Read BIOS %08X %02X %04X:%04X\n", addr, rom[addr & biosmask], CS, pc);
        return rom[addr & biosmask];
}
uint16_t mem_read_biosw(uint32_t addr, void *priv)
{
//        pclog("Read BIOS %08X %04X %04X:%04X\n", addr, *(uint16_t *)&rom[addr & biosmask], CS, pc);
        return *(uint16_t *)&rom[addr & biosmask];
}
uint32_t mem_read_biosl(uint32_t addr, void *priv)
{
//        pclog("Read BIOS %08X %02X %04X:%04X\n", addr, *(uint32_t *)&rom[addr & biosmask], CS, pc);
        return *(uint32_t *)&rom[addr & biosmask];
}

uint8_t mem_read_vrom(uint32_t addr, void *priv)
{
        return vrom[addr & 0x7fff];
}
uint16_t mem_read_vromw(uint32_t addr, void *priv)
{
        return *(uint16_t *)&vrom[addr & 0x7fff];
}
uint32_t mem_read_vroml(uint32_t addr, void *priv)
{
        return *(uint32_t *)&vrom[addr & 0x7fff];
}

uint8_t mem_read_romext(uint32_t addr, void *priv)
{
        return romext[addr & 0x7fff];
}
uint16_t mem_read_romextw(uint32_t addr, void *priv)
{
        return *(uint16_t *)&romext[addr & 0x7fff];
}
uint32_t mem_read_romextl(uint32_t addr, void *priv)
{
        return *(uint32_t *)&romext[addr & 0x7fff];
}


void mem_updatecache()
{
        flushmmucache();
        if (!is386)
        {
                cachesize=256;
                memwaitstate=0;
                return;
        }
        if (cpu_16bitbus)
           memwaitstate = 512 * cpu_multi;
        else
           memwaitstate = 384 * cpu_multi; //memspeed[cpuspeed];        
        switch (cache)
        {
                case 0: cachesize=32; break;
                case 1: cachesize=64; break;
                case 2: cachesize=128; break;
                case 3: cachesize=256; break;
                case 4: cachesize=256; memwaitstate=0; break;
        }
}

static void mem_mapping_recalc(uint32_t base, uint32_t size)
{
        uint32_t c;
        mem_mapping_t *mapping = base_mapping.next;
        
        if ((base + size) > 0xffff8000)
           size = 0xffff0000 - base;
        
        /*Clear out old mappings*/
        for (c = base; c < base + size; c += 0x4000)
        {
                _mem_read_b[c >> 14] = NULL;
                _mem_read_w[c >> 14] = NULL;
                _mem_read_l[c >> 14] = NULL;
                _mem_write_b[c >> 14] = NULL;
                _mem_write_w[c >> 14] = NULL;
                _mem_write_l[c >> 14] = NULL;
                _mem_priv[c >> 14] = NULL;
                _mem_mapping[c >> 14] = NULL;
        }
//        pclog("mem_mapping_recalc %08X %08X\n", base, size);
        /*Walk mapping list*/
        while (mapping != NULL)
        {
//                pclog("mapping=%p next=%p base=%08X size=%08X enable=%i\n", mapping, mapping->next, mapping->base, mapping->size, mapping->enable);
                /*In range?*/
                if (mapping->enable && mapping->base < (base + size) && (mapping->base + mapping->size) >= base)
                {
                        uint32_t start = (mapping->base < base) ? mapping->base : base;
                        uint32_t end   = ((mapping->base + mapping->size) < (base + size)) ? (mapping->base + mapping->size) : (base + size);

                        for (c = start; c < end; c += 0x4000)
                        {
                                if (_mem_mapping[c >> 14] == NULL)
                                {
//                                        pclog(" Add %08X\n", c);
                                        _mem_read_b[c >> 14] = mapping->read_b;
                                        _mem_read_w[c >> 14] = mapping->read_w;
                                        _mem_read_l[c >> 14] = mapping->read_l;
                                        _mem_write_b[c >> 14] = mapping->write_b;
                                        _mem_write_w[c >> 14] = mapping->write_w;
                                        _mem_write_l[c >> 14] = mapping->write_l;
                                        _mem_priv[c >> 14] = mapping->p;
                                        _mem_mapping[c >> 14] = mapping;
                                }
                        }
                }
                mapping = mapping->next;
        }       
}

void mem_mapping_add(mem_mapping_t *mapping,
                    uint32_t base, 
                    uint32_t size, 
                    uint8_t  (*read_b)(uint32_t addr, void *p),
                    uint16_t (*read_w)(uint32_t addr, void *p),
                    uint32_t (*read_l)(uint32_t addr, void *p),
                    void (*write_b)(uint32_t addr, uint8_t  val, void *p),
                    void (*write_w)(uint32_t addr, uint16_t val, void *p),
                    void (*write_l)(uint32_t addr, uint32_t val, void *p),
                    void *p)
{
        mem_mapping_t *dest = &base_mapping;

        /*Add mapping to the end of the list*/
        while (dest->next)
                dest = dest->next;        
        dest->next = mapping;
        
        if (size)
                mapping->enable  = 1;
        else
                mapping->enable  = 0;
        mapping->base    = base;
        mapping->size    = size;
        mapping->read_b  = read_b;
        mapping->read_w  = read_w;
        mapping->read_l  = read_l;
        mapping->write_b = write_b;
        mapping->write_w = write_w;
        mapping->write_l = write_l;
        mapping->p       = p;
        mapping->next    = NULL;
        
        mem_mapping_recalc(mapping->base, mapping->size);
}

void mem_mapping_set_handler(mem_mapping_t *mapping,
                    uint8_t  (*read_b)(uint32_t addr, void *p),
                    uint16_t (*read_w)(uint32_t addr, void *p),
                    uint32_t (*read_l)(uint32_t addr, void *p),
                    void (*write_b)(uint32_t addr, uint8_t  val, void *p),
                    void (*write_w)(uint32_t addr, uint16_t val, void *p),
                    void (*write_l)(uint32_t addr, uint32_t val, void *p))
{
        mapping->read_b  = read_b;
        mapping->read_w  = read_w;
        mapping->read_l  = read_l;
        mapping->write_b = write_b;
        mapping->write_w = write_w;
        mapping->write_l = write_l;
        
        mem_mapping_recalc(mapping->base, mapping->size);
}

void mem_mapping_set_addr(mem_mapping_t *mapping, uint32_t base, uint32_t size)
{
        /*Remove old mapping*/
        mapping->enable = 0;
        mem_mapping_recalc(mapping->base, mapping->size);
        
        /*Set new mapping*/
        mapping->enable = 1;
        mapping->base = base;
        mapping->size = size;
        
        mem_mapping_recalc(mapping->base, mapping->size);
}

void mem_mapping_set_p(mem_mapping_t *mapping, void *p)
{
        mapping->p = p;
}

void mem_mapping_disable(mem_mapping_t *mapping)
{
        mapping->enable = 0;
        
        mem_mapping_recalc(mapping->base, mapping->size);
}

void mem_mapping_enable(mem_mapping_t *mapping)
{
        mapping->enable = 1;
        
        mem_mapping_recalc(mapping->base, mapping->size);
}

void mem_init()
{
        int c;

        ram = malloc(mem_size * 1024 * 1024);
        rom = malloc(0x20000);
        vram = malloc(0x800000);
        vrom = malloc(0x8000);
        readlookup2  = malloc(1024 * 1024 * 4);
        writelookup2 = malloc(1024 * 1024 * 4);
        cachelookup2 = malloc(1024 * 1024);
        biosmask = 0xffff;

        memset(ram, 0, mem_size * 1024 * 1024);

        memset(isram, 0, sizeof(isram));
        for (c = 0; c < (mem_size * 16); c++)
        {
                isram[c] = 1;
                if (c >= 0xa && c <= 0xf) 
                        isram[c] = 0;
        }

        memset(_mem_read_b,  0, sizeof(_mem_read_b));
        memset(_mem_read_w,  0, sizeof(_mem_read_w));
        memset(_mem_read_l,  0, sizeof(_mem_read_l));
        memset(_mem_write_b, 0, sizeof(_mem_write_b));
        memset(_mem_write_w, 0, sizeof(_mem_write_w));
        memset(_mem_write_l, 0, sizeof(_mem_write_l));

        memset(&base_mapping, 0, sizeof(base_mapping));

        mem_mapping_add(&ram_low_mapping, 0x00000, 0xa0000, mem_read_ram,    mem_read_ramw,    mem_read_raml,    mem_write_ram, mem_write_ramw, mem_write_raml,   NULL);
        if (mem_size > 1)
                mem_mapping_add(&ram_low_mapping, 0x100000, (mem_size - 1) * 1024 * 1024, mem_read_ram,    mem_read_ramw,    mem_read_raml,    mem_write_ram, mem_write_ramw, mem_write_raml,   NULL);

        mem_mapping_add(&bios_mapping[0], 0xe0000, 0x04000, mem_read_bios,   mem_read_biosw,   mem_read_biosl,   mem_write_ram, mem_write_ramw, mem_write_raml,   NULL);
        mem_mapping_add(&bios_mapping[1], 0xe4000, 0x04000, mem_read_bios,   mem_read_biosw,   mem_read_biosl,   mem_write_ram, mem_write_ramw, mem_write_raml,   NULL);
        mem_mapping_add(&bios_mapping[2], 0xe8000, 0x04000, mem_read_bios,   mem_read_biosw,   mem_read_biosl,   mem_write_ram, mem_write_ramw, mem_write_raml,   NULL);
        mem_mapping_add(&bios_mapping[3], 0xec000, 0x04000, mem_read_bios,   mem_read_biosw,   mem_read_biosl,   mem_write_ram, mem_write_ramw, mem_write_raml,   NULL);
        mem_mapping_add(&bios_mapping[4], 0xf0000, 0x04000, mem_read_bios,   mem_read_biosw,   mem_read_biosl,   mem_write_ram, mem_write_ramw, mem_write_raml,   NULL);
        mem_mapping_add(&bios_mapping[5], 0xf4000, 0x04000, mem_read_bios,   mem_read_biosw,   mem_read_biosl,   mem_write_ram, mem_write_ramw, mem_write_raml,   NULL);
        mem_mapping_add(&bios_mapping[6], 0xf8000, 0x04000, mem_read_bios,   mem_read_biosw,   mem_read_biosl,   mem_write_ram, mem_write_ramw, mem_write_raml,   NULL);
        mem_mapping_add(&bios_mapping[7], 0xfc000, 0x04000, mem_read_bios,   mem_read_biosw,   mem_read_biosl,   mem_write_ram, mem_write_ramw, mem_write_raml,   NULL);

        mem_mapping_add(&vrom_mapping,    0xc0000, 0x08000, mem_read_vrom,   mem_read_vromw,   mem_read_vroml,   NULL, NULL, NULL,   NULL);
        mem_mapping_add(&romext_mapping,  0xc8000, 0x08000, mem_read_romext, mem_read_romextw, mem_read_romextl, NULL, NULL, NULL,   NULL);
//        pclog("Mem resize %i %i\n",mem_size,c);
}

void mem_resize()
{
        int c;
        
        free(ram);
        ram = malloc(mem_size * 1024 * 1024);
        memset(ram, 0, mem_size * 1024 * 1024);
        
        memset(isram, 0, sizeof(isram));
        for (c = 0; c < (mem_size * 16); c++)
        {
                isram[c] = 1;
                if (c >= 0xa && c <= 0xf) 
                        isram[c] = 0;
        }

        memset(_mem_read_b,  0, sizeof(_mem_read_b));
        memset(_mem_read_w,  0, sizeof(_mem_read_w));
        memset(_mem_read_l,  0, sizeof(_mem_read_l));
        memset(_mem_write_b, 0, sizeof(_mem_write_b));
        memset(_mem_write_w, 0, sizeof(_mem_write_w));
        memset(_mem_write_l, 0, sizeof(_mem_write_l));

        memset(&base_mapping, 0, sizeof(base_mapping));

        mem_mapping_add(&ram_low_mapping, 0x00000, 0xa0000, mem_read_ram,    mem_read_ramw,    mem_read_raml,    mem_write_ram, mem_write_ramw, mem_write_raml,   NULL);
        if (mem_size > 1)
                mem_mapping_add(&ram_low_mapping, 0x100000, (mem_size - 1) * 1024 * 1024, mem_read_ram,    mem_read_ramw,    mem_read_raml,    mem_write_ram, mem_write_ramw, mem_write_raml,   NULL);
           
        mem_mapping_add(&bios_mapping[0], 0xe0000, 0x04000, mem_read_bios,   mem_read_biosw,   mem_read_biosl,   mem_write_ram, mem_write_ramw, mem_write_raml,   NULL);
        mem_mapping_add(&bios_mapping[1], 0xe4000, 0x04000, mem_read_bios,   mem_read_biosw,   mem_read_biosl,   mem_write_ram, mem_write_ramw, mem_write_raml,   NULL);
        mem_mapping_add(&bios_mapping[2], 0xe8000, 0x04000, mem_read_bios,   mem_read_biosw,   mem_read_biosl,   mem_write_ram, mem_write_ramw, mem_write_raml,   NULL);
        mem_mapping_add(&bios_mapping[3], 0xec000, 0x04000, mem_read_bios,   mem_read_biosw,   mem_read_biosl,   mem_write_ram, mem_write_ramw, mem_write_raml,   NULL);
        mem_mapping_add(&bios_mapping[4], 0xf0000, 0x04000, mem_read_bios,   mem_read_biosw,   mem_read_biosl,   mem_write_ram, mem_write_ramw, mem_write_raml,   NULL);
        mem_mapping_add(&bios_mapping[5], 0xf4000, 0x04000, mem_read_bios,   mem_read_biosw,   mem_read_biosl,   mem_write_ram, mem_write_ramw, mem_write_raml,   NULL);
        mem_mapping_add(&bios_mapping[6], 0xf8000, 0x04000, mem_read_bios,   mem_read_biosw,   mem_read_biosl,   mem_write_ram, mem_write_ramw, mem_write_raml,   NULL);
        mem_mapping_add(&bios_mapping[7], 0xfc000, 0x04000, mem_read_bios,   mem_read_biosw,   mem_read_biosl,   mem_write_ram, mem_write_ramw, mem_write_raml,   NULL);

        mem_mapping_add(&vrom_mapping,    0xc0000, 0x08000, mem_read_vrom,   mem_read_vromw,   mem_read_vroml,   NULL, NULL, NULL,   NULL);
        mem_mapping_add(&romext_mapping,  0xc8000, 0x08000, mem_read_romext, mem_read_romextw, mem_read_romextl, NULL, NULL, NULL,   NULL);

//        pclog("Mem resize %i %i\n",mem_size,c);
}

void mem_bios_set_state(uint32_t base, uint32_t size, int read_ram, int write_ram)
{
        uint32_t c;
        int state = (read_ram ? 1 : 0) | (write_ram ? 2: 0);
        
        if (base < 0xe0000 || base > 0xfffff)
                return;
        if ((base + size) > 0x100000)
                size = 0x100000 - base;
        
        for (c = base; c < (base + size); c+= 0x4000)
        {
//                pclog("mem_bios_set_state: c=%08X base=%08X size=%08X bios=%i\n", c, base, size, (c - 0xe0000) >> 14);
                switch (state)
                {
                        case 0:
                        mem_mapping_set_handler(&bios_mapping[(c - 0xe0000) >> 14], mem_read_bios, mem_read_biosw, mem_read_biosl, NULL, NULL, NULL);
                        break;
                        case 1:
                        mem_mapping_set_handler(&bios_mapping[(c - 0xe0000) >> 14], mem_read_ram,  mem_read_ramw,  mem_read_raml,  NULL, NULL, NULL);
                        break;
                        case 2:
                        mem_mapping_set_handler(&bios_mapping[(c - 0xe0000) >> 14], mem_read_bios, mem_read_biosw, mem_read_biosl, mem_write_ram, mem_write_ramw, mem_write_raml);
                        break;
                        case 3:
                        mem_mapping_set_handler(&bios_mapping[(c - 0xe0000) >> 14], mem_read_ram,  mem_read_ramw,  mem_read_raml,  mem_write_ram, mem_write_ramw, mem_write_raml);
                        break;
                }
        }
}

int mem_a20_key = 0, mem_a20_alt = 0;
static int mem_a20_state = 0;

void mem_a20_recalc()
{
        int state = mem_a20_key | mem_a20_alt;
//        pclog("A20 recalc %i %i\n", state, mem_a20_state);
        if (state && !mem_a20_state)
        {
                rammask = 0xffffffff;
                flushmmucache();
        }
        else if (!state && mem_a20_state)
        {
                rammask = 0xffefffff;
                flushmmucache();
        }
//        pclog("rammask now %08X\n", rammask);
        mem_a20_state = state;
}
