#include <stdio.h>
#include <string.h>
#include "config.h"

static char config_file[256];

void set_config_file(char *s)
{
        strcpy(config_file, s);
}

void config_new()
{
        FILE *f = fopen(config_file, "wt");
        fclose(f);
}

int get_config_int(char *head, char *name, int def)
{
        char buffer[256];
        char name2[256];
        FILE *f = fopen(config_file, "rt");
        int c, d;
        int res = def;
        
        if (!f)
           return def;
           
//        pclog("Searching for %s\n", name);
        
        while (1)
        {
                fgets(buffer, 255, f);
                if (feof(f)) break;
                
                c = d = 0;
                
                while (buffer[c] == ' ' && buffer[c])
                      c++;
                      
                if (!buffer[c]) continue;
                
                while (buffer[c] != '=' && buffer[c] != ' ' && buffer[c])
                        name2[d++] = buffer[c++];
                
                if (!buffer[c]) continue;
                name2[d] = 0;
                
//                pclog("Comparing %s and %s\n", name, name2);
                if (strcmp(name, name2)) continue;
//                pclog("Found!\n");

                while ((buffer[c] == '=' || buffer[c] == ' ') && buffer[c])                
                        c++;
                        
                if (!buffer[c]) continue;
                
                sscanf(&buffer[c], "%i", &res);
//                pclog("Reading value - %i\n", res);
                break;
        }
        
        fclose(f);
        return res;
}

char config_return_string[256];

char *get_config_string(char *head, char *name, char *def)
{
        char buffer[256];
        char name2[256];
        FILE *f = fopen(config_file, "rt");
        int c, d;
        
        strcpy(config_return_string, def);
        
        if (!f)
           return config_return_string;
           
//        pclog("Searching for %s\n", name);
        
        while (1)
        {
                fgets(buffer, 255, f);
                if (feof(f)) break;
                
                c = d = 0;
                
                while (buffer[c] == ' ' && buffer[c])
                      c++;
                      
                if (!buffer[c]) continue;
                
                while (buffer[c] != '=' && buffer[c] != ' ' && buffer[c])
                        name2[d++] = buffer[c++];
                
                if (!buffer[c]) continue;
                name2[d] = 0;
                
//                pclog("Comparing %s and %s\n", name, name2);
                if (strcmp(name, name2)) continue;
//                pclog("Found!\n");

                while ((buffer[c] == '=' || buffer[c] == ' ') && buffer[c])                
                        c++;
                        
                if (!buffer[c]) continue;
                
                strcpy(config_return_string, &buffer[c]);
                
                c = strlen(config_return_string) - 1;
//                pclog("string len %i\n", c);
                while (config_return_string[c] <= 32 && config_return_string[c]) 
                      config_return_string[c--] = 0;

//                pclog("Reading value - %s\n", config_return_string);
                break;
        }
        
        fclose(f);
        return config_return_string;
}

void set_config_int(char *head, char *name, int val)
{
        FILE *f = fopen(config_file, "at");
//        if (!f) pclog("set_config_int - !f\n");
        fprintf(f, "%s = %i\n", name, val);
//        pclog("Write %s = %i\n", name, val);
        fclose(f);
//        pclog("fclose\n");
}

void set_config_string(char *head, char *name, char *val)
{
        FILE *f = fopen(config_file, "at");
//        if (!f) pclog("set_config_string - !f\n");
        fprintf(f, "%s = %s\n", name, val);
//        pclog("Write %s = %s\n", name, val);
        fclose(f);
}

char *get_filename(char *s)
{
        int c = strlen(s) - 1;
        while (c > 0)
        {
                if (s[c] == '/' || s[c] == '\\')
                   return &s[c+1];
                c--;
        }
        return s;
}

void append_filename(char *dest, char *s1, char *s2, int size)
{
        sprintf(dest, "%s%s", s1, s2);
}

void put_backslash(char *s)
{
        int c = strlen(s) - 1;
        if (s[c] != '/' && s[c] != '\\')
           s[c] = '/';
}
