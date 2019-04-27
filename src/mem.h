#ifndef _MEM_H_
#define _MEM_H_

#include "x86.h"

typedef struct mem_mapping_t
{
        struct mem_mapping_t *prev, *next;

        int enable;
                
        uint32_t base;
        uint32_t size;

        uint8_t  (*read_b)(uint32_t addr, void *priv);
        uint16_t (*read_w)(uint32_t addr, void *priv);
        uint32_t (*read_l)(uint32_t addr, void *priv);
        void (*write_b)(uint32_t addr, uint8_t  val, void *priv);
        void (*write_w)(uint32_t addr, uint16_t val, void *priv);
        void (*write_l)(uint32_t addr, uint32_t val, void *priv);
        
        uint8_t *exec;
        
        uint32_t flags;
        
        void *p;
} mem_mapping_t;

/*Only present on external bus (ISA/PCI)*/
#define MEM_MAPPING_EXTERNAL 1
/*Only present on internal bus (RAM)*/
#define MEM_MAPPING_INTERNAL 2
/*Executing from ROM may involve additional wait states*/
#define MEM_MAPPING_ROM      4

extern uint8_t *ram,*rom;
extern uint8_t romext[32768];
extern int readlnum,writelnum;
extern int memspeed[11];
extern uint32_t biosmask;

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
                    void *p);
void mem_mapping_set_handler(mem_mapping_t *mapping,
                    uint8_t  (*read_b)(uint32_t addr, void *p),
                    uint16_t (*read_w)(uint32_t addr, void *p),
                    uint32_t (*read_l)(uint32_t addr, void *p),
                    void (*write_b)(uint32_t addr, uint8_t  val, void *p),
                    void (*write_w)(uint32_t addr, uint16_t val, void *p),
                    void (*write_l)(uint32_t addr, uint32_t val, void *p));
void mem_mapping_set_p(mem_mapping_t *mapping, void *p);
void mem_mapping_set_addr(mem_mapping_t *mapping, uint32_t base, uint32_t size);
void mem_mapping_set_exec(mem_mapping_t *mapping, uint8_t *exec);
void mem_mapping_disable(mem_mapping_t *mapping);
void mem_mapping_enable(mem_mapping_t *mapping);

void mem_set_mem_state(uint32_t base, uint32_t size, int state);

#define MEM_READ_ANY       0x00
#define MEM_READ_INTERNAL  0x10
#define MEM_READ_EXTERNAL  0x20
#define MEM_READ_MASK      0xf0

#define MEM_WRITE_ANY      0x00
#define MEM_WRITE_INTERNAL 0x01
#define MEM_WRITE_EXTERNAL 0x02
#define MEM_WRITE_DISABLED 0x03
#define MEM_WRITE_MASK     0x0f

extern int mem_a20_alt;
extern int mem_a20_key;
void mem_a20_recalc();

uint8_t mem_readb_phys(uint32_t addr);
uint16_t mem_readw_phys(uint32_t addr);
uint32_t mem_readl_phys(uint32_t addr);
void mem_writeb_phys(uint32_t addr, uint8_t val);
void mem_writew_phys(uint32_t addr, uint16_t val);
void mem_writel_phys(uint32_t addr, uint32_t val);

uint8_t  mem_read_ram(uint32_t addr, void *priv);
uint16_t mem_read_ramw(uint32_t addr, void *priv);
uint32_t mem_read_raml(uint32_t addr, void *priv);

void mem_write_ram(uint32_t addr, uint8_t val, void *priv);
void mem_write_ramw(uint32_t addr, uint16_t val, void *priv);
void mem_write_raml(uint32_t addr, uint32_t val, void *priv);

uint8_t  mem_read_bios(uint32_t addr, void *priv);
uint16_t mem_read_biosw(uint32_t addr, void *priv);
uint32_t mem_read_biosl(uint32_t addr, void *priv);

void mem_write_null(uint32_t addr, uint8_t val, void *p);
void mem_write_nullw(uint32_t addr, uint16_t val, void *p);
void mem_write_nulll(uint32_t addr, uint32_t val, void *p);

FILE *romfopen(char *fn, char *mode);

mem_mapping_t bios_mapping[8];
mem_mapping_t bios_high_mapping[8];

extern mem_mapping_t ram_high_mapping;
extern mem_mapping_t ram_remapped_mapping;

extern uint64_t *byte_dirty_mask;
extern uint64_t *byte_code_present_mask;

#define PAGE_BYTE_MASK_SHIFT 6
#define PAGE_BYTE_MASK_OFFSET_MASK 63
#define PAGE_BYTE_MASK_MASK  63

#define EVICT_NOT_IN_LIST ((uint32_t)-1)
typedef struct page_t
{
        void (*write_b)(uint32_t addr, uint8_t val, struct page_t *p);
        void (*write_w)(uint32_t addr, uint16_t val, struct page_t *p);
        void (*write_l)(uint32_t addr, uint32_t val, struct page_t *p);
        
        uint8_t *mem;
        
        uint16_t block, block_2;
        
        /*Head of codeblock tree associated with this page*/
        uint16_t head;
        
        uint64_t code_present_mask, dirty_mask;
        
        uint32_t evict_prev, evict_next;
        
        uint64_t *byte_dirty_mask;
        uint64_t *byte_code_present_mask;
} page_t;

extern page_t *pages;

extern page_t **page_lookup;

extern uint32_t purgable_page_list_head;
static inline int page_in_evict_list(page_t *p)
{
        return (p->evict_prev != EVICT_NOT_IN_LIST);
}
void page_remove_from_evict_list(page_t *p);
void page_add_to_evict_list(page_t *p);

int mem_addr_is_ram(uint32_t addr);

uint32_t mmutranslate_noabrt(uint32_t addr, int rw);
extern uint32_t get_phys_virt,get_phys_phys;
static inline uint32_t get_phys(uint32_t addr)
{
        if (!((addr ^ get_phys_virt) & ~0xfff))
                return get_phys_phys | (addr & 0xfff);

        get_phys_virt = addr;
        
        if (!(cr0 >> 31))
        {
                get_phys_phys = (addr & rammask) & ~0xfff;
                return addr & rammask;
        }
        
        if (readlookup2[addr >> 12] != -1)
                get_phys_phys = ((uintptr_t)readlookup2[addr >> 12] + (addr & ~0xfff)) - (uintptr_t)ram;
        else
        {
                get_phys_phys = (mmutranslatereal(addr, 0) & rammask) & ~0xfff;
                if (!cpu_state.abrt && mem_addr_is_ram(get_phys_phys))
                        addreadlookup(get_phys_virt, get_phys_phys);
        }
                
        return get_phys_phys | (addr & 0xfff);
//        return mmutranslatereal(addr, 0) & rammask;
}

static inline uint32_t get_phys_noabrt(uint32_t addr)
{
        uint32_t phys_addr;
        
        if (!(cr0 >> 31))
                return addr & rammask;
        
        if (readlookup2[addr >> 12] != -1)
                return ((uintptr_t)readlookup2[addr >> 12] + addr) - (uintptr_t)ram;

        phys_addr = mmutranslate_noabrt(addr, 0) & rammask;
        if (phys_addr != 0xffffffff && mem_addr_is_ram(phys_addr))
                addreadlookup(addr, phys_addr);

        return phys_addr;
}

void mem_invalidate_range(uint32_t start_addr, uint32_t end_addr);

extern uint32_t mem_logical_addr;

void mem_write_ramb_page(uint32_t addr, uint8_t val, page_t *p);
void mem_write_ramw_page(uint32_t addr, uint16_t val, page_t *p);
void mem_write_raml_page(uint32_t addr, uint32_t val, page_t *p);

void mem_reset_page_blocks();

extern mem_mapping_t ram_low_mapping;
extern mem_mapping_t ram_mid_mapping;

void mem_remap_top_256k();
void mem_remap_top_384k();

void mem_flush_write_page(uint32_t addr, uint32_t virt);

void mem_add_bios();

void mem_init();
void mem_alloc();

void mem_set_704kb();

void flushmmucache();
void flushmmucache_nopc();
void flushmmucache_cr3();

void resetreadlookup();

void mmu_invalidate(uint32_t addr);

int loadbios();

extern int purgeable_page_count;
#endif
