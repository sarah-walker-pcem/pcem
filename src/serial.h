void serial1_init(uint16_t addr);
void serial2_init(uint16_t addr);
void serial1_remove();
void serial2_remove();
void serial_reset();

typedef struct
{
        uint8_t linestat,thr,mctrl,rcr,iir,ier,lcr;
        uint8_t dlab1,dlab2;
} SERIAL;

extern SERIAL serial,serial2;

extern int serial_fifo_read, serial_fifo_write;

extern void (*serial_rcr)();
