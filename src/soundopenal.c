#define USE_OPENAL
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef USE_OPENAL
#include <AL/al.h>
#include <AL/alut.h>
#endif
#include "ibm.h"

FILE *allog;
#ifdef USE_OPENAL
ALuint buffers[4]; // front and back buffers
ALuint source;     // audio source
ALenum format;     // internal format
#endif
#define FREQ 48000
#define BUFLEN SOUNDBUFLEN

void closeal();

void initalmain(int argc, char *argv[])
{
#ifdef USE_OPENAL
        alutInit(0,0);
//        printf("AlutInit\n");
        atexit(closeal);
//        printf("AlutInit\n");
#endif
}

void closeal()
{
#ifdef USE_OPENAL
        alutExit();
#endif
}

void check()
{
#ifdef USE_OPENAL
        ALenum error;
        if ((error = alGetError()) != AL_NO_ERROR)
        {
//                printf("Error : %08X\n", error);
//                exit(-1);
        }
#endif
}

void inital()
{
#ifdef USE_OPENAL
        int c;
        int16_t buf[BUFLEN*2];
        format = AL_FORMAT_STEREO16;
//        printf("1\n");
        check();

//        printf("2\n");
        alGenBuffers(4, buffers);
        check();
        
//        printf("3\n");
        alGenSources(1, &source);
        check();

//        printf("4\n");
        alSource3f(source, AL_POSITION,        0.0, 0.0, 0.0);
        alSource3f(source, AL_VELOCITY,        0.0, 0.0, 0.0);
        alSource3f(source, AL_DIRECTION,       0.0, 0.0, 0.0);
        alSourcef (source, AL_ROLLOFF_FACTOR,  0.0          );
        alSourcei (source, AL_SOURCE_RELATIVE, AL_TRUE      );
        check();

        memset(buf,0,BUFLEN*4);

//        printf("5\n");
        for (c=0;c<4;c++)
            alBufferData(buffers[c], AL_FORMAT_STEREO16, buf, BUFLEN*2*2, FREQ);
        alSourceQueueBuffers(source, 4, buffers);
        check();
//        printf("6 %08X\n",source);
        alSourcePlay(source);
        check();
//        printf("InitAL!!! %08X\n",source);
#endif
}

void givealbuffer(int16_t *buf)
{
#ifdef USE_OPENAL
        int processed;
        int state;
        
        //return;
        
//        printf("Start\n");
        check();
        
//        printf("GiveALBuffer %08X\n",source);
        
        alGetSourcei(source, AL_SOURCE_STATE, &state);

        check();
        
        if (state==0x1014)
        {
                alSourcePlay(source);
//                printf("Resetting sound\n");
        }
//        printf("State - %i %08X\n",state,state);
        alGetSourcei(source, AL_BUFFERS_PROCESSED, &processed);

//        printf("P ");
        check();
//        printf("Processed - %i\n",processed);

        if (processed>=1)
        {
                ALuint buffer;

                alSourceUnqueueBuffers(source, 1, &buffer);
//                printf("U ");
                check();

//                for (c=0;c<BUFLEN*2;c++) buf[c]^=0x8000;
                alBufferData(buffer, AL_FORMAT_STEREO16, buf, BUFLEN*2*2, FREQ);
//                printf("B ");
               check();

                alSourceQueueBuffers(source, 1, &buffer);
//                printf("Q ");
                check();
                
//                printf("\n");

//                if (!allog) allog=fopen("al.pcm","wb");
//                fwrite(buf,BUFLEN*2,1,allog);
        }
//        printf("\n");
#endif
}
