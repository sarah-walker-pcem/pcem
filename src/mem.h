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

extern uint8_t *ram,*rom;
extern uint8_t romext[32768];
extern int readlnum,writelnum;
extern int memspeed[11];
extern int nopageerrors;
extern int cache;
extern int memwaitstate;

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
void mem_writeb_phys(uint32_t addr, uint8_t val);

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
