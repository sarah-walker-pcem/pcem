typedef struct device_t
{
        char name[50];
        void *(*init)();
        void (*close)(void *priv);
        void (*speed_changed)(void *priv);
} device_t;

void device_init();
void device_add(device_t *d);
void device_close_all();
void device_speed_changed();
