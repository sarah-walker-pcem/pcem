#include <stdio.h>
#include "ibm.h"
#include "rom.h"

FILE *romfopen(char *fn, char *mode)
{
        char s[512];
        strcpy(s, pcempath);
        put_backslash(s);
        strcat(s, fn);
        return fopen(s, mode);
}

int rom_present(char *fn)
{
        FILE *f;
        char s[512];
        
        strcpy(s, pcempath);
        put_backslash(s);
        strcat(s, fn);
        f = fopen(s, "rb");
        if (f)
        {
                fclose(f);
                return 1;
        }
        return 0;
}
