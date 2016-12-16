#define MODEL_AT      1
#define MODEL_PS2     2
#define MODEL_AMSTRAD 4
#define MODEL_OLIM24  8

typedef struct
{
        char name[24];
        int id;
        struct
        {
                char name[8];
                CPU *cpus;
        } cpu[4];
        int fixed_gfxcard;
        int flags;
        int min_ram, max_ram;
        int ram_granularity;
        void (*init)();
        struct device_t *device;
} MODEL;

extern MODEL models[];

extern int model;

int model_count();
int model_getromset();
int model_getmodel(int romset);
char *model_getname();
void model_init();
struct device_t *model_getdevice(int model);
