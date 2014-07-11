#define  _WIN32_WINNT 0x0501
#define BITMAP WINDOWS_BITMAP
#include <windows.h>
#include <windowsx.h>
#undef BITMAP

#include <commctrl.h>
#include <commdlg.h>

#include <process.h>

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "ibm.h"
#include "video.h"
#include "resources.h"
#include "cpu.h"
#include "ide.h"
#include "model.h"
#include "nvr.h"
#include "sound.h"

#include "plat-midi.h"
#include "plat-keyboard.h"

#include "win.h"
#include "win-ddraw.h"
#include "win-ddraw-fs.h"
#include "win-d3d.h"
#include "win-d3d-fs.h"
//#include "win-opengl.h"

#ifndef MAPVK_VK_TO_VSC
#define MAPVK_VK_TO_VSC 0
#endif

uint64_t timer_freq;

static RAWINPUTDEVICE device;
static uint16_t scancode_map[65536];

static struct
{
        void (*init)(HWND h);
        void (*close)();
        void (*resize)(int x, int y);
} vid_apis[2][2] =
{
        {
                ddraw_init, ddraw_close, NULL,
                d3d_init, d3d_close, d3d_resize
        },
        {
                ddraw_fs_init, ddraw_fs_close, NULL,
                d3d_fs_init, d3d_fs_close, NULL
        },
};

#define TIMER_1SEC 1

int winsizex=640,winsizey=480;
int gfx_present[GFX_MAX];
#undef cs
CRITICAL_SECTION cs;

HANDLE mainthreadh;

int infocus=1;

int drawits=0;

int romspresent[ROM_MAX];
int quited=0;

RECT oldclip;
int mousecapture=0;

/*  Declare Windows procedure  */
LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK subWindowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

HWND ghwnd;

HINSTANCE hinstance;

HMENU menu;

extern int updatestatus;

int pause=0;

static int win_doresize = 0;

static int leave_fullscreen_flag = 0;

void updatewindowsize(int x, int y)
{
        RECT r;
        if (vid_resize) return;

        winsizex=x; winsizey=y;
        win_doresize = 1;
}

void releasemouse()
{
        if (mousecapture) 
        {
                ClipCursor(&oldclip);
                ShowCursor(TRUE);
                mousecapture = 0;
        }
}

void startblit()
{
        EnterCriticalSection(&cs);
}

void endblit()
{
        LeaveCriticalSection(&cs);
}

void leave_fullscreen()
{
        leave_fullscreen_flag = 1;
}

void mainthread(LPVOID param)
{
        int t = 0;
        int frames = 0;
        DWORD old_time, new_time;

//        Sleep(500);
        drawits=0;
        old_time = GetTickCount();
        while (!quited)
        {
                if (updatestatus)
                {
                        updatestatus = 0;
                        if (status_is_open)
                                SendMessage(status_hwnd, WM_USER, 0, 0);
                }
                new_time = GetTickCount();
                drawits += new_time - old_time;
                old_time = new_time;
                if (drawits > 0 && !pause)
                {
                        drawits-=10;        if (drawits>50) drawits=0;
                        runpc();
                        frames++;
                        if (frames >= 200 && nvr_dosave)
                        {
                                frames = 0;
                                nvr_dosave = 0;
                                savenvr();
                        }
                }
                else
                        Sleep(1);

                if (!video_fullscreen && win_doresize)
                {
                        RECT r;
                        GetWindowRect(ghwnd, &r);
                        MoveWindow(ghwnd, r.left, r.top,
                                winsizex + (GetSystemMetrics(SM_CXFIXEDFRAME) * 2),
                                winsizey + (GetSystemMetrics(SM_CYFIXEDFRAME) * 2) + GetSystemMetrics(SM_CYMENUSIZE) + GetSystemMetrics(SM_CYCAPTION) + 1,
                                TRUE);
                        win_doresize = 0;
                }

                if (leave_fullscreen_flag)
                {
                        leave_fullscreen_flag = 0;
                        SendMessage(ghwnd, WM_LEAVEFULLSCREEN, 0, 0);
                }
        }
}

static void initmenu(void)
{
        int c;
        HMENU m;
        char s[32];
        m=GetSubMenu(menu,2); /*Settings*/
        m=GetSubMenu(m,1); /*CD-ROM*/

        /* Loop through each Windows drive letter and test to see if
           it's a CDROM */
        for (c='A';c<='Z';c++)
        {
                sprintf(s,"%c:\\",c);
                if (GetDriveType(s)==DRIVE_CDROM)
                {
                        sprintf(s, "Host CD/DVD Drive (%c:)", c);
                        AppendMenu(m,MF_STRING,IDM_CDROM_REAL+c,s);
                }
        }
}

void get_executable_name(char *s, int size)
{
        GetModuleFileName(hinstance, s, size);
}

void set_window_title(char *s)
{
        if (video_fullscreen)
                return;
        SetWindowText(ghwnd, s);
}

uint64_t timer_read()
{
        LARGE_INTEGER qpc_time;
        QueryPerformanceCounter(&qpc_time);
        return qpc_time.QuadPart;
}

/* This is so we can disambiguate scan codes that would otherwise conflict and get
   passed on incorrectly. */
UINT16 convert_scan_code(UINT16 scan_code)
{
	switch (scan_code)
        {
		case 0xE001:
		return 0xF001;
		case 0xE002:
		return 0xF002;
		case 0xE005:
		return 0xF005;
		case 0xE006:
		return 0xF006;
		case 0xE007:
		return 0xF007;
		case 0xE071:
		return 0xF008;
		case 0xE072:
		return 0xF009;
		case 0xE07F:
		return 0xF00A;
		case 0xE0E1:
		return 0xF00B;
		case 0xE0EE:
		return 0xF00C;
		case 0xE0F1:
		return 0xF00D;
		case 0xE0FE:
		return 0xF00E;
		case 0xE0EF:
		return 0xF00F;
		
		default:
		return scan_code;
	}
}

void get_registry_key_map()
{
	char *keyName = "SYSTEM\\CurrentControlSet\\Control\\Keyboard Layout";
	char *valueName = "Scancode Map";
	char buf[32768];
	DWORD bufSize;
	HKEY hKey;
	int j;

 	/* First, prepare the default scan code map list which is 1:1.
 	   Remappings will be inserted directly into it.
 	   65536 bytes so scan codes fit in easily and it's easy to find what each maps too,
 	   since each array element is a scan code and provides for E0, etc. ones too. */
	for (j = 0; j < 65536; j++)
		scancode_map[j] = convert_scan_code(j);

	bufSize = 32768;
	pclog("Preparing scan code map list...\n");
 	/* Get the scan code remappings from:
 	   HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Keyboard Layout */
	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, keyName, 0, 1, &hKey) == ERROR_SUCCESS)
        {
		if(RegQueryValueEx(hKey, valueName, NULL, NULL, buf, &bufSize) == ERROR_SUCCESS)
                {
			UINT32 *bufEx2 = (UINT32 *) buf;
			int scMapCount = bufEx2[2];
			pclog("%lu scan code mappings found!\n", scMapCount);
			if ((bufSize != 0) && (scMapCount != 0))
                        {
				UINT16 *bufEx = (UINT16 *) (buf + 12);
				pclog("More than zero scan code mappings found, processing...\n");
				for (j = 0; j < scMapCount*2; j += 2)
 				{
 					/* Each scan code is 32-bit: 16 bits of remapped scan code,
 					   and 16 bits of original scan code. */
  					int scancode_unmapped = bufEx[j + 1];
  					int scancode_mapped = bufEx[j];

  					scancode_mapped = convert_scan_code(scancode_mapped);

  					scancode_map[scancode_unmapped] = scancode_mapped;
  					pclog("Scan code mapping %u detected: %X -> %X\n", scancode_unmapped, scancode_mapped, scancode_map[scancode_unmapped]);
  				}
				pclog("Done processing!\n");
			}
		}
		RegCloseKey(hKey);
	}
	pclog("Done preparing!\n");
}

int WINAPI WinMain (HINSTANCE hThisInstance,
                    HINSTANCE hPrevInstance,
                    LPSTR lpszArgument,
                    int nFunsterStil)

{
        HWND hwnd;               /* This is the handle for our window */
        MSG messages;            /* Here messages to the application are saved */
        WNDCLASSEX wincl;        /* Data structure for the windowclass */
        int c, d;
        LARGE_INTEGER qpc_freq;

        hinstance=hThisInstance;
        /* The Window structure */
        wincl.hInstance = hThisInstance;
        wincl.lpszClassName = szClassName;
        wincl.lpfnWndProc = WindowProcedure;      /* This function is called by windows */
        wincl.style = CS_DBLCLKS;                 /* Catch double-clicks */
        wincl.cbSize = sizeof (WNDCLASSEX);

        /* Use default icon and mouse-pointer */
        wincl.hIcon = LoadIcon (NULL, IDI_APPLICATION);
        wincl.hIconSm = LoadIcon (NULL, IDI_APPLICATION);
        wincl.hCursor = NULL;//LoadCursor (NULL, IDC_ARROW);
        wincl.lpszMenuName = NULL;                 /* No menu */
        wincl.cbClsExtra = 0;                      /* No extra bytes after the window class */
        wincl.cbWndExtra = 0;                      /* structure or the window instance */
        /* Use Windows's default color as the background of the window */
        wincl.hbrBackground = (HBRUSH) COLOR_BACKGROUND;

        /* Register the window class, and if it fails quit the program */
        if (!RegisterClassEx(&wincl))
                return 0;

        wincl.lpszClassName = szSubClassName;
        wincl.lpfnWndProc = subWindowProcedure;      /* This function is called by windows */

        if (!RegisterClassEx(&wincl))
                return 0;

        menu = LoadMenu(hThisInstance, TEXT("MainMenu"));
        initmenu();
        
        /* The class is registered, let's create the program*/
        hwnd = CreateWindowEx (
                0,                   /* Extended possibilites for variation */
                szClassName,         /* Classname */
                "PCem v8.1",         /* Title Text */
                WS_OVERLAPPEDWINDOW&~WS_SIZEBOX, /* default window */
                CW_USEDEFAULT,       /* Windows decides the position */
                CW_USEDEFAULT,       /* where the window ends up on the screen */
                640+(GetSystemMetrics(SM_CXFIXEDFRAME)*2),                 /* The programs width */
                480+(GetSystemMetrics(SM_CYFIXEDFRAME)*2)+GetSystemMetrics(SM_CYMENUSIZE)+GetSystemMetrics(SM_CYCAPTION)+1,                 /* and height in pixels */
                HWND_DESKTOP,        /* The window is a child-window to desktop */
                menu,                /* Menu */
                hThisInstance,       /* Program Instance handler */
                NULL                 /* No Window Creation data */
        );

        /* Make the window visible on the screen */
        ShowWindow (hwnd, nFunsterStil);

//        win_set_window(hwnd);
        
        memset(rawinputkey, 0, sizeof(rawinputkey));
	device.usUsagePage = 0x01;
	device.usUsage = 0x06;
	device.dwFlags = 0;
	device.hwndTarget = hwnd;
	
	if (RegisterRawInputDevices(&device, 1, sizeof(device)))
		pclog("Raw input registered!\n");
	else
		pclog("Raw input registration failed!\n");

	get_registry_key_map();

        ghwnd=hwnd;
        
        midi_init();
        atexit(midi_close);
        
        initpc();
        
        vid_apis[0][vid_api].init(ghwnd);

        if (vid_resize) SetWindowLong(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW|WS_VISIBLE);
        else            SetWindowLong(hwnd, GWL_STYLE, (WS_OVERLAPPEDWINDOW&~WS_SIZEBOX&~WS_THICKFRAME&~WS_MAXIMIZEBOX)|WS_VISIBLE);

        if (!cdrom_enabled)
           CheckMenuItem(menu, IDM_CDROM_DISABLED, MF_CHECKED);
        else           
           CheckMenuItem(menu, IDM_CDROM_REAL + cdrom_drive, MF_CHECKED);
        if (vid_resize) CheckMenuItem(menu, IDM_VID_RESIZE, MF_CHECKED);
        CheckMenuItem(menu, IDM_VID_DDRAW + vid_api, MF_CHECKED);
        CheckMenuItem(menu, IDM_VID_FS_FULL + video_fullscreen_scale, MF_CHECKED);
//        set_display_switch_mode(SWITCH_BACKGROUND);
        
        d=romset;
        for (c=0;c<ROM_MAX;c++)
        {
                romset=c;
                romspresent[c]=loadbios();
                pclog("romset %i - %i\n", c, romspresent[c]);
        }
        
        for (c = 0; c < ROM_MAX; c++)
        {
                if (romspresent[c])
                   break;
        }
        if (c == ROM_MAX)
        {
                MessageBox(hwnd,"No ROMs present!\nYou must have at least one romset to use PCem.","PCem fatal error",MB_OK);
                return 0;
        }

        romset=d;
        c=loadbios();

        if (!c)
        {
                if (romset!=-1) MessageBox(hwnd,"Configured romset not available.\nDefaulting to available romset.","PCem error",MB_OK);
                for (c=0;c<ROM_MAX;c++)
                {
                        if (romspresent[c])
                        {
                                romset = c;
                                model = model_getmodel(romset);
                                saveconfig();
                                resetpchard();
                                break;
                        }
                }
        }
        

        for (c = 0; c < GFX_MAX; c++)
                gfx_present[c] = video_card_available(video_old_to_new(c));

        if (!video_card_available(video_old_to_new(gfxcard)))
        {
                if (romset!=-1) MessageBox(hwnd,"Configured video BIOS not available.\nDefaulting to available romset.","PCem error",MB_OK);
                for (c = GFX_MAX-1; c >= 0; c--)
                {
                        if (gfx_present[c])
                        {
                                gfxcard = c;
                                saveconfig();
                                resetpchard();
                                break;
                        }
                }
        }

        loadbios();

        timeBeginPeriod(1);
        
        atexit(releasemouse);

//        QueryPerformanceFrequency(&counter_base);
///        QueryPerformanceCounter(&counter_posold);
//        counter_posold.QuadPart*=100;

        InitializeCriticalSection(&cs);
        mainthreadh=(HANDLE)_beginthread(mainthread,0,NULL);
        SetThreadPriority(mainthreadh, THREAD_PRIORITY_HIGHEST);
        
       
        updatewindowsize(640, 480);

        QueryPerformanceFrequency(&qpc_freq);
        timer_freq = qpc_freq.QuadPart;

//        focus=1;
//        setrefresh(100);

//        ShowCursor(TRUE);
        
        /* Run the message loop. It will run until GetMessage() returns 0 */
        while (!quited)
        {
/*                if (infocus)
                {
                        if (drawits)
                        {
                                drawits--;
                                if (drawits>10) drawits=0;
                                runpc();
                        }
//;                        else
//                           sleep(0);
                        if ((key[KEY_LCONTROL] || key[KEY_RCONTROL]) && key[KEY_END] && mousecapture)
                        {
                                ClipCursor(&oldclip);
                                mousecapture=0;
                        }
                }*/

                while (GetMessage(&messages,NULL,0,0) && !quited)
                {
                        if (messages.message==WM_QUIT) quited=1;
                        TranslateMessage(&messages);
                        DispatchMessage(&messages);                        
                        if ((key[KEY_LCONTROL] || key[KEY_RCONTROL]) && key[KEY_END] && mousecapture)
                        {
                                ClipCursor(&oldclip);
                                ShowCursor(TRUE);
                                mousecapture=0;
                        }
                }

                quited=1;
//                else
//                sleep(10);
        }
        
        startblit();
//        pclog("Sleep 1000\n");
        Sleep(200);
//        pclog("TerminateThread\n");
        TerminateThread(mainthreadh,0);
//        pclog("Quited? %i\n",quited);
//        pclog("Closepc\n");
        closepc();
//        pclog("dumpregs\n");

        vid_apis[video_fullscreen][vid_api].close();
        
        timeEndPeriod(1);
//        dumpregs();
        if (mousecapture) 
        {
                ClipCursor(&oldclip);
                ShowCursor(TRUE);
        }
        
        UnregisterClass(szSubClassName, hinstance);
        UnregisterClass(szClassName, hinstance);

//        pclog("Ending! %i %i\n",messages.wParam,quited);
        return messages.wParam;
}

char openfilestring[260];
int getfile(HWND hwnd, char *f, char *fn)
{
        OPENFILENAME ofn;       // common dialog box structure
        BOOL r;
        DWORD err;

        // Initialize OPENFILENAME
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hwnd;
        ofn.lpstrFile = openfilestring;
        //
        // Set lpstrFile[0] to '\0' so that GetOpenFileName does not
        // use the contents of szFile to initialize itself.
        //
//        ofn.lpstrFile[0] = '\0';
        strcpy(ofn.lpstrFile,fn);
        ofn.nMaxFile = sizeof(openfilestring);
        ofn.lpstrFilter = f;//"All\0*.*\0Text\0*.TXT\0";
        ofn.nFilterIndex = 1;
        ofn.lpstrFileTitle = NULL;
        ofn.nMaxFileTitle = 0;
        ofn.lpstrInitialDir = NULL;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

        // Display the Open dialog box.

        pclog("GetOpenFileName - lpstrFile = %s\n", ofn.lpstrFile);
        r = GetOpenFileName(&ofn);
        if (r)
        {
                pclog("GetOpenFileName return true\n");
                return 0;
        }
        pclog("GetOpenFileName return false\n");
        err = CommDlgExtendedError();
        pclog("CommDlgExtendedError return %04X\n", err);
        return 1;
}

int getsfile(HWND hwnd, char *f, char *fn)
{
        OPENFILENAME ofn;       // common dialog box structure
        BOOL r;
        DWORD err;

        // Initialize OPENFILENAME
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hwnd;
        ofn.lpstrFile = openfilestring;
        //
        // Set lpstrFile[0] to '\0' so that GetOpenFileName does not
        // use the contents of szFile to initialize itself.
        //
//        ofn.lpstrFile[0] = '\0';
        strcpy(ofn.lpstrFile,fn);
        ofn.nMaxFile = sizeof(openfilestring);
        ofn.lpstrFilter = f;//"All\0*.*\0Text\0*.TXT\0";
        ofn.nFilterIndex = 1;
        ofn.lpstrFileTitle = NULL;
        ofn.nMaxFileTitle = 0;
        ofn.lpstrInitialDir = NULL;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

        // Display the Open dialog box.

        pclog("GetSaveFileName - lpstrFile = %s\n", ofn.lpstrFile);
        r = GetSaveFileName(&ofn);
        if (r)
        {
                pclog("GetSaveFileName return true\n");
                return 0;
        }
        pclog("GetSaveFileName return false\n");
        err = CommDlgExtendedError();
        pclog("CommDlgExtendedError return %04X\n", err);
        return 1;
}




HHOOK hKeyboardHook;

LRESULT CALLBACK LowLevelKeyboardProc( int nCode, WPARAM wParam, LPARAM lParam )
{
        if (nCode < 0 || nCode != HC_ACTION || (!mousecapture && !video_fullscreen))
                return CallNextHookEx( hKeyboardHook, nCode, wParam, lParam); 
	
	KBDLLHOOKSTRUCT* p = (KBDLLHOOKSTRUCT*)lParam;

        if (p->vkCode == VK_TAB && p->flags & LLKHF_ALTDOWN) return 1; //disable alt-tab
        if (p->vkCode == VK_SPACE && p->flags & LLKHF_ALTDOWN) return 1; //disable alt-tab    
	if((p->vkCode == VK_LWIN) || (p->vkCode == VK_RWIN)) return 1;//disable windows keys
	if (p->vkCode == VK_ESCAPE && p->flags & LLKHF_ALTDOWN) return 1;//disable alt-escape
	BOOL bControlKeyDown = GetAsyncKeyState (VK_CONTROL) >> ((sizeof(SHORT) * 8) - 1);//checks ctrl key pressed
	if (p->vkCode == VK_ESCAPE && bControlKeyDown) return 1; //disable ctrl-escape
	
	return CallNextHookEx( hKeyboardHook, nCode, wParam, lParam );
}

LRESULT CALLBACK WindowProcedure (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
        HMENU hmenu;
        RECT rect;
//        pclog("Message %i %08X\n",message,message);
        switch (message)
        {
                case WM_CREATE:
                SetTimer(hwnd, TIMER_1SEC, 1000, NULL);
                hKeyboardHook = SetWindowsHookEx( WH_KEYBOARD_LL,  LowLevelKeyboardProc, GetModuleHandle(NULL), 0 );
                break;
                
                case WM_COMMAND:
//                        pclog("WM_COMMAND %i\n",LOWORD(wParam));
                hmenu=GetMenu(hwnd);
                switch (LOWORD(wParam))
                {
                        case IDM_FILE_RESET:
                        pause=1;
                        Sleep(100);
                        resetpc();
                        pause=0;
                        break;
                        case IDM_FILE_HRESET:
                        pause=1;
                        Sleep(100);
                        resetpchard();
                        pause=0;
                        break;
                        case IDM_FILE_EXIT:
                        PostQuitMessage (0);       /* send a WM_QUIT to the message queue */
                        break;
                        case IDM_DISC_A:
                        if (!getfile(hwnd,"Disc image (*.IMG;*.IMA)\0*.IMG;*.IMA\0All files (*.*)\0*.*\0",discfns[0]))
                        {
                                savedisc(0);
                                loaddisc(0,openfilestring);
                                saveconfig();
                        }
                        break;
                        case IDM_DISC_B:
                        if (!getfile(hwnd,"Disc image (*.IMG;*.IMA)\0*.IMG;*.IMA\0All files (*.*)\0*.*\0",discfns[1]))
                        {
                                savedisc(1);
                                loaddisc(1,openfilestring);
                                saveconfig();
                        }
                        break;
                        case IDM_EJECT_A:
                        savedisc(0);
                        ejectdisc(0);
                        saveconfig();
                        break;
                        case IDM_EJECT_B:
                        savedisc(1);
                        ejectdisc(1);
                        saveconfig();
                        break;
                        case IDM_HDCONF:
                        hdconf_open(hwnd);
                        break;
                        case IDM_CONFIG:
                        config_open(hwnd);
                        break;
                        case IDM_STATUS:
                        status_open(hwnd);
                        break;
                        
                        case IDM_VID_RESIZE:
                        vid_resize=!vid_resize;
                        CheckMenuItem(hmenu, IDM_VID_RESIZE, (vid_resize)?MF_CHECKED:MF_UNCHECKED);
                        if (vid_resize) SetWindowLong(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW|WS_VISIBLE);
                        else            SetWindowLong(hwnd, GWL_STYLE, (WS_OVERLAPPEDWINDOW&~WS_SIZEBOX&~WS_THICKFRAME&~WS_MAXIMIZEBOX)|WS_VISIBLE);
                        GetWindowRect(hwnd,&rect);
                        SetWindowPos(hwnd, 0, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER | SWP_FRAMECHANGED);
                        saveconfig();
                        break;
                        
                        case IDM_VID_DDRAW: case IDM_VID_D3D:
                        startblit();
                        CheckMenuItem(hmenu, IDM_VID_DDRAW + vid_api, MF_UNCHECKED);
                        vid_apis[0][vid_api].close();
                        vid_api = LOWORD(wParam) - IDM_VID_DDRAW;
                        CheckMenuItem(hmenu, IDM_VID_DDRAW + vid_api, MF_CHECKED);
                        vid_apis[0][vid_api].init(ghwnd);
                        endblit();
                        saveconfig();
                        device_force_redraw();
                        break;

                        case IDM_VID_FULLSCREEN:
                        if (video_fullscreen_first)
                        {
                                video_fullscreen_first = 0;
                                MessageBox(hwnd, "Use CTRL + ALT + PAGE DOWN to return to windowed mode", "PCem", MB_OK);
                        }
                        startblit();
                        mouse_close();
                        vid_apis[0][vid_api].close();
                        video_fullscreen = 1;
                        vid_apis[1][vid_api].init(ghwnd);
                        mouse_init();
                        endblit();
                        device_force_redraw();
                        break;

                        case IDM_VID_FS_FULL:
                        case IDM_VID_FS_43:
                        case IDM_VID_FS_SQ:                                
                        case IDM_VID_FS_INT:
                        CheckMenuItem(hmenu, IDM_VID_FS_FULL + video_fullscreen_scale, MF_UNCHECKED);
                        video_fullscreen_scale = LOWORD(wParam) - IDM_VID_FS_FULL;
                        CheckMenuItem(hmenu, IDM_VID_FS_FULL + video_fullscreen_scale, MF_CHECKED);
                        saveconfig();
                        break;
                        
                        case IDM_CDROM_DISABLED:
                        if (cdrom_enabled)
                        {
                                if (MessageBox(NULL,"This will reset PCem!\nOkay to continue?","PCem",MB_OKCANCEL) != IDOK)
                                   break;
                        }   
                        CheckMenuItem(hmenu, IDM_CDROM_REAL + cdrom_drive, MF_UNCHECKED);
                        CheckMenuItem(hmenu, IDM_CDROM_DISABLED,           MF_CHECKED);
                        CheckMenuItem(hmenu, IDM_CDROM_EMPTY,              MF_UNCHECKED);
                        if (cdrom_enabled)
                        {
                                cdrom_enabled = 0;                                             
                                saveconfig();
                                resetpchard();
                        }
                        break;
                        
                        case IDM_CDROM_EMPTY:
                        if (!cdrom_enabled)
                        {
                                if (MessageBox(NULL,"This will reset PCem!\nOkay to continue?","PCem",MB_OKCANCEL) != IDOK)
                                   break;
                        }                        
                        atapi->exit();
                        ioctl_open(0);
                        CheckMenuItem(hmenu, IDM_CDROM_REAL + cdrom_drive, MF_UNCHECKED);
                        CheckMenuItem(hmenu, IDM_CDROM_DISABLED,           MF_UNCHECKED);
                        cdrom_drive=0;
                        CheckMenuItem(hmenu, IDM_CDROM_EMPTY, MF_CHECKED);
                        saveconfig();
                        if (!cdrom_enabled)
                        {
                                cdrom_enabled = 1;
                                saveconfig();
                                resetpchard();
                        }
                        break;
                        default:
                        if (LOWORD(wParam)>=IDM_CDROM_REAL && LOWORD(wParam)<(IDM_CDROM_REAL+100))
                        {
                                if (!cdrom_enabled)
                                {
                                        if (MessageBox(NULL,"This will reset PCem!\nOkay to continue?","PCem",MB_OKCANCEL) != IDOK)
                                           break;
                                }         
                                atapi->exit();
                                ioctl_open(LOWORD(wParam)-IDM_CDROM_REAL);
                                CheckMenuItem(hmenu, IDM_CDROM_REAL + cdrom_drive, MF_UNCHECKED);
                                CheckMenuItem(hmenu, IDM_CDROM_DISABLED,           MF_UNCHECKED);
                                cdrom_drive = LOWORD(wParam) - IDM_CDROM_REAL;
                                CheckMenuItem(hmenu, IDM_CDROM_REAL + cdrom_drive, MF_CHECKED);
                                saveconfig();
                                if (!cdrom_enabled)
                                {
                                        cdrom_enabled = 1;
                                        saveconfig();
                                        resetpchard();
                                }
                        }
                        break;
                }
                return 0;
                
		case WM_INPUT:
                {
                        UINT size;
                        RAWINPUT *raw;
                        
                        GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &size, sizeof(RAWINPUTHEADER));
                        
                        raw = malloc(size);

        		/* Here we read the raw input data for the keyboard */
        		GetRawInputData((HRAWINPUT)(lParam), RID_INPUT, raw, &size, sizeof(RAWINPUTHEADER));
 
        		/* If the input is keyboard, we process it */
        		if (raw->header.dwType == RIM_TYPEKEYBOARD)
        		{
        			const RAWKEYBOARD rawKB = raw->data.keyboard;
                                USHORT scancode = rawKB.MakeCode;
                                
        			// pclog("Keyboard input received: S:%X VK:%X F:%X\n", c, d, e);

        			if (rawKB.VKey == VK_NUMLOCK)
        			{
        				/* This is for proper handling of Pause/Break and Num Lock */
        				scancode = (MapVirtualKey(rawKB.VKey, MAPVK_VK_TO_VSC) | 0x100);
        			}
        			/* If it's not a scan code that starts with 0xE1 */
        			if (!(rawKB.Flags & RI_KEY_E1))
        			{
        				if (rawKB.Flags & RI_KEY_E0)
                                                scancode |= (0xE0 << 8);

        				/* Remap it according to the list from the Registry */
        				scancode = scancode_map[scancode];

        				if ((scancode >> 8) == 0xF0)
        					scancode |= 0x100; /* Extended key code in disambiguated format */
        				else if ((scancode >> 8) == 0xE0)
        					scancode |= 0x80; /* Normal extended key code */

        				/* If it's not 0 (therefore not 0xE1, 0xE2, etc),
        				   then pass it on to the rawinputkey array */
        				if (!(scancode & 0xf00))
                                                rawinputkey[scancode & 0x1ff] = !(rawKB.Flags & RI_KEY_BREAK);
        			}
                        }
                        free(raw);
			
		}
		break;

                case WM_SETFOCUS:
                infocus=1;
 //               QueryPerformanceCounter(&counter_posold);
//                pclog("Set focus!\n");
                break;
                case WM_KILLFOCUS:
                infocus=0;
                if (mousecapture)
                {
                        ClipCursor(&oldclip);
                        ShowCursor(TRUE);
                        mousecapture=0;
                }
//                pclog("Lost focus!\n");
                break;

                case WM_LBUTTONUP:
                if (!mousecapture && !video_fullscreen)
                {
                        RECT pcclip;

                        GetClipCursor(&oldclip);
                        GetWindowRect(hwnd, &pcclip);
                        pcclip.left   += GetSystemMetrics(SM_CXFIXEDFRAME) + 10;
                        pcclip.right  -= GetSystemMetrics(SM_CXFIXEDFRAME) + 10;
                        pcclip.top    += GetSystemMetrics(SM_CXFIXEDFRAME) + GetSystemMetrics(SM_CYMENUSIZE) + GetSystemMetrics(SM_CYCAPTION) + 10;
                        pcclip.bottom -= GetSystemMetrics(SM_CXFIXEDFRAME) + 10;
                        ClipCursor(&pcclip);
                        mousecapture = 1;
                        ShowCursor(FALSE);
                }
                break;

                case WM_MBUTTONUP:
                releasemouse();
                break;

                case WM_ENTERMENULOOP:
//                if (key[KEY_ALT] || key[KEY_ALTGR]) return 0;
                break;

                case WM_SIZE:
                winsizex=lParam&0xFFFF;
                winsizey=lParam>>16;

                if (vid_apis[video_fullscreen][vid_api].resize)
                {
                        startblit();
                        vid_apis[video_fullscreen][vid_api].resize(winsizex, winsizey);
                        endblit();
                }

                if (mousecapture)
                {
                        RECT pcclip;

                        GetWindowRect(hwnd, &pcclip);
                        pcclip.left   += GetSystemMetrics(SM_CXFIXEDFRAME) + 10;
                        pcclip.right  -= GetSystemMetrics(SM_CXFIXEDFRAME) + 10;
                        pcclip.top    += GetSystemMetrics(SM_CXFIXEDFRAME) + GetSystemMetrics(SM_CYMENUSIZE) + GetSystemMetrics(SM_CYCAPTION) + 10;
                        pcclip.bottom -= GetSystemMetrics(SM_CXFIXEDFRAME) + 10;
                        ClipCursor(&pcclip);
                }
                break;

                case WM_TIMER:
                if (wParam == TIMER_1SEC)
                        onesec();
                break;
                
                case WM_RESETD3D:
                startblit();
                if (video_fullscreen)
                        d3d_fs_reset();
                else
                        d3d_reset();
                endblit();
                break;
                
                case WM_LEAVEFULLSCREEN:
                startblit();
                mouse_close();
                vid_apis[1][vid_api].close();
                video_fullscreen = 0;
                vid_apis[0][vid_api].init(ghwnd);
                mouse_init();
                endblit();
                device_force_redraw();
                break;

                case WM_KEYDOWN:
                case WM_SYSKEYDOWN:
                case WM_KEYUP:
                case WM_SYSKEYUP:
//                if (mousecapture)
                   return 0;
//                return DefWindowProc (hwnd, message, wParam, lParam);

                               
                case WM_DESTROY:
                UnhookWindowsHookEx( hKeyboardHook );
                KillTimer(hwnd, TIMER_1SEC);
                PostQuitMessage (0);       /* send a WM_QUIT to the message queue */
                break;

                case WM_SYSCOMMAND:
                if (wParam == SC_KEYMENU && HIWORD(lParam) <= 0 && (video_fullscreen || mousecapture))
                        return 0; /*disable ALT key for menu*/

                default:
//                        pclog("Def %08X %i\n",message,message);
                return DefWindowProc (hwnd, message, wParam, lParam);
        }
        return 0;
}

LRESULT CALLBACK subWindowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
        switch (message)
        {
                default:
                return DefWindowProc(hwnd, message, wParam, lParam);
        }
        return 0;
}
