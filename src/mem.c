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
#include "x86_ops.h"
#include "codegen.h"
#include "xi8088.h"

page_t *pages;
page_t **page_lookup;

static mem_mapping_t  *read_mapping[0x40000];
static mem_mapping_t *write_mapping[0x40000];
static uint8_t           *_mem_exec[0x40000];
static uint8_t           _mem_state[0x40000];

static mem_mapping_t base_mapping;
mem_mapping_t ram_low_mapping;
mem_mapping_t ram_high_mapping;
mem_mapping_t ram_mid_mapping;
mem_mapping_t ram_remapped_mapping;
mem_mapping_t bios_mapping[8];
mem_mapping_t bios_high_mapping[8];
static mem_mapping_t romext_mapping;

static uint8_t ff_array[0x1000];

int mem_size;
uint32_t biosmask;
int readlnum=0,writelnum=0;
int cachesize=256;

uint8_t *ram, *rom = NULL;
uint8_t romext[32768];

uint64_t *byte_dirty_mask;
uint64_t *byte_code_present_mask;

uint32_t mem_logical_addr;

int mmuflush=0;
int mmu_perm=4;

int mem_addr_is_ram(uint32_t addr)
{
        mem_mapping_t *mapping = read_mapping[addr >> 14];
        
        return (mapping == &ram_low_mapping) || (mapping == &ram_high_mapping) || (mapping == &ram_mid_mapping) || (mapping == &ram_remapped_mapping);
}

void resetreadlookup()
{
        int c;
//        /*if (output) */pclog("resetreadlookup\n");
        memset(readlookup2,0xFF,1024*1024*sizeof(uintptr_t));
        for (c=0;c<256;c++) readlookup[c]=0xFFFFFFFF;
        readlnext=0;
        memset(writelookup2,0xFF,1024*1024*sizeof(uintptr_t));
        memset(page_lookup, 0, (1 << 20) * sizeof(page_t *));
        for (c=0;c<256;c++) writelookup[c]=0xFFFFFFFF;
        writelnext=0;
        pccache=0xFFFFFFFF;
//        readlnum=writelnum=0;

}

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
                        readlookup2[readlookup[c]] = -1;
                        readlookup[c]=0xFFFFFFFF;
                }
                if (writelookup[c] != 0xFFFFFFFF)
                {
                        page_lookup[writelookup[c]] = NULL;
                        writelookup2[writelookup[c]] = -1;
                        writelookup[c] = 0xFFFFFFFF;
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
        codegen_flush();
}

void flushmmucache_nopc()
{
        int c;
        for (c=0;c<256;c++)
        {
                if (readlookup[c]!=0xFFFFFFFF)
                {
                        readlookup2[readlookup[c]] = -1;
                        readlookup[c]=0xFFFFFFFF;
                }
                if (writelookup[c] != 0xFFFFFFFF)
                {
                        page_lookup[writelookup[c]] = NULL;
                        writelookup2[writelookup[c]] = -1;
                        writelookup[c] = 0xFFFFFFFF;
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
                        readlookup2[readlookup[c]] = -1;
                        readlookup[c]=0xFFFFFFFF;
                }
                if (writelookup[c] != 0xFFFFFFFF)// && !writelookupp[c])
                {
                        page_lookup[writelookup[c]] = NULL;
                        writelookup2[writelookup[c]] = -1;
                        writelookup[c] = 0xFFFFFFFF;                        
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

void mem_flush_write_page(uint32_t addr, uint32_t virt)
{
        int c;
        page_t *page_target = &pages[addr >> 12];
//        pclog("mem_flush_write_page %08x %08x\n", virt, addr);

        for (c = 0; c < 256; c++)        
        {
                if (writelookup[c] != 0xffffffff)
                {
                        uintptr_t target = (uintptr_t)&ram[(uintptr_t)(addr & ~0xfff) - (virt & ~0xfff)];

//                        if ((virt & ~0xfff) == 0xc022e000)
//                                pclog(" Checking %02x %p %p\n", (void *)writelookup2[writelookup[c]], (void *)target);
                        if (writelookup2[writelookup[c]] == target || page_lookup[writelookup[c]] == page_target)
                        {
//                                pclog("  throw out %02x %p %p\n", writelookup[c], (void *)page_lookup[writelookup[c]], (void *)writelookup2[writelookup[c]]);
                                writelookup2[writelookup[c]] = -1;
                                page_lookup[writelookup[c]] = NULL;
                                writelookup[c] = 0xffffffff;
                        }
                }
        }
}

#define mmutranslate_read(addr) mmutranslatereal(addr,0)
#define mmutranslate_write(addr) mmutranslatereal(addr,1)

static inline uint32_t mmu_readl(uint32_t addr)
{
        return *(uint32_t *)&_mem_exec[addr >> 14][addr & 0x3fff];
}

static inline void mmu_writel(uint32_t addr, uint32_t val)
{
        *(uint32_t *)&_mem_exec[addr >> 14][addr & 0x3fff] = val;
}

uint32_t mmutranslatereal(uint32_t addr, int rw)
{
        uint32_t addr2;
        uint32_t temp,temp2,temp3;
        
                if (cpu_state.abrt) 
                {
//                        pclog("Translate recursive abort\n");
                        return -1;
                }
/*                if ((addr&~0xFFFFF)==0x77f00000) pclog("Do translate %08X %i  %08X  %08X\n",addr,rw,EAX,pc);
                if (addr==0x77f61000) output = 3;
                if (addr==0x77f62000) { dumpregs(); exit(-1); }
                if (addr==0x77f9a000) { dumpregs(); exit(-1); }*/
        addr2 = ((cr3 & ~0xfff) + ((addr >> 20) & 0xffc));
        temp = temp2 = mmu_readl(addr2);
//        if (output == 3) pclog("Do translate %08X %i %08X\n", addr, rw, temp);
        if (!(temp&1))// || (CPL==3 && !(temp&4) && !cpl_override) || (rw && !(temp&2) && (CPL==3 || cr0&WP_FLAG)))
        {
//                if (!nopageerrors) pclog("Section not present! %08X  %08X  %02X  %04X:%08X  %i %i\n",addr,temp,opcode,CS,pc,CPL,rw);

                cr2=addr;
                temp&=1;
                if (CPL==3) temp|=4;
                if (rw) temp|=2;
                cpu_state.abrt = ABRT_PF;
                abrt_error = temp;
/*                if (addr == 0x70046D)
                {
                        dumpregs();
                        exit(-1);
                }*/
                return -1;
        }
        
        if ((temp & 0x80) && (cr4 & CR4_PSE))
        {
                /*4MB page*/
                if ((CPL == 3 && !(temp & 4) && !cpl_override) || (rw && !(temp & 2) && ((CPL == 3 && !cpl_override) || cr0 & WP_FLAG)))
                {
//                        if (!nopageerrors) pclog("Page not present!  %08X   %08X   %02X %02X  %i  %08X  %04X:%08X  %04X:%08X %i  %i %i\n",addr,temp,opcode,opcode2,frame,rmdat32, CS,pc,SS,ESP,ins,CPL,rw);

                        cr2 = addr;
                        temp &= 1;
                        if (CPL == 3)
                                temp |= 4;
                        if (rw)
                                temp |= 2;
                        cpu_state.abrt = ABRT_PF;
                        abrt_error = temp;

                        return -1;
                }

                mmu_perm = temp & 4;
                ((uint32_t *)ram)[addr2>>2] |= 0x20;

                return (temp & ~0x3fffff) + (addr & 0x3fffff);
        }
        
        temp = mmu_readl((temp & ~0xfff) + ((addr >> 10) & 0xffc));
        temp3=temp&temp2;
//        if (output == 3) pclog("Do translate %08X %08X\n", temp, temp3);
        if (!(temp&1) || (CPL==3 && !(temp3&4) && !cpl_override) || (rw && !(temp3&2) && ((CPL == 3 && !cpl_override) || cr0&WP_FLAG)))
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
                cpu_state.abrt = ABRT_PF;
                abrt_error = temp;
//                pclog("%04X\n",abrt);
                return -1;
        }
        mmu_perm=temp&4;
        mmu_writel(addr2, temp2 | 0x20);
        mmu_writel((temp2 & ~0xfff) + ((addr >> 10) & 0xffc), temp | (rw ? 0x60 : 0x20));
//        /*if (output) */pclog("Translate %08X %08X %08X  %08X:%08X  %08X\n",addr,(temp&~0xFFF)+(addr&0xFFF),temp,cs,pc,EDI);

        return (temp&~0xFFF)+(addr&0xFFF);
}

uint32_t mmutranslate_noabrt(uint32_t addr, int rw)
{
        uint32_t addr2;
        uint32_t temp,temp2,temp3;
        
        if (cpu_state.abrt) 
                return -1;

        addr2 = ((cr3 & ~0xfff) + ((addr >> 20) & 0xffc));
        temp = temp2 = mmu_readl(addr2);

        if (!(temp&1))
                return -1;

        if ((temp & 0x80) && (cr4 & CR4_PSE))
        {
                /*4MB page*/
                if ((CPL == 3 && !(temp & 4) && !cpl_override) || (rw && !(temp & 2) && (CPL == 3 || cr0 & WP_FLAG)))
                        return -1;

                return (temp & ~0x3fffff) + (addr & 0x3fffff);
        }

        temp = mmu_readl((temp & ~0xfff) + ((addr >> 10) & 0xffc));
        temp3=temp&temp2;

        if (!(temp&1) || (CPL==3 && !(temp3&4) && !cpl_override) || (rw && !(temp3&2) && (CPL==3 || cr0&WP_FLAG)))
                return -1;

        return (temp&~0xFFF)+(addr&0xFFF);
}

void mmu_invalidate(uint32_t addr)
{
//        readlookup2[addr >> 12] = writelookup2[addr >> 12] = 0xFFFFFFFF;
        flushmmucache_cr3();
}

void addreadlookup(uint32_t virt, uint32_t phys)
{
//        return;
//        printf("Addreadlookup %08X %08X %08X %08X %08X %08X %02X %08X\n",virt,phys,cs,ds,es,ss,opcode,pc);
        if (virt == 0xffffffff)
                return;
                
        if (readlookup2[virt>>12] != -1) 
        {
/*                if (readlookup2[virt>>12] != phys&~0xfff)
                {
                        pclog("addreadlookup mismatch - %05X000 %05X000\n", readlookup[readlnext], virt >> 12);
                        dumpregs();
                        exit(-1);
                }*/
                return;
        }
        
        
        if (readlookup[readlnext]!=0xFFFFFFFF)
        {
                readlookup2[readlookup[readlnext]] = -1;
//                readlnum--;
        }
        readlookup2[virt>>12] = (uintptr_t)&ram[(uintptr_t)(phys & ~0xFFF) - (uintptr_t)(virt & ~0xfff)];
        readlookupp[readlnext]=mmu_perm;
        readlookup[readlnext++]=virt>>12;
        readlnext&=(cachesize-1);
        
        cycles -= 9;
}

void addwritelookup(uint32_t virt, uint32_t phys)
{
//        return;
//        printf("Addwritelookup %08X %08X\n",virt,phys);
        if (virt == 0xffffffff)
                return;

        if (page_lookup[virt >> 12])
        {
/*                if (writelookup2[virt>>12] != phys&~0xfff)
                {
                        pclog("addwritelookup mismatch - %05X000 %05X000\n", readlookup[readlnext], virt >> 12);
                        dumpregs();
                        exit(-1);
                }*/
                return;
        }
        
        if (writelookup[writelnext] != -1)
        {
                page_lookup[writelookup[writelnext]] = NULL;
                writelookup2[writelookup[writelnext]] = -1;
//                writelnum--;
        }
//        if (page_lookup[virt >> 12] && (writelookup2[virt>>12] != 0xffffffff))
//                fatal("Bad write mapping\n");

        if (pages[phys >> 12].block || (phys & ~0xfff) == recomp_page)
                page_lookup[virt >> 12] = &pages[phys >> 12];//(uintptr_t)&ram[(uintptr_t)(phys & ~0xFFF) - (uintptr_t)(virt & ~0xfff)];
        else
                writelookup2[virt>>12] = (uintptr_t)&ram[(uintptr_t)(phys & ~0xFFF) - (uintptr_t)(virt & ~0xfff)];
//        pclog("addwritelookup %08x %08x %p %p %016llx %p\n", virt, phys, (void *)page_lookup[virt >> 12], (void *)writelookup2[virt >> 12], pages[phys >> 12].dirty_mask, (void *)&pages[phys >> 12]);
        writelookupp[writelnext] = mmu_perm;
        writelookup[writelnext++] = virt >> 12;
        writelnext &= (cachesize - 1);

        cycles -= 9;
}

uint8_t *getpccache(uint32_t a)
{
        uint32_t a2=a;

        if (cr0>>31)
        {
                a = mmutranslate_read(a);

                if (a==0xFFFFFFFF) return ram;
        }
        a&=rammask;

        if (_mem_exec[a >> 14])
        {
                if (read_mapping[a >> 14]->flags & MEM_MAPPING_ROM)
                        cpu_prefetch_cycles = cpu_rom_prefetch_cycles;
                else
                        cpu_prefetch_cycles = cpu_mem_prefetch_cycles;

                return &_mem_exec[a >> 14][(uintptr_t)(a & 0x3000) - (uintptr_t)(a2 & ~0xFFF)];
        }

        pclog("Bad getpccache %08X\n", a);
        return &ff_array[0-(uintptr_t)(a2 & ~0xFFF)];
}

uint8_t readmembl(uint32_t addr)
{
        mem_mapping_t *map;
        
        mem_logical_addr = addr;
        if (cr0 >> 31)
        {
                addr = mmutranslate_read(addr);
                if (addr == 0xFFFFFFFF) return 0xFF;
        }
        addr &= rammask;

        map = read_mapping[addr >> 14];
        if (map && map->read_b)
                return map->read_b(addr, map->p);
//        pclog("Bad readmembl %08X %04X:%08X\n", addr, CS, pc);
        return 0xFF;
}

void writemembl(uint32_t addr, uint8_t val)
{
        mem_mapping_t *map;

        mem_logical_addr = addr;

        if (page_lookup[addr>>12])
        {
                page_lookup[addr>>12]->write_b(addr, val, page_lookup[addr>>12]);
                return;
        }
        if (cr0 >> 31)
        {
                addr = mmutranslate_write(addr);
                if (addr == 0xFFFFFFFF) return;
        }
        addr &= rammask;

        map = write_mapping[addr >> 14];
        if (map && map->write_b)
                return map->write_b(addr, val, map->p);
//        else                          pclog("Bad writemembl %08X %02X  %04X:%08X\n", addr, val, CS, pc);
}

uint16_t readmemwl(uint32_t addr)
{
        mem_mapping_t *map;

        mem_logical_addr = addr;

        if (addr & 1)
        {
                if (!cpu_cyrix_alignment || (addr & 7) == 7)
                        cycles -= timing_misaligned;
                if ((addr & 0xFFF) > 0xFFE)
                {
                        if (cr0 >> 31)
                        {
                                if (mmutranslate_read(addr)   == 0xffffffff) return 0xffff;
                                if (mmutranslate_read(addr+1) == 0xffffffff) return 0xffff;
                        }
                        return readmembl(addr)|(readmembl(addr+1)<<8);
                }
                else if (readlookup2[addr >> 12] != -1)
                        return *(uint16_t *)(readlookup2[addr >> 12] + addr);
        }
        if (cr0>>31)
        {
                addr = mmutranslate_read(addr);
                if (addr==0xFFFFFFFF) return 0xFFFF;
        }

        addr &= rammask;

        map = read_mapping[addr >> 14];
        if (map)
        {
                if (map->read_w)
                        return map->read_w(addr, map->p);

                if (map->read_b)
                        return map->read_b(addr, map->p) | (map->read_b(addr + 1, map->p) << 8);
        }

//        pclog("Bad readmemwl %08X\n", addr);
        return 0xffff;
}

void writememwl(uint32_t addr, uint16_t val)
{
        mem_mapping_t *map;

        mem_logical_addr = addr;

        if (addr & 1)
        {
                if (!cpu_cyrix_alignment || (addr & 7) == 7)
                        cycles -= timing_misaligned;
                if ((addr & 0xFFF) > 0xFFE)
                {
                        if (cr0 >> 31)
                        {
                                if (mmutranslate_write(addr)   == 0xffffffff) return;
                                if (mmutranslate_write(addr+1) == 0xffffffff) return;
                        }
                        writemembl(addr,val);
                        writemembl(addr+1,val>>8);
                        return;
                }
                else if (writelookup2[addr >> 12] != -1)
                {
                        *(uint16_t *)(writelookup2[addr >> 12] + addr) = val;
                        return;
                }
        }

        if (page_lookup[addr>>12])
        {
                page_lookup[addr>>12]->write_w(addr, val, page_lookup[addr>>12]);
                return;
        }
        if (cr0>>31)
        {
                addr = mmutranslate_write(addr);
                if (addr==0xFFFFFFFF) return;
        }
        
        addr &= rammask;

        map = write_mapping[addr >> 14];
        if (map)
        {
                if (map->write_w)
                        map->write_w(addr, val, map->p);
                else if (map->write_b)
                {
                        map->write_b(addr, val, map->p);
                        map->write_b(addr + 1, val >> 8, map->p);
                }
        }

//        pclog("Bad writememwl %08X %04X\n", addr, val);
}

uint32_t readmemll(uint32_t addr)
{
        mem_mapping_t *map;

        mem_logical_addr = addr;

        if (addr & 3)
        {
                if (!cpu_cyrix_alignment || (addr & 7) > 4)
                        cycles -= timing_misaligned;
                if ((addr&0xFFF)>0xFFC)
                {
                        if (cr0>>31)
                        {
                                if (mmutranslate_read(addr)   == 0xffffffff) return 0xffffffff;
                                if (mmutranslate_read(addr+3) == 0xffffffff) return 0xffffffff;
                        }
                        return readmemwl(addr)|(readmemwl(addr+2)<<16);
                }
                else if (readlookup2[addr >> 12] != -1)
                        return *(uint32_t *)(readlookup2[addr >> 12] + addr);
        }

        if (cr0>>31)
        {
                addr = mmutranslate_read(addr);
                if (addr==0xFFFFFFFF) return 0xFFFFFFFF;
        }

        addr&=rammask;

        map = read_mapping[addr >> 14];
        if (map)
        {
                if (map->read_l)
                        return map->read_l(addr, map->p);

                if (map->read_w)
                        return map->read_w(addr, map->p) | (map->read_w(addr + 2, map->p) << 16);

                if (map->read_b)
                        return map->read_b(addr, map->p) | (map->read_b(addr + 1, map->p) << 8) |
                                (map->read_b(addr + 2, map->p) << 16) | (map->read_b(addr + 3, map->p) << 24);
        }

//        pclog("Bad readmemll %08X\n", addr);
        return 0xffffffff;
}

void writememll(uint32_t addr, uint32_t val)
{
        mem_mapping_t *map;

        mem_logical_addr = addr;

        if (addr & 3)
        {
                if (!cpu_cyrix_alignment || (addr & 7) > 4)
                        cycles -= timing_misaligned;
                if ((addr & 0xFFF) > 0xFFC)
                {
                        if (cr0>>31)
                        {
                                if (mmutranslate_write(addr)   == 0xffffffff) return;
                                if (mmutranslate_write(addr+3) == 0xffffffff) return;
                        }
                        writememwl(addr,val);
                        writememwl(addr+2,val>>16);
                        return;
                }
                else if (writelookup2[addr >> 12] != -1)
                {
                        *(uint32_t *)(writelookup2[addr >> 12] + addr) = val;
                        return;
                }
        }
        if (page_lookup[addr>>12])
        {
                page_lookup[addr>>12]->write_l(addr, val, page_lookup[addr>>12]);
                return;
        }
        if (cr0>>31)
        {
                addr = mmutranslate_write(addr);
                if (addr==0xFFFFFFFF) return;
        }
        
        addr&=rammask;

        map = write_mapping[addr >> 14];
        if (map)
        {
                if (map->write_l)
                        map->write_l(addr, val, map->p);
                else if (map->write_w)
                {
                        map->write_w(addr, val, map->p);
                        map->write_w(addr + 2, val >> 16, map->p);
                }
                else if (map->write_b)
                {
                        map->write_b(addr, val, map->p);
                        map->write_b(addr + 1, val >> 8, map->p);
                        map->write_b(addr + 2, val >> 16, map->p);
                        map->write_b(addr + 3, val >> 24, map->p);
                }
        }
//        pclog("Bad writememll %08X %08X\n", addr, val);
}

uint64_t readmemql(uint32_t addr)
{
        mem_mapping_t *map;

        mem_logical_addr = addr;

        if (addr & 7)
        {
                cycles -= timing_misaligned;
                if ((addr & 0xFFF) > 0xFF8)
                {
                        if (cr0>>31)
                        {
                                if (mmutranslate_read(addr)   == 0xffffffff) return 0xffffffff;
                                if (mmutranslate_read(addr+7) == 0xffffffff) return 0xffffffff;
                        }
                        return readmemll(addr)|((uint64_t)readmemll(addr+4)<<32);
                }
                else if (readlookup2[addr >> 12] != -1)
                        return *(uint64_t *)(readlookup2[addr >> 12] + addr);
        }
        
        if (cr0>>31)
        {
                addr = mmutranslate_read(addr);
                if (addr==0xFFFFFFFF) return 0xFFFFFFFF;
        }

        addr&=rammask;

        map = read_mapping[addr >> 14];
        if (map && map->read_l)
                return map->read_l(addr, map->p) | ((uint64_t)map->read_l(addr + 4, map->p) << 32);

        return readmemll(addr) | ((uint64_t)readmemll(addr+4)<<32);
}

void writememql(uint32_t addr, uint64_t val)
{
        mem_mapping_t *map;

        mem_logical_addr = addr;

        if (addr & 7)
        {
                cycles -= timing_misaligned;
                if ((addr & 0xFFF) > 0xFF8)
                {
                        if (cr0>>31)
                        {
                                if (mmutranslate_write(addr)   == 0xffffffff) return;
                                if (mmutranslate_write(addr+7) == 0xffffffff) return;
                        }
                        writememll(addr, val);
                        writememll(addr+4, val >> 32);
                        return;
                }
                else if (writelookup2[addr >> 12] != -1)
                {
                        *(uint64_t *)(writelookup2[addr >> 12] + addr) = val;
                        return;
                }
        }
        if (page_lookup[addr>>12])
        {
                page_lookup[addr>>12]->write_l(addr, val, page_lookup[addr>>12]);
                page_lookup[addr>>12]->write_l(addr + 4, val >> 32, page_lookup[addr>>12]);
                return;
        }
        if (cr0>>31)
        {
                addr = mmutranslate_write(addr);
                if (addr==0xFFFFFFFF) return;
        }
        
        addr&=rammask;

        map = write_mapping[addr >> 14];
        if (map)
        {
                if (map->write_l)
                {
                        map->write_l(addr, val, map->p);
                        map->write_l(addr + 4, val >> 32, map->p);
                }
                else if (map->write_w)
                {
                        map->write_w(addr, val, map->p);
                        map->write_w(addr + 2, val >> 16, map->p);
                        map->write_w(addr + 4, val >> 32, map->p);
                        map->write_w(addr + 6, val >> 48, map->p);
                }
                else if (map->write_b)
                {
                        map->write_b(addr, val, map->p);
                        map->write_b(addr + 1, val >> 8, map->p);
                        map->write_b(addr + 2, val >> 16, map->p);
                        map->write_b(addr + 3, val >> 24, map->p);
                        map->write_b(addr + 4, val >> 32, map->p);
                        map->write_b(addr + 5, val >> 40, map->p);
                        map->write_b(addr + 6, val >> 48, map->p);
                        map->write_b(addr + 7, val >> 56, map->p);
                }
        }
//        pclog("Bad writememql %08X %08X\n", addr, val);
}

uint8_t mem_readb_phys(uint32_t addr)
{
        mem_mapping_t *map = read_mapping[addr >> 14];

        mem_logical_addr = 0xffffffff;
        
        if (map && map->read_b)
                return map->read_b(addr, map->p);
                
        return 0xff;
}
uint16_t mem_readw_phys(uint32_t addr)
{
        mem_mapping_t *map = read_mapping[addr >> 14];
        
        mem_logical_addr = 0xffffffff;
        
        if (map && map->read_w)
                return map->read_w(addr, map->p);
                
        return mem_readb_phys(addr) | (mem_readb_phys(addr + 1) << 8);
}
uint32_t mem_readl_phys(uint32_t addr)
{
        mem_mapping_t *map = read_mapping[addr >> 14];
        
        mem_logical_addr = 0xffffffff;
        
        if (map && map->read_l)
                return map->read_l(addr, map->p);
                
        return mem_readw_phys(addr) | (mem_readw_phys(addr + 2) << 16);
}

void mem_writeb_phys(uint32_t addr, uint8_t val)
{
        mem_mapping_t *map = write_mapping[addr >> 14];

        mem_logical_addr = 0xffffffff;

        if (map && map->write_b)
                map->write_b(addr, val, map->p);
}
void mem_writew_phys(uint32_t addr, uint16_t val)
{
        mem_mapping_t *map = write_mapping[addr >> 14];
        
        mem_logical_addr = 0xffffffff;

        if (map && map->write_w && !(addr & 1))
                map->write_w(addr, val, map->p);
        else
        {
                mem_writeb_phys(addr, val);
                mem_writeb_phys(addr+1, val >> 8);
        }
}
void mem_writel_phys(uint32_t addr, uint32_t val)
{
        mem_mapping_t *map = write_mapping[addr >> 14];
        
        mem_logical_addr = 0xffffffff;

        if (map && map->write_l && !(addr & 3))
                map->write_l(addr, val, map->p);
        else
        {
                mem_writew_phys(addr, val);
                mem_writew_phys(addr+2, val >> 16);
        }
}

uint8_t mem_read_ram(uint32_t addr, void *priv)
{
//        if (addr >= 0xc0000 && addr < 0x0c8000) pclog("Read RAMb %08X\n", addr);
        addreadlookup(mem_logical_addr, addr);
        return ram[addr];
}
uint16_t mem_read_ramw(uint32_t addr, void *priv)
{
//        if (addr >= 0xc0000 && addr < 0x0c8000)  pclog("Read RAMw %08X\n", addr);
        addreadlookup(mem_logical_addr, addr);
        return *(uint16_t *)&ram[addr];
}
uint32_t mem_read_raml(uint32_t addr, void *priv)
{
//        if (addr >= 0xc0000 && addr < 0x0c8000) pclog("Read RAMl %08X\n", addr);
        addreadlookup(mem_logical_addr, addr);
        return *(uint32_t *)&ram[addr];
}

uint32_t purgable_page_list_head = 0;
int purgeable_page_count = 0;

static inline int page_index(page_t *p)
{
        return ((uintptr_t)p - (uintptr_t)pages) / sizeof(page_t);
}
void page_add_to_evict_list(page_t *p)
{
//        pclog("page_add_to_evict_list: %08x %i\n", page_index(p), purgeable_page_count);
        pages[purgable_page_list_head].evict_prev = page_index(p);
        p->evict_next = purgable_page_list_head;
        p->evict_prev = 0;
        purgable_page_list_head = pages[purgable_page_list_head].evict_prev;
        purgeable_page_count++;
}
void page_remove_from_evict_list(page_t *p)
{
//        pclog("page_remove_from_evict_list: %08x %i\n", page_index(p), purgeable_page_count);
        if (!page_in_evict_list(p))
                fatal("page_remove_from_evict_list: not in evict list!\n");
        if (p->evict_prev)
                pages[p->evict_prev].evict_next = p->evict_next;
        else
                purgable_page_list_head = p->evict_next;
        if (p->evict_next)
                pages[p->evict_next].evict_prev = p->evict_prev;
        p->evict_prev = EVICT_NOT_IN_LIST;
        purgeable_page_count--;
}

void mem_write_ramb_page(uint32_t addr, uint8_t val, page_t *p)
{      
        if (val != p->mem[addr & 0xfff] || codegen_in_recompile)
        {
                uint64_t mask = (uint64_t)1 << ((addr >> PAGE_MASK_SHIFT) & PAGE_MASK_MASK);
                int byte_offset = (addr >> PAGE_BYTE_MASK_SHIFT) & PAGE_BYTE_MASK_OFFSET_MASK;
                uint64_t byte_mask = (uint64_t)1 << (addr & PAGE_BYTE_MASK_MASK);
                
//        pclog("mem_write_ramb_page: %08x %02x %08x %llx %llx\n", addr, val, cs+cpu_state.pc, p->dirty_mask, mask);
                p->mem[addr & 0xfff] = val;
                p->dirty_mask |= mask;
                if ((p->code_present_mask & mask) && !page_in_evict_list(p))
                {
//                        pclog("ramb add %08x %016llx %016llx\n", addr, p->code_present_mask[index], mask);
                        page_add_to_evict_list(p);
                }
                p->byte_dirty_mask[byte_offset] |= byte_mask;
                if ((p->byte_code_present_mask[byte_offset] & byte_mask) && !page_in_evict_list(p))
                {
//                        pclog("   ramb add %08x %016llx %016llx\n", addr, p->byte_code_present_mask[byte_offset], byte_mask);
                        page_add_to_evict_list(p);
                }
        }
}
void mem_write_ramw_page(uint32_t addr, uint16_t val, page_t *p)
{
        if (val != *(uint16_t *)&p->mem[addr & 0xfff] || codegen_in_recompile)
        {
                uint64_t mask = (uint64_t)1 << ((addr >> PAGE_MASK_SHIFT) & PAGE_MASK_MASK);
                int byte_offset = (addr >> PAGE_BYTE_MASK_SHIFT) & PAGE_BYTE_MASK_OFFSET_MASK;
                uint64_t byte_mask = (uint64_t)1 << (addr & PAGE_BYTE_MASK_MASK);

                if ((addr & 0xf) == 0xf)
                        mask |= (mask << 1);
//        pclog("mem_write_ramw_page: %08x %04x %08x  %016llx %016llx %016llx  %08x %08x  %p\n", addr, val, cs+cpu_state.pc, p->dirty_mask[index], p->code_present_mask[index], mask, p->evict_prev, p->evict_next,  p);
                *(uint16_t *)&p->mem[addr & 0xfff] = val;
                p->dirty_mask |= mask;
                if ((p->code_present_mask & mask) && !page_in_evict_list(p))
                {
//                        pclog("ramw add %08x %016llx %016llx\n", addr, p->code_present_mask[index], mask);
                        page_add_to_evict_list(p);
                }
                if ((addr & PAGE_BYTE_MASK_MASK) == PAGE_BYTE_MASK_MASK)
                {
                        p->byte_dirty_mask[byte_offset+1] |= 1;
                        if ((p->byte_code_present_mask[byte_offset+1] & 1) && !page_in_evict_list(p))
                        {
//                                pclog("ramw add %08x %016llx %016llx\n", addr, p->code_present_mask[index], mask);
                                page_add_to_evict_list(p);
                        }
                }
                else
                        byte_mask |= (byte_mask << 1);
                        
                p->byte_dirty_mask[byte_offset] |= byte_mask;

                if ((p->byte_code_present_mask[byte_offset] & byte_mask) && !page_in_evict_list(p))
                {
//                        pclog("ramw add %08x %016llx %016llx\n", addr, p->code_present_mask[index], mask);
                        page_add_to_evict_list(p);
                }
        }
}
void mem_write_raml_page(uint32_t addr, uint32_t val, page_t *p)
{       
        if (val != *(uint32_t *)&p->mem[addr & 0xfff] || codegen_in_recompile)
        {
                uint64_t mask = (uint64_t)1 << ((addr >> PAGE_MASK_SHIFT) & PAGE_MASK_MASK);
                int byte_offset = (addr >> PAGE_BYTE_MASK_SHIFT) & PAGE_BYTE_MASK_OFFSET_MASK;
                uint64_t byte_mask = (uint64_t)0xf << (addr & PAGE_BYTE_MASK_MASK);

                if ((addr & 0xf) >= 0xd)
                        mask |= (mask << 1);
//        pclog("mem_write_raml_page: %08x %08x %08x   %016llx %016llx %016llx\n", addr, val, cs+cpu_state.pc, p->dirty_mask[index], p->code_present_mask[index], mask);
                *(uint32_t *)&p->mem[addr & 0xfff] = val;
                p->dirty_mask |= mask;
                p->byte_dirty_mask[byte_offset] |= byte_mask;
                if (!page_in_evict_list(p) && ((p->code_present_mask & mask) || (p->byte_code_present_mask[byte_offset] & byte_mask)))
                {
//                        pclog("raml add %08x %016llx %016llx\n", addr, p->code_present_mask[index], mask);
                        page_add_to_evict_list(p);
                }
                if ((addr & PAGE_BYTE_MASK_MASK) > (PAGE_BYTE_MASK_MASK-3))
                {
                        uint32_t byte_mask_2 = 0xf >> (4 - (addr & 3));

                        p->byte_dirty_mask[byte_offset+1] |= byte_mask_2;
                        if ((p->byte_code_present_mask[byte_offset+1] & byte_mask_2) && !page_in_evict_list(p))
                        {
//                                pclog("raml add %08x %016llx %016llx\n", addr, p->code_present_mask[index], mask);
                                page_add_to_evict_list(p);
                        }
                }
        }
}

void mem_write_ram(uint32_t addr, uint8_t val, void *priv)
{
        addwritelookup(mem_logical_addr, addr);
        mem_write_ramb_page(addr, val, &pages[addr >> 12]);
}
void mem_write_ramw(uint32_t addr, uint16_t val, void *priv)
{
        addwritelookup(mem_logical_addr, addr);
        mem_write_ramw_page(addr, val, &pages[addr >> 12]);
}
void mem_write_raml(uint32_t addr, uint32_t val, void *priv)
{
        addwritelookup(mem_logical_addr, addr);
        mem_write_raml_page(addr, val, &pages[addr >> 12]);
}


static uint32_t remap_start_addr;

uint8_t mem_read_remapped(uint32_t addr, void *priv)
{
        addr = 0xA0000 + (addr - remap_start_addr);
        addreadlookup(mem_logical_addr, addr);
        return ram[addr];
}
uint16_t mem_read_remappedw(uint32_t addr, void *priv)
{
        addr = 0xA0000 + (addr - remap_start_addr);
        addreadlookup(mem_logical_addr, addr);
        return *(uint16_t *)&ram[addr];
}
uint32_t mem_read_remappedl(uint32_t addr, void *priv)
{
        addr = 0xA0000 + (addr - remap_start_addr);
        addreadlookup(mem_logical_addr, addr);
        return *(uint32_t *)&ram[addr];
}

void mem_write_remapped(uint32_t addr, uint8_t val, void *priv)
{
        uint32_t oldaddr = addr;
        addr = 0xA0000 + (addr - remap_start_addr);
        addwritelookup(mem_logical_addr, addr);
        mem_write_ramb_page(addr, val, &pages[oldaddr >> 12]);
}
void mem_write_remappedw(uint32_t addr, uint16_t val, void *priv)
{
        uint32_t oldaddr = addr;
        addr = 0xA0000 + (addr - remap_start_addr);
        addwritelookup(mem_logical_addr, addr);
        mem_write_ramw_page(addr, val, &pages[oldaddr >> 12]);
}
void mem_write_remappedl(uint32_t addr, uint32_t val, void *priv)
{
        uint32_t oldaddr = addr;
        addr = 0xA0000 + (addr - remap_start_addr);
        addwritelookup(mem_logical_addr, addr);
        mem_write_raml_page(addr, val, &pages[oldaddr >> 12]);
}

uint8_t mem_read_bios(uint32_t addr, void *priv)
{
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

void mem_write_null(uint32_t addr, uint8_t val, void *p)
{
}
void mem_write_nullw(uint32_t addr, uint16_t val, void *p)
{
}
void mem_write_nulll(uint32_t addr, uint32_t val, void *p)
{
}

void mem_invalidate_range(uint32_t start_addr, uint32_t end_addr)
{
        start_addr &= ~PAGE_MASK_MASK;
        end_addr = (end_addr + PAGE_MASK_MASK) & ~PAGE_MASK_MASK;        
        
        for (; start_addr <= end_addr; start_addr += (1 << PAGE_MASK_SHIFT))
        {
                uint64_t mask = (uint64_t)1 << ((start_addr >> PAGE_MASK_SHIFT) & PAGE_MASK_MASK);
                page_t *p = &pages[start_addr >> 12];

//pclog("mem_invalidate: %08x\n", start_addr);
                p->dirty_mask |= mask;
                if ((p->code_present_mask & mask) && !page_in_evict_list(p))
                {
//                        pclog("invalidate add %08x %016llx %016llx\n", start_addr, p->code_present_mask[index], mask);
                        page_add_to_evict_list(p);
                }
        }
}

static inline int mem_mapping_read_allowed(uint32_t flags, int state)
{
//        pclog("mem_mapping_read_allowed: flags=%x state=%x\n", flags, state);
        switch (state & MEM_READ_MASK)
        {
                case MEM_READ_ANY:
                return 1;
                case MEM_READ_EXTERNAL:
                return !(flags & MEM_MAPPING_INTERNAL);
                case MEM_READ_INTERNAL:
                return !(flags & MEM_MAPPING_EXTERNAL);
                default:
                fatal("mem_mapping_read_allowed : bad state %x\n", state);
        }
        return 0;
}

static inline int mem_mapping_write_allowed(uint32_t flags, int state)
{
        switch (state & MEM_WRITE_MASK)
        {
                case MEM_WRITE_DISABLED:
                return 0;
                case MEM_WRITE_ANY:
                return 1;
                case MEM_WRITE_EXTERNAL:
                return !(flags & MEM_MAPPING_INTERNAL);
                case MEM_WRITE_INTERNAL:
                return !(flags & MEM_MAPPING_EXTERNAL);
                default:
                fatal("mem_mapping_write_allowed : bad state %x\n", state);
        }
        return 0;
}

static void mem_mapping_recalc(uint64_t base, uint64_t size)
{
        uint64_t c;
        mem_mapping_t *mapping = base_mapping.next;

        if (!size)
                return;
        /*Clear out old mappings*/
        for (c = base; c < base + size; c += 0x4000)
        {
                read_mapping[c >> 14] = NULL;
                write_mapping[c >> 14] = NULL;
                _mem_exec[c >> 14] = NULL;
        }

        /*Walk mapping list*/
        while (mapping != NULL)
        {
                /*In range?*/
                if (mapping->enable && (uint64_t)mapping->base < ((uint64_t)base + (uint64_t)size) && ((uint64_t)mapping->base + (uint64_t)mapping->size) > (uint64_t)base)
                {
                        uint64_t start = (mapping->base < base) ? mapping->base : base;
                        uint64_t end   = (((uint64_t)mapping->base + (uint64_t)mapping->size) < (base + size)) ? ((uint64_t)mapping->base + (uint64_t)mapping->size) : (base + size);
                        if (start < mapping->base)
                                start = mapping->base;

                        for (c = start; c < end; c += 0x4000)
                        {
                                if ((mapping->read_b || mapping->read_w || mapping->read_l) &&
                                     mem_mapping_read_allowed(mapping->flags, _mem_state[c >> 14]))
                                {
                                        read_mapping[c >> 14] = mapping;
                                        if (mapping->exec)
                                                _mem_exec[c >> 14] = mapping->exec + (c - mapping->base);
                                        else
                                                _mem_exec[c >> 14] = NULL;
                                }
                                if ((mapping->write_b || mapping->write_w || mapping->write_l) &&
                                     mem_mapping_write_allowed(mapping->flags, _mem_state[c >> 14]))
                                {
                                        write_mapping[c >> 14] = mapping;
                                }
                        }
                }
                mapping = mapping->next;
        }       
        flushmmucache_cr3();
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
                    uint8_t *exec,
                    uint32_t flags,
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
        mapping->exec    = exec;
        mapping->flags   = flags;
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

void mem_mapping_set_exec(mem_mapping_t *mapping, uint8_t *exec)
{
        mapping->exec = exec;
        
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

void mem_set_mem_state(uint32_t base, uint32_t size, int state)
{
        uint32_t c;

//        pclog("mem_set_pci_enable: base=%08x size=%08x\n", base, size);
        for (c = 0; c < size; c += 0x4000)
                _mem_state[(c + base) >> 14] = state;

        mem_mapping_recalc(base, size);
}

void mem_add_bios()
{
        if (AT || (romset == ROM_XI8088 && xi8088_bios_128kb()))
        {
                mem_mapping_add(&bios_mapping[0], 0xe0000, 0x04000, mem_read_bios,   mem_read_biosw,   mem_read_biosl,   mem_write_null, mem_write_nullw, mem_write_nulll, rom,                        MEM_MAPPING_EXTERNAL|MEM_MAPPING_ROM, 0);
                mem_mapping_add(&bios_mapping[1], 0xe4000, 0x04000, mem_read_bios,   mem_read_biosw,   mem_read_biosl,   mem_write_null, mem_write_nullw, mem_write_nulll, rom + (0x4000  & biosmask), MEM_MAPPING_EXTERNAL|MEM_MAPPING_ROM, 0);
                mem_mapping_add(&bios_mapping[2], 0xe8000, 0x04000, mem_read_bios,   mem_read_biosw,   mem_read_biosl,   mem_write_null, mem_write_nullw, mem_write_nulll, rom + (0x8000  & biosmask), MEM_MAPPING_EXTERNAL|MEM_MAPPING_ROM, 0);
                mem_mapping_add(&bios_mapping[3], 0xec000, 0x04000, mem_read_bios,   mem_read_biosw,   mem_read_biosl,   mem_write_null, mem_write_nullw, mem_write_nulll, rom + (0xc000  & biosmask), MEM_MAPPING_EXTERNAL|MEM_MAPPING_ROM, 0);
        }
        mem_mapping_add(&bios_mapping[4], 0xf0000, 0x04000, mem_read_bios,   mem_read_biosw,   mem_read_biosl,   mem_write_null, mem_write_nullw, mem_write_nulll, rom + (0x10000 & biosmask), MEM_MAPPING_EXTERNAL|MEM_MAPPING_ROM, 0);
        mem_mapping_add(&bios_mapping[5], 0xf4000, 0x04000, mem_read_bios,   mem_read_biosw,   mem_read_biosl,   mem_write_null, mem_write_nullw, mem_write_nulll, rom + (0x14000 & biosmask), MEM_MAPPING_EXTERNAL|MEM_MAPPING_ROM, 0);
        mem_mapping_add(&bios_mapping[6], 0xf8000, 0x04000, mem_read_bios,   mem_read_biosw,   mem_read_biosl,   mem_write_null, mem_write_nullw, mem_write_nulll, rom + (0x18000 & biosmask), MEM_MAPPING_EXTERNAL|MEM_MAPPING_ROM, 0);
        mem_mapping_add(&bios_mapping[7], 0xfc000, 0x04000, mem_read_bios,   mem_read_biosw,   mem_read_biosl,   mem_write_null, mem_write_nullw, mem_write_nulll, rom + (0x1c000 & biosmask), MEM_MAPPING_EXTERNAL|MEM_MAPPING_ROM, 0);

        mem_mapping_add(&bios_high_mapping[0], (AT && cpu_16bitbus) ? 0xfe0000 : 0xfffe0000, 0x04000, mem_read_bios,   mem_read_biosw,   mem_read_biosl,   mem_write_null, mem_write_nullw, mem_write_nulll, rom,                        MEM_MAPPING_ROM, 0);
        mem_mapping_add(&bios_high_mapping[1], (AT && cpu_16bitbus) ? 0xfe4000 : 0xfffe4000, 0x04000, mem_read_bios,   mem_read_biosw,   mem_read_biosl,   mem_write_null, mem_write_nullw, mem_write_nulll, rom + (0x4000  & biosmask), MEM_MAPPING_ROM, 0);
        mem_mapping_add(&bios_high_mapping[2], (AT && cpu_16bitbus) ? 0xfe8000 : 0xfffe8000, 0x04000, mem_read_bios,   mem_read_biosw,   mem_read_biosl,   mem_write_null, mem_write_nullw, mem_write_nulll, rom + (0x8000  & biosmask), MEM_MAPPING_ROM, 0);
        mem_mapping_add(&bios_high_mapping[3], (AT && cpu_16bitbus) ? 0xfec000 : 0xfffec000, 0x04000, mem_read_bios,   mem_read_biosw,   mem_read_biosl,   mem_write_null, mem_write_nullw, mem_write_nulll, rom + (0xc000  & biosmask), MEM_MAPPING_ROM, 0);
        mem_mapping_add(&bios_high_mapping[4], (AT && cpu_16bitbus) ? 0xff0000 : 0xffff0000, 0x04000, mem_read_bios,   mem_read_biosw,   mem_read_biosl,   mem_write_null, mem_write_nullw, mem_write_nulll, rom + (0x10000 & biosmask), MEM_MAPPING_ROM, 0);
        mem_mapping_add(&bios_high_mapping[5], (AT && cpu_16bitbus) ? 0xff4000 : 0xffff4000, 0x04000, mem_read_bios,   mem_read_biosw,   mem_read_biosl,   mem_write_null, mem_write_nullw, mem_write_nulll, rom + (0x14000 & biosmask), MEM_MAPPING_ROM, 0);
        mem_mapping_add(&bios_high_mapping[6], (AT && cpu_16bitbus) ? 0xff8000 : 0xffff8000, 0x04000, mem_read_bios,   mem_read_biosw,   mem_read_biosl,   mem_write_null, mem_write_nullw, mem_write_nulll, rom + (0x18000 & biosmask), MEM_MAPPING_ROM, 0);
        mem_mapping_add(&bios_high_mapping[7], (AT && cpu_16bitbus) ? 0xffc000 : 0xffffc000, 0x04000, mem_read_bios,   mem_read_biosw,   mem_read_biosl,   mem_write_null, mem_write_nullw, mem_write_nulll, rom + (0x1c000 & biosmask), MEM_MAPPING_ROM, 0);
}

int mem_a20_key = 0, mem_a20_alt = 0;
static int mem_a20_state = 2;

static void mem_remap_top(int max_size)
{
        int c;
        
        if (mem_size > 640)
        {
                uint32_t start = (mem_size >= 1024) ? mem_size : 1024;
                int size = mem_size - 640;
                if (size > max_size)
                        size = max_size;
                
                remap_start_addr = start * 1024;
                        
                for (c = ((start * 1024) >> 12); c < (((start + size) * 1024) >> 12); c++)
                {
                        int offset = c - ((start * 1024) >> 12);
                        pages[c].mem = &ram[0xA0000 + (offset << 12)];
                        pages[c].write_b = mem_write_ramb_page;
                        pages[c].write_w = mem_write_ramw_page;
                        pages[c].write_l = mem_write_raml_page;
                        pages[c].evict_prev = EVICT_NOT_IN_LIST;
                        pages[c].byte_dirty_mask = &byte_dirty_mask[offset * 64];
                        pages[c].byte_code_present_mask = &byte_code_present_mask[offset * 64];
                }

                mem_set_mem_state(start * 1024, size * 1024, MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
                mem_mapping_add(&ram_remapped_mapping, start * 1024, size * 1024, mem_read_remapped,    mem_read_remappedw,    mem_read_remappedl,    mem_write_remapped, mem_write_remappedw, mem_write_remappedl,   ram + 0xA0000,  MEM_MAPPING_INTERNAL, NULL);
        }
}

void mem_remap_top_256k()
{
        mem_remap_top(256);
}
void mem_remap_top_384k()
{
        mem_remap_top(384);
}

void mem_set_704kb()
{
        mem_set_mem_state(0x000000, (mem_size > 704) ? 0xb0000 : mem_size * 1024, MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
        mem_mapping_set_addr(&ram_low_mapping, 0x00000, (mem_size > 704) ? 0xb0000 : mem_size * 1024);
}

void mem_init()
{
        readlookup2  = malloc(1024 * 1024 * sizeof(uintptr_t));
        writelookup2 = malloc(1024 * 1024 * sizeof(uintptr_t));
        page_lookup = malloc((1 << 20) * sizeof(page_t *));

        memset(ff_array, 0xff, sizeof(ff_array));
}

void mem_alloc()
{
        int c;
        
        free(ram);
        ram = malloc(mem_size * 1024);
        memset(ram, 0, mem_size * 1024);
        
        free(byte_dirty_mask);
        byte_dirty_mask = malloc((mem_size * 1024) / 8);
        memset(byte_dirty_mask, 0, (mem_size * 1024) / 8);
        free(byte_code_present_mask);
        byte_code_present_mask = malloc((mem_size * 1024) / 8);
        memset(byte_code_present_mask, 0, (mem_size * 1024) / 8);
        
        free(pages);
        pages = malloc((((mem_size + 384) * 1024) >> 12) * sizeof(page_t));
        memset(pages, 0, (((mem_size + 384) * 1024) >> 12) * sizeof(page_t));
        for (c = 0; c < (((mem_size + 384) * 1024) >> 12); c++)
        {
                pages[c].mem = &ram[c << 12];
                pages[c].write_b = mem_write_ramb_page;
                pages[c].write_w = mem_write_ramw_page;
                pages[c].write_l = mem_write_raml_page;
                pages[c].evict_prev = EVICT_NOT_IN_LIST;
                pages[c].byte_dirty_mask = &byte_dirty_mask[c * 64];
                pages[c].byte_code_present_mask = &byte_code_present_mask[c * 64];
        }

        memset(page_lookup, 0, (1 << 20) * sizeof(page_t *));

        memset(read_mapping, 0, sizeof(read_mapping));
        memset(write_mapping, 0, sizeof(write_mapping));
        memset(_mem_exec, 0, sizeof(_mem_exec));
        
        memset(&base_mapping, 0, sizeof(base_mapping));
        
        memset(_mem_state, 0, sizeof(_mem_state));

        mem_set_mem_state(0x000000, (mem_size > 640) ? 0xa0000 : mem_size * 1024, MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
        mem_set_mem_state(0x0c0000, 0x40000, MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
        
        mem_mapping_add(&ram_low_mapping, 0x00000, (mem_size > 640) ? 0xa0000 : mem_size * 1024, mem_read_ram,    mem_read_ramw,    mem_read_raml,    mem_write_ram, mem_write_ramw, mem_write_raml,   ram,  MEM_MAPPING_INTERNAL, NULL);
        if (mem_size > 1024)
        {
                if (cpu_16bitbus && mem_size > 16256)
                {
                        mem_set_mem_state(0x100000, (16256 - 1024) * 1024, MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
                        mem_mapping_add(&ram_high_mapping, 0x100000, ((16256 - 1024) * 1024), mem_read_ram,    mem_read_ramw,    mem_read_raml,    mem_write_ram, mem_write_ramw, mem_write_raml,   ram + 0x100000, MEM_MAPPING_INTERNAL, NULL);
                }
                else
                {
                        mem_set_mem_state(0x100000, (mem_size - 1024) * 1024, MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
                        mem_mapping_add(&ram_high_mapping, 0x100000, ((mem_size - 1024) * 1024), mem_read_ram,    mem_read_ramw,    mem_read_raml,    mem_write_ram, mem_write_ramw, mem_write_raml,   ram + 0x100000, MEM_MAPPING_INTERNAL, NULL);
                }
        }
	if (mem_size > 768)
	        mem_mapping_add(&ram_mid_mapping,   0xc0000, 0x40000, mem_read_ram,    mem_read_ramw,    mem_read_raml,    mem_write_ram, mem_write_ramw, mem_write_raml,   ram + 0xc0000,  MEM_MAPPING_INTERNAL, NULL);

        if (romset == ROM_IBMPS1_2011)
                mem_mapping_add(&romext_mapping,  0xc8000, 0x08000, mem_read_romext, mem_read_romextw, mem_read_romextl, NULL, NULL, NULL,   romext, 0, NULL);

//        pclog("Mem resize %i %i\n",mem_size,c);
        mem_a20_key = 2;
        mem_a20_alt = 0;
        mem_a20_recalc();

        purgable_page_list_head = 0;
        purgeable_page_count = 0;
}

void mem_reset_page_blocks()
{
        int c;
        
        for (c = 0; c < ((mem_size * 1024) >> 12); c++)
        {
                pages[c].write_b = mem_write_ramb_page;
                pages[c].write_w = mem_write_ramw_page;
                pages[c].write_l = mem_write_raml_page;
                pages[c].block = BLOCK_INVALID;
                pages[c].block_2 = BLOCK_INVALID;
        }
}

void mem_a20_recalc()
{
        int state = mem_a20_key | mem_a20_alt;
//        pclog("A20 recalc %i %i\n", state, mem_a20_state);
        if (state && !mem_a20_state)
        {
                rammask = (AT && cpu_16bitbus) ? 0xffffff : 0xffffffff;
                flushmmucache();
        }
        else if (!state && mem_a20_state)
        {
                rammask = (AT && cpu_16bitbus) ? 0xefffff : 0xffefffff;
                flushmmucache();
        }
//        pclog("rammask now %08X\n", rammask);
        mem_a20_state = state;
}

uint32_t get_phys_virt,get_phys_phys;
