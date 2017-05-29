#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include "ibm.h"
#include "ide.h"
#include "cdrom-ioctl.h"

static ATAPI ioctl_atapi;

void ioctl_audio_callback(int16_t *output, int len)
{
	memset(output, 0, len * 2);
}

void ioctl_audio_stop()
{
}

static int get_track_nr(uint32_t pos)
{
    return 0;
}

static int is_track_audio(uint32_t pos)
{
    return 0;
}

static int ioctl_is_track_audio(uint32_t pos, int ismsf)
{
    return 0;
}

static void ioctl_playaudio(uint32_t pos, uint32_t len, int ismsf)
{
}

static void ioctl_pause(void)
{
}

static void ioctl_resume(void)
{
}

static void ioctl_stop(void)
{
}

static void ioctl_seek(uint32_t pos)
{
}

static int read_toc(int fd, struct cdrom_tocentry *btoc)
{
	return 0;
}

static int ioctl_ready(void)
{
    return 0;
}

static int ioctl_get_last_block(unsigned char starttrack, int msf, int maxlen, int single)
{
    return 0;
}

static int ioctl_medium_changed(void)
{
    return 0;
}

static uint8_t ioctl_getcurrentsubchannel(uint8_t *b, int msf)
{
    return 0x13;
}

static void ioctl_eject(void)
{
}

static void ioctl_load(void)
{
}

static int ioctl_readsector(uint8_t *b, int sector)
{
	return 0;
}

static int lba_to_msf(int lba)
{
    return 0;
}

static void ioctl_readsector_raw(uint8_t *b, int sector)
{
}

static int ioctl_readtoc(unsigned char *b, unsigned char starttrack, int msf, int maxlen, int single)
{
    return 0;
}

static int ioctl_readtoc_session(unsigned char *b, int msf, int maxlen)
{
	return 0;
}

static int ioctl_readtoc_raw(unsigned char *b, int maxlen)
{
	return 0;
}

static uint32_t ioctl_size()
{
	return 0;
}

static int ioctl_status()
{
	return CD_STATUS_EMPTY;
}
void ioctl_reset()
{
}

void ioctl_set_drive(char d)
{
	atapi = &ioctl_atapi;
}

int ioctl_open(char d)
{
	atapi = &ioctl_atapi;
        return 0;
}

void ioctl_close(void)
{
}

static void ioctl_exit(void)
{

}

static ATAPI ioctl_atapi=
{
        ioctl_ready,
	ioctl_medium_changed,
        ioctl_readtoc,
        ioctl_readtoc_session,
	ioctl_readtoc_raw,
        ioctl_getcurrentsubchannel,
        ioctl_readsector,
	ioctl_readsector_raw,
        ioctl_playaudio,
        ioctl_seek,
        ioctl_load,
        ioctl_eject,
        ioctl_pause,
        ioctl_resume,
        ioctl_size,
	ioctl_status,
	ioctl_is_track_audio,
        ioctl_stop,
        ioctl_exit
};
