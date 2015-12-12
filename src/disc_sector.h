void disc_sector_reset(int drive, int side);
void disc_sector_add(int drive, int side, uint8_t c, uint8_t h, uint8_t r, uint8_t n, int rate, uint8_t *data);
void disc_sector_readsector(int drive, int sector, int track, int side, int density, int sector_size);
void disc_sector_writesector(int drive, int sector, int track, int side, int density, int sector_size);
void disc_sector_readaddress(int drive, int sector, int side, int density);
void disc_sector_format(int drive, int sector, int side, int density, uint8_t fill);
void disc_sector_stop();
void disc_sector_poll();
void disc_sector_stop();

extern void (*disc_sector_writeback[2])(int drive, int track);
