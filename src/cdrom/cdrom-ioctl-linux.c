/*Linux CD-ROM support via IOCTL*/

#include <linux/cdrom.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <unistd.h>
#include "ibm.h"
#include "ide.h"
#include "cdrom-ioctl.h"

static ATAPI ioctl_atapi;

static uint32_t last_block = 0;
static uint32_t cdrom_capacity = 0;
static int ioctl_inited = 0;
static int tocvalid = 0;
static struct cdrom_tocentry toc[256];
static int toc_tracks;
static int first_track, last_track;
static int ioctl_fd = 0;

int old_cdrom_drive;

#define MSFtoLBA(m,s,f)  (((((m*60)+s)*75)+f)-150)

enum
{
    CD_STOPPED = 0,
    CD_PLAYING,
    CD_PAUSED
};

static int ioctl_cd_state = CD_STOPPED;
static uint32_t ioctl_cd_pos = 0, ioctl_cd_end = 0;
#define BUF_SIZE 32768
static int16_t cd_buffer[BUF_SIZE];
static int cd_buflen = 0;
void ioctl_audio_callback(int16_t *output, int len)
{
	struct cdrom_read_audio read_audio;

//        pclog("Audio callback %08X %08X %i %i %i %04X %i\n", ioctl_cd_pos, ioctl_cd_end, ioctl_cd_state, cd_buflen, len, cd_buffer[4], GetTickCount());
        if (ioctl_cd_state != CD_PLAYING) 
        {
                memset(output, 0, len * 2);
                return;
        }

	if (ioctl_fd <= 0)
        {
                memset(output, 0, len * 2);
                return;
        }

        while (cd_buflen < len)
        {
                if (ioctl_cd_pos < ioctl_cd_end)
                {
			read_audio.addr.lba = ioctl_cd_pos - 150;
			read_audio.addr_format = CDROM_LBA;
			read_audio.nframes = 1;
			read_audio.buf = (__u8 *)&cd_buffer[cd_buflen];
			
        		if (ioctl(ioctl_fd, CDROMREADAUDIO, &read_audio) < 0)
        		{
//                                pclog("DeviceIoControl returned false\n");
                                memset(&cd_buffer[cd_buflen], 0, (BUF_SIZE - cd_buflen) * 2);
                                ioctl_cd_state = CD_STOPPED;
                                cd_buflen = len;
                        }
                        else
                        {
//                                pclog("DeviceIoControl returned true\n");
                                ioctl_cd_pos++;
                                cd_buflen += (2352 / 2);
                        }
                }
                else
                {
                        memset(&cd_buffer[cd_buflen], 0, (BUF_SIZE - cd_buflen) * 2);
                        ioctl_cd_state = CD_STOPPED;
                        cd_buflen = len;
                }
        }

        memcpy(output, cd_buffer, len * 2);
//        for (c = 0; c < BUF_SIZE - len; c++)
//            cd_buffer[c] = cd_buffer[c + cd_buflen];
        memcpy(&cd_buffer[0], &cd_buffer[len], (BUF_SIZE - len) * 2);
        cd_buflen -= len;
//        pclog("Done %i\n", GetTickCount());
}

void ioctl_audio_stop()
{
	ioctl_cd_state = CD_STOPPED;
}

static int get_track_nr(uint32_t pos)
{
        int c;
        int track = 0;
        
        if (!tocvalid)
                return 0;

        for (c = first_track; c < last_track; c++)
        {
                uint32_t track_address = toc[c].cdte_addr.msf.frame +
                                         (toc[c].cdte_addr.msf.second * 75) +
                                         (toc[c].cdte_addr.msf.minute * 75 * 60);
//pclog("get_track_nr: track=%i pos=%x track_address=%x\n", c, pos, track_address);
                if (track_address <= pos)
                        track = c;
        }
        return track;
}

static int is_track_audio(uint32_t pos)
{
        int c;
        int control = 0;
        
        if (!tocvalid)
                return 0;

        for (c = first_track; c < last_track; c++)
        {
                uint32_t track_address = toc[c].cdte_addr.msf.frame +
                                         (toc[c].cdte_addr.msf.second * 75) +
                                         (toc[c].cdte_addr.msf.minute * 75 * 60);
//pclog("get_track_nr: track=%i pos=%x track_address=%x\n", c, pos, track_address);
                if (track_address <= pos)
                        control = toc[c].cdte_ctrl;
        }
        return (control & 4) ? 0 : 1;
}

static int ioctl_is_track_audio(uint32_t pos, int ismsf)
{
	if (ismsf)
	{
		int m = (pos >> 16) & 0xff;
		int s = (pos >> 8) & 0xff;
		int f = pos & 0xff;
		pos = MSFtoLBA(m, s, f);
	}
	return is_track_audio(pos);
}

static void ioctl_playaudio(uint32_t pos, uint32_t len, int ismsf)
{
//        pclog("Play audio - %08X %08X %i\n", pos, len, ismsf);
        if (ismsf)
        {
                pos = (pos & 0xff) + (((pos >> 8) & 0xff) * 75) + (((pos >> 16) & 0xff) * 75 * 60);
                len = (len & 0xff) + (((len >> 8) & 0xff) * 75) + (((len >> 16) & 0xff) * 75 * 60);
//                pclog("MSF - pos = %08X len = %08X\n", pos, len);
        }
        else
        	len += pos;
        ioctl_cd_pos   = pos;// + 150;
        ioctl_cd_end   = pos+len;// + 150;
        ioctl_cd_state = CD_PLAYING;
	if (ioctl_cd_pos < 150)
		ioctl_cd_pos = 150;
//        pclog("Audio start %08X %08X %i %i %i\n", ioctl_cd_pos, ioctl_cd_end, ioctl_cd_state, 0, len);
}

static void ioctl_pause(void)
{
        if (ioctl_cd_state == CD_PLAYING)
        	ioctl_cd_state = CD_PAUSED;
}

static void ioctl_resume(void)
{
        if (ioctl_cd_state == CD_PAUSED)
        	ioctl_cd_state = CD_PLAYING;
}

static void ioctl_stop(void)
{
        ioctl_cd_state = CD_STOPPED;
}

static void ioctl_seek(uint32_t pos)
{
//        pclog("Seek %08X\n", pos);
        ioctl_cd_pos   = pos;
        ioctl_cd_state = CD_STOPPED;
}

static int read_toc(int fd, struct cdrom_tocentry *btoc)
{
	struct cdrom_tochdr toc_hdr;
	int track, err;
	int c;
//pclog("read_toc\n");
	err = ioctl(fd, CDROMREADTOCHDR, &toc_hdr);
	if (err == -1)
	{
		pclog("read_toc: CDROMREADTOCHDR failed\n");
		return 0;
	}

	first_track = toc_hdr.cdth_trk0;
	last_track = toc_hdr.cdth_trk1;
//pclog("read_toc: first_track=%i last_track=%i\n", first_track, last_track);
	memset(btoc, 0, sizeof(struct cdrom_tocentry));

	c = 0;
	for (track = 0; track < 256; track++)
	{
		btoc[c].cdte_track = track;
		btoc[c].cdte_format = CDROM_MSF;
		err = ioctl(fd, CDROMREADTOCENTRY, &btoc[c]);
		if (err == -1)
		{
//			pclog("read_toc: CDROMREADTOCENTRY failed on track %i\n", track);
			continue;
		}
		c++;
//		pclog("read_toc: %i Track %02X - number %02X control %02X adr %02X address %02X %02X %02X %02X\n", c, track, btoc[c].cdte_track, btoc[c].cdte_ctrl, btoc[c].cdte_adr, 0, btoc[c].cdte_addr.msf.minute, btoc[c].cdte_addr.msf.second, btoc[c].cdte_addr.msf.frame);
	}

	toc_tracks = c;

	return 1;
}

static int ioctl_ready(void)
{
	struct cdrom_tochdr toc_hdr;
	struct cdrom_tocentry toc_entry;
	int err;

	if (ioctl_fd <= 0)
		return 0;

	err = ioctl(ioctl_fd, CDROM_MEDIA_CHANGED, 0);
	if (err)
		tocvalid = 0;

	if (tocvalid)
		return 1;

	err = ioctl(ioctl_fd, CDROMREADTOCHDR, &toc_hdr);
	if (err == -1)
		return 0;

//	pclog("CDROMREADTOCHDR: start track=%i end track=%i\n", toc_hdr.cdth_trk0, toc_hdr.cdth_trk1);
	toc_entry.cdte_track = toc_hdr.cdth_trk1;
	toc_entry.cdte_format = CDROM_MSF;
	err = ioctl(ioctl_fd, CDROMREADTOCENTRY, &toc_entry);
	if (err == -1)
		return 0;

//	pclog("CDROMREADTOCENTRY: addr=%02i:%02i:%02i\n", toc_entry.cdte_addr.msf.minute, toc_entry.cdte_addr.msf.second, toc_entry.cdte_addr.msf.frame);
        if ((toc_entry.cdte_addr.msf.minute != toc[toc_hdr.cdth_trk1].cdte_addr.msf.minute) ||
            (toc_entry.cdte_addr.msf.second != toc[toc_hdr.cdth_trk1].cdte_addr.msf.second) ||
            (toc_entry.cdte_addr.msf.frame  != toc[toc_hdr.cdth_trk1].cdte_addr.msf.frame ) ||
            !tocvalid)
        {
                ioctl_cd_state = CD_STOPPED;

		tocvalid = read_toc(ioctl_fd, toc);
        }

        return 1;
}

static int ioctl_get_last_block(unsigned char starttrack, int msf, int maxlen, int single)
{
        int c;
	int lb = 0;
	int tv = 0;
	struct cdrom_tocentry lbtoc[100];

	if (ioctl_fd <= 0)
		return 0;

        ioctl_cd_state = CD_STOPPED;

	if (!tocvalid)
		tv = read_toc(ioctl_fd, lbtoc);

	if (!tv)
		return 0;

        last_block = 0;
        for (c = 0; c <= last_track; c++)
        {
                uint32_t address;
                address = MSFtoLBA(toc[c].cdte_addr.msf.minute, toc[c].cdte_addr.msf.second, toc[c].cdte_addr.msf.frame);
                if (address > last_block)
                        lb = address;
        }
        return lb;
}

static int ioctl_medium_changed(void)
{
	struct cdrom_tochdr toc_hdr;
	struct cdrom_tocentry toc_entry;
	int err;

	if (ioctl_fd <= 0)
		return 0;

	err = ioctl(ioctl_fd, CDROMREADTOCHDR, &toc_hdr);
	if (err == -1)
		return 0;

	toc_entry.cdte_track = toc_hdr.cdth_trk1;
	toc_entry.cdte_format = CDROM_MSF;
	err = ioctl(ioctl_fd, CDROMREADTOCENTRY, &toc_entry);
	if (err == -1)
		return 0;

//	pclog("CDROMREADTOCENTRY: addr=%02i:%02i:%02i\n", toc_entry.cdte_addr.msf.minute, toc_entry.cdte_addr.msf.second, toc_entry.cdte_addr.msf.frame);
        if ((toc_entry.cdte_addr.msf.minute != toc[toc_hdr.cdth_trk1].cdte_addr.msf.minute) ||
            (toc_entry.cdte_addr.msf.second != toc[toc_hdr.cdth_trk1].cdte_addr.msf.second) ||
            (toc_entry.cdte_addr.msf.frame  != toc[toc_hdr.cdth_trk1].cdte_addr.msf.frame ))
	{
		cdrom_capacity = ioctl_get_last_block(0, 0, 4096, 0);
		return 1;
	}
        return 0;
}

static uint8_t ioctl_getcurrentsubchannel(uint8_t *b, int msf)
{
        uint32_t cdpos = ioctl_cd_pos;
        int track = get_track_nr(cdpos);
        uint32_t track_address = toc[track].cdte_addr.msf.frame +
                                (toc[track].cdte_addr.msf.second * 75) +
                                (toc[track].cdte_addr.msf.minute * 75 * 60);
	int pos=0;
	uint8_t ret;
//pclog("ioctl_getsubchannel: cdpos=%x track_address=%x track=%i\n", cdpos, track_address, track);
        if (ioctl_cd_state == CD_PLAYING)
		ret = 0x11;
	else if (ioctl_cd_state == CD_PAUSED)
		ret = 0x12;
	else
		ret = 0x13;

        b[pos++] = (toc[track].cdte_adr << 4) | toc[track].cdte_ctrl;
        b[pos++] = track;
        b[pos++] = 0;

        if (msf)
        {
                uint32_t dat = cdpos;
                b[pos + 3] = (uint8_t)(dat % 75); dat /= 75;
                b[pos + 2] = (uint8_t)(dat % 60); dat /= 60;
                b[pos + 1] = (uint8_t)dat;
                b[pos]     = 0;
                pos += 4;
                dat = cdpos - track_address;
                b[pos + 3] = (uint8_t)(dat % 75); dat /= 75;
                b[pos + 2] = (uint8_t)(dat % 60); dat /= 60;
                b[pos + 1] = (uint8_t)dat;
                b[pos]     = 0;
                pos += 4;
        }
        else
        {
                b[pos++] = (cdpos >> 24) & 0xff;
                b[pos++] = (cdpos >> 16) & 0xff;
                b[pos++] = (cdpos >> 8) & 0xff;
                b[pos++] = cdpos & 0xff;
                cdpos -= track_address;
                b[pos++] = (cdpos >> 24) & 0xff;
                b[pos++] = (cdpos >> 16) & 0xff;
                b[pos++] = (cdpos >> 8) & 0xff;
                b[pos++] = cdpos & 0xff;
        }

        return ret;
}

static void ioctl_eject(void)
{
	if (ioctl_fd <= 0)
		return;

	ioctl(ioctl_fd, CDROMEJECT);
}

static void ioctl_load(void)
{
	if (ioctl_fd <= 0)
		return;

	ioctl(ioctl_fd, CDROMEJECT);

	cdrom_capacity = ioctl_get_last_block(0, 0, 4096, 0);
}

static int ioctl_readsector(uint8_t *b, int sector, int count)
{
        if (ioctl_fd <= 0)
		return -1;

        lseek(ioctl_fd, sector*2048, SEEK_SET);
        read(ioctl_fd, b, count*2048);

	return 0;
}

union
{
	struct cdrom_msf *msf;
	char b[CD_FRAMESIZE_RAW];
} raw_read_params;

static int lba_to_msf(int lba)
{
        return (((lba / 75) / 60) << 16) + (((lba / 75) % 60) << 8) + (lba % 75);
}

static void ioctl_readsector_raw(uint8_t *b, int sector)
{
	int err;
	int imsf = lba_to_msf(sector);

        if (ioctl_fd <= 0)
		return;

	raw_read_params.msf = malloc(sizeof(struct cdrom_msf));
	raw_read_params.msf->cdmsf_frame0 = imsf & 0xff;
	raw_read_params.msf->cdmsf_sec0 = (imsf >> 8) & 0xff;
	raw_read_params.msf->cdmsf_min0 = (imsf >> 16) & 0xff;

	/* This will read the actual raw sectors from the disc. */
	err = ioctl(ioctl_fd, CDROMREADRAW, (void *) &raw_read_params);
	if (err == -1)
	{
		pclog("read_toc: CDROMREADTOCHDR failed\n");
		return;
	}

	memcpy(b, raw_read_params.b, 2352);

	free(raw_read_params.msf);
}

static int ioctl_readtoc(unsigned char *b, unsigned char starttrack, int msf, int maxlen, int single)
{
        int len=4;
        int c,d;
        uint32_t temp;
	uint32_t last_address = 0;

	if (ioctl_fd <= 0)
		return 0;

        ioctl_cd_state = CD_STOPPED;

	if (!tocvalid)
		tocvalid = read_toc(ioctl_fd, toc);

	if (!tocvalid)
		return 4;

//        pclog("Read TOC done! %i\n",single);
        b[2] = first_track;
        b[3] = last_track;
        d = 0;
//pclog("Read TOC starttrack=%i\n", starttrack);
        for (c = 0; c <= toc_tracks; c++)
        {
                if (toc[c].cdte_track && toc[c].cdte_track >= starttrack)
                {
                        d = c;
                        break;
                }
        }
        b[2] = toc[c].cdte_track;
        last_block = 0;

        for (c = d; c <= toc_tracks; c++)
        {
                uint32_t address;
                if ((len + 8) > maxlen)
			break;
//                pclog("Len %i max %i Track %02X - %02X %02X %02i:%02i:%02i %08X\n",len,maxlen,toc[c].cdte_track,toc[c].cdte_adr,toc[c].cdte_ctrl,toc[c].cdte_addr.msf.minute, toc[c].cdte_addr.msf.second, toc[c].cdte_addr.msf.frame,MSFtoLBA(toc[c].cdte_addr.msf.minute, toc[c].cdte_addr.msf.second, toc[c].cdte_addr.msf.frame));
                address = MSFtoLBA(toc[c].cdte_addr.msf.minute, toc[c].cdte_addr.msf.second, toc[c].cdte_addr.msf.frame);
		if (address < last_address)
			continue;
		last_address = address;
                b[len++] = 0; /*Reserved*/
                b[len++] = (toc[c].cdte_adr << 4) | toc[c].cdte_ctrl;
                b[len++] = toc[c].cdte_track;
                b[len++] = 0; /*Reserved*/
                if (address > last_block)
                        last_block = address;

                if (msf)
                {
                        b[len++] = 0;
                        b[len++] = toc[c].cdte_addr.msf.minute;
                        b[len++] = toc[c].cdte_addr.msf.second;
                        b[len++] = toc[c].cdte_addr.msf.frame;
                }
                else
                {
                        temp = MSFtoLBA(toc[c].cdte_addr.msf.minute, toc[c].cdte_addr.msf.second, toc[c].cdte_addr.msf.frame);
                        b[len++] = temp >> 24;
                        b[len++] = temp >> 16;
                        b[len++] = temp >> 8;
                        b[len++] = temp;
                }
                if (single)
			break;
        }

        b[0] = (uint8_t)(((len-2) >> 8) & 0xff);
        b[1] = (uint8_t)((len-2) & 0xff);
/*        pclog("Table of Contents (%i bytes) : \n", size);
        pclog("First track - %02X\n", first_track);
        pclog("Last  track - %02X\n", last_track);
        for (c = 0; c <= last_track; c++)
            pclog("Track %02X - number %02X control %02X adr %02X address %02X %02X %02X %02X\n", c, toc[c].cdte_track, toc[c].cdte_ctrl, toc[c].cdte_adr, 0, toc[c].cdte_addr.msf.minute, toc[c].cdte_addr.msf.second, toc[c].cdte_addr.msf.frame);
        for (c = 0;c <= last_track; c++)
            pclog("Track %02X - number %02X control %02X adr %02X address %06X\n", c, toc[c].cdte_track, toc[c].cdte_ctrl, toc[c].cdte_adr, MSFtoLBA(toc[c].cdte_addr.msf.minute, toc[c].cdte_addr.msf.second, toc[c].cdte_addr.msf.frame));*/
        return len;
}

static int ioctl_readtoc_session(unsigned char *b, int msf, int maxlen)
{
	struct cdrom_multisession session;
        int len = 4;
	int err;

	if (ioctl_fd <= 0)
		return 0;

	session.addr_format = CDROM_MSF;
	err = ioctl(ioctl_fd, CDROMMULTISESSION, &session);

	if (err == -1)
		return 0;

        b[2] = 1;
        b[3] = 1;
        b[len++] = 0; /*Reserved*/
        b[len++] = (toc[first_track].cdte_adr << 4) | toc[first_track].cdte_ctrl;
        b[len++] = toc[first_track].cdte_track;
        b[len++] = 0; /*Reserved*/
        if (msf)
        {
                b[len++] = 0;
                b[len++] = session.addr.msf.minute;
                b[len++] = session.addr.msf.second;
                b[len++] = session.addr.msf.frame;
        }
        else
        {
                uint32_t temp = MSFtoLBA(session.addr.msf.minute, session.addr.msf.second, session.addr.msf.frame);
                b[len++] = temp >> 24;
                b[len++] = temp >> 16;
                b[len++] = temp >> 8;
                b[len++] = temp;
        }

	return len;
}

static int ioctl_readtoc_raw(unsigned char *b, int maxlen)
{
	struct cdrom_tochdr toc_hdr;
	struct cdrom_tocentry toc2[100];
	int track, err;
	int len = 4;

//pclog("read_toc\n");
	if (ioctl_fd <= 0)
		return 0;

	err = ioctl(ioctl_fd, CDROMREADTOCHDR, &toc_hdr);

	if (err == -1)
	{
		pclog("read_toc: CDROMREADTOCHDR failed\n");
		return 0;
	}

	b[2] = toc_hdr.cdth_trk0;
	b[3] = toc_hdr.cdth_trk1;

	//pclog("read_toc: first_track=%i last_track=%i\n", first_track, last_track);
	memset(toc, 0, sizeof(toc));

	for (track = toc_hdr.cdth_trk0; track <= toc_hdr.cdth_trk1; track++)
	{
		if ((len + 11) > maxlen)
		{
			pclog("ioctl_readtocraw: This iteration would fill the buffer beyond the bounds, aborting...\n");
			return len;
		}

		toc2[track].cdte_track = track;
		toc2[track].cdte_format = CDROM_MSF;
		err = ioctl(ioctl_fd, CDROMREADTOCENTRY, &toc2[track]);
		if (err == -1)
			return 0;

//		pclog("read_toc: Track %02X - number %02X control %02X adr %02X address %02X %02X %02X %02X\n", track, toc[track].cdte_track, toc[track].cdte_ctrl, toc[track].cdte_adr, 0, toc[track].cdte_addr.msf.minute, toc[track].cdte_addr.msf.second, toc[track].cdte_addr.msf.frame);

		b[len++] = toc2[track].cdte_track;
		b[len++]= (toc2[track].cdte_adr << 4) | toc[track].cdte_ctrl;
		b[len++]=0;
		b[len++]=0;
		b[len++]=0;
		b[len++]=0;
		b[len++]=0;
		b[len++]=0;
		b[len++] = toc2[track].cdte_addr.msf.minute;
		b[len++] = toc2[track].cdte_addr.msf.second;
		b[len++] = toc2[track].cdte_addr.msf.frame;
	}

	return len;
}

static uint32_t ioctl_size()
{
	return cdrom_capacity;
}

static int ioctl_status()
{
	if (!ioctl_ready() && (cdrom_drive <= 0))  return CD_STATUS_EMPTY;

	switch(ioctl_cd_state)
	{
		case CD_PLAYING:
		return CD_STATUS_PLAYING;
		case CD_PAUSED:
		return CD_STATUS_PAUSED;
		case CD_STOPPED:
		default:
		return CD_STATUS_STOPPED;
	}
}
void ioctl_reset()
{
//pclog("ioctl_reset: fd=%i\n", fd);
	tocvalid = 0;

	if (ioctl_fd <= 0)
		return;

	tocvalid = read_toc(ioctl_fd, toc);
}

void ioctl_set_drive(char d)
{
	ioctl_close();
	atapi=&ioctl_atapi;
	ioctl_open(d);
}

int ioctl_open(char d)
{
	atapi=&ioctl_atapi;
	ioctl_fd = open("/dev/cdrom", O_RDONLY|O_NONBLOCK);

        return 0;
}

void ioctl_close(void)
{
	if (ioctl_fd)
	{
		close(ioctl_fd);
		ioctl_fd = 0;
	}
}

static void ioctl_exit(void)
{
        ioctl_stop();
        ioctl_inited = 0;
        tocvalid=0;
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
