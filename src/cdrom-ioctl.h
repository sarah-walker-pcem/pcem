#ifndef CDROM_IOCTL_H
#define CDROM_IOCTL_H

/* this header file lists the functions provided by
   various platform specific cdrom-ioctl files */

extern int ioctl_open(char d);
extern void ioctl_reset();
extern void ioctl_close();
void ioctl_set_drive(char d);
void ioctl_audio_callback(int16_t *output, int len);
void ioctl_audio_stop();

#endif /* ! CDROM_IOCTL_H */
