#include <dinput.h>
#include "plat-mouse.h"
#include "plat-dinput.h"
#include "win.h"

extern "C" void fatal(const char *format, ...);
extern "C" void pclog(const char *format, ...);

extern "C" void mouse_init();
extern "C" void mouse_close();
extern "C" void poll_mouse();
extern "C" void position_mouse(int x, int y);
extern "C" void get_mouse_mickeys(int *x, int *y);

static LPDIRECTINPUTDEVICE lpdi_mouse = NULL;
static DIMOUSESTATE mousestate;
static int mouse_x = 0, mouse_y = 0;
int mouse_b = 0;

void mouse_init()
{
        atexit(mouse_close);
        
        if (FAILED(lpdi->CreateDevice(GUID_SysMouse, &lpdi_mouse, NULL)))
           fatal("mouse_init : CreateDevice failed\n");
        if (FAILED(lpdi_mouse->SetCooperativeLevel(ghwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE)))
           fatal("mouse_init : SetCooperativeLevel failed\n");
        if (FAILED(lpdi_mouse->SetDataFormat(&c_dfDIMouse)))
           fatal("mouse_init : SetDataFormat failed\n");
        if (FAILED(lpdi_mouse->Acquire()))
           fatal("mouse_init : Acquire failed\n");
}

void mouse_close()
{
        if (lpdi_mouse)
        {
                lpdi_mouse->Release();
                lpdi_mouse = NULL;
        }
}

void poll_mouse()
{
        if (FAILED(lpdi_mouse->GetDeviceState(sizeof(DIMOUSESTATE), (LPVOID)&mousestate)))
        {
                lpdi_mouse->Acquire();
                lpdi_mouse->GetDeviceState(sizeof(DIMOUSESTATE), (LPVOID)&mousestate);
        }                
        mouse_b = 0;
        if (mousestate.rgbButtons[0] & 0x80)
           mouse_b |= 1;
        if (mousestate.rgbButtons[1] & 0x80)
           mouse_b |= 2;
        if (mousestate.rgbButtons[2] & 0x80)
           mouse_b |= 4;
        mouse_x += mousestate.lX;
        mouse_y += mousestate.lY;        
        if (!mousecapture)
           mouse_x = mouse_y = mouse_b = 0;
}

void position_mouse(int x, int y)
{
}

void get_mouse_mickeys(int *x, int *y)
{
        *x = mouse_x;
        *y = mouse_y;
        mouse_x = mouse_y = 0;
}
