#include <stdio.h>
#include <string.h>
#include "wx-utils.h"
#include "wx-display.h"
#include "wx-glslp-parser.h"
#include "wx-shaderconfig.h"
#include "config.h"

#define MAX_USER_SHADERS 20

extern char *get_filename(char *s);

extern char gl3_shader_file[MAX_USER_SHADERS][512];
static char shaders[MAX_USER_SHADERS][512];

void read_shader_config(glslp_t* shader)
{
        char s[512];
        int i;
        char* name = shader->name;
        sprintf(s, "GL3 Shaders - %s", name);
        for (i = 0; i < shader->num_parameters; ++i)
        {
                struct parameter* param = &shader->parameters[i];
                param->value = config_get_float(CFG_MACHINE, s, param->id, param->default_value);
        }
}

void write_shader_config(glslp_t* shader)
{
        char s[512];
        int i;
        char* name = shader->name;
        sprintf(s, "GL3 Shaders - %s", name);
        for (i = 0; i < shader->num_parameters; ++i)
        {
                struct parameter* param = &shader->parameters[i];
                config_set_float(CFG_MACHINE, s, param->id, param->value);
        }
}

static void add_shader(int p, const char* s)
{
        memmove(&shaders[p+1], &shaders[p], (MAX_USER_SHADERS-p-2)*512);
        strcpy(shaders[p], s);
}

static int shader_list_update(void* hdlg)
{
        int c;
        void* h;
        int num = 0;

        h = wx_getdlgitem(hdlg, WX_ID("IDC_LIST"));
        wx_sendmessage(h, WX_LB_RESETCONTENT, 0, 0);

        for (c = 0; c < MAX_USER_SHADERS; c++)
        {
                if (strlen(shaders[c]))
                {
                        char* p = get_filename(shaders[c]);
                        wx_sendmessage(h, WX_LB_INSERTSTRING, c, (LONG_PARAM)p);
                        ++num;
                }
        }
        return num;
}

static void set_shaders()
{
        int i, j, available;
        char s[128];
        char name[64];
        for (i = 0; i < MAX_USER_SHADERS; ++i)
        {
                if (strlen(gl3_shader_file[i]))
                {
                        available = 0;
                        for (j = 0; j < MAX_USER_SHADERS; ++j)
                        {
                                if (!strcmp(gl3_shader_file[i], shaders[j]))
                                {
                                        available = 1;
                                        continue;
                                }
                        }
                        if (!available)
                        {
                                get_glslp_name(gl3_shader_file[i], name, sizeof(name));
                                sprintf(s, "GL3 Shaders - %s", name);
                                config_free_section(CFG_MACHINE, s);
                        }
                }
        }
        memcpy(&gl3_shader_file, &shaders, sizeof(gl3_shader_file));

}

static int shader_manager_dlgproc(void* hdlg, int message, INT_PARAM wParam, LONG_PARAM lParam)
{
        char s[512];
        switch (message)
        {
                case WX_INITDIALOG:
                {
                        void* h = wx_getdlgitem(hdlg, WX_ID("IDC_LIST"));
                        memcpy(&shaders, &gl3_shader_file, sizeof(gl3_shader_file));
                        int len = shader_list_update(hdlg);
                        if (len)
                                wx_sendmessage(h, WX_LB_SETCURSEL, len-1, 0);
                        return TRUE;
                }
                case WX_COMMAND:
                {
                        if (wParam == wxID_OK)
                        {
                                set_shaders();
                                wx_enddialog(hdlg, 1);
                                return TRUE;
                        }
                        else if (wParam == wxID_CANCEL)
                        {
                                wx_enddialog(hdlg, 0);
                                return TRUE;
                        }
                        else if (wParam == WX_ID("IDC_APPLY"))
                        {
                                set_shaders();
                                renderer_doreset = 1;
                                return TRUE;
                        }
                        else if (wParam == WX_ID("IDC_ADD"))
                        {
                                void* h = wx_getdlgitem(hdlg, WX_ID("IDC_LIST"));
                                int c = wx_sendmessage(h, WX_LB_GETCURSEL, 0, 0);
                                int count = wx_sendmessage(h, WX_LB_GETCOUNT, 0, 0);
                                if (count >= MAX_USER_SHADERS)
                                {
                                        wx_simple_messagebox("Error", "Can't add more shaders.");
                                        return FALSE;
                                }

                                int ret = !wx_filedialog(hdlg, "Open", shaders[c], "GLSL Shaders (*.glslp;*.glsl)|*.glslp;*.glsl|All files (*.*)|*.*", 0, 1, s);

                                if (ret)
                                {
                                        add_shader(c+1, s);
                                        shader_list_update(hdlg);
                                        wx_sendmessage(h, WX_LB_SETCURSEL, c+1, 0);
                                }

                                return TRUE;
                        }
                        else if (wParam == WX_ID("IDC_CONFIG"))
                        {
                                void* h = wx_getdlgitem(hdlg, WX_ID("IDC_LIST"));
                                int c = wx_sendmessage(h, WX_LB_GETCURSEL, 0, 0);
                                if (c >= 0)
                                {
                                        glslp_t* glsl = glslp_parse(shaders[c]);
                                        if (glsl)
                                        {
                                                read_shader_config(glsl);
                                                shaderconfig_open(hdlg, glsl);
                                                write_shader_config(glsl);
                                                glslp_free(glsl);
                                                return TRUE;
                                        }
                                }
                                return FALSE;
                        }
                        else if (wParam == WX_ID("IDC_REMOVE"))
                        {
                                void* h;
                                int c;
                                h = wx_getdlgitem(hdlg, WX_ID("IDC_LIST"));
                                c = wx_sendmessage(h, WX_LB_GETCURSEL, 0, 0);
                                if (c >= 0)
                                {
                                        memmove(&shaders[c], &shaders[c+1], (MAX_USER_SHADERS-c-1)*512);
                                        int len = shader_list_update(hdlg);
                                        if (len > 0)
                                        {
                                                if (c > 0)
                                                        c--;
                                                wx_sendmessage(h, WX_LB_SETCURSEL, c, 0);
                                        }
                                }
                        }
                        else if (wParam == WX_ID("IDC_MOVE_UP"))
                        {
                                void* h = wx_getdlgitem(hdlg, WX_ID("IDC_LIST"));
                                int c = wx_sendmessage(h, WX_LB_GETCURSEL, 0, 0);
                                if (c > 0)
                                {
                                        strcpy(s, shaders[c-1]);
                                        strcpy(shaders[c-1], shaders[c]);
                                        strcpy(shaders[c], s);
                                        shader_list_update(hdlg);
                                        wx_sendmessage(h, WX_LB_SETCURSEL, c-1, 0);
                                }
                        }
                        else if (wParam == WX_ID("IDC_MOVE_DOWN"))
                        {
                                void* h = wx_getdlgitem(hdlg, WX_ID("IDC_LIST"));
                                int c = wx_sendmessage(h, WX_LB_GETCURSEL, 0, 0);
                                if (c < MAX_USER_SHADERS-1)
                                {
                                        if (strlen(shaders[c+1]))
                                        {
                                                strcpy(s, shaders[c+1]);
                                                strcpy(shaders[c+1], shaders[c]);
                                                strcpy(shaders[c], s);
                                                shader_list_update(hdlg);
                                                wx_sendmessage(h, WX_LB_SETCURSEL, c+1, 0);
                                        }
                                }
                        }
                }
                break;
        }
        return FALSE;
}

int shader_manager_open(void* hwnd)
{
        return wx_dialogbox(hwnd, "ShaderManagerDlg", shader_manager_dlgproc);
}
