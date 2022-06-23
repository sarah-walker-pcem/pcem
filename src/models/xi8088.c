#include "ibm.h"
#include "io.h"
#include "cpu.h"
#include "keyboard_at.h"

#include "xi8088.h"

typedef struct xi8088_t {
        uint8_t turbo;

        int turbo_setting;
        int bios_128kb;
} xi8088_t;

static xi8088_t xi8088;

uint8_t xi8088_turbo_get() { return xi8088.turbo; }

void xi8088_turbo_set(uint8_t value) {
        if (!xi8088.turbo_setting)
                return;

        xi8088.turbo = value;
        if (!value) {
                //                pclog("Xi8088 turbo off\n");
                cpu_set_turbo(0);
        } else {
                //                pclog("Xi8088 turbo on\n");
                cpu_set_turbo(1);
        }
}

int xi8088_bios_128kb() { return xi8088.bios_128kb; }

static void *xi8088_init() {
        xi8088.turbo = 1;
        xi8088.turbo_setting = device_get_config_int("turbo_setting");
        xi8088.bios_128kb = device_get_config_int("bios_128kb");

        return &xi8088;
}

static device_config_t xi8088_config[] = {
        {.name = "turbo_setting",
         .description = "Turbo",
         .type = CONFIG_SELECTION,
         .selection = {{.description = "Always at selected speed", .value = 0},
                       {.description = "BIOS setting + Hotkeys (off during POST)", .value = 1}},
         .default_int = 0},
        {.name = "bios_128kb",
         .description = "BIOS size",
         .type = CONFIG_SELECTION,
         .selection = {{.description = "64KB starting from 0xF0000", .value = 0},
                       {.description = "128KB starting from 0xE0000 (address MSB inverted, last 64KB first)", .value = 1}},
         .default_int = 1},
        {.name = "umb_c0000h_c7fff", .description = "Map 0xc0000-0xc7fff as UMB", .type = CONFIG_BINARY, .default_int = 0},
        {.name = "umb_c8000h_cffff", .description = "Map 0xc8000-0xcffff as UMB", .type = CONFIG_BINARY, .default_int = 0},
        {.name = "umb_d0000h_d7fff", .description = "Map 0xd0000-0xd7fff as UMB", .type = CONFIG_BINARY, .default_int = 0},
        {.name = "umb_d8000h_dffff", .description = "Map 0xd8000-0xdffff as UMB", .type = CONFIG_BINARY, .default_int = 0},
        {.name = "umb_e0000h_e7fff", .description = "Map 0xe0000-0xe7fff as UMB", .type = CONFIG_BINARY, .default_int = 0},
        {.name = "umb_e8000h_effff", .description = "Map 0xe8000-0xeffff as UMB", .type = CONFIG_BINARY, .default_int = 0},
        {.type = -1}};

device_t xi8088_device = {"Xi8088", 0, xi8088_init, NULL, NULL, NULL, NULL, NULL, xi8088_config};
