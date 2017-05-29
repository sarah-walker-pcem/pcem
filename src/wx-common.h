extern int show_machine_info;
extern int show_status;
extern int show_machine_on_start;
extern int confirm_on_stop_emulation;
extern int confirm_on_reset_machine;
extern int show_speed_history;
extern int show_disc_activity;
extern int show_mount_paths;
extern int wx_window_x;
extern int wx_window_y;

typedef enum { EMULATION_STOPPED, EMULATION_PAUSED, EMULATION_RUNNING } emulation_state_t;
extern emulation_state_t emulation_state;

#define IS_PAUSED (emulation_state == EMULATION_PAUSED)

typedef enum { DRIVE_TYPE_HDD, DRIVE_TYPE_FDD, DRIVE_TYPE_CDROM } drive_type_t;

typedef struct drive_info_t {
        drive_type_t type;
        char fn[256];
        int enabled;
        int drive;
        char drive_letter;
        int readflash;
} drive_info_t;
