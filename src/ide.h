#ifndef __IDE__
#define __IDE__

#include "device.h"

struct IDE;

extern void writeide(int ide_board, uint16_t addr, uint8_t val);
extern void writeidew(int ide_board, uint16_t val);
extern uint8_t readide(int ide_board, uint16_t addr);
extern uint16_t readidew(int ide_board);
extern void callbackide(int ide_board);
extern void resetide(void);
extern void ide_pri_enable();
extern void ide_sec_enable();
extern void ide_pri_disable();
extern void ide_sec_disable();
extern void ide_set_bus_master(int (*read_sector)(int channel, uint8_t *data), int (*write_sector)(int channel, uint8_t *data), void (*set_irq)(int channel));

#include "ide_atapi.h"

extern int ideboard;

extern int idecallback[2];

extern char ide_fn[4][512];

extern int cdrom_channel;

uint32_t atapi_get_cd_channel(int channel);
uint32_t atapi_get_cd_volume(int channel);

#define CDROM_IMAGE 200

extern device_t ide_device;

#endif //__IDE__
