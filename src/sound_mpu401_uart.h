#ifndef SOUND_MPU401_UART
#define SOUND_MPU401_UART

typedef struct mpu401_uart_t
{
        uint8_t status;
        uint8_t rx_data;

        int uart_mode;
        uint16_t addr;
        int irq;
        
        int is_aztech;
} mpu401_uart_t;

void mpu401_uart_init(mpu401_uart_t *mpu, uint16_t addr, int irq, int is_aztech);
void mpu401_uart_update_addr(mpu401_uart_t *mpu, uint16_t addr);
void mpu401_uart_update_irq(mpu401_uart_t *mpu, int irq);

#endif /* SOUND_MPU401_UART */
