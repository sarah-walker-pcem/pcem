extern scsi_device_t scsi_cd;

#define MAX_CD_SPEED 72
int cd_get_speed(int i);
void cd_set_speed(int speed);

extern int cd_speed;
