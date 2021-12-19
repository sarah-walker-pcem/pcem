#define SEEK_RECALIBRATE -999
uint64_t fdd_seek(int drive, int track_diff);
int fdd_track0(int drive);
int fdd_getrpm(int drive);
void fdd_set_densel(int densel);
int fdd_can_read_medium(int drive);
int fdd_doublestep_40(int drive);
int fdd_is_525(int drive);
int fdd_is_ed(int drive);
void fdd_disc_changed(int drive);

void fdd_set_type(int drive, int type);
int fdd_get_type(int drive);

extern int fdd_swap;
