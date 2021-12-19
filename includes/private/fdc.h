void fdc_init();
void fdc_add();
void fdc_add_pcjr();
void fdc_add_tandy();
void fdc_remove();
void fdc_reset();
void fdc_poll();
void fdc_abort();
void fdc_discchange_clear(int drive);
int fdc_discchange_read();
void fdc_set_dskchg_activelow();
void fdc_3f1_enable(int enable);
void fdc_set_ps1();
int fdc_get_bitcell_period();
uint8_t fdc_read(uint16_t addr, void *priv);

/* A few functions to communicate between Super I/O chips and the FDC. */
void fdc_update_is_nsc(int is_nsc);
void fdc_update_enh_mode(int enh_mode);
int fdc_get_rwc(int drive);
void fdc_update_rwc(int drive, int rwc);
int fdc_get_boot_drive();
void fdc_update_boot_drive(int boot_drive);
void fdc_update_densel_polarity(int densel_polarity);
void fdc_update_densel_force(int densel_force);
void fdc_update_drvrate(int drive, int drvrate);



enum
{
        FDC_STATUS_AM_NOT_FOUND,
        FDC_STATUS_NOT_FOUND,
        FDC_STATUS_WRONG_CYLINDER,
        FDC_STATUS_BAD_CYLINDER
};
