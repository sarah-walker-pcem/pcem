#include <stdio.h>
#include "ibm.h"
#include "filters.h"

int soundon = 1;
int16_t *adbuffer,*adbuffer2;
int16_t *psgbuffer;
uint16_t *cmsbuffer;
int16_t *spkbuffer;
int16_t *outbuffer;
uint16_t *gusbuffer;

void initsound()
{
        initalmain(0,NULL);
        inital();
//        install_sound(DIGI_AUTODETECT,MIDI_NONE,0);
//        as=play_audio_stream(SOUNDBUFLEN,16,1,48000,255,128);
        adbuffer=malloc((SOUNDBUFLEN)*2);
        adbuffer2=malloc((SOUNDBUFLEN)*2);
        psgbuffer=malloc((SOUNDBUFLEN)*2);
        cmsbuffer=malloc((SOUNDBUFLEN)*2*2);
        gusbuffer=malloc((SOUNDBUFLEN)*2*2);
        spkbuffer=malloc(((SOUNDBUFLEN)*2)+32);
        outbuffer=malloc((SOUNDBUFLEN)*2*2);
}

int adpoll=0;
void pollad()
{
/*        if (adpoll>=20) return;
        getadlibl(adbuffer+(adpoll*(48000/200)),48000/200);
        getadlibr(adbuffer2+(adpoll*(48000/200)),48000/200);
        adpoll++;*/
//        printf("ADPOLL %i\n",adpoll);
}

void polladlib()
{
        getadlib(adbuffer+(adpoll),adbuffer2+(adpoll),1);
        adpoll++;
}

int psgpoll=0;
void pollpsg()
{
        if (psgpoll>=20) return;
        getpsg(psgbuffer+(psgpoll*(48000/200)),48000/200);
        psgpoll++;
}

int cmspoll=0;
void pollcms()
{
        if (cmspoll>=20) return;
        getcms(cmsbuffer+(cmspoll*(48000/200)*2),48000/200);
        cmspoll++;
}

int guspoll=0;
void pollgussnd()
{
        if (guspoll>=20) return;
        getgus(gusbuffer+(guspoll*(48000/200)*2),48000/200);
        guspoll++;
}

int spkpos=0;
int wasgated = 0;
void pollspk()
{
        if (spkpos>=SOUNDBUFLEN) return;
//        printf("SPeaker - %i %i %i %02X\n",speakval,gated,speakon,pit.m[2]);
        if (gated)
        {
                if (!pit.m[2] || pit.m[2]==4)
                   spkbuffer[spkpos]=speakval;
                else 
                   spkbuffer[spkpos]=(speakon)?0x1400:0;
        }
        else
           spkbuffer[spkpos]=(wasgated)?0x1400:0;
        spkpos++;
        wasgated=0;
}

FILE *soundf;

static int16_t cd_buffer[(SOUNDBUFLEN) * 2];
void pollsound()
{
        int c;
        int16_t t[4];
        uint32_t pos;
//        printf("Pollsound! %i\n",soundon);
//        if (soundon)
//        {
                for (c=0;c<(SOUNDBUFLEN);c++) outbuffer[c<<1]=outbuffer[(c<<1)+1]=0;
                for (c=0;c<(SOUNDBUFLEN);c++)
                {
                        outbuffer[c<<1]+=((adbuffer[c]*mixer.fm_l)>>16);
                        outbuffer[(c<<1)+1]+=((adbuffer[c]*mixer.fm_r)>>16);
//                        if (!c) pclog("F %04X %04X %04X\n",adbuffer[c],outbuffer[c<<1],mixer.fm_l);
                }
                addsb(outbuffer);
                for (c=0;c<(SOUNDBUFLEN);c++)
                {
//                        if (!c) pclog("M %04X ",outbuffer[c<<1]);
                        outbuffer[c<<1]=(outbuffer[c<<1]*mixer.master_l)>>16;
                        outbuffer[(c<<1)+1]=(outbuffer[(c<<1)+1]*mixer.master_r)>>16;
//                        if (!c) pclog("%04X %04X\n",outbuffer[c<<1],mixer.master_l);
                }
                if (mixer.bass_l!=8 || mixer.bass_r!=8 || mixer.treble_l!=8 || mixer.treble_r!=8)
                {
                        for (c=0;c<(SOUNDBUFLEN);c++)
                        {
                                if (mixer.bass_l>8)   outbuffer[c<<1]    =(outbuffer[c<<1]    +(((int16_t)low_iir(0,(float)outbuffer[c<<1])         *(mixer.bass_l-8))>>1))*((15-mixer.bass_l)+16)>>5;
                                if (mixer.bass_r>8)   outbuffer[(c<<1)+1]=(outbuffer[(c<<1)+1]+(((int16_t)low_iir(1,(float)outbuffer[(c<<1)+1])     *(mixer.bass_r-8))>>1))*((15-mixer.bass_r)+16)>>5;
                                if (mixer.treble_l>8) outbuffer[c<<1]    =(outbuffer[c<<1]    +(((int16_t)high_iir(0,(float)outbuffer[c<<1])        *(mixer.treble_l-8))>>1))*((15-mixer.treble_l)+16)>>5;
                                if (mixer.treble_r>8) outbuffer[(c<<1)+1]=(outbuffer[(c<<1)+1]+(((int16_t)high_iir(1,(float)outbuffer[(c<<1)+1])    *(mixer.treble_r-8))>>1))*((15-mixer.treble_r)+16)>>5;
                                if (mixer.bass_l<8)   outbuffer[c<<1]    =(outbuffer[c<<1]    +(((int16_t)low_cut_iir(0,(float)outbuffer[c<<1])     *(8-mixer.bass_l))>>1))*(mixer.bass_l+16)>>5;
                                if (mixer.bass_r<8)   outbuffer[(c<<1)+1]=(outbuffer[(c<<1)+1]+(((int16_t)low_cut_iir(1,(float)outbuffer[(c<<1)+1]) *(8-mixer.bass_r))>>1))*(mixer.bass_r+16)>>5;
                                if (mixer.treble_l<8) outbuffer[c<<1]    =(outbuffer[c<<1]    +(((int16_t)high_cut_iir(0,(float)outbuffer[c<<1])    *(8-mixer.treble_l))>>1))*(mixer.treble_l+16)>>5;
                                if (mixer.treble_r<8) outbuffer[(c<<1)+1]=(outbuffer[(c<<1)+1]+(((int16_t)high_cut_iir(1,(float)outbuffer[(c<<1)+1])*(8-mixer.treble_r))>>1))*(mixer.treble_r+16)>>5;
                        }
                }
                for (c=0;c<(SOUNDBUFLEN);c++)
                {
                        outbuffer[c<<1]+=(spkbuffer[c]/2);
                        outbuffer[(c<<1)+1]+=(spkbuffer[c]/2);
                }
                for (c=0;c<(SOUNDBUFLEN);c++)
                {
                        outbuffer[c<<1]+=(psgbuffer[c]/2);
                        outbuffer[(c<<1)+1]+=(psgbuffer[c]/2);
                }
                for (c=0;c<((SOUNDBUFLEN)*2);c++)
                    outbuffer[c]+=(cmsbuffer[c]/2);
                for (c=0;c<((SOUNDBUFLEN)*2);c++)
                    outbuffer[c]+=(gusbuffer[c]);
                adddac(outbuffer);
                ioctl_audio_callback(cd_buffer, ((SOUNDBUFLEN) * 2  * 441) / 480);
                pos = 0;
                for (c = 0; c < (SOUNDBUFLEN) * 2; c+=2)
                {
                        outbuffer[c]     += cd_buffer[((pos >> 16) << 1)]     / 2;
                        outbuffer[c + 1] += cd_buffer[((pos >> 16) << 1) + 1] / 2;                        
//                        outbuffer[c] += (int16_t)((int32_t)cd_buffer[pos >> 16] * (65536 - (pos & 0xffff))) / 65536;
//                        outbuffer[c] += (int16_t)((int32_t)cd_buffer[(pos >> 16) + 1] * (pos & 0xffff)) / 65536;                        
                        pos += 60211; //(44100 * 65536) / 48000;
                }
                
//                if (!soundf) soundf=fopen("sound.pcm","wb");
//                fwrite(outbuffer,(SOUNDBUFLEN)*2*2,1,soundf);
                if (soundon) givealbuffer(outbuffer);
//        }
//        addsb(outbuffer);
//        adddac(outbuffer);
        adpoll=0;
        psgpoll=0;
        cmspoll=0;
        spkpos=0;
        guspoll=0;
}

int sndcount;
void pollsound60hz()
{
//        printf("Poll sound %i\n",sndcount);
//                pollad();
                pollpsg();
                pollcms();
                pollgussnd();
                sndcount++;
                if (sndcount==20)
                {
                        sndcount=0;
                        pollsound();
                }
}
