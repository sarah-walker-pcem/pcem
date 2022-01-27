#include <pcem/plugin.h>
#include <pcem/unsafe/config.h>
#include <string.h>

#include "lpt_epsonlx810.h"

char pcem_path[512];
char base_path[512];
LPT_DEVICE l_epsonlx810 = { "Epson LX-810 Printer", "lpt_epsonlx810", &lpt_epsonprinter_device };

#define safe_strncpy(a,b,n) do { strncpy((a),(b),(n)-1); (a)[(n)-1] = 0; } while (0)

char printer_path[512];

void set_printer_path(char *s)
{
        safe_strncpy(printer_path, s, 512);
        append_slash(printer_path, 512);
}

PLUGIN_INIT(printer_epsonlx810)
{
        char *cfg_printer_path = config_get_string(CFG_GLOBAL, "Paths", "printer_path", 0);

        if(cfg_printer_path)
        {
                safe_strncpy(printer_path, cfg_printer_path, 512);
        }
        else
        {
                char s[512];
#ifdef linux
                append_filename(s, pcem_path, "printer/", 512);
#else
                append_filename(s, base_path, "printer/", 512);
#endif
                set_printer_path(s);
                config_set_string(CFG_GLOBAL, "Paths", "printer_path", printer_path);
        }

        pcem_add_lpt(&l_epsonlx810);
}
