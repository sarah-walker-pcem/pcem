#ifndef __IDE__
#define __IDE__

struct IDE;

extern void writeide(int ide_board, uint16_t addr, uint8_t val);
extern void writeidew(int ide_board, uint16_t val);
extern uint8_t readide(int ide_board, uint16_t addr);
extern uint16_t readidew(int ide_board);
extern void callbackide(int ide_board);
extern void resetide(void);
extern void ide_init();

/*ATAPI stuff*/
typedef struct ATAPI
{
        int (*ready)(void);
        int (*readtoc)(uint8_t *b, uint8_t starttrack, int msf, int maxlen, int single);
        void (*readtoc_session)(uint8_t *b, int msf, int maxlen);
        uint8_t (*getcurrentsubchannel)(uint8_t *b, int msf);
        void (*readsector)(uint8_t *b, int sector);
        void (*playaudio)(uint32_t pos, uint32_t len, int ismsf);
        void (*seek)(uint32_t pos);
        void (*load)(void);
        void (*eject)(void);
        void (*pause)(void);
        void (*resume)(void);
        void (*stop)(void);
        void (*exit)(void);
} ATAPI;

extern ATAPI *atapi;

void atapi_discchanged();

extern int ideboard;

extern int idecallback[2];

extern char ide_fn[2][512];

#endif //__IDE__
