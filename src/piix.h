void piix_init(int card, int pci_a, int pci_b, int pci_c, int pci_d, void (*nb_reset)());
void piix4_init(int card, int pci_a, int pci_b, int pci_c, int pci_d, void (*nb_reset)());

typedef struct piix_t
{
        int type;

        /*PIIX4*/
        uint8_t card_pm[256];

        uint16_t pmba;
        struct
        {
                uint8_t apmc, apms;

                uint64_t timer_offset;

                uint16_t pmsts;
                uint16_t pmen;
                uint16_t pmcntrl;
                uint32_t devctl;
                uint32_t devsts;
                uint16_t glbsts;
                uint16_t gpen;
                uint16_t gpsts;
                uint32_t pcntrl;
                uint16_t glben;
                uint32_t glbctl;
                uint32_t gporeg;

                int dev13_enable;
                uint16_t dev13_base, dev13_size;
        } pm;

        uint16_t smbba;
        struct
        {
                uint8_t host_ctrl;
        } smbus;

        uint8_t port_92;
} piix_t;
