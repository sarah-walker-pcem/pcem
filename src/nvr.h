void nvr_init();

extern int enable_sync;

extern int nvr_dosave;

void loadnvr();
void savenvr();

void nvr_recalc();

FILE *nvrfopen(char *fn, char *mode);

extern uint8_t nvrram[128];
extern int nvrmask;
extern int oldromset;
