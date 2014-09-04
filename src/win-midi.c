#include <windows.h>
#include <mmsystem.h>
#include "ibm.h"
#include "plat-midi.h"

static int midi_id;
static HMIDIOUT midi_out_device = NULL;

void midi_close();

void midi_init()
{
        int c;
        int n;
        MIDIOUTCAPS ocaps;
        MMRESULT hr;
        
        midi_id=0;
        
        hr = midiOutOpen(&midi_out_device, midi_id, 0,
		   0, CALLBACK_NULL);
        if (hr != MMSYSERR_NOERROR) {
                printf("midiOutOpen error - %08X\n",hr);
                return;
        }
        
        midiOutReset(midi_out_device);
}

void midi_close()
{
        if (midi_out_device != NULL)
        {
                midiOutReset(midi_out_device);
                midiOutClose(midi_out_device);
                midi_out_device = NULL;
        }
}

static int midi_pos, midi_len;
static uint32_t midi_command;
static int midi_lengths[8] = {3, 3, 3, 3, 2, 2, 3, 0};

void midi_write(uint8_t val)
{
        if (val & 0x80)
        {
                midi_pos = 0;
                midi_len = midi_lengths[(val >> 4) & 7];
                midi_command = 0;
        }

        if (midi_len)
        {                
                midi_command |= (val << (midi_pos * 8));
                
                midi_pos++;
                
                if (midi_pos == midi_len)
                        midiOutShortMsg(midi_out_device, midi_command);
        }
}
