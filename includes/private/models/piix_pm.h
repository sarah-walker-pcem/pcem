#ifndef _PIIX_PM_H_
#define _PIIX_PM_H_
void piix_pm_init(piix_t *piix);
void piix_pm_pci_write(int addr, uint8_t val, void *p);
uint8_t piix_pm_pci_read(int addr, void *p);


#endif /* _PIIX_PM_H_ */
