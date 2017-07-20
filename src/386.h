extern void cpu_386_flags_extract();
extern void cpu_386_flags_rebuild();

void pmodeint(int num, int soft);
void loadcscall(uint16_t seg);
void loadcsjmp(uint16_t seg, uint32_t oxpc);
void pmoderetf(int is32, uint16_t off);
void pmodeiret(int is32);
void x86_int_sw(int num);
int x86_int_sw_rm(int num);

int divl(uint32_t val);
int idivl(int32_t val);
