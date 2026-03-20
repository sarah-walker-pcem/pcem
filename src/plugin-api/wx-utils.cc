#include "wx-utils.h"

#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#ifdef _WIN32
#include <direct.h>
#include <shlobj.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

int wx_dir_exists(char *path) {
        struct stat st;
        if (stat(path, &st) == 0)
                return (st.st_mode & S_IFDIR) != 0;
        return 0;
}

void wx_get_home_directory(char *path) {
#ifdef _WIN32
        const char *home = getenv("USERPROFILE");
        if (!home)
                home = getenv("HOME");
        if (home) {
                strcpy(path, home);
                int len = strlen(path);
                if (len > 0 && path[len - 1] != '\\' && path[len - 1] != '/')
                        strcat(path, "\\");
        } else {
                strcpy(path, ".\\");
        }
#else
        const char *home = getenv("HOME");
        if (home) {
                strcpy(path, home);
                int len = strlen(path);
                if (len > 0 && path[len - 1] != '/')
                        strcat(path, "/");
        } else {
                strcpy(path, "./");
        }
#endif
}

int wx_create_directory(char *path) {
#ifdef _WIN32
        return _mkdir(path) == 0 || errno == EEXIST;
#else
        return mkdir(path, 0755) == 0 || errno == EEXIST;
#endif
}
