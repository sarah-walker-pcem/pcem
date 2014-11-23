#include "ibm.h"
#include "io.h"
#include "mem.h"
#include "pic.h"
#include "pit.h"
#include "sound.h"
#include "sound_speaker.h"
#include "timer.h"

#include "keyboard.h"
#include "keyboard_at.h"

#define STAT_PARITY     0x80
#define STAT_RTIMEOUT   0x40
#define STAT_TTIMEOUT   0x20
#define STAT_MFULL      0x20
#define STAT_LOCK       0x10
#define STAT_CD         0x08
#define STAT_SYSFLAG    0x04
#define STAT_IFULL      0x02
#define STAT_OFULL      0x01

struct
{
        int initialised;
        int want60;
        int wantirq, wantirq12;
        uint8_t command;
        uint8_t status;
        uint8_t mem[0x20];
        uint8_t out;
        int out_new;
        
        uint8_t input_port;
        uint8_t output_port;
        
        uint8_t key_command;
        int key_wantdata;
        
        int last_irq;
} keyboard_at;

static uint8_t key_ctrl_queue[16];
static int key_ctrl_queue_start = 0, key_ctrl_queue_end = 0;

static uint8_t key_queue[16];
static int key_queue_start = 0, key_queue_end = 0;

static uint8_t mouse_queue[16];
int mouse_queue_start = 0, mouse_queue_end = 0;

void keyboard_at_poll()
{
	keybsenddelay += (1000 * TIMER_USEC);
        if (keyboard_at.out_new != -1)
        {
                keyboard_at.wantirq = 0;
                if (keyboard_at.mem[0] & 0x01)
                        picint(2);
                keyboard_at.out = keyboard_at.out_new;
                keyboard_at.out_new = -1;
                keyboard_at.status |=  STAT_OFULL;
                keyboard_at.status &= ~STAT_IFULL;
//                pclog("keyboard_at : take IRQ\n");
                keyboard_at.last_irq = 2;
        }
        else if (keyboard_at.wantirq12)
        {
                keyboard_at.wantirq12 = 0;
                picint(0x1000);
//                pclog("keyboard_at : take IRQ 12\n");
                keyboard_at.last_irq = 0x1000;
        }
        if (!(keyboard_at.status & STAT_OFULL) && !(keyboard_at.mem[0] & 0x10) &&
            mouse_queue_start != mouse_queue_end)
        {
//                pclog("Reading %02X from the mouse queue at %i\n", keyboard_at.out, key_queue_start);
                keyboard_at.out = mouse_queue[mouse_queue_start];
                mouse_queue_start = (mouse_queue_start + 1) & 0xf;
                keyboard_at.status |=  STAT_OFULL | STAT_MFULL;
                keyboard_at.status &= ~STAT_IFULL;
                if (keyboard_at.mem[0] & 0x02)
                   keyboard_at.wantirq12 = 1;        
        }                
        else if (!(keyboard_at.status & STAT_OFULL) && keyboard_at.out_new == -1 &&
                 !(keyboard_at.mem[0] & 0x10) && key_queue_start != key_queue_end)
        {
                keyboard_at.out_new = key_queue[key_queue_start];
//                pclog("Reading %02X from the key queue at %i\n", keyboard_at.out, key_queue_start);
                key_queue_start = (key_queue_start + 1) & 0xf;
        }                
        else if (keyboard_at.out_new == -1 && !(keyboard_at.status & STAT_OFULL) && 
            key_ctrl_queue_start != key_ctrl_queue_end)
        {
                keyboard_at.out_new = key_ctrl_queue[key_ctrl_queue_start];
//                pclog("Reading %02X from the key ctrl_queue at %i\n", keyboard_at.out, key_ctrl_queue_start);
                key_ctrl_queue_start = (key_ctrl_queue_start + 1) & 0xf;
        }                
}

void keyboard_at_adddata(uint8_t val)
{
//        if (keyboard_at.status & STAT_OFULL)
//        {
                key_ctrl_queue[key_ctrl_queue_end] = val;
                key_ctrl_queue_end = (key_ctrl_queue_end + 1) & 0xf;
//                pclog("keyboard_at : %02X added to queue\n", val);                
/*                return;
        }
        keyboard_at.out = val;
        keyboard_at.status |=  STAT_OFULL;
        keyboard_at.status &= ~STAT_IFULL;
        if (keyboard_at.mem[0] & 0x01)
           keyboard_at.wantirq = 1;        
        pclog("keyboard_at : output %02X (IRQ %i)\n", val, keyboard_at.wantirq);*/
}

void keyboard_at_adddata_keyboard(uint8_t val)
{
/*        if (val == 0x1c)
        {
                key_1c++;
                if (key_1c == 4)
                        output = 3;
        }*/
        key_queue[key_queue_end] = val;
        key_queue_end = (key_queue_end + 1) & 0xf;
//        pclog("keyboard_at : %02X added to key queue\n", val);
        return;
}

void keyboard_at_adddata_mouse(uint8_t val)
{
        mouse_queue[mouse_queue_end] = val;
        mouse_queue_end = (mouse_queue_end + 1) & 0xf;
//        pclog("keyboard_at : %02X added to mouse queue\n", val);
        return;
}

void keyboard_at_write(uint16_t port, uint8_t val, void *priv)
{
//        pclog("keyboard_at : write %04X %02X %i  %02X\n", port, val, keyboard_at.key_wantdata, ram[8]);
/*        if (ram[8] == 0xc3) 
        {
                output = 3;
        }*/
        switch (port)
        {
                case 0x60:
                if (keyboard_at.want60)
                {
                        /*Write to controller*/
                        keyboard_at.want60 = 0;
                        switch (keyboard_at.command)
                        {
                                case 0x60: case 0x61: case 0x62: case 0x63:
                                case 0x64: case 0x65: case 0x66: case 0x67:
                                case 0x68: case 0x69: case 0x6a: case 0x6b:
                                case 0x6c: case 0x6d: case 0x6e: case 0x6f:
                                case 0x70: case 0x71: case 0x72: case 0x73:
                                case 0x74: case 0x75: case 0x76: case 0x77:
                                case 0x78: case 0x79: case 0x7a: case 0x7b:
                                case 0x7c: case 0x7d: case 0x7e: case 0x7f:
                                keyboard_at.mem[keyboard_at.command & 0x1f] = val;
                                if (keyboard_at.command == 0x60)
                                {
                                        if ((val & 1) && (keyboard_at.status & STAT_OFULL))
                                           keyboard_at.wantirq = 1;
                                        if (!(val & 1) && keyboard_at.wantirq)
                                           keyboard_at.wantirq = 0;
                                }                                           
                                break;

                                case 0xcb: /*AMI - set keyboard mode*/
                                break;
                                
                                case 0xcf: /*??? - sent by MegaPC BIOS*/
                                break;
                                
                                case 0xd1: /*Write output port*/
//                                pclog("Write output port - %02X %02X %04X:%04X\n", keyboard_at.output_port, val, CS, pc);
                                if ((keyboard_at.output_port ^ val) & 0x02) /*A20 enable change*/
                                {
                                        mem_a20_key = val & 0x02;
                                        mem_a20_recalc();
//                                        pclog("Rammask change to %08X %02X\n", rammask, val & 0x02);
                                        flushmmucache();
                                }
                                keyboard_at.output_port = val;
                                break;
                                
                                case 0xd3: /*Write to mouse output buffer*/
                                keyboard_at_adddata_mouse(val);
                                break;
                                
                                case 0xd4: /*Write to mouse*/
                                if (mouse_write)
                                   mouse_write(val);
                                break;     
                                
                                default:
                                pclog("Bad AT keyboard controller 0060 write %02X command %02X\n", val, keyboard_at.command);
//                                dumpregs();
//                                exit(-1);
                        }
                }
                else
                {
                        /*Write to keyboard*/                        
                        keyboard_at.mem[0] &= ~0x10;
                        if (keyboard_at.key_wantdata)
                        {
                                keyboard_at.key_wantdata = 0;
                                switch (keyboard_at.key_command)
                                {
                                        case 0xed: /*Set/reset LEDs*/
                                        keyboard_at_adddata_keyboard(0xfa);
                                        break;

                                        case 0xf3: /*Set typematic rate/delay*/
                                        keyboard_at_adddata_keyboard(0xfa);
                                        break;
                                        
                                        default:
                                        pclog("Bad AT keyboard 0060 write %02X command %02X\n", val, keyboard_at.key_command);
//                                        dumpregs();
//                                        exit(-1);
                                }
                        }
                        else
                        {
                                keyboard_at.key_command = val;
                                switch (val)
                                {
                                        case 0x05: /*??? - sent by NT 4.0*/
                                        keyboard_at_adddata_keyboard(0xfe);
                                        break;

                                        case 0xed: /*Set/reset LEDs*/
                                        keyboard_at.key_wantdata = 1;
                                        keyboard_at_adddata_keyboard(0xfa);
                                        break;
                                        
                                        case 0xf2: /*Read ID*/
                                        keyboard_at_adddata_keyboard(0xfa);
                                        keyboard_at_adddata_keyboard(0xab);
                                        keyboard_at_adddata_keyboard(0x41);
                                        break;
                                        
                                        case 0xf3: /*Set typematic rate/delay*/
                                        keyboard_at.key_wantdata = 1;
                                        keyboard_at_adddata_keyboard(0xfa);
                                        break;
                                        
                                        case 0xf4: /*Enable keyboard*/
                                        keyboard_scan = 1;
                                        keyboard_at_adddata_keyboard(0xfa);
                                        break;
                                        case 0xf5: /*Disable keyboard*/
                                        keyboard_scan = 0;
                                        keyboard_at_adddata_keyboard(0xfa);
                                        break;
                                        
                                        case 0xff: /*Reset*/
                                        key_queue_start = key_queue_end = 0; /*Clear key queue*/
                                        keyboard_at_adddata_keyboard(0xfa);
                                        keyboard_at_adddata_keyboard(0xaa);
                                        break;
                                        
                                        default:
                                        pclog("Bad AT keyboard command %02X\n", val);
                                        keyboard_at_adddata_keyboard(0xfe);
//                                        dumpregs();
//                                        exit(-1);
                                }
                        }
                }
                break;
               
                case 0x61:
                ppi.pb = val;
                speaker_gated = val & 1;
                speaker_enable = val & 2;
                if (speaker_enable) 
                        was_speaker_enable = 1;
                pit_set_gate(2, val & 1);
                break;
                
                case 0x64:
                keyboard_at.want60 = 0;
                keyboard_at.command = val;
                /*New controller command*/
                switch (val)
                {
                        case 0x20: case 0x21: case 0x22: case 0x23:
                        case 0x24: case 0x25: case 0x26: case 0x27:
                        case 0x28: case 0x29: case 0x2a: case 0x2b:
                        case 0x2c: case 0x2d: case 0x2e: case 0x2f:
                        case 0x30: case 0x31: case 0x32: case 0x33:
                        case 0x34: case 0x35: case 0x36: case 0x37:
                        case 0x38: case 0x39: case 0x3a: case 0x3b:
                        case 0x3c: case 0x3d: case 0x3e: case 0x3f:
                        keyboard_at_adddata(keyboard_at.mem[val & 0x1f]);
                        break;

                        case 0x60: case 0x61: case 0x62: case 0x63:
                        case 0x64: case 0x65: case 0x66: case 0x67:
                        case 0x68: case 0x69: case 0x6a: case 0x6b:
                        case 0x6c: case 0x6d: case 0x6e: case 0x6f:
                        case 0x70: case 0x71: case 0x72: case 0x73:
                        case 0x74: case 0x75: case 0x76: case 0x77:
                        case 0x78: case 0x79: case 0x7a: case 0x7b:
                        case 0x7c: case 0x7d: case 0x7e: case 0x7f:
                        keyboard_at.want60 = 1;
                        break;
                        
                        case 0xa1: /*AMI - get controlled version*/
                        break;
                                
                        case 0xa7: /*Disable mouse port*/
                        break;
                        
                        case 0xa9: /*Test mouse port*/
                        keyboard_at_adddata(0x00); /*no error*/
                        break;
                        
                        case 0xaa: /*Self-test*/
                        if (!keyboard_at.initialised)
                        {
                                keyboard_at.initialised = 1;
                                key_ctrl_queue_start = key_ctrl_queue_end = 0;
                                keyboard_at.status &= ~STAT_OFULL;
                        }
                        keyboard_at.status |= STAT_SYSFLAG;
                        keyboard_at.mem[0] |= 0x04;
                        keyboard_at_adddata(0x55);
                        break;
                        
                        case 0xab: /*Interface test*/
                        keyboard_at_adddata(0x00); /*no error*/
                        break;
                        
                        case 0xad: /*Disable keyboard*/
                        keyboard_at.mem[0] |=  0x10;
                        break;

                        case 0xae: /*Enable keyboard*/
                        keyboard_at.mem[0] &= ~0x10;
                        break;
                        
                        case 0xc0: /*Read input port*/
                        keyboard_at_adddata(keyboard_at.input_port);
                        keyboard_at.input_port = ((keyboard_at.input_port + 1) & 3) | (keyboard_at.input_port & 0xfc);
                        break;
                        
                        case 0xc9: /*AMI - block P22 and P23 ??? */
                        break;
                        
                        case 0xca: /*AMI - read keyboard mode*/
                        keyboard_at_adddata(0x00); /*ISA mode*/
                        break;
                        
                        case 0xcb: /*AMI - set keyboard mode*/
                        keyboard_at.want60 = 1;
                        break;
                        
                        case 0xcf: /*??? - sent by MegaPC BIOS*/
                        keyboard_at.want60 = 1;
                        break;
                        
                        case 0xd0: /*Read output port*/
                        keyboard_at_adddata(keyboard_at.output_port);
                        break;
                        
                        case 0xd1: /*Write output port*/
                        keyboard_at.want60 = 1;
                        break;
                        
                        case 0xd3: /*Write mouse output buffer*/
                        keyboard_at.want60 = 1;
                        break;
                        
                        case 0xd4: /*Write to mouse*/
                        keyboard_at.want60 = 1;
                        break;
                        
                        case 0xe0: /*Read test inputs*/
                        keyboard_at_adddata(0x00);
                        break;
                        
                        case 0xef: /*??? - sent by AMI486*/
                        break;
                        
                        case 0xfe: /*Pulse output port - pin 0 selected - x86 reset*/
                        softresetx86(); /*Pulse reset!*/
                        break;
                                                
                        case 0xff: /*Pulse output port - but no pins selected - sent by MegaPC BIOS*/
                        break;
                                
                        default:
                        pclog("Bad AT keyboard controller command %02X\n", val);
//                        dumpregs();
//                        exit(-1);
                }
        }
}

uint8_t keyboard_at_read(uint16_t port, void *priv)
{
        uint8_t temp = 0xff;
        cycles -= 4;
//        if (port != 0x61) pclog("keyboard_at : read %04X ", port);
        switch (port)
        {
                case 0x60:
                temp = keyboard_at.out;
                keyboard_at.status &= ~(STAT_OFULL | STAT_MFULL);
                picintc(keyboard_at.last_irq);
                keyboard_at.last_irq = 0;
                break;

                case 0x61:                
                if (ppispeakon) return (ppi.pb&~0xC0)|0x20;
                return ppi.pb&~0xC0;
                break;
                
                case 0x64:
                temp = keyboard_at.status;
                keyboard_at.status &= ~(STAT_RTIMEOUT | STAT_TTIMEOUT);
                break;
        }
//        if (port != 0x61) pclog("%02X  %08X\n", temp, rammask);
        return temp;
}

void keyboard_at_reset()
{
        keyboard_at.initialised = 0;
        keyboard_at.status = STAT_LOCK | STAT_CD;
        keyboard_at.mem[0] = 0x11;
        keyboard_at.wantirq = 0;
        keyboard_at.output_port = 0;
        keyboard_at.input_port = 0xb0;
        keyboard_at.out_new = -1;
        keyboard_at.last_irq = 0;
        
        keyboard_at.key_wantdata = 0;
        
        keyboard_scan = 1;
}

void keyboard_at_init()
{
        //return;
        io_sethandler(0x0060, 0x0005, keyboard_at_read, NULL, NULL, keyboard_at_write, NULL, NULL,  NULL);
        keyboard_at_reset();
        keyboard_send = keyboard_at_adddata_keyboard;
        keyboard_poll = keyboard_at_poll;
        mouse_write = NULL;
        
        timer_add(keyboard_at_poll, &keybsenddelay, TIMER_ALWAYS_ENABLED,  NULL);
}
