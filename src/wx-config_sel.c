#include "ibm.h"
#include "config.h"
#include "wx-utils.h"
#include "wx-sdl2.h"

extern void config_open(void* hwnd);
extern void hdconf_open(void* hwnd);

static void config_list_update(void* hdlg)
{
        char s[512];
        int num, c;
        void* h;

        strcpy(s, pcempath);
        put_backslash(s);
        strcat(s, "configs/*.cfg");
        pclog("Dir %s\n", s);

        wx_dlgdirlist(hdlg, s, WX_ID("IDC_LIST"), 0, 0);

        h = wx_getdlgitem(hdlg, WX_ID("IDC_LIST"));
        num = wx_sendmessage(h, WX_LB_GETCOUNT, 0, 0);

        for (c = 0; c < num; c++)
        {
                char *ext;

                wx_sendmessage(h, WX_LB_GETTEXT, c, (LONG_PARAM)s);
                ext = get_extension(s);
                if (ext && ext[0])
                {
                        ext--;
                        *ext = 0;
                }

                wx_sendmessage(h, WX_LB_DELETESTRING, c, 0);
                wx_sendmessage(h, WX_LB_INSERTSTRING, c, (LONG_PARAM)s);
        }
}

static int config_selection_dlgproc(void* hdlg, int message, INT_PARAM wParam, LONG_PARAM lParam)
{
        //void* h;

        switch (message)
        {
                case WX_INITDIALOG:
                {
        //                pause = 1;
                        config_list_update(hdlg);
        //                h = wx_getdlgitem(hdlg, IDC_LIST);
        //                wx_sendmessage(h, LB_ADDSTRING, 0, (LONG_PARAM)(LPCSTR)"AAA");
        //                wx_sendmessage(h, LB_ADDSTRING, 0, (LONG_PARAM)(LPCSTR)"BBB");
                        return TRUE;
                }
                case WX_COMMAND:
                {
                        if (wParam == wxID_OK)
                        {
                                char s[512];

                                int ret = wx_dlgdirselectex(hdlg, (LONG_PARAM)s, 512, WX_ID("IDC_LIST"));

                                pclog("wx_dlgdirselectex returned %i %s\n", ret, s);
                                if (s[0])
                                {
                                        char cfg[512];

                                        strcpy(cfg, pcempath);
                                        put_backslash(cfg);
                                        strcat(cfg, "configs/");
                                        strcat(cfg, s);
                                        strcat(cfg, "cfg");
//                                        sprintf(cfg, "%s\\configs\\%scfg", pcempath, s);
                                        pclog("Config name %s\n", cfg);

                                        strcpy(config_file_default, cfg);

                                        wx_enddialog(hdlg, 1);
//                                        pause = 0;
                                        return TRUE;
                                }
                                return FALSE;
                        }
                        else if (wParam == wxID_CANCEL)
                        {
                                wx_enddialog(hdlg, 0);
        //                        pause = 0;
                                return TRUE;
                        }
                        else if (wParam == WX_ID("IDC_NEW"))
                        {
                                char name[64];

//                                if (!getsfile(hdlg, "Configuration (*.cfg)|*.cfg|All files (*.*)|*.*", "", s, "cfg"))
                                if (wx_textentrydialog(hdlg, "Enter name:", "New config", 0, 1, 64, (LONG_PARAM)name))
                                {
                                        strcpy(openfilestring, pcempath);
                                        put_backslash(openfilestring);
                                        strcat(openfilestring, "configs/");
                                        strcat(openfilestring, name);
                                        strcat(openfilestring, ".cfg");

                                        pclog("Config %s\n", openfilestring);

                                        config_open(hdlg);

                                        saveconfig(openfilestring);

                                        config_list_update(hdlg);
                                }

                                return TRUE;
                        }
                        else if (wParam == WX_ID("IDC_CONFIG"))
                        {
                                char s[512];

                                int ret = wx_dlgdirselectex(hdlg, (LONG_PARAM)s, 512, WX_ID("IDC_LIST"));

                                pclog("wx_dlgdirselectex returned %i %s\n", ret, s);
                                if (s[0])
                                {
                                        char cfg[512];

                                        strcpy(cfg, pcempath);
                                        put_backslash(cfg);
                                        strcat(cfg, "configs/");
                                        strcat(cfg, s);
                                        strcat(cfg, "cfg");
                                        pclog("Config name %s\n", cfg);

                                        loadconfig(cfg);
                                        config_open(hdlg);
                                        saveconfig(cfg);
                                }

                                return TRUE;
                        }
                        else if (wParam == WX_ID("IDC_HDCONF"))
                        {
                                char s[512];

                                int ret = wx_dlgdirselectex(hdlg, (LONG_PARAM)s, 512, WX_ID("IDC_LIST"));

                                pclog("wx_dlgdirselectex returned %i %s\n", ret, s);
                                if (s[0])
                                {
                                        char cfg[512];

                                        strcpy(cfg, pcempath);
                                        put_backslash(cfg);
                                        strcat(cfg, "configs/");
                                        strcat(cfg, s);
                                        strcat(cfg, "cfg");
                                        pclog("Config name %s\n", cfg);

                                        loadconfig(cfg);
                                        hdconf_open(hdlg);
                                        saveconfig(cfg);
                                }

                                return TRUE;
                        }
                        else if (wParam == WX_ID("IDC_RENAME"))
                        {
                                char name[64];
                                char old_name[64];
                                void* h;
                                int c;

                                h = wx_getdlgitem(hdlg, WX_ID("IDC_LIST"));
                                c = wx_sendmessage(h, WX_LB_GETCURSEL, 0, 0);
                                if (c >= 0)
                                {
                                        wx_sendmessage(h, WX_LB_GETTEXT, c, (LONG_PARAM)old_name);

                                        if (wx_textentrydialog(hdlg, "Enter name:", "New name", old_name, 1, 64, (LONG_PARAM)name))
                                        {
                                                char old_path[512];
                                                char new_path[512];

                                                strcpy(old_path, pcempath);
                                                put_backslash(old_path);
                                                strcat(old_path, "configs/");
                                                strcat(old_path, old_name);
                                                strcat(old_path, ".cfg");

                                                strcpy(new_path, pcempath);
                                                put_backslash(new_path);
                                                strcat(new_path, "configs/");
                                                strcat(new_path, name);
                                                strcat(new_path, ".cfg");

                                                pclog("Rename %s to %s\n", old_path, new_path);

                                                rename(old_path, new_path);

                                                config_list_update(hdlg);
                                        }
                                }

                                return TRUE;
                        }
                        else if (wParam == WX_ID("IDC_DELETE"))
                        {
                                char name[64];
                                void* h;
                                int c;
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_LIST"));
                                c = wx_sendmessage(h, WX_LB_GETCURSEL, 0, 0);
                                if (c >= 0)
                                {
                                        char s[512];
                                        wx_sendmessage(h, WX_LB_GETTEXT, c, (LONG_PARAM)name);
                                        sprintf(s, "Do you want to delete \"%s\"?", name);
                                        if (wx_messagebox(NULL, s, "PCem", WX_MB_OKCANCEL) == WX_IDOK)
                                        {
                                                strcpy(s, pcempath);
                                                put_backslash(s);
                                                strcat(s, "configs/");
                                                strcat(s, name);
                                                strcat(s, ".cfg");

                                                remove(s);

                                                config_list_update(hdlg);
                                        }
                                }
                        }
                }
                break;
        }
        return FALSE;
}

int config_selection_open(void* hwnd, int inited)
{
        int ret;

        has_been_inited = inited;

        ret = wx_dialogbox(hwnd, "ConfigureSelectionDlg", config_selection_dlgproc);

        has_been_inited = 1;

        return ret;
}
