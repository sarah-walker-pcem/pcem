#include "timer.h"

void serial1_init(uint16_t addr, int irq, int has_fifo);
void serial2_init(uint16_t addr, int irq, int has_fifo);
void serial1_set(uint16_t addr, int irq);
void serial2_set(uint16_t addr, int irq);
void serial1_remove();
void serial2_remove();
void serial_reset();

struct SERIAL;

typedef struct
{
        uint8_t lsr,thr,mctrl,rcr,iir,ier,lcr,msr;
        uint8_t dlab1,dlab2;
        uint8_t dat;
        uint8_t int_status;
        uint8_t scratch;
        uint8_t fcr;
        
        int irq;
        uint16_t addr;

        void (*rcr_callback)(struct SERIAL *serial, void *p);
        void *rcr_callback_p;
        uint8_t fifo[256];
        int fifo_read, fifo_write;
        int has_fifo;
        
        pc_timer_t receive_timer;
} SERIAL;

void serial_write_fifo(SERIAL *serial, uint8_t dat);

extern SERIAL serial1, serial2;
