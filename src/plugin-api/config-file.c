#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config-file.h"

typedef struct config_file_entry_t {
        char *name;
        char *value;
        struct config_file_entry_t *next;
} config_file_entry_t;

typedef struct config_file_t {
        config_file_entry_t *entries;
} config_file_t;

static char *trim(char *s) {
        char *end;

        while (*s && isspace((unsigned char)*s))
                s++;
        end = s + strlen(s);
        while (end > s && isspace((unsigned char)end[-1]))
                end--;
        *end = 0;
        return s;
}

static void unescape_value(char *s) {
        char *src = s, *dst = s;
        int quoted = 0;

        if (*src == '"') {
                quoted = 1;
                src++;
        }
        while (*src) {
                if (*src == '\\' && src[1]) {
                        src++;
                        switch (*src) {
                        case 'n':
                                *dst++ = '\n';
                                break;
                        case 't':
                                *dst++ = '\t';
                                break;
                        case 'r':
                                *dst++ = '\r';
                                break;
                        default:
                                *dst++ = *src;
                                break;
                        }
                        src++;
                } else if (quoted && *src == '"' && !src[1]) {
                        src++;
                } else
                        *dst++ = *src++;
        }
        *dst = 0;
}

static config_file_entry_t *find_entry(void *config, const char *name) {
        config_file_entry_t *entry;

        if (!config)
                return NULL;
        for (entry = ((config_file_t *)config)->entries; entry; entry = entry->next) {
                if (!strcmp(entry->name, name))
                        return entry;
        }
        return NULL;
}

static config_file_entry_t *get_or_create_entry(void *config, const char *name) {
        config_file_entry_t *entry = find_entry(config, name);
        config_file_entry_t **tail;

        if (entry || !config)
                return entry;
        entry = calloc(1, sizeof(config_file_entry_t));
        entry->name = strdup(name);
        entry->value = strdup("");
        for (tail = &((config_file_t *)config)->entries; *tail; tail = &(*tail)->next)
                ;
        *tail = entry;
        return entry;
}

static void write_value(FILE *f, const char *value) {
        int len = strlen(value);
        int quote = len && (isspace((unsigned char)value[0]) || isspace((unsigned char)value[len - 1]) || value[0] == '"');

        if (quote)
                fputc('"', f);
        for (; *value; value++) {
                switch (*value) {
                case '\\':
                        fputs("\\\\", f);
                        break;
                case '\n':
                        fputs("\\n", f);
                        break;
                case '\t':
                        fputs("\\t", f);
                        break;
                case '\r':
                        fputs("\\r", f);
                        break;
                case '"':
                        fputs(quote ? "\\\"" : "\"", f);
                        break;
                default:
                        fputc(*value, f);
                        break;
                }
        }
        if (quote)
                fputc('"', f);
}

void *config_file_new(void) { return calloc(1, sizeof(config_file_t)); }

int config_file_save(void *config, const char *path) {
        config_file_entry_t *entry, *other;
        FILE *f;

        if (!config)
                return 0;
        f = fopen(path, "w");
        if (!f)
                return 0;

        for (entry = ((config_file_t *)config)->entries; entry; entry = entry->next) {
                if (!strchr(entry->name, '/')) {
                        fprintf(f, "%s = ", entry->name);
                        write_value(f, entry->value);
                        fputc('\n', f);
                }
        }
        for (entry = ((config_file_t *)config)->entries; entry; entry = entry->next) {
                const char *slash = strchr(entry->name, '/');
                int group_len, seen = 0;

                if (!slash)
                        continue;
                group_len = slash - entry->name;
                for (other = ((config_file_t *)config)->entries; other != entry; other = other->next) {
                        if (!strncmp(other->name, entry->name, group_len) && other->name[group_len] == '/') {
                                seen = 1;
                                break;
                        }
                }
                if (seen)
                        continue;
                fprintf(f, "\n[%.*s]\n", group_len, entry->name);
                for (other = entry; other; other = other->next) {
                        if (!strncmp(other->name, entry->name, group_len) && other->name[group_len] == '/') {
                                fprintf(f, "%s = ", other->name + group_len + 1);
                                write_value(f, other->value);
                                fputc('\n', f);
                        }
                }
        }
        fclose(f);
        return 1;
}

void config_file_set_string(void *config, const char *name, const char *value) {
        config_file_entry_t *entry = get_or_create_entry(config, name);

        if (!entry)
                return;
        free(entry->value);
        entry->value = strdup(value ? value : "");
}

void config_file_set_int(void *config, const char *name, int value) {
        char s[32];

        snprintf(s, sizeof(s), "%d", value);
        config_file_set_string(config, name, s);
}

void config_file_set_float(void *config, const char *name, float value) {
        char s[64];

        snprintf(s, sizeof(s), "%g", value);
        config_file_set_string(config, name, s);
}

void config_file_set_bool(void *config, const char *name, int value) { config_file_set_string(config, name, value ? "true" : "false"); }

void *config_file_load(const char *path) {
        char line[4096];
        char group[256] = "";
        config_file_t *config;
        config_file_entry_t **tail;
        FILE *f = fopen(path, "r");

        if (!f)
                return NULL;

        config = calloc(1, sizeof(config_file_t));
        tail = &config->entries;
        while (fgets(line, sizeof(line), f)) {
                char *s = trim(line);
                char *eq, *name, *value;
                config_file_entry_t *entry;

                if (!*s || *s == '#' || *s == ';')
                        continue;
                if (*s == '[') {
                        char *end = strchr(s, ']');
                        if (end) {
                                *end = 0;
                                snprintf(group, sizeof(group), "%s", trim(s + 1));
                        }
                        continue;
                }
                eq = strchr(s, '=');
                if (!eq)
                        continue;
                *eq = 0;
                name = trim(s);
                value = trim(eq + 1);
                unescape_value(value);

                entry = calloc(1, sizeof(config_file_entry_t));
                if (group[0]) {
                        entry->name = malloc(strlen(group) + strlen(name) + 2);
                        sprintf(entry->name, "%s/%s", group, name);
                } else
                        entry->name = strdup(name);
                entry->value = strdup(value);

                if (find_entry(config, entry->name)) {
                        free(entry->name);
                        free(entry->value);
                        free(entry);
                        continue;
                }
                *tail = entry;
                tail = &entry->next;
        }
        fclose(f);
        return config;
}

int config_file_get_string(void *config, const char *name, char *dst, int size, const char *def) {
        config_file_entry_t *entry = find_entry(config, name);
        const char *value = entry ? entry->value : (def ? def : "");

        strncpy(dst, value, size - 1);
        dst[size - 1] = 0;
        return entry != NULL;
}

int config_file_get_int(void *config, const char *name, int *dst, int def) {
        config_file_entry_t *entry = find_entry(config, name);
        char *end;
        long value;

        if (entry) {
                value = strtol(entry->value, &end, 0);
                if (end != entry->value && !*trim(end)) {
                        *dst = (int)value;
                        return 1;
                }
        }
        *dst = def;
        return 0;
}

int config_file_get_float(void *config, const char *name, float *dst, float def) {
        config_file_entry_t *entry = find_entry(config, name);
        char *end;
        double value;

        if (entry) {
                value = strtod(entry->value, &end);
                if (end != entry->value && !*trim(end)) {
                        *dst = (float)value;
                        return 1;
                }
        }
        *dst = def;
        return 0;
}

int config_file_get_bool(void *config, const char *name, int *dst, int def) {
        config_file_entry_t *entry = find_entry(config, name);
        int value;

        if (!entry)
                return 1;
        if (!strncasecmp(entry->value, "true", 4))
                *dst = 1;
        else if (!strncasecmp(entry->value, "false", 5))
                *dst = 0;
        else if (config_file_get_int(config, name, &value, def))
                *dst = value != 0;
        else {
                *dst = def;
                return 0;
        }
        return 1;
}

int config_file_has_entry(void *config, const char *name) { return find_entry(config, name) != NULL; }

void config_file_free(void *config) {
        config_file_entry_t *entry, *next;

        if (!config)
                return;
        for (entry = ((config_file_t *)config)->entries; entry; entry = next) {
                next = entry->next;
                free(entry->name);
                free(entry->value);
                free(entry);
        }
        free(config);
}
