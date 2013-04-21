#include <stdio.h>
#include <string.h>
#include <stdint.h>
#define DIRECTINPUT_VERSION	0x0700
#include <dinput.h>
#include "plat-keyboard.h"
#include "win.h"

extern "C" int key[256];
uint8_t dinput_key[256];

extern "C" void fatal(const char *format, ...);
extern "C" void pclog(const char *format, ...);

extern "C" void keyboard_init();
extern "C" void keyboard_close();
extern "C" void keyboard_poll();

LPDIRECTINPUT lpdi = NULL;
LPDIRECTINPUTDEVICE lpdi_key = NULL;

static int keyboard_lookup[256] = 
{
        -1,             DIK_ESCAPE,  DIK_1,           DIK_2,         DIK_3,       DIK_4,        DIK_5,          DIK_6,        /*00*/
        DIK_7,          DIK_8,       DIK_9,           DIK_0,         DIK_MINUS,   DIK_EQUALS,   DIK_BACKSPACE,  DIK_TAB,      /*08*/
        DIK_Q,          DIK_W,       DIK_E,           DIK_R,         DIK_T,       DIK_Y,        DIK_U,          DIK_I,        /*10*/
        DIK_O,          DIK_P,       DIK_LBRACKET,    DIK_RBRACKET,  DIK_RETURN,  DIK_LCONTROL, DIK_A,          DIK_S,        /*18*/
        DIK_D,          DIK_F,       DIK_G,           DIK_H,         DIK_J,       DIK_K,        DIK_L,          DIK_SEMICOLON,/*20*/
        DIK_APOSTROPHE, DIK_GRAVE,   DIK_LSHIFT,      DIK_BACKSLASH, DIK_Z,       DIK_X,        DIK_C,          DIK_V,        /*28*/  
        DIK_B,          DIK_N,       DIK_M,           DIK_COMMA,     DIK_PERIOD,  DIK_SLASH,    DIK_RSHIFT,     DIK_MULTIPLY, /*30*/
        DIK_LMENU,      DIK_SPACE,   DIK_CAPSLOCK,    DIK_F1,        DIK_F2,      DIK_F3,       DIK_F4,         DIK_F5,       /*38*/
        DIK_F6,         DIK_F7,      DIK_F8,          DIK_F9,        DIK_F10,     DIK_NUMLOCK,  DIK_SCROLL,     DIK_NUMPAD7,  /*40*/
        DIK_NUMPAD8,    DIK_NUMPAD9, DIK_NUMPADMINUS, DIK_NUMPAD4,   DIK_NUMPAD5, DIK_NUMPAD6,  DIK_NUMPADPLUS, DIK_NUMPAD1,  /*48*/
        DIK_NUMPAD2,    DIK_NUMPAD3, DIK_NUMPAD0,     DIK_DECIMAL,   DIK_SYSRQ,   -1,           DIK_OEM_102,    DIK_F11,      /*50*/
        DIK_F12,        -1,          -1,              DIK_LWIN,      DIK_RWIN,    DIK_LMENU,    -1,             -1,           /*58*/
        -1,             -1,          -1,              -1,            -1,          -1,           -1,             -1,           /*60*/
        -1,             -1,          -1,              -1,            -1,          -1,           -1,             -1,           /*68*/
        -1,             -1,          -1,              -1,            -1,          -1,           -1,             -1,           /*70*/
        -1,             -1,          -1,              -1,            -1,          -1,           -1,             -1,           /*78*/
        
        -1,             -1,          -1,              -1,            -1,          -1,           -1,             -1,           /*80*/
        -1,             -1,          -1,              -1,            -1,          -1,           -1,             -1,           /*88*/
        -1,             -1,          -1,              -1,            -1,          -1,           -1,             -1,           /*90*/
        -1,             -1,          -1,              -1,        DIK_NUMPADENTER, DIK_RCONTROL, -1,             -1,           /*98*/
        -1,             -1,          -1,              -1,            -1,          -1,           -1,             -1,           /*a0*/
        -1,             -1,          -1,              -1,            -1,          -1,           -1,             -1,           /*a8*/
        -1,             -1,          -1,              -1,            -1,          -1,           -1,             -1,           /*b0*/
        DIK_RMENU,      -1,          -1,              -1,            -1,          -1,           -1,             -1,           /*b8*/ 
        -1,             -1,          -1,              -1,            -1,          -1,           -1,             DIK_HOME,     /*c0*/
        DIK_UP,         DIK_PRIOR,   -1,              DIK_LEFT,      -1,          DIK_RIGHT,    -1,             DIK_END,      /*c8*/
        DIK_DOWN,       DIK_NEXT,    DIK_INSERT,      DIK_DELETE,    -1,          -1,           -1,             -1,           /*d0*/ 
        -1,             -1,          -1,              -1,            -1,          -1,           -1,             -1,           /*d8*/
        -1,             -1,          -1,              -1,            -1,          -1,           -1,             -1,           /*e0*/
        -1,             -1,          -1,              -1,            -1,          -1,           -1,             -1,           /*e8*/
        -1,             -1,          -1,              -1,            -1,          -1,           -1,             -1,           /*f0*/ 
        -1,             -1,          -1,              -1,            -1,          -1,           -1,             DIK_PAUSE,    /*f8*/

};
        
void keyboard_init()
{
        atexit(keyboard_close);
        
        if (FAILED(DirectInputCreate(hinstance, DIRECTINPUT_VERSION, &lpdi, NULL)))
           fatal("install_keyboard : DirectInputCreate failed\n");
        if (FAILED(lpdi->CreateDevice(GUID_SysKeyboard, &lpdi_key, NULL)))
           fatal("install_keyboard : CreateDevice failed\n");
        if (FAILED(lpdi_key->SetCooperativeLevel(ghwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE)))
           fatal("install_keyboard : SetCooperativeLevel failed\n");
        if (FAILED(lpdi_key->SetDataFormat(&c_dfDIKeyboard)))
           fatal("install_keyboard : SetDataFormat failed\n");
        if (FAILED(lpdi_key->Acquire()))
           fatal("install_keyboard : Acquire failed\n");
                      
        memset(key, 0, sizeof(key));
}

void keyboard_close()
{
        if (lpdi_key)
        {
                lpdi_key->Release();
                lpdi_key = NULL;
        }
        if (lpdi)
        {
                lpdi->Release();
                lpdi = NULL;
        }
}

void keyboard_poll_host()
{
        int c;
        if (FAILED(lpdi_key->GetDeviceState(256, (LPVOID)dinput_key)))
        {
                lpdi_key->Acquire();
                lpdi_key->GetDeviceState(256, (LPVOID)dinput_key);
        }
        for (c = 0; c < 256; c++)
        {
                if (dinput_key[c] & 0x80) pclog("Dinput key down %i %02X\n", c, c);
                if (keyboard_lookup[c] != -1)
                {
                        key[c] = dinput_key[keyboard_lookup[c]] & 0x80;
                        if (key[c]) pclog("Key down %i %02X  %i %02X\n", c, c, keyboard_lookup[c], keyboard_lookup[c]);
                }
        }
}
