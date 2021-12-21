#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include "ibm.h"
#include "ide.h"
#include "cdrom-ioctl.h"
#include <util.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOBSD.h>
#include <IOKit/storage/IOCDMedia.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/storage/IOCDTypes.h>
#include <IOKit/storage/IOCDMediaBSDClient.h>
#include <IOKit/storage/IOMedia.h>
#include <errno.h>
static ATAPI ioctl_atapi;

int old_cdrom_drive;

static uint32_t cdrom_capacity = 0;
static int tocvalid = 0;
static int toc_tracks;
static int first_track, last_track;
static CDTOC *toc = NULL;
static CDDiscInfo disc_info;
static int track_addr[256];

static int msf_to_lba(CDMSF *msf)
{
        return msf->frame + (msf->second * 75) + (msf->minute * 60 * 75);
}

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
static int cd_drive = -1;

static int cd_open()
{
        char s[80];
  
        if (cd_drive < 0)
                return 0;
        
        sprintf(s, "disk%i", cd_drive);
        return opendev(s, O_RDONLY, 0, NULL);
}
static void cd_close(int fd)
{
        close(fd);
}

void ioctl_audio_callback(int16_t *output, int len)
{
        int ioctl_fd;
        
//        pclog("Audio callback %08X %08X %i %i %i %04X %i\n", ioctl_cd_pos, ioctl_cd_end, ioctl_cd_state, cd_buflen, len, cd_buffer[4], GetTickCount());
        if (ioctl_cd_state != CD_PLAYING) 
        {
                memset(output, 0, len * 2);
                return;
        }

        ioctl_fd = cd_open();
	if (ioctl_fd <= 0)
        {
                memset(output, 0, len * 2);
                return;
        }

        while (cd_buflen < len)
        {
                if (ioctl_cd_pos < ioctl_cd_end)
                {
                        dk_cd_read_t cd_read;
        
                        bzero(&cd_read, sizeof(cd_read));
        
                        cd_read.offset = (ioctl_cd_pos-150)*kCDSectorSizeCDDA;
                        cd_read.buffer = &cd_buffer[cd_buflen];
                        cd_read.bufferLength = kCDSectorSizeCDDA;
                        cd_read.sectorType = kCDSectorTypeCDDA;
                        cd_read.sectorArea = kCDSectorAreaUser;
                        //pclog("CDDA read %llx %d %d\n", cd_read.offset, cd_read.bufferLength, ioctl_cd_pos);
        
                        if (ioctl(ioctl_fd, DKIOCCDREAD, &cd_read) < 0)
        		{
                                //pclog("DeviceIoControl returned false %d\n", errno);
                                memset(&cd_buffer[cd_buflen], 0, (BUF_SIZE - cd_buflen) * 2);
                                ioctl_cd_state = CD_STOPPED;
                                cd_buflen = len;
                        }
                        else
                        {
                                //pclog("DeviceIoControl returned true\n");
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
        
        cd_close(ioctl_fd);

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
                uint32_t track_address = toc->descriptors[c].p.frame +
                                         (toc->descriptors[c].p.second * 75) +
                                         (toc->descriptors[c].p.minute * 75 * 60);
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
        //pclog("is_track_audio: %08x %i\n", pos, tocvalid);
        if (!tocvalid)
                return 0;
        //pclog(" scan tracks %i-%i\n", first_track, last_track);
        for (c = first_track; c < last_track; c++)
        {
                uint32_t track_address = toc->descriptors[c].p.frame +
                                         (toc->descriptors[c].p.second * 75) +
                                         (toc->descriptors[c].p.minute * 75 * 60);
//pclog("get_track_nr: track=%i pos=%x track_address=%x\n", c, pos, track_address);
                if (track_address <= pos)
                        control = toc->descriptors[c].control;
        }
        return (control & 4) ? 0 : 1;
}

static int ioctl_is_track_audio(uint32_t pos, int ismsf)
{
	if (ismsf)
	{
                CDMSF msf;
		msf.minute = (pos >> 16) & 0xff;
		msf.second = (pos >> 8) & 0xff;
		msf.frame = pos & 0xff;
		pos = msf_to_lba(&msf);
	}
	return is_track_audio(pos);
}

static void ioctl_playaudio(uint32_t pos, uint32_t len, int ismsf)
{
        //pclog("Play audio - %08X %08X %i\n", pos, len, ismsf);
        if (ismsf)
        {
                pos = (pos & 0xff) + (((pos >> 8) & 0xff) * 75) + (((pos >> 16) & 0xff) * 75 * 60);
                len = (len & 0xff) + (((len >> 8) & 0xff) * 75) + (((len >> 16) & 0xff) * 75 * 60);
                //pclog("MSF - pos = %08X len = %08X\n", pos, len);
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

static int read_toc(int fd)
{
        dk_cd_read_toc_t cd_read_toc;
        dk_cd_read_disc_info_t cd_read_disc_info;
        int result;
        int c;

        bzero(&cd_read_toc, sizeof(cd_read_toc));
        bzero(&cd_read_disc_info, sizeof(cd_read_disc_info));
        bzero(toc, sizeof(CDTOC) + sizeof(CDTOCDescriptor)*100);
	
        cd_read_toc.format = kCDTOCFormatTOC;
        cd_read_toc.buffer = toc;
        cd_read_toc.bufferLength = 2048;//sizeof(CDTOC) + sizeof(CDTOCDescriptor)*100;
	//pclog("buffer=%p bufferLength=%u\n", cd_read_toc.buffer, cd_read_toc.bufferLength);
        result = ioctl(fd, DKIOCCDREADTOC, &cd_read_toc);
        
        //pclog("Read_toc - fd=%d result=%d errno=%d toc=%p\n", fd, result, errno, toc);
	if (result < 0)
	{
		//pclog("read_toc: DKIOCCDREADTOC failed\n");
		return 0;
	}
        
        //pclog("toc length=%u first=%u last=%u\n", toc->length, toc->sessionFirst, toc->sessionFirst);
        first_track = 1;
        last_track = 0;
        toc_tracks = CDTOCGetDescriptorCount(toc);
        
        for (c = 0; c < toc_tracks; c++)
        {
                if (toc->descriptors[c].session > 1)
                        continue;
                track_addr[c] = msf_to_lba(&toc->descriptors[c].p);
                //pclog("Track %i %02x addr %08x\n", c, toc->descriptors[c].point, track_addr[c]);
                if (toc->descriptors[c].point <= 99 && toc->descriptors[c].adr == 1)
                        last_track++;
        }
        
        //pclog("Tracks %i %i  %i\n", first_track, last_track, toc_tracks);
  
        cd_read_disc_info.buffer = &disc_info;
        cd_read_disc_info.bufferLength = sizeof(disc_info);
        result = ioctl(fd, DKIOCCDREADDISCINFO, &cd_read_disc_info);
	if (result < 0)
	{
		//pclog("read_toc: DKIOCCDREADDISCINFO failed\n");
		return 0;
	}    
          
        return 1;
}

static int ioctl_ready(void)
{
        int ioctl_fd = cd_open();
        //pclog("ioctl_ready - %d %d\n", ioctl_fd, tocvalid);
	if (ioctl_fd <= 0)
		return 0;
        cd_close(ioctl_fd);

	if (tocvalid)
		return 1;
        
        return 0;
}

static int ioctl_get_last_block(unsigned char starttrack, int msf, int maxlen, int single)
{
        int c;
	int lb = 0;
	int tv = 0;
        int ioctl_fd;

        ioctl_fd = cd_open();
	if (ioctl_fd <= 0)
		return 0;

        //ioctl_cd_state = CD_STOPPED;

	if (!tocvalid)
		tv = read_toc(ioctl_fd);

        cd_close(ioctl_fd);
        
	if (!tv)
		return 0;

        for (c = 0; c <= toc_tracks; c++)
        {
                if (track_addr[c] > lb)
                        lb = track_addr[c];
        }
        return lb;
}

static int ioctl_medium_changed(void)
{
        dk_cd_read_disc_info_t cd_read_disc_info;
        CDDiscInfo new_disc_info;
        int result;
        int ioctl_fd;
        
        ioctl_fd = cd_open();
        if (ioctl_fd <= 0)
                return 1;
        
        bzero(&cd_read_disc_info, sizeof(cd_read_disc_info));
        cd_read_disc_info.buffer = &new_disc_info;
        cd_read_disc_info.bufferLength = sizeof(new_disc_info);
        result = ioctl(ioctl_fd, DKIOCCDREADDISCINFO, &cd_read_disc_info);
	if (result < 0)
	{
		//pclog("medium_changed: DKIOCCDREADDISCINFO failed\n");
                ioctl_cd_state = CD_STOPPED;
                tocvalid = 0;
                cd_close(ioctl_fd);
		return 1;
	}
        
        if (memcmp(&disc_info, &new_disc_info, sizeof(CDDiscInfo)))
        {
                //pclog("medium_changed: disc changed\n");
                ioctl_cd_state = CD_STOPPED;
                tocvalid = read_toc(ioctl_fd);
                cd_close(ioctl_fd);
                return 1;
        }
        
        cd_close(ioctl_fd);
        return 0;
}

static uint8_t ioctl_getcurrentsubchannel(uint8_t *b, int msf)
{
        uint32_t cdpos = ioctl_cd_pos;
        int track = get_track_nr(cdpos);
        uint32_t track_address = track_addr[track];
	long size;
	int pos=0;
	int err;
	uint8_t ret;
        

        if (ioctl_cd_state == CD_PLAYING)
		ret = 0x11;
	else if (ioctl_cd_state == CD_PAUSED)
		ret = 0x12;
	else
		ret = 0x13;

        b[pos++] = (toc->descriptors[track].adr << 4) | toc->descriptors[track].control;
        b[pos++] = toc->descriptors[track].point;
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

        //pclog("ioctl_getsubchannel: cdpos=%x track_address=%x track=%i state=%i ret=%02x\n", cdpos, track_address, track, ioctl_cd_state, ret);
        
        return ret;
}

static void ioctl_eject(void)
{
}

static void ioctl_load(void)
{
}

static int ioctl_readsector(uint8_t *b, int sector, int count)
{
        dk_cd_read_t cd_read;
        int res;
        int ioctl_fd;
        
        ioctl_fd = cd_open();
        if (ioctl_fd <= 0)
		return -1;
        //pclog("Read sector %x %i\n", sector, count);
        
        bzero(&cd_read, sizeof(cd_read));
        
        cd_read.offset = sector*2048;
        cd_read.buffer = b;
        cd_read.bufferLength = count*2048;
        cd_read.sectorType = kCDSectorTypeMode1;
        cd_read.sectorArea = kCDSectorAreaUser;
        
        res = ioctl(ioctl_fd, DKIOCCDREAD, &cd_read);
        //pclog("read res = %i %i %i\n", res, cd_read.bufferLength, errno);

        cd_close(ioctl_fd);

	return 0;
}

static void ioctl_readsector_raw(uint8_t *b, int sector)
{
        dk_cd_read_t cd_read;
        int res;
        int ioctl_fd;
        
        ioctl_fd = cd_open();
        if (ioctl_fd <= 0)
		return;
        //pclog("Read sector %x %i\n", sector, count);
        
        bzero(&cd_read, sizeof(cd_read));
        
        cd_read.offset = sector*kCDSectorSizeWhole;
        cd_read.buffer = b;
        cd_read.bufferLength = kCDSectorSizeWhole;
        cd_read.sectorType = kCDSectorTypeUnknown;
        cd_read.sectorArea = kCDSectorAreaUser;
        
        res = ioctl(ioctl_fd, DKIOCCDREAD, &cd_read);
        //pclog("read res = %i %i %i\n", res, cd_read.bufferLength, errno);

        cd_close(ioctl_fd);
}

static int ioctl_readtoc(unsigned char *b, unsigned char starttrack, int msf, int maxlen, int single)
{
        int len=4;
        int c,d;
        uint32_t temp;
	uint32_t last_address = 0;
        int seenAA = 0;
        
        pclog("ioctl_readtoc\n");
        //ioctl_cd_state = CD_STOPPED;

	if (!tocvalid)
        {
                int ioctl_fd = cd_open();
                
                if (ioctl_fd <= 0)
                        return 4;
                
		tocvalid = read_toc(ioctl_fd);
                cd_close(ioctl_fd);
        }
	if (!tocvalid)
		return 4;

                //pclog("Read TOC done! %i\n",single);
        b[2] = first_track;
        b[3] = last_track;
        d = 0;
        //pclog("Read TOC starttrack=%i\n", starttrack);
        for (c = 0; c <= toc_tracks; c++)
        {
                if (toc->descriptors[c].point && toc->descriptors[c].point >= starttrack && toc->descriptors[c].point <= 99)
                {
                        d = c;
                        break;
                }
        }
        //pclog("Start from track %i\n",d);
        b[2] = toc->descriptors[c].point;
        //last_block = 0;

        for (c = d; c <= toc_tracks; c++)
        {
                uint32_t address;
                if ((len + 8) > maxlen)
			break;
                //pclog("Len %i max %i Track %02X - %02X %02X %02i:%02i:%02i %08X\n",len,maxlen,toc->descriptors[c].point,toc->descriptors[c].adr,toc->descriptors[c].control,toc->descriptors[c].p.minute, toc->descriptors[c].p.second, toc->descriptors[c].p.frame,track_addr[c]);
                address = track_addr[c];
		if (address < last_address)
			continue;
                if (toc->descriptors[c].point == 0xaa)
                        seenAA = 1;
		last_address = address;
                b[len++] = 0; /*Reserved*/
                b[len++] = (toc->descriptors[c].adr << 4) | toc->descriptors[c].control;
                b[len++] = toc->descriptors[c].point;
                b[len++] = 0; /*Reserved*/
                //if (address > last_block)
                //        last_block = address;

                if (msf)
                {
                        b[len++] = 0;
                        b[len++] = toc->descriptors[c].p.minute;
                        b[len++] = toc->descriptors[c].p.second;
                        b[len++] = toc->descriptors[c].p.frame;
                }
                else
                {
                        temp = track_addr[c];
                        b[len++] = temp >> 24;
                        b[len++] = temp >> 16;
                        b[len++] = temp >> 8;
                        b[len++] = temp;
                }
                if (single)
			break;
        }
        
        if (!seenAA && !single)
        {
                uint32_t lb = 0;
                        
                for (c = 0; c <= toc_tracks; c++)
                {
                        if (track_addr[c] > lb)
                                lb = track_addr[c];
                }
                //pclog("Track AA - %08x %i\n", lb, len);
                b[len++] = 0; /*Reserved*/
                b[len++] = (1 << 4) | 1;
                b[len++] = 0xaa;
                b[len++] = 0; /*Reserved*/

                if (msf)
                {
                        b[len++] = 0;
                        b[len++] = (lb / 60) / 75;
                        b[len++] = (lb / 75) % 60;
                        b[len++] = lb % 75;
                }
                else
                {
                        temp = track_addr[c];
                        b[len++] = lb >> 24;
                        b[len++] = lb >> 16;
                        b[len++] = lb >> 8;
                        b[len++] = lb;
                }
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
        int len = 4;
        int session = 0, first_track_nr = 0;
        int c, d;

	if (!tocvalid)
        {
                int ioctl_fd = cd_open();
                
                if (ioctl_fd <= 0)
                        return 4;
                
		tocvalid = read_toc(ioctl_fd);
                cd_close(ioctl_fd);
        }

	if (!tocvalid)
		return 4;
        
        for (c = 0; c <= toc_tracks; c++)
        {
                if (toc->descriptors[c].point && toc->descriptors[c].point <= 99)
                {
                        if (toc->descriptors[c].session > session || (toc->descriptors[c].session == session && toc->descriptors[c].point < first_track_nr))
                        {
                                session = toc->descriptors[c].session;
                                first_track_nr = toc->descriptors[c].point;
                                d = c;
                        }
                }
        }
        
        b[2] = toc->sessionFirst;
        b[3] = toc->sessionLast;
        b[len++] = 0; /*Reserved*/
        b[len++] = (toc->descriptors[d].adr << 4) | toc->descriptors[d].control;
        b[len++] = toc->descriptors[d].point;
        b[len++] = 0; /*Reserved*/
        if (msf)
        {
                b[len++] = 0;
                b[len++] = toc->descriptors[d].p.minute;
                b[len++] = toc->descriptors[d].p.second;
                b[len++] = toc->descriptors[d].p.frame;
        }
        else
        {
                uint32_t temp = track_addr[d];
                b[len++] = temp >> 24;
                b[len++] = temp >> 16;
                b[len++] = temp >> 8;
                b[len++] = temp;
        }

	return len;
}

static int ioctl_readtoc_raw(unsigned char *b, int maxlen)
{
	int len = 4;
        int ioctl_fd;
        int track;

//pclog("read_toc\n");
        if (!tocvalid)
                return 0;
        
        ioctl_fd = cd_open();
	if (ioctl_fd <= 0)
		return 0;
        cd_close(ioctl_fd);

	b[2] = toc->sessionFirst;
	b[3] = toc->sessionLast;

	for (track = 0; track <= toc_tracks; track++)
	{
		if ((len + 11) > maxlen)
		{
			pclog("ioctl_readtocraw: This iteration would fill the buffer beyond the bounds, aborting...\n");
			return len;
		}


//		pclog("read_toc: Track %02X - number %02X control %02X adr %02X address %02X %02X %02X %02X\n", track, toc[track].cdte_track, toc[track].cdte_ctrl, toc[track].cdte_adr, 0, toc[track].cdte_addr.msf.minute, toc[track].cdte_addr.msf.second, toc[track].cdte_addr.msf.frame);

		b[len++] = toc->descriptors[track].session;
		b[len++] = (toc->descriptors[track].adr << 4) | toc->descriptors[track].control;
		b[len++] = toc->descriptors[track].tno;
		b[len++] = toc->descriptors[track].point;
		b[len++] = toc->descriptors[track].address.minute;
		b[len++] = toc->descriptors[track].address.second;
		b[len++] = toc->descriptors[track].address.frame;
		b[len++] = toc->descriptors[track].zero;
		b[len++] = toc->descriptors[track].p.minute;
		b[len++] = toc->descriptors[track].p.second;
		b[len++] = toc->descriptors[track].p.frame;
	}

	return len;
}

static uint32_t ioctl_size()
{
	return cdrom_capacity;
}

static int ioctl_status()
{
        if (!ioctl_ready)
                return CD_STATUS_EMPTY;
	return CD_STATUS_STOPPED;
}
void ioctl_reset()
{
        int ioctl_fd;
        
	tocvalid = 0;

        ioctl_fd = cd_open();
	if (ioctl_fd <= 0)
		return;

	tocvalid = read_toc(ioctl_fd);
        cd_close(ioctl_fd);
}

void ioctl_set_drive(char d)
{
	ioctl_close();
	atapi=&ioctl_atapi;
	ioctl_open(d);
}

int ioctl_open(char d)
{
	atapi = &ioctl_atapi;

        toc = malloc(2048);

        cd_drive = d;
        return 0;
}

void ioctl_close(void)
{
        if (toc)
        {
                free(toc);
                toc = NULL;
        }
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
