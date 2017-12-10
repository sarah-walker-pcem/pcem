#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ibm.h"
#include "config.h"

char config_file_default[256];
char config_name[256];

static char config_file[256];

typedef struct list_t
{
        struct list_t *next;
} list_t;

static list_t global_config_head;
static list_t machine_config_head;

typedef struct section_t
{
        struct list_t list;
        
        char name[256];
        
        struct list_t entry_head;
} section_t;

typedef struct entry_t
{
        struct list_t list;
        
        char name[256];
        char data[256];
} entry_t;

#define list_add(new, head)                             \
        {                                               \
                struct list_t *next = head;             \
                                                        \
                while (next->next)                      \
                        next = next->next;              \
                                                        \
                (next)->next = new;                     \
                (new)->next = NULL;                     \
        }

void config_dump(int is_global)
{
        section_t *current_section;
        list_t *head = is_global ? &global_config_head : &machine_config_head;
                
        pclog("Config data :\n");
        
        current_section = (section_t *)head->next;
        
        while (current_section)
        {
                entry_t *current_entry;
                
                pclog("[%s]\n", current_section->name);
                
                current_entry = (entry_t *)current_section->entry_head.next;
                
                while (current_entry)
                {
                        pclog("%s = %s\n", current_entry->name, current_entry->data);

                        current_entry = (entry_t *)current_entry->list.next;
                }

                current_section = (section_t *)current_section->list.next;
        }
}

void config_free(int is_global)
{
        section_t *current_section;
        list_t *head = is_global ? &global_config_head : &machine_config_head;
        current_section = (section_t *)head->next;
        
        while (current_section)
        {
                section_t *next_section = (section_t *)current_section->list.next;
                entry_t *current_entry;
                
                current_entry = (entry_t *)current_section->entry_head.next;
                
                while (current_entry)
                {
                        entry_t *next_entry = (entry_t *)current_entry->list.next;
                        
                        free(current_entry);
                        current_entry = next_entry;
                }

                free(current_section);                
                current_section = next_section;
        }
}

int config_free_section(int is_global, char *name)
{
        section_t *current_section, *prev_section;
        list_t *head = is_global ? &global_config_head : &machine_config_head;
        current_section = (section_t *)head->next;
        prev_section = 0;

        while (current_section)
        {
                section_t *next_section = (section_t *)current_section->list.next;
                if (!strcmp(current_section->name, name))
                {
                        entry_t *current_entry;

                        current_entry = (entry_t *)current_section->entry_head.next;

                        while (current_entry)
                        {
                                entry_t *next_entry = (entry_t *)current_entry->list.next;

                                free(current_entry);
                                current_entry = next_entry;
                        }

                        free(current_section);
                        if (!prev_section)
                                head->next = (list_t*)next_section;
                        else
                                prev_section->list.next = (list_t*)next_section;
                        return 1;
                }
                prev_section = current_section;
                current_section = next_section;
        }
        return 0;
}

void config_load(int is_global, char *fn)
{
        FILE *f = fopen(fn, "rt");
        section_t *current_section;
        list_t *head = is_global ? &global_config_head : &machine_config_head;
        
        memset(head, 0, sizeof(list_t));

        current_section = malloc(sizeof(section_t));
        memset(current_section, 0, sizeof(section_t));
        list_add(&current_section->list, head);

        if (!f)
                return;

        while (1)
        {
                int c;
                char buffer[256];

                fgets(buffer, 255, f);
                if (feof(f)) break;
                
                c = 0;
                
                while (buffer[c] == ' ' && buffer[c])
                      c++;

                if (!buffer[c]) continue;
                
                if (buffer[c] == '#') /*Comment*/
                        continue;

                if (buffer[c] == '[') /*Section*/
                {
                        section_t *new_section;
                        char name[256];
                        int d = 0;
                        
                        c++;
                        while (buffer[c] != ']' && buffer[c])
                                name[d++] = buffer[c++];

                        if (buffer[c] != ']')
                                continue;
                        name[d] = 0;
                        
                        new_section = malloc(sizeof(section_t));
                        memset(new_section, 0, sizeof(section_t));
                        strncpy(new_section->name, name, 256);
                        list_add(&new_section->list, head);
                        
                        current_section = new_section;
                        
//                        pclog("New section : %s %p\n", name, (void *)current_section);
                }
                else
                {
                        entry_t *new_entry;
                        char name[256];
                        int d = 0, data_pos;

                        while (buffer[c] != '=' && buffer[c] != ' ' && buffer[c])
                                name[d++] = buffer[c++];
                
                        if (!buffer[c]) continue;
                        name[d] = 0;

                        while ((buffer[c] == '=' || buffer[c] == ' ') && buffer[c])
                                c++;
                        
                        if (!buffer[c]) continue;
                        
                        data_pos = c;
                        while (buffer[c])
                        {
                                if (buffer[c] == '\n')
                                        buffer[c] = 0;
                                c++;
                        }

                        new_entry = malloc(sizeof(entry_t));
                        memset(new_entry, 0, sizeof(entry_t));
                        strncpy(new_entry->name, name, 256);
                        strncpy(new_entry->data, &buffer[data_pos], 256);
                        list_add(&new_entry->list, &current_section->entry_head);

//                        pclog("New data under section [%s] : %s = %s\n", current_section->name, new_entry->name, new_entry->data);
                }
        }
        
        fclose(f);
}



void config_new()
{
        FILE *f = fopen(config_file, "wt");
        fclose(f);
}

static section_t *find_section(char *name, int is_global)
{
        section_t *current_section;
        char blank[] = "";
        list_t *head = is_global ? &global_config_head : &machine_config_head;
                
        current_section = (section_t *)head->next;
        if (!name)
                name = blank;

        while (current_section)
        {
                if (!strncmp(current_section->name, name, 256))
                        return current_section;
                
                current_section = (section_t *)current_section->list.next;
        }
        return NULL;
}

static entry_t *find_entry(section_t *section, char *name)
{
        entry_t *current_entry;
        
        current_entry = (entry_t *)section->entry_head.next;
        
        while (current_entry)
        {
                if (!strncmp(current_entry->name, name, 256))
                        return current_entry;

                current_entry = (entry_t *)current_entry->list.next;
        }
        return NULL;
}

static section_t *create_section(char *name, int is_global)
{
        section_t *new_section = malloc(sizeof(section_t));
        list_t *head = is_global ? &global_config_head : &machine_config_head;
        
        memset(new_section, 0, sizeof(section_t));
        strncpy(new_section->name, name, 256);
        list_add(&new_section->list, head);
        
        return new_section;
}

static entry_t *create_entry(section_t *section, char *name)
{
        entry_t *new_entry = malloc(sizeof(entry_t));
        memset(new_entry, 0, sizeof(entry_t));
        strncpy(new_entry->name, name, 256);
        list_add(&new_entry->list, &section->entry_head);
        
        return new_entry;
}
        
int config_get_int(int is_global, char *head, char *name, int def)
{
        section_t *section;
        entry_t *entry;
        int value;

        section = find_section(head, is_global);
        
        if (!section)
                return def;
                
        entry = find_entry(section, name);

        if (!entry)
                return def;
        
        sscanf(entry->data, "%i", &value);
        
        return value;
}

float config_get_float(int is_global, char *head, char *name, float def)
{
        section_t *section;
        entry_t *entry;
        float value;

        section = find_section(head, is_global);

        if (!section)
                return def;

        entry = find_entry(section, name);

        if (!entry)
                return def;

        sscanf(entry->data, "%f", &value);

        return value;
}

char *config_get_string(int is_global, char *head, char *name, char *def)
{
        section_t *section;
        entry_t *entry;

        section = find_section(head, is_global);
        
        if (!section)
                return def;
                
        entry = find_entry(section, name);

        if (!entry)
                return def;
       
        return entry->data; 
}

void config_set_int(int is_global, char *head, char *name, int val)
{
        section_t *section;
        entry_t *entry;

        section = find_section(head, is_global);
        
        if (!section)
                section = create_section(head, is_global);
                
        entry = find_entry(section, name);

        if (!entry)
                entry = create_entry(section, name);

        sprintf(entry->data, "%i", val);
}

void config_set_float(int is_global, char *head, char *name, float val)
{
        section_t *section;
        entry_t *entry;

        section = find_section(head, is_global);

        if (!section)
                section = create_section(head, is_global);

        entry = find_entry(section, name);

        if (!entry)
                entry = create_entry(section, name);

        sprintf(entry->data, "%f", val);
}

void config_set_string(int is_global, char *head, char *name, char *val)
{
        section_t *section;
        entry_t *entry;

        section = find_section(head, is_global);
        
        if (!section)
                section = create_section(head, is_global);
                
        entry = find_entry(section, name);

        if (!entry)
                entry = create_entry(section, name);

        strncpy(entry->data, val, 256);
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

void append_slash(char *s, int size)
{
        int c = strlen(s)-1;
        if (s[c] != '/' && s[c] != '\\')
        {
                if (c < size-2)
                        strcat(s, "/");
                else
                        s[c] = '/';
        }
}

void put_backslash(char *s)
{
        int c = strlen(s) - 1;
        if (s[c] != '/' && s[c] != '\\')
        {
                s[c+1] = '/';
                s[c+2] = 0;
        }
}

char *get_extension(char *s)
{
        int c = strlen(s) - 1;

        if (c <= 0)
                return s;
        
        while (c && s[c] != '.')
                c--;
                
        if (!c)
                return &s[strlen(s)];

        return &s[c+1];
}               

void config_save(int is_global, char *fn)
{
        FILE *f = fopen(fn, "wt");
        section_t *current_section;
        list_t *head = is_global ? &global_config_head : &machine_config_head;
                
        current_section = (section_t *)head->next;
        
        while (current_section)
        {
                entry_t *current_entry;
                
                if (current_section->name[0])
                        fprintf(f, "\n[%s]\n", current_section->name);
                
                current_entry = (entry_t *)current_section->entry_head.next;
                
                while (current_entry)
                {
                        fprintf(f, "%s = %s\n", current_entry->name, current_entry->data);

                        current_entry = (entry_t *)current_entry->list.next;
                }

                current_section = (section_t *)current_section->list.next;
        }
        
        fclose(f);
}
