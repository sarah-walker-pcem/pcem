#include <CoreFoundation/CoreFoundation.h>
#include <CoreMIDI/CoreMIDI.h>
#include "ibm.h"
#include "config.h"
#include "plat-midi.h"

#define MIDI_NAME_LEN 100
#define MIDI_SYSEX_MAX 65536

static MIDIClientRef midi_client = 0;
static MIDIPortRef midi_out_port = 0;
static MIDIEndpointRef midi_dest = 0;

static int midi_pos, midi_len;
static uint8_t midi_command[4];
static int midi_lengths[8] = {3, 3, 3, 3, 2, 2, 3, 1};
static int midi_insysex;
static uint8_t midi_sysex_data[MIDI_SYSEX_MAX];

static Byte midi_packet_buffer[MIDI_SYSEX_MAX + 1024];

static int midi_create_client() {
        OSStatus err;

        if (midi_client)
                return 1;

        err = MIDIClientCreate(CFSTR("PCem"), NULL, NULL, &midi_client);
        if (err != noErr) {
                pclog("CoreMIDI: MIDIClientCreate failed (%d)\n", (int)err);
                midi_client = 0;
                return 0;
        }
        return 1;
}

static void midi_send(const uint8_t *data, int len) {
        MIDIPacketList *list = (MIDIPacketList *)midi_packet_buffer;
        MIDIPacket *packet;

        if (!midi_out_port || !midi_dest || len <= 0)
                return;

        packet = MIDIPacketListInit(list);
        packet = MIDIPacketListAdd(list, sizeof(midi_packet_buffer), packet, 0, len, data);
        if (packet == NULL) {
                pclog("CoreMIDI: packet too large (%d bytes)\n", len);
                return;
        }

        MIDISend(midi_out_port, midi_dest, list);
}

void midi_init() {
        OSStatus err;
        ItemCount num_dests;
        int midi_id;

        midi_pos = midi_len = midi_insysex = 0;

        if (!midi_create_client())
                return;

        num_dests = MIDIGetNumberOfDestinations();
        if (num_dests == 0) {
                pclog("CoreMIDI: no MIDI destinations available\n");
                return;
        }

        midi_id = config_get_int(CFG_MACHINE, NULL, "midi", 0);
        if (midi_id < 0 || (ItemCount)midi_id >= num_dests) {
                pclog("CoreMIDI: MIDI device %d not present, falling back to 0\n", midi_id);
                midi_id = 0;
        }

        midi_dest = MIDIGetDestination(midi_id);
        if (!midi_dest) {
                pclog("CoreMIDI: failed to get destination %d\n", midi_id);
                return;
        }

        if (!midi_out_port) {
                err = MIDIOutputPortCreate(midi_client, CFSTR("PCem MIDI Out"), &midi_out_port);
                if (err != noErr) {
                        pclog("CoreMIDI: MIDIOutputPortCreate failed (%d)\n", (int)err);
                        midi_out_port = 0;
                        midi_dest = 0;
                        return;
                }
        }

        pclog("CoreMIDI: opened MIDI destination %d\n", midi_id);
}

void midi_close() {
        if (midi_out_port && midi_dest) {
                uint8_t msg[3];
                int ch;

                for (ch = 0; ch < 16; ch++) {
                        msg[0] = 0xb0 | ch;
                        msg[1] = 0x7b;
                        msg[2] = 0;
                        midi_send(msg, 3);
                        msg[1] = 0x79;
                        midi_send(msg, 3);
                }
                MIDIFlushOutput(midi_dest);
        }

        if (midi_out_port) {
                MIDIPortDispose(midi_out_port);
                midi_out_port = 0;
        }
        midi_dest = 0;
}

void midi_write(uint8_t val) {
        if (!midi_out_port || !midi_dest)
                return;

        if ((val & 0x80) && !(val == 0xf7 && midi_insysex)) {
                midi_pos = 0;
                midi_len = midi_lengths[(val >> 4) & 7];
                midi_command[0] = midi_command[1] = midi_command[2] = midi_command[3] = 0;
                if (val == 0xf0)
                        midi_insysex = 1;
        }

        if (midi_insysex) {
                midi_sysex_data[midi_pos++] = val;

                if (val == 0xf7 || midi_pos >= MIDI_SYSEX_MAX) {
                        midi_send(midi_sysex_data, midi_pos);
                        midi_insysex = 0;
                }
                return;
        }

        if (midi_len) {
                if (midi_pos < 3) {
                        midi_command[midi_pos] = val;

                        midi_pos++;

                        if (midi_pos == midi_len)
                                midi_send(midi_command, midi_len);
                }
        }
}

int midi_get_num_devs() {
        midi_create_client();
        return (int)MIDIGetNumberOfDestinations();
}

void midi_get_dev_name(int num, char *s) {
        MIDIEndpointRef dest;
        CFStringRef name = NULL;

        s[0] = 0;

        if (num < 0 || (ItemCount)num >= MIDIGetNumberOfDestinations())
                return;

        dest = MIDIGetDestination(num);
        if (!dest)
                return;

        if (MIDIObjectGetStringProperty(dest, kMIDIPropertyDisplayName, &name) != noErr || name == NULL) {
                if (MIDIObjectGetStringProperty(dest, kMIDIPropertyName, &name) != noErr)
                        name = NULL;
        }

        if (name) {
                if (!CFStringGetCString(name, s, MIDI_NAME_LEN, kCFStringEncodingUTF8))
                        snprintf(s, MIDI_NAME_LEN, "MIDI Device %d", num);
                CFRelease(name);
        } else
                snprintf(s, MIDI_NAME_LEN, "MIDI Device %d", num);
}
