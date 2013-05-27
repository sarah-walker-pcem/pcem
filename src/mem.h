extern uint8_t *ram,*rom,*vram,*vrom;
extern uint8_t romext[32768];
extern int readlnum,writelnum;
extern int memspeed[11];
extern int nopageerrors;
extern int cache;
extern int memwaitstate;

void mem_sethandler(uint32_t base, uint32_t size, 
                    uint8_t  (*read_b)(uint32_t addr, void *priv),
                    uint16_t (*read_w)(uint32_t addr, void *priv),
                    uint32_t (*read_l)(uint32_t addr, void *priv),
                    void (*write_b)(uint32_t addr, uint8_t  val, void *priv),
                    void (*write_w)(uint32_t addr, uint16_t val, void *priv),
                    void (*write_l)(uint32_t addr, uint32_t val, void *priv),
                    void *priv);

void mem_removehandler(uint32_t base, uint32_t size, 
                       uint8_t  (*read_b)(uint32_t addr, void *priv),
                       uint16_t (*read_w)(uint32_t addr, void *priv),
                       uint32_t (*read_l)(uint32_t addr, void *priv),
                       void (*write_b)(uint32_t addr, uint8_t  val, void *priv),
                       void (*write_w)(uint32_t addr, uint16_t val, void *priv),
                       void (*write_l)(uint32_t addr, uint32_t val, void *priv),
                       void *priv);
extern int mem_a20_alt;
extern int mem_a20_key;
void mem_a20_recalc();

uint8_t  mem_read_ram(uint32_t addr, void *priv);
uint16_t mem_read_ramw(uint32_t addr, void *priv);
uint32_t mem_read_raml(uint32_t addr, void *priv);

void mem_write_ram(uint32_t addr, uint8_t val, void *priv);
void mem_write_ramw(uint32_t addr, uint16_t val, void *priv);
void mem_write_raml(uint32_t addr, uint32_t val, void *priv);

uint8_t  mem_read_bios(uint32_t addr, void *priv);
uint16_t mem_read_biosw(uint32_t addr, void *priv);
uint32_t mem_read_biosl(uint32_t addr, void *priv);

FILE *romfopen(char *fn, char *mode);

int mem_load_video_bios();
