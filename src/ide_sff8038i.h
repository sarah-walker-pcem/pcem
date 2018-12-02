typedef struct sff_busmaster_t
{
        uint8_t command;
        uint8_t status;
        uint32_t ptr, ptr_cur;
        int count;
        uint32_t addr;
        int eot;
} sff_busmaster_t;

uint8_t sff_bus_master_read(uint16_t port, void *p);
void sff_bus_master_write(uint16_t port, uint8_t val, void *p);
void sff_bus_master_init(sff_busmaster_t *busmaster);
