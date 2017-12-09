extern void lpt_init();
extern void lpt1_init(uint16_t port);
extern void lpt1_remove();
extern void lpt2_init(uint16_t port);
extern void lpt2_remove();
extern void lpt2_remove_ams();

void lpt1_write(uint16_t port, uint8_t val, void *priv);
uint8_t lpt1_read(uint16_t port, void *priv);

void lpt1_device_init();
void lpt1_device_close();

char *lpt_device_get_name(int id);
char *lpt_device_get_internal_name(int id);

extern char lpt1_device_name[16];

typedef struct
{
        char name[80];
        void *(*init)();
        void (*close)(void *p);
        void (*write_data)(uint8_t val, void *p);
        void (*write_ctrl)(uint8_t val, void *p);
        uint8_t (*read_status)(void *p);
} lpt_device_t;
