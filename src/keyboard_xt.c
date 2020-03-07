#include "ibm.h"
#include "device.h"
#include "cassette.h"
#include "fdd.h"
#include "io.h"
#include "mem.h"
#include "pic.h"
#include "pit.h"
#include "sound.h"
#include "sound_speaker.h"
#include "tandy_eeprom.h"
#include "timer.h"
#include "t1000.h"
#include "video.h"

#include "keyboard.h"
#include "keyboard_xt.h"

#define STAT_PARITY     0x80
#define STAT_RTIMEOUT   0x40
#define STAT_TTIMEOUT   0x20
#define STAT_LOCK       0x10
#define STAT_CD         0x08
#define STAT_SYSFLAG    0x04
#define STAT_IFULL      0x02
#define STAT_OFULL      0x01

struct
{
        int wantirq;
        uint8_t key_waiting;
        
        uint8_t pa;        
        uint8_t pb;
        
        int shift_full;
        
        int tandy;
        int pb2_turbo;

	pc_timer_t send_delay_timer;
} keyboard_xt;

static uint8_t key_queue[16];
static int key_queue_start = 0, key_queue_end = 0;

void keyboard_xt_poll()
{
        timer_advance_u64(&keyboard_xt.send_delay_timer, (1000 * TIMER_USEC));
        if (!(keyboard_xt.pb & 0x40) && romset != ROM_TANDY)
                return;
        if (keyboard_xt.wantirq)
        {
                keyboard_xt.wantirq = 0;
                keyboard_xt.pa = keyboard_xt.key_waiting;
                keyboard_xt.shift_full = 1;
                picint(2);
                pclog("keyboard_xt : take IRQ\n");
        }
        if (key_queue_start != key_queue_end && !keyboard_xt.shift_full)
        {
                keyboard_xt.key_waiting = key_queue[key_queue_start];
                pclog("Reading %02X from the key queue at %i\n", keyboard_xt.key_waiting, key_queue_start);
                key_queue_start = (key_queue_start + 1) & 0xf;
                keyboard_xt.wantirq = 1;        
        }                
}

void keyboard_xt_adddata(uint8_t val)
{
	/* Test for T1000 'Fn' key (Right Alt / Right Ctrl) */
	if (romset == ROM_T1000 || romset == ROM_T1200)
	{
	 	if (pcem_key[0xb8] || pcem_key[0x9D])	/* 'Fn' pressed */
		{
			t1000_syskey(0x00, 0x04, 0x00);	/* Set 'Fn' indicator */
			switch (val)
			{
				case 0x45: /* Num Lock => toggle numpad */
					   t1000_syskey(0x00, 0x00, 0x10); break;
				case 0x47: /* Home => internal display */
					   t1000_syskey(0x40, 0x00, 0x00); break;
				case 0x49: /* PgDn => turbo on */
					   t1000_syskey(0x80, 0x00, 0x00); break;
				case 0x4D: /* Right => toggle LCD font */
					   t1000_syskey(0x00, 0x00, 0x20); break;
				case 0x4F: /* End => external display */
					   t1000_syskey(0x00, 0x40, 0x00); break;
				case 0x51: /* PgDn => turbo off */
					   t1000_syskey(0x00, 0x80, 0x00); break;
				case 0x54: /* SysRQ => toggle window */
					   t1000_syskey(0x00, 0x00, 0x08); break;
			}
		}
		else
		{
			t1000_syskey(0x04, 0x00, 0x00);	/* Reset 'Fn' indicator */
		}
	}

        key_queue[key_queue_end] = val;
        pclog("keyboard_xt : %02X added to key queue at %i\n", val, key_queue_end);
        key_queue_end = (key_queue_end + 1) & 0xf;
        return;
}

void keyboard_xt_write(uint16_t port, uint8_t val, void *priv)
{
//        pclog("keyboard_xt : write %04X %02X %02X\n", port, val, keyboard_xt.pb);
/*        if (ram[8] == 0xc3) 
        {
                output = 3;
        }*/
        switch (port)
        {
                case 0x61:
//                pclog("keyboard_xt : pb write %02X %02X  %i %02X %i\n", val, keyboard_xt.pb, !(keyboard_xt.pb & 0x40), keyboard_xt.pb & 0x40, (val & 0x40));
                if (!(keyboard_xt.pb & 0x40) && (val & 0x40)) /*Reset keyboard*/
                {
                        pclog("keyboard_xt : reset keyboard\n");
                        key_queue_start = key_queue_end = 0;
                        keyboard_xt.wantirq = 0;
                        keyboard_xt.shift_full = 0;
                        keyboard_xt_adddata(0xaa);
                }
                keyboard_xt.pb = val;
                ppi.pb = val;

                timer_process();

                if (romset == ROM_IBMPC)
                	cassette_set_motor((val & 8) ? 0 : 1);
                else if (keyboard_xt.pb2_turbo)
                        cpu_set_turbo((val & 4) ? 0 : 1);
        
                speaker_update();
                speaker_gated = val & 1;
                speaker_enable = val & 2;
                if (speaker_enable) 
                        was_speaker_enable = 1;
                pit_set_gate(&pit, 2, val & 1);
                   
                if (val & 0x80)
                {
                        keyboard_xt.pa = 0;
                        keyboard_xt.shift_full = 0;
                        picintc(2);
                }
                break;
        }
}

uint8_t keyboard_xt_read(uint16_t port, void *priv)
{
        uint8_t temp = 0xff;
//        pclog("keyboard_xt : read %04X ", port);
        switch (port)
        {
                case 0x60:
                if ((romset == ROM_IBMPC || romset == ROM_LEDGE_MODELM) && (keyboard_xt.pb & 0x80))
                {
                        if (video_is_ega_vga())
                                temp = 0x4D;
                        else if (video_is_mda())
                                temp = 0x7D;
                        else            
                                temp = 0x6D;
                        if (hasfpu)
                                temp |= 0x02;
                }
                else if ((romset == ROM_ATARIPC3) && (keyboard_xt.pb & 0x80))
                {
                        temp = 0x7f;
                }
                else
                {
                        temp = keyboard_xt.pa;
                }        
                break;
                
                case 0x61:
                temp = keyboard_xt.pb;
                break;
                
                case 0x62:
                if (romset == ROM_IBMPC)
                {
                        if (keyboard_xt.pb & 0x04)
                                temp = ((mem_size-64) / 32) & 0xf;
                        else
                                temp = ((mem_size-64) / 32) >> 4;

        		temp |= (cassette_input()) ? 0x10 : 0;
                }
                else if (romset == ROM_LEDGE_MODELM)
                {
                        /*High bit of memory size is read from port 0xa0*/
                        temp = ((mem_size-64) / 32) & 0xf;
                }
                else if (romset == ROM_ATARIPC3)
                {
                        if (keyboard_xt.pb & 0x04)
                                temp = 0xf;
                        else
                                temp = 4;
                }
                else
                {
                        if (keyboard_xt.pb & 0x08)
                        {
                                if (video_is_ega_vga())
                                        temp = 4;
                                else if (video_is_mda())
                                        temp = 7;
                                else
                                        temp = 6;
                        }
                        else
                                temp = hasfpu ? 0xf : 0xd;
                }
                temp |= (ppispeakon ? 0x20 : 0);
                if (keyboard_xt.tandy)
                        temp |= (tandy_eeprom_read() ? 0x10 : 0);
                break;
                
                default:
                pclog("\nBad XT keyboard read %04X\n", port);
                //dumpregs();
                //exit(-1);
        }
//        pclog("%02X\n", temp);
        return temp;
}

static uint8_t ledge_modelm_read(uint16_t port, void *p)
{
        uint8_t temp = 0;
        
        if (((mem_size-64) / 32) >> 4)
                temp |= 0x40;

        return temp;
}

void keyboard_xt_reset()
{
        keyboard_xt.wantirq = 0;
        
        keyboard_scan = 1;
}

void keyboard_xt_init()
{
        //return;
        io_sethandler(0x0060, 0x0004, keyboard_xt_read, NULL, NULL, keyboard_xt_write, NULL, NULL,  NULL);
        if (romset == ROM_LEDGE_MODELM)
                io_sethandler(0x00a0, 0x0001, ledge_modelm_read, NULL, NULL, NULL, NULL, NULL, NULL);
        keyboard_xt_reset();
        keyboard_send = keyboard_xt_adddata;
        keyboard_poll = keyboard_xt_poll;
        keyboard_xt.tandy = 0;
        keyboard_xt.pb2_turbo = (romset == ROM_GENXT || romset == ROM_DTKXT || romset == ROM_AMIXT || romset == ROM_PXXT) ? 1 : 0;

        timer_add(&keyboard_xt.send_delay_timer, keyboard_xt_poll, NULL, 1);
}

void keyboard_tandy_init()
{
        //return;
        io_sethandler(0x0060, 0x0004, keyboard_xt_read, NULL, NULL, keyboard_xt_write, NULL, NULL,  NULL);
        keyboard_xt_reset();
        keyboard_send = keyboard_xt_adddata;
        keyboard_poll = keyboard_xt_poll;
        keyboard_xt.tandy = (romset != ROM_TANDY) ? 1 : 0;
        
        timer_add(&keyboard_xt.send_delay_timer, keyboard_xt_poll, NULL, 1);
}
