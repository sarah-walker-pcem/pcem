void gui_enter();

extern int quited;

extern int romspresent[ROM_MAX];
extern int gfx_present[GFX_MAX];

int disc_hdconf();

int settings_configure();

void deviceconfig_open(device_t *device);
int config_sel();

extern int has_been_inited;
