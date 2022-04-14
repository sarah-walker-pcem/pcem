#ifndef _PCEM_DEVICES_H_
#define _PCEM_DEVICES_H_

#include <pcem/cpu.h>

#define CONFIG_STRING 0
#define CONFIG_INT 1
#define CONFIG_BINARY 2
#define CONFIG_SELECTION 3
#define CONFIG_MIDI 4

enum
{
        DEVICE_NOT_WORKING = 1, /*Device does not currently work correctly and will be disabled in a release build*/
        DEVICE_AT = 2,          /*Device requires an AT-compatible system*/
        DEVICE_MCA = 0x20,      /*Device requires an MCA system*/
        DEVICE_PCI = 0x40,      /*Device requires a PCI system*/
        DEVICE_PS1 = 0x80       /*Device is only for IBM PS/1 Model 2011*/
};

typedef struct device_config_selection_t
{
        char description[256];
        int value;
} device_config_selection_t;

typedef struct device_config_t
{
        char name[256];
        char description[256];
        int type;
        char default_string[256];
        int default_int;
        device_config_selection_t selection[30];
} device_config_t;

typedef struct device_t
{
        char name[50];
        uint32_t flags;
        void *(*init)();
        void (*close)(void *p);
        int  (*available)();
        void (*speed_changed)(void *p);
        void (*force_redraw)(void *p);
        void (*add_status_info)(char *s, int max_len, void *p);
        device_config_t *config;
} device_t;

typedef struct SOUND_CARD
{
        char name[64];
        char internal_name[24];
        device_t *device;
} SOUND_CARD;

typedef struct video_timings_t
{
        int type;
        int write_b, write_w, write_l;
        int read_b, read_w, read_l;
} video_timings_t;

typedef struct VIDEO_CARD
{
        char name[64];
        char internal_name[24];
        device_t *device;
        int legacy_id;
        int flags;
        video_timings_t timing;
} VIDEO_CARD;

typedef struct MODEL
{
        char name[64];
        int id;
        char internal_name[24];
        struct
        {
                char name[8];
                CPU *cpus;
        } cpu[5];
        int flags;
        int min_ram, max_ram;
        int ram_granularity;
        void (*init)();
        struct device_t *device;
} MODEL;

typedef struct HDD_CONTROLLER
{
        char name[50];
        char internal_name[16];
        device_t *device;
        int is_mfm;
        int is_ide;
        int is_scsi;
} HDD_CONTROLLER;

typedef struct NETWORK_CARD
{
        char name[32];
        char internal_name[24];
        device_t *device;
} NETWORK_CARD;

typedef struct lpt_device_t
{
        char name[50];
        uint32_t flags;
        void *(*init)();
        void (*close)(void *p);
        int  (*available)();
        void (*speed_changed)(void *p);
        void (*force_redraw)(void *p);
        void (*add_status_info)(char *s, int max_len, void *p);
        device_config_t *config;
        void (*write_data)(uint8_t val, void *p);
        void (*write_ctrl)(uint8_t val, void *p);
        uint8_t (*read_status)(void *p);
} lpt_device_t;

typedef struct LPT_DEVICE
{
        char name[64];
        char internal_name[16];
        lpt_device_t *device;
} LPT_DEVICE;

extern void pcem_add_model(MODEL *model);
extern void pcem_add_video(VIDEO_CARD *video);
extern void pcem_add_sound(SOUND_CARD *sound);
extern void pcem_add_lpt(LPT_DEVICE *lpt);
extern void pcem_add_hddcontroller(HDD_CONTROLLER *hddcontroller);
extern void pcem_add_networkcard(NETWORK_CARD *netcard);
extern void pcem_add_device(device_t *device);

#endif /* _PCEM_DEVICES_H_ */
