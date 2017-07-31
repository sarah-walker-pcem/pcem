#define BITMAP WINDOWS_BITMAP
#include <windows.h>
#include <windowsx.h>
#undef BITMAP

#include <commctrl.h>

#include "ibm.h"
#include "config.h"
#include "resources.h"
#include "win.h"
#include "paths.h"

static void config_list_update(HWND hdlg)
{
        char s[512];
        int num, c;
        HWND h;

        strcpy(s, configs_path);
        put_backslash(s);
        strcat(s, "*.cfg");
        pclog("Dir %s\n", s);

        DlgDirList(hdlg, s, IDC_LIST, 0, 0);
        
        h = GetDlgItem(hdlg, IDC_LIST);
        num = SendMessage(h, LB_GETCOUNT, 0, 0);
        
        for (c = 0; c < num; c++)
        {
                char *ext;
                
                SendMessage(h, LB_GETTEXT, c, (LPARAM)s);
                ext = get_extension(s);
                if (ext && ext[0])
                {
                        ext--;
                        *ext = 0;
                }

                SendMessage(h, LB_DELETESTRING, c, 0);                        
                SendMessage(h, LB_INSERTSTRING, c, (LPARAM)s);
        }
}

static BOOL CALLBACK config_selection_dlgproc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
{
        //HWND h;

        switch (message)
        {
                case WM_INITDIALOG:
//                pause = 1;
                config_list_update(hdlg);
//                h = GetDlgItem(hdlg, IDC_LIST);
//                SendMessage(h, LB_ADDSTRING, 0, (LPARAM)(LPCSTR)"AAA");
//                SendMessage(h, LB_ADDSTRING, 0, (LPARAM)(LPCSTR)"BBB");
                return TRUE;
                
                case WM_COMMAND:
                switch (LOWORD(wParam))
                {
                        case IDOK:
                        {
                                char s[512];
                                
                                int ret = DlgDirSelectEx(hdlg, s, 512, IDC_LIST);
                                
                                pclog("DlgDirSelectEx returned %i %s\n", ret, s);
                                if (s[0])
                                {
                                        char cfg[512];
                                        
                                        strcpy(cfg, configs_path);
                                        put_backslash(cfg);
                                        strcat(cfg, s);
                                        strcat(cfg, "cfg");
//                                        sprintf(cfg, "%s\\configs\\%scfg", configs_path, s);
                                        pclog("Config name %s\n", cfg);
                                        
                                        strcpy(config_file_default, cfg);

                                        EndDialog(hdlg, 1);
//                                        pause = 0;
                                        return TRUE;
                                }
                        }
                        return FALSE;
                        
                        case IDCANCEL:
                        EndDialog(hdlg, 0);
//                        pause = 0;
                        return TRUE;
                        
                        case IDC_NEW:
                        {
                                char s[512];
                                int c;
                                
                                strcpy(s, configs_path);
                                put_backslash(s);
                                strcat(s, "*.cfg");
                                for (c = 0; c < strlen(s); c++)
                                {
                                        if (s[c] == '/')
                                                s[c] = '\\';
                                }

                                if (!getsfile(hdlg, "Configuration (*.CFG)\0*.CFG\0All files (*.*)\0*.*\0", s, s, "cfg"))
                                {
                                        config_open(hdlg);
                                        
                                        saveconfig(openfilestring);
                                }

                                config_list_update(hdlg);
                                
                                return TRUE;
                        }

                        case IDC_CONFIG:
                        {
                                char s[512];
                                
                                int ret = DlgDirSelectEx(hdlg, s, 512, IDC_LIST);
                                
                                pclog("DlgDirSelectEx returned %i %s\n", ret, s);
                                if (s[0])
                                {
                                        char cfg[512];
                                        
                                        strcpy(cfg, configs_path);
                                        put_backslash(cfg);
                                        strcat(cfg, s);
                                        strcat(cfg, "cfg");
                                        pclog("Config name %s\n", cfg);
                                                                                
                                        loadconfig(cfg);
                                        config_open(hdlg);                                        
                                        saveconfig(cfg);
                                }
                                
                                return TRUE;
                        }

                        case IDC_HDCONF:
                        {
                                char s[512];
                                
                                int ret = DlgDirSelectEx(hdlg, s, 512, IDC_LIST);
                                
                                pclog("DlgDirSelectEx returned %i %s\n", ret, s);
                                if (s[0])
                                {
                                        char cfg[512];
                                        
                                        strcpy(cfg, configs_path);
                                        put_backslash(cfg);
                                        strcat(cfg, s);
                                        strcat(cfg, "cfg");
                                        pclog("Config name %s\n", cfg);
                                                                                
                                        loadconfig(cfg);
                                        hdconf_open(hdlg);                                        
                                        saveconfig(cfg);
                                }
                                
                                return TRUE;
                        }
#ifdef USE_NETWORKING                        
                        case IDC_NETWORK:
                        networkconfig_open(hdlg);
                        return TRUE;
#endif
                }
                break;
        }
        return FALSE;
}

int config_selection_open(HWND hwnd, int inited)
{
        int ret;
        
        has_been_inited = inited;
        
        ret = DialogBox(hinstance, TEXT("ConfigureSelectionDlg"), hwnd, (DLGPROC)config_selection_dlgproc);
        
        has_been_inited = 1;
        
        return ret;
}
