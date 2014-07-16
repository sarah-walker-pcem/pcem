#include "ibm.h"
#include "io.h"
#include "mem.h"
#include "pic.h"
#include "sound.h"
#include "sound_speaker.h"
#include "timer.h"

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
} keyboard_xt;

static uint8_t key_queue[16];
static int key_queue_start = 0, key_queue_end = 0;

void keyboard_xt_poll()
{
        keybsenddelay += (1000 * TIMER_USEC);
        if (keyboard_xt.wantirq)
        {
                keyboard_xt.wantirq = 0;
                keyboard_xt.pa = keyboard_xt.key_waiting;
                picint(2);
                pclog("keyboard_xt : take IRQ\n");
        }
        if (key_queue_start != key_queue_end && !keyboard_xt.pa)
        {
                keyboard_xt.key_waiting = key_queue[key_queue_start];
                pclog("Reading %02X from the key queue at %i\n", keyboard_xt.key_waiting, key_queue_start);
                key_queue_start = (key_queue_start + 1) & 0xf;
                keyboard_xt.wantirq = 1;        
        }                
}

void keyboard_xt_adddata(uint8_t val)
{
        key_queue[key_queue_end] = val;
        pclog("keyboard_xt : %02X added to key queue at %i\n", val, key_queue_end);
        key_queue_end = (key_queue_end + 1) & 0xf;
        return;
}

void keyboard_xt_write(uint16_t port, uint8_t val, void *priv)
{
        pclog("keyboard_xt : write %04X %02X %02X\n", port, val, keyboard_xt.pb);
/*        if (ram[8] == 0xc3) 
        {
                output = 3;
        }*/
        switch (port)
        {
                case 0x61:
                pclog("keyboard_xt : pb write %02X %02X  %i %02X %i\n", val, keyboard_xt.pb, !(keyboard_xt.pb & 0x40), keyboard_xt.pb & 0x40, (val & 0x40));
                if (!(keyboard_xt.pb & 0x40) && (val & 0x40)) /*Reset keyboard*/
                {
                        pclog("keyboard_xt : reset keyboard\n");
                        keyboard_xt_adddata(0xaa);
                }
                keyboard_xt.pb = val;
                ppi.pb = val;

                speaker_gated = val & 1;
                speaker_enable = val & 2;
                if (speaker_enable) 
                        was_speaker_enable = 1;
                pit_set_gate(2, val & 1);
                   
                if (val & 0x80)
                {
                        keyboard_xt.pa = 0;
                        picintc(2);
                }
                break;
        }
}

uint8_t keyboard_xt_read(uint16_t port, void *priv)
{
        uint8_t temp;
        pclog("keyboard_xt : read %04X ", port);
        switch (port)
        {
                case 0x60:
                if (keyboard_xt.pb & 0x80)
                {
                        if (VGA) 
                           temp = 0x4D;
                        else if (MDA) 
                           temp = 0x7D;
                        else            
                           temp = 0x6D;
                }
                else
                {
                        temp = keyboard_xt.pa;
                        if (key_queue_start == key_queue_end)
                        {
                                keyboard_xt.wantirq = 0;
                        }
                        else
                        {
                                keyboard_xt.key_waiting = key_queue[key_queue_start];
                                key_queue_start = (key_queue_start + 1) & 0xf;
                                keyboard_xt.wantirq = 1;        
                        }
                }        
                break;
                
                case 0x61:
                temp = keyboard_xt.pb;
                break;
                
                case 0x62:
                if (keyboard_xt.pb & 0x08)
                {
                        if (VGA) 
                           temp = 4;
                        else if (MDA)
                           temp = 7;
                        else
                           temp = 6;
                }
                else
                   temp = 0xD;
                temp |= (ppispeakon ? 0x20 : 0);
                break;
                
                default:
                pclog("\nBad XT keyboard read %04X\n", port);
                //dumpregs();
                //exit(-1);
        }
        pclog("%02X\n", temp);
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
        keyboard_xt_reset();
        keyboard_send = keyboard_xt_adddata;
        keyboard_poll = keyboard_xt_poll;

        timer_add(keyboard_xt_poll, &keybsenddelay, TIMER_ALWAYS_ENABLED,  NULL);
}
