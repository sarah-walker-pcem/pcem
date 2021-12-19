#define MODEL_AT      1
#define MODEL_PS2     2
#define MODEL_AMSTRAD 4
#define MODEL_OLIM24  8
#define MODEL_HAS_IDE 0x10
#define MODEL_MCA     0x20
#define MODEL_PCI     0x40

/*Machine has no integrated graphics*/
#define MODEL_GFX_NONE       0x000
/*Machine has integrated graphics that can not be disabled*/
#define MODEL_GFX_FIXED      0x100
/*Machine has integrated graphics that can be disabled by jumpers or switches*/
#define MODEL_GFX_DISABLE_HW 0x200
/*Machine has integrated graphics that can be disabled through software*/
#define MODEL_GFX_DISABLE_SW 0x300
#define MODEL_GFX_MASK       0x300

typedef struct
{
        char name[64];
        int id;
        char internal_name[24];
        struct
        {
                char name[8];
                CPU *cpus;
        } cpu[5];
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
int model_getromset_from_model(int model);
int model_getmodel(int romset);
char *model_getname();
char *model_get_internal_name();
int model_get_model_from_internal_name(char *s);
void model_init();
struct device_t *model_getdevice(int model);
int model_has_fixed_gfx(int model);
int model_has_optional_gfx(int model);
