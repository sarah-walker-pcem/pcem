#include <alsa/asoundlib.h>
#include "ibm.h"
#include "config.h"
#include "plat-midi.h"

#define MAX_MIDI_DEVICES 128

static struct
{
	int card;
	int device;
	int sub;
	char name[50];
} midi_devices[MAX_MIDI_DEVICES];

static int midi_device_count = 0;

static int midi_queried = 0;

static snd_rawmidi_t *midiout = NULL;

static void midi_query()
{
	int status;
	int card = -1;

	midi_queried = 1;

	if ((status = snd_card_next(&card)) < 0)
		return;

	if (card < 0)
		return; /*No cards*/

	while (card >= 0)
	{
		char *shortname;

		if ((status = snd_card_get_name(card, &shortname)) >= 0)
		{
			snd_ctl_t *ctl;
			char name[32];

			sprintf(name, "hw:%i", card);

			if ((status = snd_ctl_open(&ctl, name, 0)) >= 0)
			{
				int device = -1;

				do
				{
					status = snd_ctl_rawmidi_next_device(ctl, &device);
					if (status >= 0 && device != -1)
					{
						snd_rawmidi_info_t *info;
						int sub_nr, sub;

						snd_rawmidi_info_alloca(&info);
						snd_rawmidi_info_set_device(info, device);
						snd_rawmidi_info_set_stream(info, SND_RAWMIDI_STREAM_OUTPUT);
						snd_ctl_rawmidi_info(ctl, info);
						sub_nr = snd_rawmidi_info_get_subdevices_count(info);
						pclog("sub_nr=%i\n",sub_nr);

						for (sub = 0; sub < sub_nr; sub++)
						{
							snd_rawmidi_info_set_subdevice(info, sub);

							if (snd_ctl_rawmidi_info(ctl, info) == 0)
							{
								pclog("%s: MIDI device=%i:%i:%i\n", shortname, card, device,sub);

								midi_devices[midi_device_count].card = card;
								midi_devices[midi_device_count].device = device;
								midi_devices[midi_device_count].sub = sub;
								snprintf(midi_devices[midi_device_count].name, 50, "%s (%i:%i:%i)", shortname, card, device, sub);
								midi_device_count++;
								if (midi_device_count >= MAX_MIDI_DEVICES)
									return;
							}
						}
					}
				} while (device >= 0);
			}
		}

		if (snd_card_next(&card) < 0)
			break;
	}
}

void midi_init()
{
	char portname[32];
	int midi_id;

	if (!midi_queried)
		midi_query();

	midi_id = config_get_int(CFG_MACHINE, NULL, "midi", 0);

	sprintf(portname, "hw:%i,%i,%i", midi_devices[midi_id].card,
					 midi_devices[midi_id].device,
					 midi_devices[midi_id].sub);
	pclog("Opening MIDI port %s\n", portname);

	if (snd_rawmidi_open(NULL, &midiout, portname, SND_RAWMIDI_SYNC) < 0)
	{
		pclog("Failed to open MIDI\n");
		return;
	}
}

void midi_close()
{
	if (midiout != NULL)
	{
		snd_rawmidi_close(midiout);
		midiout = NULL;
	}
}

static int midi_pos, midi_len;
static uint8_t midi_command[4];
static int midi_lengths[8] = {3, 3, 3, 3, 2, 2, 3, 1};
static int midi_insysex;
static uint8_t midi_sysex_data[1024+2];

void midi_write(uint8_t val)
{
	//pclog("Write MIDI %02x\n", val);

        if ((val & 0x80) && !(val == 0xf7 && midi_insysex))
        {
                midi_pos = 0;
                midi_len = midi_lengths[(val >> 4) & 7];
                midi_command[0] = midi_command[1] = midi_command[2] = midi_command[3] = 0;
                if (val == 0xf0)
                        midi_insysex = 1;
        }

        if (midi_insysex)
        {
                midi_sysex_data[midi_pos++] = val;
                
                if (val == 0xf7 || midi_pos >= 1024+2)
		{
/*			pclog("MIDI send sysex %i: ", midi_pos);
			for (int i = 0; i < midi_pos; i++)
				pclog("%02x ", midi_sysex_data[i]);
			pclog("\n");*/
			snd_rawmidi_write(midiout, midi_sysex_data, midi_pos);
//			pclog("Sent sysex\n");
			midi_insysex = 0;
		}
                return;
        }
                        
        if (midi_len)
        {                
                midi_command[midi_pos] = val;
                
                midi_pos++;
                
                if (midi_pos == midi_len)
		{
//			pclog("MIDI send %i: %02x %02x %02x %02x\n", midi_len, midi_command[0], midi_command[1], midi_command[2], midi_command[3]);
                        snd_rawmidi_write(midiout, midi_command, midi_len);
		}
        }
}

int midi_get_num_devs()
{
	if (!midi_queried)
		midi_query();

	return midi_device_count;
}

void midi_get_dev_name(int num, char *s)
{
	strcpy(s, midi_devices[num].name);
}
