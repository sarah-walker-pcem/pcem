/*This is the Chips and Technologies C82100 chipset used in the Amstrad PC5086 model*/
#include "ibm.h"
#include "io.h"
#include "x86.h"
#include "superxt.h"

static uint8_t superxt_regs[256];
static int superxt_index;
static int superxt_emspage[4];

void superxt_write(uint16_t port, uint8_t val, void *priv)
{
        switch (port)
        {
                case 0x22:
                superxt_index = val;
                break;
                
                case 0x23:
                superxt_regs[superxt_index] = val;

                case 0x0208: case 0x4208: case 0x8208: case 0xC208: 
                superxt_emspage[port >> 14] = val;                
                break;
                
                // default:
                // pclog("%04X:%04X SUPERXT WRITE : %04X, %02X\n", CS, cpu_state.pc, port, val);
        }
}

uint8_t superxt_read(uint16_t port, void *priv)
{
        switch (port)
        {
                case 0x22:
                return superxt_index;
                
                case 0x23:
                return superxt_regs[superxt_index];
                
                case 0x0208: case 0x4208: case 0x8208: case 0xC208: 
                return superxt_emspage[port >> 14];  
                
                // default:
                // pclog("%04X:%04X SUPERXT READ : %04X\n", CS, cpu_state.pc, port);
        }
        return 0xff;
}

void superxt_init()
{
        /* Set register 0x42 to invalid configuration at startup */
        superxt_regs[0x42] = 0;

        io_sethandler(0x0022, 0x0002, superxt_read, NULL, NULL, superxt_write, NULL, NULL,  NULL);
        io_sethandler(0x0208, 0x0001, superxt_read, NULL, NULL, superxt_write, NULL, NULL,  NULL);
        io_sethandler(0x4208, 0x0001, superxt_read, NULL, NULL, superxt_write, NULL, NULL,  NULL);
        io_sethandler(0x8208, 0x0001, superxt_read, NULL, NULL, superxt_write, NULL, NULL,  NULL);
        io_sethandler(0xc208, 0x0001, superxt_read, NULL, NULL, superxt_write, NULL, NULL,  NULL);
}
