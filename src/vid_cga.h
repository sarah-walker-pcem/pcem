int     cga_init();
void    cga_out(uint16_t addr, uint8_t val, void *priv);
uint8_t cga_in(uint16_t addr, void *priv);
void    cga_poll();
void    cga_write(uint32_t addr, uint8_t val, void *priv);
uint8_t cga_read(uint32_t addr, void *priv);
void    cga_recalctimings();
