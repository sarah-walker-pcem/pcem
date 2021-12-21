/*Dummy CD-ROM support for other OSes*/

#include <stdint.h>

int old_cdrom_drive;

void ioctl_audio_callback(int16_t *output, int len) { }

void ioctl_audio_stop() { }

void ioctl_reset() { }

void ioctl_set_drive(char d) { }

int ioctl_open(char d)
{
        return 0;
}

void ioctl_close(void) { }
