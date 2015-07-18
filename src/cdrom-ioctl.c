/*Win32 CD-ROM support via IOCTL*/

#include <windows.h>
#include <io.h>
#ifdef __MINGW64__
#include "ntddcdrm.h"
#else
#include "ddk/ntddcdrm.h"
#endif
#include "ibm.h"
#include "ide.h"
#include "cdrom-ioctl.h"

int cdrom_drive;

#ifndef __MINGW64__
typedef struct _CDROM_TOC_SESSION_DATA {
  UCHAR      Length[2];
  UCHAR      FirstCompleteSession;
  UCHAR      LastCompleteSession;
  TRACK_DATA TrackData[1];
} CDROM_TOC_SESSION_DATA, *PCDROM_TOC_SESSION_DATA;
#endif

static ATAPI ioctl_atapi;

static uint32_t last_block = 0;
static int ioctl_inited = 0;
static char ioctl_path[8];
static void ioctl_close(void);
static HANDLE hIOCTL;
static CDROM_TOC toc;
static int tocvalid = 0;

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
	RAW_READ_INFO in;
	DWORD count;

//	return;
//        pclog("Audio callback %08X %08X %i %i %i %04X %i\n", ioctl_cd_pos, ioctl_cd_end, ioctl_cd_state, cd_buflen, len, cd_buffer[4], GetTickCount());
        if (ioctl_cd_state != CD_PLAYING) 
        {
                memset(output, 0, len * 2);
                return;
        }
        while (cd_buflen < len)
        {
                if (ioctl_cd_pos < ioctl_cd_end)
                {
		        in.DiskOffset.LowPart  = ioctl_cd_pos * 2048;
        		in.DiskOffset.HighPart = 0;
        		in.SectorCount	       = 1;
        		in.TrackMode	       = CDDA;		
        		ioctl_open(0);
//        		pclog("Read to %i\n", cd_buflen);
        		if (!DeviceIoControl(hIOCTL, IOCTL_CDROM_RAW_READ, &in, sizeof(in), &cd_buffer[cd_buflen], 2352, &count, NULL))
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
                        ioctl_close();
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

        for (c = toc.FirstTrack; c < toc.LastTrack; c++)
        {
                uint32_t track_address = toc.TrackData[c].Address[3] +
                                         (toc.TrackData[c].Address[2] * 75) +
                                         (toc.TrackData[c].Address[1] * 75 * 60);

                if (track_address <= pos)
                        track = c;
        }
        return track;
}

static void ioctl_playaudio(uint32_t pos, uint32_t len, int ismsf)
{
        if (!cdrom_drive) return;
        pclog("Play audio - %08X %08X %i\n", pos, len, ismsf);
        if (ismsf)
        {
                pos = (pos & 0xff) + (((pos >> 8) & 0xff) * 75) + (((pos >> 16) & 0xff) * 75 * 60);
                len = (len & 0xff) + (((len >> 8) & 0xff) * 75) + (((len >> 16) & 0xff) * 75 * 60);
                pclog("MSF - pos = %08X len = %08X\n", pos, len);
        }
        else
           len += pos;
        ioctl_cd_pos   = pos;// + 150;
        ioctl_cd_end   = len;// + 150;
        ioctl_cd_state = CD_PLAYING;
        pclog("Audio start %08X %08X %i %i %i\n", ioctl_cd_pos, ioctl_cd_end, ioctl_cd_state, cd_buflen, len);        
/*        CDROM_PLAY_AUDIO_MSF msf;
        long size;
        BOOL b;
        if (ismsf)
        {
                msf.StartingF=pos&0xFF;
                msf.StartingS=(pos>>8)&0xFF;
                msf.StartingM=(pos>>16)&0xFF;
                msf.EndingF=len&0xFF;
                msf.EndingS=(len>>8)&0xFF;
                msf.EndingM=(len>>16)&0xFF;
        }
        else
        {
                msf.StartingF=(uint8_t)(addr%75); addr/=75;
                msf.StartingS=(uint8_t)(addr%60); addr/=60;
                msf.StartingM=(uint8_t)(addr);
                addr=pos+len+150;
                msf.EndingF=(uint8_t)(addr%75); addr/=75;
                msf.EndingS=(uint8_t)(addr%60); addr/=60;
                msf.EndingM=(uint8_t)(addr);
        }
        ioctl_open(0);
        b = DeviceIoControl(hIOCTL,IOCTL_CDROM_PLAY_AUDIO_MSF,&msf,sizeof(msf),NULL,0,&size,NULL);
        pclog("DeviceIoControl returns %i\n", (int) b);
        ioctl_close();*/
}

static void ioctl_pause(void)
{
        if (!cdrom_drive) return;
        if (ioctl_cd_state == CD_PLAYING)
           ioctl_cd_state = CD_PAUSED;
//        ioctl_open(0);
//        DeviceIoControl(hIOCTL,IOCTL_CDROM_PAUSE_AUDIO,NULL,0,NULL,0,&size,NULL);
//        ioctl_close();
}

static void ioctl_resume(void)
{
        if (!cdrom_drive) return;
        if (ioctl_cd_state == CD_PAUSED)
           ioctl_cd_state = CD_PLAYING;
//        ioctl_open(0);
//        DeviceIoControl(hIOCTL,IOCTL_CDROM_RESUME_AUDIO,NULL,0,NULL,0,&size,NULL);
//        ioctl_close();
}

static void ioctl_stop(void)
{
        if (!cdrom_drive) return;
        ioctl_cd_state = CD_STOPPED;
//        ioctl_open(0);
//        DeviceIoControl(hIOCTL,IOCTL_CDROM_STOP_AUDIO,NULL,0,NULL,0,&size,NULL);
//        ioctl_close();
}

static void ioctl_seek(uint32_t pos)
{
        if (!cdrom_drive) return;
 //       ioctl_cd_state = CD_STOPPED;
        pclog("Seek %08X\n", pos);
        ioctl_cd_pos   = pos;
        ioctl_cd_state = CD_STOPPED;
/*        pos+=150;
        CDROM_SEEK_AUDIO_MSF msf;
        msf.F=(uint8_t)(pos%75); pos/=75;
        msf.S=(uint8_t)(pos%60); pos/=60;
        msf.M=(uint8_t)(pos);
//        pclog("Seek to %02i:%02i:%02i\n",msf.M,msf.S,msf.F);
        ioctl_open(0);
        DeviceIoControl(hIOCTL,IOCTL_CDROM_SEEK_AUDIO_MSF,&msf,sizeof(msf),NULL,0,&size,NULL);
        ioctl_close();*/
}

static int ioctl_ready(void)
{
        long size;
        int temp;
        CDROM_TOC ltoc;
//        pclog("Ready? %i\n",cdrom_drive);
        if (!cdrom_drive) return 0;
        ioctl_open(0);
        temp=DeviceIoControl(hIOCTL,IOCTL_CDROM_READ_TOC, NULL,0,&ltoc,sizeof(ltoc),&size,NULL);
        ioctl_close();
        if (!temp)
                return 0;
        if ((ltoc.TrackData[ltoc.LastTrack].Address[1] != toc.TrackData[toc.LastTrack].Address[1]) ||
            (ltoc.TrackData[ltoc.LastTrack].Address[2] != toc.TrackData[toc.LastTrack].Address[2]) ||
            (ltoc.TrackData[ltoc.LastTrack].Address[3] != toc.TrackData[toc.LastTrack].Address[3]) ||
            !tocvalid)
        {
                ioctl_cd_state = CD_STOPPED;                
  /*              pclog("Not ready %02X %02X %02X  %02X %02X %02X  %i\n",ltoc.TrackData[ltoc.LastTrack].Address[1],ltoc.TrackData[ltoc.LastTrack].Address[2],ltoc.TrackData[ltoc.LastTrack].Address[3],
                                                                        toc.TrackData[ltoc.LastTrack].Address[1], toc.TrackData[ltoc.LastTrack].Address[2], toc.TrackData[ltoc.LastTrack].Address[3],tocvalid);*/
        //        atapi_discchanged();
/*                ioctl_open(0);
                temp=DeviceIoControl(hIOCTL,IOCTL_CDROM_READ_TOC, NULL,0,&toc,sizeof(toc),&size,NULL);
                ioctl_close();*/
                toc=ltoc;
                tocvalid=1;
                return 0;
        }
//        pclog("IOCTL says ready\n");
        return 1;
//        return (temp)?1:0;
}

static uint8_t ioctl_getcurrentsubchannel(uint8_t *b, int msf)
{
	CDROM_SUB_Q_DATA_FORMAT insub;
	SUB_Q_CHANNEL_DATA sub;
	long size;
	int pos=0;
        if (!cdrom_drive) return 0;
        
	insub.Format = IOCTL_CDROM_CURRENT_POSITION;
        ioctl_open(0);
        DeviceIoControl(hIOCTL,IOCTL_CDROM_READ_Q_CHANNEL,&insub,sizeof(insub),&sub,sizeof(sub),&size,NULL);
        ioctl_close();

        if (ioctl_cd_state == CD_PLAYING || ioctl_cd_state == CD_PAUSED)
        {
                uint32_t cdpos = ioctl_cd_pos;
                int track = get_track_nr(cdpos);
                uint32_t track_address = toc.TrackData[track].Address[3] +
                                         (toc.TrackData[track].Address[2] * 75) +
                                         (toc.TrackData[track].Address[1] * 75 * 60);

                b[pos++] = sub.CurrentPosition.Control;
                b[pos++] = track + 1;
                b[pos++] = sub.CurrentPosition.IndexNumber;

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

                if (ioctl_cd_state == CD_PLAYING) return 0x11;
                return 0x12;
        }

        b[pos++]=sub.CurrentPosition.Control;
        b[pos++]=sub.CurrentPosition.TrackNumber;
        b[pos++]=sub.CurrentPosition.IndexNumber;
        
        if (msf)
        {
                int c;
                for (c = 0; c < 4; c++)
                        b[pos++] = sub.CurrentPosition.AbsoluteAddress[c];
                for (c = 0; c < 4; c++)
                        b[pos++] = sub.CurrentPosition.TrackRelativeAddress[c];
        }
        else
        {
                uint32_t temp = MSFtoLBA(sub.CurrentPosition.AbsoluteAddress[1], sub.CurrentPosition.AbsoluteAddress[2], sub.CurrentPosition.AbsoluteAddress[3]);
                b[pos++] = temp >> 24;
                b[pos++] = temp >> 16;
                b[pos++] = temp >> 8;
                b[pos++] = temp;
                temp = MSFtoLBA(sub.CurrentPosition.TrackRelativeAddress[1], sub.CurrentPosition.TrackRelativeAddress[2], sub.CurrentPosition.TrackRelativeAddress[3]);
                b[pos++] = temp >> 24;
                b[pos++] = temp >> 16;
                b[pos++] = temp >> 8;
                b[pos++] = temp;
        }

        return 0x13;
}

static void ioctl_eject(void)
{
        long size;
        if (!cdrom_drive) return;
        ioctl_cd_state = CD_STOPPED;        
        ioctl_open(0);
        DeviceIoControl(hIOCTL,IOCTL_STORAGE_EJECT_MEDIA,NULL,0,NULL,0,&size,NULL);
        ioctl_close();
}

static void ioctl_load(void)
{
        long size;
        if (!cdrom_drive) return;
        ioctl_cd_state = CD_STOPPED;        
        ioctl_open(0);
        DeviceIoControl(hIOCTL,IOCTL_STORAGE_LOAD_MEDIA,NULL,0,NULL,0,&size,NULL);
        ioctl_close();
}

static void ioctl_readsector(uint8_t *b, int sector)
{
        LARGE_INTEGER pos;
        long size;
        if (!cdrom_drive) return;
        ioctl_cd_state = CD_STOPPED;        
        pos.QuadPart=sector*2048;
        ioctl_open(0);
        SetFilePointer(hIOCTL,pos.LowPart,&pos.HighPart,FILE_BEGIN);
        ReadFile(hIOCTL,b,2048,&size,NULL);
        ioctl_close();
}

static int ioctl_readtoc(unsigned char *b, unsigned char starttrack, int msf, int maxlen, int single)
{
        int len=4;
        long size;
        int c,d;
        uint32_t temp;
        if (!cdrom_drive) return 0;
        ioctl_cd_state = CD_STOPPED;        
        ioctl_open(0);
        DeviceIoControl(hIOCTL,IOCTL_CDROM_READ_TOC, NULL,0,&toc,sizeof(toc),&size,NULL);
        ioctl_close();
        tocvalid=1;
//        pclog("Read TOC done! %i\n",single);
        b[2]=toc.FirstTrack;
        b[3]=toc.LastTrack;
        d=0;
        for (c=0;c<=toc.LastTrack;c++)
        {
                if (toc.TrackData[c].TrackNumber>=starttrack)
                {
                        d=c;
                        break;
                }
        }
        b[2]=toc.TrackData[c].TrackNumber;
        last_block = 0;
        for (c=d;c<=toc.LastTrack;c++)
        {
                uint32_t address;
                if ((len+8)>maxlen) break;
//                pclog("Len %i max %i Track %02X - %02X %02X %i %i %i %i %08X\n",len,maxlen,toc.TrackData[c].TrackNumber,toc.TrackData[c].Adr,toc.TrackData[c].Control,toc.TrackData[c].Address[0],toc.TrackData[c].Address[1],toc.TrackData[c].Address[2],toc.TrackData[c].Address[3],MSFtoLBA(toc.TrackData[c].Address[1],toc.TrackData[c].Address[2],toc.TrackData[c].Address[3]));
                b[len++]=0; /*Reserved*/
                b[len++]=(toc.TrackData[c].Adr<<4)|toc.TrackData[c].Control;
                b[len++]=toc.TrackData[c].TrackNumber;
                b[len++]=0; /*Reserved*/
                address = MSFtoLBA(toc.TrackData[c].Address[1],toc.TrackData[c].Address[2],toc.TrackData[c].Address[3]);
                if (address > last_block)
                        last_block = address;

                if (msf)
                {
                        b[len++]=toc.TrackData[c].Address[0];
                        b[len++]=toc.TrackData[c].Address[1];
                        b[len++]=toc.TrackData[c].Address[2];
                        b[len++]=toc.TrackData[c].Address[3];
                }
                else
                {
                        temp=MSFtoLBA(toc.TrackData[c].Address[1],toc.TrackData[c].Address[2],toc.TrackData[c].Address[3]);
                        b[len++]=temp>>24;
                        b[len++]=temp>>16;
                        b[len++]=temp>>8;
                        b[len++]=temp;
                }
                if (single) break;
        }
        b[0] = (uint8_t)(((len-2) >> 8) & 0xff);
        b[1] = (uint8_t)((len-2) & 0xff);
/*        pclog("Table of Contents (%i bytes) : \n",size);
        pclog("First track - %02X\n",toc.FirstTrack);
        pclog("Last  track - %02X\n",toc.LastTrack);
        for (c=0;c<=toc.LastTrack;c++)
            pclog("Track %02X - number %02X control %02X adr %02X address %02X %02X %02X %02X\n",c,toc.TrackData[c].TrackNumber,toc.TrackData[c].Control,toc.TrackData[c].Adr,toc.TrackData[c].Address[0],toc.TrackData[c].Address[1],toc.TrackData[c].Address[2],toc.TrackData[c].Address[3]);
        for (c=0;c<=toc.LastTrack;c++)
            pclog("Track %02X - number %02X control %02X adr %02X address %06X\n",c,toc.TrackData[c].TrackNumber,toc.TrackData[c].Control,toc.TrackData[c].Adr,MSFtoLBA(toc.TrackData[c].Address[1],toc.TrackData[c].Address[2],toc.TrackData[c].Address[3]));*/
        return len;
}

static void ioctl_readtoc_session(unsigned char *b, int msf, int maxlen)
{
        int len=4;
        int size;
        uint32_t temp;
        CDROM_READ_TOC_EX toc_ex;
        CDROM_TOC_SESSION_DATA toc;
        if (!cdrom_drive) return;
        ioctl_cd_state = CD_STOPPED;        
        memset(&toc_ex,0,sizeof(toc_ex));
        memset(&toc,0,sizeof(toc));
        toc_ex.Format=CDROM_READ_TOC_EX_FORMAT_SESSION;
        toc_ex.Msf=msf;
        toc_ex.SessionTrack=0;
        ioctl_open(0);
        DeviceIoControl(hIOCTL,IOCTL_CDROM_READ_TOC_EX, &toc_ex,sizeof(toc_ex),&toc,sizeof(toc),(PDWORD)&size,NULL);
        ioctl_close();
//        pclog("Read TOC session - %i %02X %02X %i %i %02X %02X %02X\n",size,toc.Length[0],toc.Length[1],toc.FirstCompleteSession,toc.LastCompleteSession,toc.TrackData[0].Adr,toc.TrackData[0].Control,toc.TrackData[0].TrackNumber);
        b[2]=toc.FirstCompleteSession;
        b[3]=toc.LastCompleteSession;
                b[len++]=0; /*Reserved*/
                b[len++]=(toc.TrackData[0].Adr<<4)|toc.TrackData[0].Control;
                b[len++]=toc.TrackData[0].TrackNumber;
                b[len++]=0; /*Reserved*/
                if (msf)
                {
                        b[len++]=toc.TrackData[0].Address[0];
                        b[len++]=toc.TrackData[0].Address[1];
                        b[len++]=toc.TrackData[0].Address[2];
                        b[len++]=toc.TrackData[0].Address[3];
                }
                else
                {
                        temp=MSFtoLBA(toc.TrackData[0].Address[1],toc.TrackData[0].Address[2],toc.TrackData[0].Address[3]);
                        b[len++]=temp>>24;
                        b[len++]=temp>>16;
                        b[len++]=temp>>8;
                        b[len++]=temp;
                }
}

static uint32_t ioctl_size()
{
        unsigned char b[4096];

        atapi->readtoc(b, 0, 0, 4096, 0);
        
        return last_block;
}

void ioctl_reset()
{
        CDROM_TOC ltoc;
        int temp;
        long size;

        if (!cdrom_drive)
        {
                tocvalid = 0;
                return;
        }
        
        ioctl_open(0);
        temp = DeviceIoControl(hIOCTL, IOCTL_CDROM_READ_TOC, NULL, 0, &ltoc, sizeof(ltoc), &size, NULL);
        ioctl_close();

        toc = ltoc;
        tocvalid = 1;
}

int ioctl_open(char d)
{
//        char s[8];
        if (!ioctl_inited)
        {
                sprintf(ioctl_path,"\\\\.\\%c:",d);
                pclog("Path is %s\n",ioctl_path);
                tocvalid=0;
        }
//        pclog("Opening %s\n",ioctl_path);
	hIOCTL	= CreateFile(/*"\\\\.\\g:"*/ioctl_path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
	if (!hIOCTL)
	{
                //fatal("IOCTL");
        }
        atapi=&ioctl_atapi;
        if (!ioctl_inited)
        {
                ioctl_inited=1;
                CloseHandle(hIOCTL);
        }
        return 0;
}

static void ioctl_close(void)
{
        CloseHandle(hIOCTL);
}

static void ioctl_exit(void)
{
        ioctl_stop();
        ioctl_inited=0;
        tocvalid=0;
}

static ATAPI ioctl_atapi=
{
        ioctl_ready,
        ioctl_readtoc,
        ioctl_readtoc_session,
        ioctl_getcurrentsubchannel,
        ioctl_readsector,
        ioctl_playaudio,
        ioctl_seek,
        ioctl_load,
        ioctl_eject,
        ioctl_pause,
        ioctl_resume,
        ioctl_size,
        ioctl_stop,
        ioctl_exit
};
