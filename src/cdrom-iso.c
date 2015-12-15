/*ISO CD-ROM support*/

#include "ibm.h"
#include "ide.h"
#include "cdrom-iso.h"
#include <sys/stat.h>

static ATAPI iso_atapi;

static uint32_t last_block = 0;
static uint64_t image_size = 0;
static int iso_inited = 0;
char iso_path[1024];
static FILE *iso_image = NULL;
static int iso_changed = 0;

static uint32_t iso_cd_pos = 0, iso_cd_end = 0;

void iso_audio_callback(int16_t *output, int len)
{
        memset(output, 0, len * 2);
        return;
}

void iso_audio_stop()
{
        pclog("iso_audio_stop stub\n");
}

static int get_track_nr(uint32_t pos)
{
        pclog("get_track_nr stub\n");
        return 0;
}

static void iso_playaudio(uint32_t pos, uint32_t len, int ismsf)
{
        pclog("iso_playaudio stub\n");
        return;
}

static void iso_pause(void)
{
        pclog("iso_pause stub\n");
        return;
}

static void iso_resume(void)
{
        pclog("iso_resume stub\n");
        return;
}

static void iso_stop(void)
{
        pclog("iso_stop stub\n");
        return;
}

static void iso_seek(uint32_t pos)
{
        pclog("iso_seek stub\n");
        return;
}

static int iso_ready(void)
{
	if (iso_changed)
	{
		iso_changed = 0;
		return 0;
	}

        return 1;
}

static uint8_t iso_getcurrentsubchannel(uint8_t *b, int msf)
{
        pclog("iso_getcurrentsubchannel stub\n");
        return 0;
}

static void iso_eject(void)
{
        pclog("iso_eject stub\n");
}

static void iso_load(void)
{
        pclog("iso_load stub\n");
}

static void iso_readsector(uint8_t *b, int sector)
{
        if (!cdrom_drive) return;
        fseek(iso_image,sector*2048,SEEK_SET);
        fread(b,2048,1,iso_image);
}

static void lba_to_msf(uint8_t *buf, int lba)
{
        lba += 150;
        buf[0] = (lba / 75) / 60;
        buf[1] = (lba / 75) % 60;
        buf[2] = lba & 75;
}

static int iso_readtoc(unsigned char *buf, unsigned char start_track, int msf, int maxlen, int single)
{
        uint8_t *q;
        int len;

        if (start_track > 1 && start_track != 0xaa)
                return -1;
        q = buf + 2;
        *q++ = 1; /* first session */
        *q++ = 1; /* last session */
        if (start_track <= 1)
        {
                *q++ = 0; /* reserved */
                *q++ = 0x14; /* ADR, control */
                *q++ = 1;    /* track number */
                *q++ = 0; /* reserved */
                if (msf)
                {
                        *q++ = 0; /* reserved */
                        lba_to_msf(q, 0);
                        q += 3;
                }
                else
                {
                        /* sector 0 */
                        *q++ = 0;
                        *q++ = 0;
                        *q++ = 0;
                        *q++ = 0;
                }
        }
        /* lead out track */
        *q++ = 0; /* reserved */
        *q++ = 0x16; /* ADR, control */
        *q++ = 0xaa; /* track number */
        *q++ = 0; /* reserved */
        last_block = image_size >> 11;
        if (msf)
        {
                *q++ = 0; /* reserved */
                lba_to_msf(q, last_block);
                q += 3;
        }
        else
        {
                *q++ = last_block >> 24;
                *q++ = last_block >> 16;
                *q++ = last_block >> 8;
                *q++ = last_block;
        }
        len = q - buf;
        buf[0] = (uint8_t)(((len-2) >> 8) & 0xff);
        buf[1] = (uint8_t)((len-2) & 0xff);
        return len;
}

static void iso_readtoc_session(unsigned char *buf, int msf, int maxlen)
{
        uint8_t *q;

        q = buf + 2;
        *q++ = 1; /* first session */
        *q++ = 1; /* last session */

        *q++ = 1; /* session number */
        *q++ = 0x14; /* data track */
        *q++ = 0; /* track number */
        *q++ = 0xa0; /* lead-in */
        *q++ = 0; /* min */
        *q++ = 0; /* sec */
        *q++ = 0; /* frame */
        *q++ = 0;
}

static uint32_t iso_size()
{
        unsigned char b[4096];

        atapi->readtoc(b, 0, 0, 4096, 0);
    
        return last_block;
}

void iso_reset()
{
}

int iso_open(char *fn)
{
        struct stat st;

        if (iso_image)
                fclose(iso_image);

	iso_changed = 1;

        sprintf(iso_path, "%s", fn);
        pclog("Path is %s\n", iso_path);

        iso_image = fopen(iso_path, "rb");
        atapi = &iso_atapi;
    
        stat(iso_path, &st);
        image_size = st.st_size;
    
        return 0;
}

static void iso_close(void)
{
        if (iso_image)
        {
                fclose(iso_image);
                iso_image = NULL;
        }
}

static void iso_exit(void)
{
        iso_stop();
        iso_inited = 0;
        iso_close();
}

static ATAPI iso_atapi = 
{
        iso_ready,
        iso_readtoc,
        iso_readtoc_session,
        iso_getcurrentsubchannel,
        iso_readsector,
        iso_playaudio,
        iso_seek,
        iso_load,
        iso_eject,
        iso_pause,
        iso_resume,
        iso_size,
        iso_stop,
        iso_exit
};
