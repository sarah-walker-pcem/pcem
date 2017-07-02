typedef struct hdd_file_t
{
        FILE *f;
        int spt;
        int hpc;
        int tracks;
        int sectors;
} hdd_file_t;

void hdd_load(hdd_file_t *hdd, int d, const char *fn);
void hdd_close(hdd_file_t *hdd);
void hdd_read_sectors(hdd_file_t *hdd, int offset, int nr_sectors, void *buffer);
void hdd_write_sectors(hdd_file_t *hdd, int offset, int nr_sectors, void *buffer);
void hdd_format_sectors(hdd_file_t *hdd, int offset, int nr_sectors);
