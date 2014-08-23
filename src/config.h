void set_config_file(char *s);
int get_config_int(char *head, char *name, int def);
char *get_config_string(char *head, char *name, char *def);
void set_config_int(char *head, char *name, int val);
void set_config_string(char *head, char *name, char *val);

char *get_filename(char *s);
void append_filename(char *dest, char *s1, char *s2, int size);
void put_backslash(char *s);

void config_load(char *fn);
void config_save(char *fn);
void config_dump();
void config_free();

extern char config_file_default[256];
