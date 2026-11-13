#ifndef _WX_FRONTEND_H_
#define _WX_FRONTEND_H_

#ifdef __cplusplus
extern "C" {
#endif

void wx_frontend_init(void);
int wx_load_config(void *hwnd);
int wx_start(void *hwnd);
int wx_stop(void *window);
int wx_setupmenu(void *data);
void wx_initmenu();
int wx_handle_command(void *hwnd, int wParam, int checked);

#ifdef __cplusplus
}
#endif

#endif /* _WX_FRONTEND_H_ */
