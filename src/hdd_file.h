typedef enum hdd_img_type
{
        HDD_IMG_RAW,
        HDD_IMG_VHD,
} hdd_img_type;

typedef struct hdd_file_t
{
        void *f;
        int spt;
        int hpc;
        int tracks;
        int sectors;
        int read_only;
        hdd_img_type img_type;
} hdd_file_t;

void hdd_load(hdd_file_t *hdd, int d, const char *fn);
void hdd_load_ext(hdd_file_t *hdd, const char *fn, int spt, int hpc, int tracks, int read_only);
void hdd_close(hdd_file_t *hdd);
int hdd_read_sectors(hdd_file_t *hdd, int offset, int nr_sectors, void *buffer);
int hdd_write_sectors(hdd_file_t *hdd, int offset, int nr_sectors, void *buffer);
int hdd_format_sectors(hdd_file_t *hdd, int offset, int nr_sectors);
