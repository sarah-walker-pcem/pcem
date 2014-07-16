#include "ibm.h"
#include "sound.h"
#include "sound_speaker.h"

int speaker_mute = 0;

static int16_t speaker_buffer[SOUNDBUFLEN];

static int speaker_pos = 0;

int speaker_gated = 0;
int speaker_enable = 0, was_speaker_enable = 0;

static void speaker_poll(void *p)
{
        if (speaker_pos >= SOUNDBUFLEN) return;

//        printf("SPeaker - %i %i %i %02X\n",speakval,gated,speakon,pit.m[2]);
        if (speaker_gated && was_speaker_enable)
        {
                if (!pit.m[2] || pit.m[2]==4)
                        speaker_buffer[speaker_pos] = speakval;
                else 
                        speaker_buffer[speaker_pos] = speakon ? 0x1400 : 0;
        }
        else
                speaker_buffer[speaker_pos] = was_speaker_enable ? 0x1400 : 0;
        speaker_pos++;
        if (!speaker_enable)
                was_speaker_enable = 0;
}

static void speaker_get_buffer(int16_t *buffer, int len, void *p)
{
        int c;

        if (!speaker_mute)
        {
                for (c = 0; c < len * 2; c++)
                        buffer[c] += speaker_buffer[c >> 1];
        }

        speaker_pos = 0;
}

void speaker_init()
{
        sound_add_handler(speaker_poll, speaker_get_buffer, NULL);
        speaker_mute = 0;
}
