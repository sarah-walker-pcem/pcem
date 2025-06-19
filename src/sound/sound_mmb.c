#include <stdlib.h>
#include "ibm.h"
#include "device.h"
#include "io.h"
#include "sound.h"
#include "sound_mmb.h"
#include "cpu.h"

void mmb_update(mmb_t *mmb) {
        for (; mmb->pos < sound_pos_global; mmb->pos++) {
                ayumi_process(&mmb->first.chip);
                ayumi_process(&mmb->second.chip);

                ayumi_remove_dc(&mmb->first.chip);
                ayumi_remove_dc(&mmb->second.chip);

                mmb->buffer[mmb->pos << 1] = (mmb->first.chip.left + mmb->second.chip.left) * 16000;
                mmb->buffer[(mmb->pos << 1) + 1] = (mmb->first.chip.right + mmb->second.chip.right) * 16000;
        }
}

void mmb_get_buffer(int32_t *buffer, int len, void *p) {
        mmb_t *mmb = (mmb_t *)p;

        int c;

        mmb_update(mmb);

        for (c = 0; c < len * 2 ; c++)
                buffer[c] += mmb->buffer[c];

        mmb->pos = 0;
}

void mmb_write(uint16_t addr, uint8_t data, void *p) {
        mmb_t *mmb = (mmb_t *)p;
        int freq;

        mmb_update(mmb);

        switch (addr & 3) {
                case 0:
                        mmb->first.index = data;
                        break;
                case 2:
                        mmb->second.index = data;
                        break;
                case 1:
                case 3: {
                        ay_3_891x_t *ay = ((addr & 2) == 0) ? &mmb->first : &mmb->second;

                        switch (ay->index) {
                                case 0:
                                        ay->regs[0] = data;
                                        ayumi_set_tone(&ay->chip, 0, (ay->regs[1] << 8) | ay->regs[0]);
                                        break;
                                case 1:
                                        ay->regs[1] = data & 0xf;
                                        ayumi_set_tone(&ay->chip, 0, (ay->regs[1] << 8) | ay->regs[0]);
                                        break;
                                case 2:
                                        ay->regs[2] = data;
                                        ayumi_set_tone(&ay->chip, 1, (ay->regs[3] << 8) | ay->regs[2]);
                                        break;
                                case 3:
                                        ay->regs[3] = data & 0xf;
                                        ayumi_set_tone(&ay->chip, 1, (ay->regs[3] << 8) | ay->regs[2]);
                                        break;
                                case 4:
                                        ay->regs[4] = data;
                                        ayumi_set_tone(&ay->chip, 2, (ay->regs[5] << 8) | ay->regs[4]);
                                        break;
                                case 5:
                                        ay->regs[5] = data & 0xf;
                                        ayumi_set_tone(&ay->chip, 2, (ay->regs[5] << 8) | ay->regs[4]);
                                        break;
                                case 6:
                                        ay->regs[6] = data & 0x1f;
                                        ayumi_set_noise(&ay->chip, ay->regs[6]);
                                        break;
                                case 7:
                                        ay->regs[7] = data;
                                        ayumi_set_mixer(&ay->chip, 0, data & 1, (data >> 3) & 1, (ay->regs[8] >> 4) & 1);
                                        ayumi_set_mixer(&ay->chip, 1, (data >> 1) & 1, (data >> 4) & 1, (ay->regs[9] >> 4) & 1);
                                        ayumi_set_mixer(&ay->chip, 2, (data >> 2) & 1, (data >> 5) & 1, (ay->regs[10] >> 4) & 1);
                                        break;
                                case 8:
                                        ay->regs[8] = data;
                                        ayumi_set_volume(&ay->chip, 0, data & 0xf);
                                        ayumi_set_mixer(&ay->chip, 0, ay->regs[7] & 1, (ay->regs[7] >> 3) & 1, (data >> 4) & 1);
                                        break;
                                case 9:
                                        ay->regs[9] = data;
                                        ayumi_set_volume(&ay->chip, 1, data & 0xf);
                                        ayumi_set_mixer(&ay->chip, 1, (ay->regs[7] >> 1) & 1, (ay->regs[7] >> 4) & 1, (data >> 4) & 1);
                                        break;
                                case 10:
                                        ay->regs[10] = data;
                                        ayumi_set_volume(&ay->chip, 2, data & 0xf);
                                        ayumi_set_mixer(&ay->chip, 2, (ay->regs[7] >> 2) & 1, (ay->regs[7] >> 5) & 1, (data >> 4) & 1);
                                        break;
                                case 11:
                                        ay->regs[11] = data;
                                        ayumi_set_envelope(&ay->chip, (ay->regs[12] >> 8) | ay->regs[11]);
                                        break;
                                case 12:
                                        ay->regs[12] = data;
                                        ayumi_set_envelope(&ay->chip, (ay->regs[12] >> 8) | ay->regs[11]);
                                        break;
                                case 13:
                                        ay->regs[13] = data;
                                        ayumi_set_envelope_shape(&ay->chip, data & 0xf);
                                        break;
                                case 14:
                                        ay->regs[14] = data;
                                        break;
                                case 15:
                                        ay->regs[15] = data;
                                        break;
                        }
                        break;
                }
        }
}

uint8_t mmb_read(uint16_t addr, void *p) {
        mmb_t *mmb = (mmb_t *)p;
        ay_3_891x_t *ay = ((addr & 2) == 0) ? &mmb->first : &mmb->second;

        switch (ay->index) {
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                case 10:
                case 11:
                case 12:
                case 13:
                        return ay->regs[ay->index];
                case 14:
                        if (ay->regs[7] & 0x40)
                                return ay->regs[14];
                        else
                                return 0;
                case 15:
                        if (ay->regs[7] & 0x80)
                                return ay->regs[15];
                        else
                                return 0;
        }
}

void mmb_init(mmb_t *mmb, uint16_t base, uint16_t size, int freq) {
        sound_add_handler(mmb_get_buffer, mmb);

        ayumi_configure(&mmb->first.chip, 0, freq, 48000);
        ayumi_configure(&mmb->second.chip, 0, freq, 48000);

        for (int i = 0; i < 3; i++) {
                ayumi_set_pan(&mmb->first.chip, i, 0.5, 1);
                ayumi_set_pan(&mmb->second.chip, i, 0.5, 1);
        }

        io_sethandler(base, size, mmb_read, NULL, NULL, mmb_write, NULL, NULL, mmb);
}

void *mmb_device_init() {
        mmb_t *mmb = malloc(sizeof(mmb_t));
        uint16_t base_addr = (device_get_config_int("addr96") << 6) | (device_get_config_int("addr52") << 2);
        memset(mmb, 0, sizeof(mmb_t));

        /* NOTE:
         * The constant clock rate is a deviation from the real hardware which has
         * the design flaw that the clock rate is always half the ISA bus clock. */
        mmb_init(mmb, base_addr, 0x0004, 2386364);

        return mmb;
}

void mmb_device_close(void *p) {
        mmb_t *mmb = (mmb_t *)p;

        free(mmb);
}

static device_config_t mmb_config[] = {
        {.name = "addr96",
         .description = "Base address A9...A6",
         .type = CONFIG_SELECTION,
         .selection = {{.description = "0000", .value = 0},
                       {.description = "0001", .value = 1},
                       {.description = "0010", .value = 2},
                       {.description = "0011", .value = 3},
                       {.description = "0100", .value = 4},
                       {.description = "0101", .value = 5},
                       {.description = "0110", .value = 6},
                       {.description = "0111", .value = 7},
                       {.description = "1000", .value = 8},
                       {.description = "1001", .value = 9},
                       {.description = "1010", .value = 10},
                       {.description = "1011", .value = 11},
                       {.description = "1100", .value = 12},
                       {.description = "1101", .value = 13},
                       {.description = "1110", .value = 14},
                       {.description = "1111", .value = 15},
                       {.description = ""}},
         .default_int = 12},
        {.name = "addr52",
         .description = "Base address A5...A2",
         .type = CONFIG_SELECTION,
         .selection = {{.description = "0000", .value = 0},
                       {.description = "0001", .value = 1},
                       {.description = "0010", .value = 2},
                       {.description = "0011", .value = 3},
                       {.description = "0100", .value = 4},
                       {.description = "0101", .value = 5},
                       {.description = "0110", .value = 6},
                       {.description = "0111", .value = 7},
                       {.description = "1000", .value = 8},
                       {.description = "1001", .value = 9},
                       {.description = "1010", .value = 10},
                       {.description = "1011", .value = 11},
                       {.description = "1100", .value = 12},
                       {.description = "1101", .value = 13},
                       {.description = "1110", .value = 14},
                       {.description = "1111", .value = 15},
                       {.description = ""}},
         .default_int = 0},
        {.type = -1}};

device_t mmb_device = {"Mindscape Music Board", 0, mmb_device_init, mmb_device_close, NULL, NULL, NULL, NULL, mmb_config};
