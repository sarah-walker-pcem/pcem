extern scsi_device_t scsi_cd;

#define MAX_CD_SPEED 72
int cd_get_speed(int i);
void cd_set_speed(int speed);

extern int cd_speed;

#define MAX_CD_MODEL 1

#define CD_MODEL_INTERFACE_ALL  0 // even when controller is not IDE and not SCSI (to always have the PCemCD model as default, even when not selectable)
#define CD_MODEL_INTERFACE_IDE  1
#define CD_MODEL_INTERFACE_SCSI 2

char *cd_get_model(int i);
char *cd_get_config_model(int i);
void cd_set_model(char *model);
int cd_get_model_interfaces(int i);
int cd_get_model_speed(int i);
char *cd_model_to_config(char *model);
char *cd_model_from_config(char *config);

extern char *cd_model;
