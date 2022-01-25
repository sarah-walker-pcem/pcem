#ifndef __IDE_ATAPI__
#define __IDE_ATAPI__

#include "scsi.h"

/*ATAPI stuff*/
typedef struct ATAPI
{
        int (*ready)(void);
        int (*medium_changed)(void);
        int (*readtoc)(uint8_t *b, uint8_t starttrack, int msf, int maxlen, int single);
        int (*readtoc_session)(uint8_t *b, int msf, int maxlen);
        int (*readtoc_raw)(uint8_t *b, int maxlen);
        uint8_t (*getcurrentsubchannel)(uint8_t *b, int msf);
        int (*readsector)(uint8_t *b, int sector, int count);
        void (*readsector_raw)(uint8_t *b, int sector);
        void (*playaudio)(uint32_t pos, uint32_t len, int ismsf);
        void (*seek)(uint32_t pos);
        void (*load)(void);
        void (*eject)(void);
        void (*pause)(void);
        void (*resume)(void);
        uint32_t (*size)(void);
        int (*status)(void);
        int (*is_track_audio)(uint32_t pos, int ismsf);
        void (*stop)(void);
        void (*exit)(void);
} ATAPI;

extern ATAPI *atapi;

void atapi_discchanged();

typedef struct atapi_device_t
{
        scsi_bus_t bus;
        
        uint8_t command[12];
        int command_pos;
        
        int state;
        
        int max_transfer_len;
        
        uint8_t data[65536];
        int data_read_pos, data_write_pos;
        
        int bus_state;
        
        struct IDE *ide;

        int board;
        uint8_t *atastat;
        uint8_t *error;
        int *cylinder;
        
        int use_dma;
} atapi_device_t;

void atapi_data_write(atapi_device_t *atapi_dev, uint16_t val);
uint16_t atapi_data_read(atapi_device_t *atapi_dev);
void atapi_command_start(atapi_device_t *atapi, uint8_t features);
uint8_t atapi_read_iir(atapi_device_t *atapi_dev);
uint8_t atapi_read_drq(atapi_device_t *atapi_dev);
void atapi_process_packet(atapi_device_t *atapi_dev);
void atapi_set_transfer_granularity(atapi_device_t *atapi_dev, int size);
void atapi_reset(atapi_device_t *atapi_dev);

#endif
