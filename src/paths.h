extern int num_roms_paths;
extern char pcem_path[512];
extern char configs_path[512];
extern char nvr_path[512];
extern char logs_path[512];
extern char screenshots_path[512];

void get_pcem_path(char *s, int size);
char get_path_separator();
void paths_init();
int dir_exists(char* path);

int get_roms_path(int p, char* s, int size);

/* set the default paths to make them permanent */
void set_default_roms_paths(char *s);
void set_default_nvr_path(char *s);
void set_default_logs_path(char *s);
void set_default_configs_path(char *s);
void set_default_screenshots_path(char *s);

/* set the paths temporarily for this session */
void set_roms_paths(char* path);
void set_nvr_path(char *s);
void set_logs_path(char *s);
void set_configs_path(char *s);
void set_screenshots_path(char *s);
