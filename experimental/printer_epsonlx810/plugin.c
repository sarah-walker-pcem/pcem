#include "config.h"

#include <pcem/plugin.h>
#include <pcem/config.h>
#include <string.h>

#include "lpt_epsonlx810.h"

extern char pcem_path[512];

LPT_DEVICE l_epsonlx810 = {"Epson LX-810 Printer", "lpt_epsonlx810", &lpt_epsonprinter_device};

#define safe_strncpy(a, b, n)                                                                                                    \
        do {                                                                                                                     \
                strncpy((a), (b), (n)-1);                                                                                        \
                (a)[(n)-1] = 0;                                                                                                  \
        } while (0)

char printer_path[512];

void set_printer_path(char *s) {
        safe_strncpy(printer_path, s, 512);
        append_slash(printer_path, 512);
}

void load_config() {
        char *cfg_printer_path = config_get_string(CFG_GLOBAL, "Paths", "printer_path", 0);

        if (cfg_printer_path)
                set_printer_path(cfg_printer_path);
}

void save_config() { config_set_string(CFG_GLOBAL, "Paths", "printer_path", printer_path); }

void init_config() {
        char s[512];
        append_filename(s, pcem_path, "printer/", 512);
        set_printer_path(s);
}

PLUGIN_INIT(printer_epsonlx810) {
        add_config_callback(load_config, save_config, init_config);
        pcem_add_lpt(&l_epsonlx810);
}
