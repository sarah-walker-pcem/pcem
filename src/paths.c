#include "paths.h"
#include "config.h"
#include <string.h>
#include "ibm.h"

#define safe_strncpy(a,b,n) do { strncpy((a),(b),(n)-1); (a)[(n)-1] = 0; } while (0)

char default_roms_paths[4096];
char default_nvr_path[512];
char default_configs_path[512];
char default_logs_path[512];
char default_screenshots_path[512];

/* the number of roms paths */
int num_roms_paths;
char roms_paths[4096];
/* this is where pcem.cfg is */
char pcem_path[512];
/* this is where nvr-files as stored */
char nvr_path[512];
/* this is where configs as stored */
char configs_path[512];
/* this is where log-files as stored */
char logs_path[512];
/* this is where screenshots as stored */
char screenshots_path[512];

char get_path_separator()
{
#ifdef _WIN32
        return ';';
#else
        return ':';
#endif
}

int get_roms_path(int pos, char *s, int size)
{
        int j, i, z, len;
        char path_separator;
        
        path_separator = get_path_separator();
        len = strlen(roms_paths);
        j = 0;
        for (i = 0; i < len; i++)
        {
                if (roms_paths[i] == path_separator || i == len-1)
                {
                        if ((pos--) == 0)
                        {
                                z = (i-j) + ((i == len-1) ? 1 : 0);
                                safe_strncpy(s, roms_paths+j, size);
                                s[(size-1 < z) ? size-1 : z] = 0;
                                return 1;
                        }
                        j = i+1;
                }
        }
        return 0;
}

void set_roms_paths(char *path)
{
        char s[512];
        int j, i, z, len;
        char path_separator[2];
        
        roms_paths[0] = 0;
        path_separator[0] = get_path_separator();
        path_separator[1] = 0;
        len = strlen(path);
        j = 0;
        num_roms_paths = 0;
        for (i = 0; i < len; i++)
        {
                if (path[i] == path_separator[0] || i == len-1)
                {
                        z = (i-j) + ((i == len-1) ? 1 : 0) + 1;
                        safe_strncpy(s, path+j, z);
                        s[(511 < z) ? 511 : z] = 0;
                        append_slash(s, 512);
                        if (dir_exists(s))
                        {
                                if (num_roms_paths > 0)
                                        strcat(roms_paths, path_separator);
                                strcat(roms_paths, s);
                                num_roms_paths++;
                        }
                        j = i+1;
                }
        }
}

void set_nvr_path(char *s)
{
        safe_strncpy(nvr_path, s, 512);
        append_slash(nvr_path, 512);
}

void set_logs_path(char *s)
{
        safe_strncpy(logs_path, s, 512);
        append_slash(logs_path, 512);
}

void set_configs_path(char *s)
{
        safe_strncpy(configs_path, s, 512);
        append_slash(configs_path, 512);
}

void set_screenshots_path(char *s)
{
        safe_strncpy(screenshots_path, s, 512);
        append_slash(screenshots_path, 512);
}

/* set the default roms paths, this makes them permanent */
void set_default_roms_paths(char *s)
{
        safe_strncpy(default_roms_paths, s, 4096);
        set_roms_paths(s);
}

/* set the default nvr path, this makes it permanent */
void set_default_nvr_path(char *s)
{
        safe_strncpy(default_nvr_path, s, 512);
        set_nvr_path(s);
}

/* set the default logs path, this makes it permanent */
void set_default_logs_path(char *s)
{
        safe_strncpy(default_logs_path, s, 512);
        set_logs_path(s);
}

/* set the default configs path, this makes it permanent */
void set_default_configs_path(char *s)
{
        safe_strncpy(default_configs_path, s, 512);
        set_configs_path(s);
}

/* set the default screenshots path, this makes it permanent */
void set_default_screenshots_path(char *s)
{
        safe_strncpy(default_screenshots_path, s, 512);
        set_screenshots_path(s);
}

void paths_loadconfig()
{
        char *cfg_roms_paths = config_get_string(CFG_GLOBAL, "Paths", "roms_paths", 0);
        char *cfg_nvr_path = config_get_string(CFG_GLOBAL, "Paths", "nvr_path", 0);
        char *cfg_configs_path = config_get_string(CFG_GLOBAL, "Paths", "configs_path", 0);
        char *cfg_logs_path = config_get_string(CFG_GLOBAL, "Paths", "logs_path", 0);
        char *cfg_screenshots_path = config_get_string(CFG_GLOBAL, "Paths", "screenshots_path", 0);

        if (cfg_roms_paths)
                safe_strncpy(default_roms_paths, cfg_roms_paths, 4096);
        if (cfg_nvr_path)
                safe_strncpy(default_nvr_path, cfg_nvr_path, 512);
        if (cfg_configs_path)
                safe_strncpy(default_configs_path, cfg_configs_path, 512);
        if (cfg_logs_path)
                safe_strncpy(default_logs_path, cfg_logs_path, 512);
        if (cfg_screenshots_path)
                safe_strncpy(default_screenshots_path, cfg_screenshots_path, 512);
}

void paths_saveconfig()
{
        config_set_string(CFG_GLOBAL, "Paths", "roms_paths", default_roms_paths);
        config_set_string(CFG_GLOBAL, "Paths", "nvr_path", default_nvr_path);
        config_set_string(CFG_GLOBAL, "Paths", "configs_path", default_configs_path);
        config_set_string(CFG_GLOBAL, "Paths", "logs_path", default_logs_path);
        config_set_string(CFG_GLOBAL, "Paths", "screenshots_path", default_screenshots_path);
}

void paths_onconfigloaded()
{
        if (strlen(default_roms_paths) > 0)
                set_roms_paths(default_roms_paths);

        if (strlen(default_nvr_path) > 0)
                set_nvr_path(default_nvr_path);

        if (strlen(default_configs_path) > 0)
                set_configs_path(default_configs_path);

        if (strlen(default_logs_path) > 0)
                set_logs_path(default_logs_path);

        if (strlen(default_screenshots_path) > 0)
                set_screenshots_path(default_screenshots_path);

        pclog("path = %s\n", pcem_path);
}

/* initialize default paths */
void paths_init()
{
        char s[512];
        char *p;

        get_pcem_path(pcem_path, 512);
        /* pcem_path may be path to executable */
        p=get_filename(pcem_path);
        *p=0;

        /* set up default paths for this session */
        append_filename(s, pcem_path, "roms/", 512);
        set_roms_paths(s);
        append_filename(s, pcem_path, "nvr/", 512);
        set_nvr_path(s);
        append_filename(s, pcem_path, "configs/", 512);
        set_configs_path(s);
        append_filename(s, pcem_path, "screenshots/", 512);
        set_screenshots_path(s);
        set_logs_path(pcem_path);

        add_config_callback(paths_loadconfig, paths_saveconfig, paths_onconfigloaded);

}
