float config_get_float(int is_global, char *head, char *name, float def);
int config_get_int(int is_global, char *head, char *name, int def);
char *config_get_string(int is_global, char *head, char *name, char *def);
void config_set_float(int is_global, char *head, char *name, float val);
void config_set_int(int is_global, char *head, char *name, int val);
void config_set_string(int is_global, char *head, char *name, char *val);

int config_free_section(int is_global, char *head);

void add_config_callback(void(*loadconfig)(), void(*saveconfig)(), void(*onloaded)());

char *get_filename(char *s);
void append_filename(char *dest, char *s1, char *s2, int size);
void append_slash(char *s, int size);
void put_backslash(char *s);
char *get_extension(char *s);

void config_load(int is_global, char *fn);
void config_save(int is_global, char *fn);
void config_dump(int is_global);
void config_free(int is_global);

extern char config_file_default[256];
extern char config_name[256];

#define CFG_MACHINE 0
#define CFG_GLOBAL  1
