#include "ibm.h"
#include "io.h"
#include "pic.h"
#include "plat-midi.h"
#include "sound_mpu401_uart.h"

enum
{
        STATUS_OUTPUT_NOT_READY = 0x40,
        STATUS_INPUT_NOT_READY  = 0x80
};

static void mpu401_uart_raise_irq(void *p)
{
        mpu401_uart_t *mpu = (mpu401_uart_t *)p;

        if (mpu->irq != -1)
                picint(1 << mpu->irq);
}

static void mpu401_uart_write(uint16_t addr, uint8_t val, void *p)
{
        mpu401_uart_t *mpu = (mpu401_uart_t *)p;
        
        if (addr & 1) /*Command*/
        {
                switch (val)
                {
                        case 0xff: /*Reset*/
                        // From Roland: "An ACK will not be sent back upon sending a SYSTEM RESET to leave the UART MODE ($3F)."
                        // But actual behaviour is weird. For example, the MPU401 port test in the AZT1605 drivers for Windows NT
                        // want this to return an Ack but the IRQ test in the same driver wants this to raise no interrupts!
                        mpu->rx_data = 0xfe; /*Acknowledge*/
                        mpu->uart_mode = 0;
                        if (mpu->is_aztech)
                                mpu->status = STATUS_OUTPUT_NOT_READY;
                        else
                        {
                                mpu->status = 0;
                                mpu401_uart_raise_irq(p);
                        }
                        break;
                        
                        case 0x3f: /*Enter UART mode*/
                        mpu->rx_data = 0xfe; /*Acknowledge*/
                        mpu->uart_mode = 1;
                        if (mpu->is_aztech)
                        {
                                mpu->status = STATUS_OUTPUT_NOT_READY;
                                mpu401_uart_raise_irq(p);
                        }
                        else
                                mpu->status = 0;
                        break;
                }
                return;
        }
                        
        /*Data*/
        if (mpu->uart_mode)
                midi_write(val);
}

static uint8_t mpu401_uart_read(uint16_t addr, void *p)
{
        mpu401_uart_t *mpu = (mpu401_uart_t *)p;
        
        if (addr & 1) /*Status*/
                return mpu->status;
        
        /*Data*/
        mpu->status = STATUS_INPUT_NOT_READY;
        return mpu->rx_data;
}

void mpu401_uart_init(mpu401_uart_t *mpu, uint16_t addr, int irq, int is_aztech)
{
        mpu->status = STATUS_INPUT_NOT_READY;
        mpu->uart_mode = 0;
        mpu->addr = addr;
        mpu->irq = irq;
        mpu->is_aztech = is_aztech;
        
        io_sethandler(addr, 0x0002, mpu401_uart_read, NULL, NULL, mpu401_uart_write, NULL, NULL, mpu);
}

void mpu401_uart_update_addr(mpu401_uart_t *mpu, uint16_t addr)
{
        io_removehandler(mpu->addr, 0x0002, mpu401_uart_read, NULL, NULL, mpu401_uart_write, NULL, NULL, mpu);
        mpu->addr = addr;
        if (addr != 0)
                io_sethandler(addr, 0x0002, mpu401_uart_read, NULL, NULL, mpu401_uart_write, NULL, NULL, mpu);
}

void mpu401_uart_update_irq(mpu401_uart_t *mpu, int irq)
{
        mpu->irq = irq;
}

