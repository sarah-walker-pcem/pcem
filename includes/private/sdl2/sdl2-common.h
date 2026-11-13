#ifndef _SDL2_COMMON_H_
#define _SDL2_COMMON_H_

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

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

#define IMAGE_JPG "jpg"
#define IMAGE_PNG "png"
#define IMAGE_BMP "bmp"
#define IMAGE_TIFF "tiff"

#endif /* _SDL2_COMMON_H_ */
