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
int hdd_read_sectors(hdd_file_t *hdd, int offset, int nr_sectors, void *buffer);
int hdd_write_sectors(hdd_file_t *hdd, int offset, int nr_sectors, void *buffer);
int hdd_format_sectors(hdd_file_t *hdd, int offset, int nr_sectors);
