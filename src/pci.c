#include "ibm.h"
#include "io.h"
#include "mem.h"

#include "pci.h"

void    (*pci_card_write[32])(int func, int addr, uint8_t val, void *priv);
uint8_t  (*pci_card_read[32])(int func, int addr, void *priv);
void           *pci_priv[32];
static int pci_index, pci_func, pci_card, pci_bus, pci_enable;

void pci_write(uint16_t port, uint8_t val, void *priv)
{
        switch (port)
        {
                case 0xcf8:
                pci_index = val;
                break;
                case 0xcf9:
                pci_func = val & 7;
                pci_card = val >> 3;
                break;
                case 0xcfa:
                pci_bus = val;
                break;
                case 0xcfb:
                pci_enable = val & 0x80;
                break;
                
                case 0xcfc: case 0xcfd: case 0xcfe: case 0xcff:
                if (!pci_enable) 
                   return;
                   
//                pclog("PCI write bus %i card %i func %i index %02X val %02X  %04X:%04X\n", pci_bus, pci_card, pci_func, pci_index | (port & 3), val, CS, pc);
                
                if (!pci_bus && pci_card_write[pci_card])
                   pci_card_write[pci_card](pci_func, pci_index | (port & 3), val, pci_priv[pci_card]);
                
                break;
        }
}

uint8_t pci_read(uint16_t port, void *priv)
{
        switch (port)
        {
                case 0xcf8:
                return pci_index;
                case 0xcf9:
                return pci_card << 3;
                case 0xcfa:
                return pci_bus;
                case 0xcfb:
                return pci_enable;
                
                case 0xcfc: case 0xcfd: case 0xcfe: case 0xcff:
                if (!pci_enable) 
                   return 0xff;

//                pclog("PCI read  bus %i card %i func %i index %02X\n", pci_bus, pci_card, pci_func, pci_index | (port & 3));

                if (!pci_bus && pci_card_read[pci_card])
                   return pci_card_read[pci_card](pci_func, pci_index | (port & 3), pci_priv[pci_card]);

                return 0xff;
        }
}
                
void pci_init()
{
        int c;

        io_sethandler(0x0cf8, 0x0008, pci_read, NULL, NULL, pci_write, NULL, NULL,  NULL);
        
        for (c = 0; c < 32; c++)
            pci_card_read[c] = pci_card_write[c] = pci_priv[c] = NULL;
}

void pci_add_specific(int card, uint8_t (*read)(int func, int addr, void *priv), void (*write)(int func, int addr, uint8_t val, void *priv), void *priv)
{
         pci_card_read[card] = read;
        pci_card_write[card] = write;
              pci_priv[card] = priv;
}

void pci_add(uint8_t (*read)(int func, int addr, void *priv), void (*write)(int func, int addr, uint8_t val, void *priv), void *priv)
{
        int c;
        
        for (c = 0; c < 32; c++)
        {
                if (!pci_card_read[c] && !pci_card_write[c])
                {
                         pci_card_read[c] = read;
                        pci_card_write[c] = write;
                              pci_priv[c] = priv;
                        return;
                }
        }
}
