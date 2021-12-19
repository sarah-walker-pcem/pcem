#ifndef SOUND_SB_H
#define SOUND_SB_H

#include "sound_emu8k.h"
#include "sound_mpu401_uart.h"
#include "sound_sb_dsp.h"

extern device_t sb_1_device;
extern device_t sb_15_device;
extern device_t sb_mcv_device;
extern device_t sb_2_device;
extern device_t sb_pro_v1_device;
extern device_t sb_pro_v2_device;
extern device_t sb_pro_mcv_device;
extern device_t sb_16_device;
extern device_t sb_awe32_device;

/* SB 2.0 CD version */
typedef struct sb_ct1335_mixer_t
{
        int32_t master;
        int32_t voice;
        int32_t fm;
        int32_t cd;

        uint8_t index;
        uint8_t regs[256];
} sb_ct1335_mixer_t;
/* SB PRO */
typedef struct sb_ct1345_mixer_t
{
        int32_t master_l, master_r;
        int32_t voice_l,  voice_r;
        int32_t fm_l,     fm_r;
        int32_t cd_l,     cd_r;
        int32_t line_l,   line_r;
        int32_t mic;
        /*see sb_ct1745_mixer for values for input selector*/
        int32_t input_selector;
        
        int input_filter;
        int in_filter_freq;
        int output_filter;
        
        int stereo;
        int stereo_isleft;
        
        uint8_t index;
        uint8_t regs[256];
    
} sb_ct1345_mixer_t;
/* SB16 and AWE32 */
typedef struct sb_ct1745_mixer_t
{
        int32_t master_l, master_r;
        int32_t voice_l,  voice_r;
        int32_t fm_l,     fm_r;
        int32_t cd_l,     cd_r;
        int32_t line_l,   line_r;
        int32_t mic;
        int32_t speaker;

        int bass_l,   bass_r;
        int treble_l, treble_r;
        
        int output_selector;
        #define OUTPUT_MIC 1
        #define OUTPUT_CD_R 2
        #define OUTPUT_CD_L 4
        #define OUTPUT_LINE_R 8
        #define OUTPUT_LINE_L 16

        int input_selector_left;
        int input_selector_right;
        #define INPUT_MIC 1
        #define INPUT_CD_R 2
        #define INPUT_CD_L 4
        #define INPUT_LINE_R 8
        #define INPUT_LINE_L 16
        #define INPUT_MIDI_R 32
        #define INPUT_MIDI_L 64

        int mic_agc;
        
        int32_t input_gain_L;
        int32_t input_gain_R;
        int32_t output_gain_L;
        int32_t output_gain_R;
        
        uint8_t index;
        uint8_t regs[256];
} sb_ct1745_mixer_t;

typedef struct sb_t
{
        opl_t           opl;
        sb_dsp_t        dsp;
        union {
                sb_ct1335_mixer_t mixer_sb2;
                sb_ct1345_mixer_t mixer_sbpro;
                sb_ct1745_mixer_t mixer_sb16;
        };
        mpu401_uart_t   mpu;
        emu8k_t         emu8k;

        int pos;
        
        uint8_t pos_regs[8];
        
        int opl_emu;
} sb_t;

// exported for clone cards
void sb_get_buffer_sbpro(int32_t *buffer, int len, void *p);
void sb_ct1345_mixer_write(uint16_t addr, uint8_t val, void *p);
uint8_t sb_ct1345_mixer_read(uint16_t addr, void *p);
void sb_ct1345_mixer_reset(sb_t* sb);

void sb_close(void *p);
void sb_speed_changed(void *p);
void sb_add_status_info(char *s, int max_len, void *p);

#endif /* SOUND_SB_H */
