int     cga_init();
void    cga_out(uint16_t addr, uint8_t val);
uint8_t cga_in(uint16_t addr);
void    cga_poll();
void    cga_write(uint32_t addr, uint8_t val);
uint8_t cga_read(uint32_t addr);
void    cga_recalctimings();
