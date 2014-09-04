#define DIRECTINPUT_VERSION 0x0700
#include <dinput.h>
#include "plat-joystick.h"
#include "win.h"

extern "C" int video_fullscreen;

extern "C" void fatal(const char *format, ...);
extern "C" void pclog(const char *format, ...);

extern "C" void joystick_init();
extern "C" void joystick_close();
extern "C" void poll_joystick();

joystick_t joystick_state[2];

static LPDIRECTINPUT lpdi;
static LPDIRECTINPUTDEVICE2 lpdi_joystick[2] = {NULL, NULL};

int joysticks_present = 0;
static GUID joystick_guids[2];

static BOOL CALLBACK joystick_enum_callback(LPCDIDEVICEINSTANCE lpddi, LPVOID data)
{
        if (joysticks_present >= 2)
                return DIENUM_STOP;
        
        pclog("joystick_enum_callback : found joystick %i : %s\n", joysticks_present, lpddi->tszProductName);
        
        joystick_guids[joysticks_present++] = lpddi->guidInstance;

        if (joysticks_present >= 2)
                return DIENUM_STOP;
        
        return DIENUM_CONTINUE;
}

void joystick_init()
{
        int c;

        atexit(joystick_close);
        
        joysticks_present = 0;
        
        if (FAILED(DirectInputCreate(hinstance, DIRECTINPUT_VERSION, &lpdi, NULL)))
                fatal("joystick_init : DirectInputCreate failed\n"); 

        if (FAILED(lpdi->EnumDevices(DIDEVTYPE_JOYSTICK, joystick_enum_callback, NULL, DIEDFL_ATTACHEDONLY)))
                fatal("joystick_init : EnumDevices failed\n");

        pclog("joystick_init: joysticks_present=%i\n", joysticks_present);
        
        for (c = 0; c < joysticks_present; c++)
        {                
                LPDIRECTINPUTDEVICE lpdi_joystick_temp = NULL;
                DIPROPRANGE joy_axis_range;
            
                if (FAILED(lpdi->CreateDevice(joystick_guids[c], &lpdi_joystick_temp, NULL)))
                        fatal("joystick_init : CreateDevice failed\n");
                if (FAILED(lpdi_joystick_temp->QueryInterface(IID_IDirectInputDevice2, (void **)&lpdi_joystick[c])))
                        fatal("joystick_init : CreateDevice failed\n");
                lpdi_joystick_temp->Release();
                if (FAILED(lpdi_joystick[c]->SetCooperativeLevel(ghwnd, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE)))
                        fatal("joystick_init : SetCooperativeLevel failed\n");
                if (FAILED(lpdi_joystick[c]->SetDataFormat(&c_dfDIJoystick)))
                        fatal("joystick_init : SetDataFormat failed\n");

                joy_axis_range.lMin = -32768;
                joy_axis_range.lMax =  32767;
                joy_axis_range.diph.dwSize = sizeof(DIPROPRANGE);
                joy_axis_range.diph.dwHeaderSize = sizeof(DIPROPHEADER);
                joy_axis_range.diph.dwObj = DIJOFS_X;
                joy_axis_range.diph.dwHow = DIPH_BYOFFSET;
                lpdi_joystick[c]->SetProperty(DIPROP_RANGE, &joy_axis_range.diph);
        
                joy_axis_range.lMin = -32768;
                joy_axis_range.lMax =  32767;
                joy_axis_range.diph.dwSize = sizeof(DIPROPRANGE);
                joy_axis_range.diph.dwHeaderSize = sizeof(DIPROPHEADER);
                joy_axis_range.diph.dwObj = DIJOFS_Y;
                joy_axis_range.diph.dwHow = DIPH_BYOFFSET;
                lpdi_joystick[c]->SetProperty(DIPROP_RANGE, &joy_axis_range.diph);

                if (FAILED(lpdi_joystick[c]->Acquire()))
                        fatal("joystick_init : Acquire failed\n");
        }
}

void joystick_close()
{
        if (lpdi_joystick[1])
        {
                lpdi_joystick[1]->Release();
                lpdi_joystick[1] = NULL;
        }
        if (lpdi_joystick[0])
        {
                lpdi_joystick[0]->Release();
                lpdi_joystick[0] = NULL;
        }
}

void joystick_poll()
{
        int c;

        for (c = 0; c < joysticks_present; c++)
        {                
                DIJOYSTATE joystate;
                
                if (FAILED(lpdi_joystick[c]->Poll()))
                {
                        lpdi_joystick[c]->Acquire();
                        lpdi_joystick[c]->Poll();
                }
                if (FAILED(lpdi_joystick[c]->GetDeviceState(sizeof(DIJOYSTATE), (LPVOID)&joystate)))
                {
                        lpdi_joystick[c]->Acquire();
                        lpdi_joystick[c]->Poll();
                        lpdi_joystick[c]->GetDeviceState(sizeof(DIJOYSTATE), (LPVOID)&joystate);
                }
                
                joystick_state[c].x = joystate.lX;
                joystick_state[c].y = joystate.lY;
                joystick_state[c].b[0] = joystate.rgbButtons[0] & 0x80;
                joystick_state[c].b[1] = joystate.rgbButtons[1] & 0x80;
                joystick_state[c].b[2] = joystate.rgbButtons[2] & 0x80;
                joystick_state[c].b[3] = joystate.rgbButtons[3] & 0x80;
                
//                pclog("joystick %i - x=%i y=%i b[0]=%i b[1]=%i  %i\n", c, joystick_state[c].x, joystick_state[c].y, joystick_state[c].b[0], joystick_state[c].b[1], joysticks_present);
        }                
}

