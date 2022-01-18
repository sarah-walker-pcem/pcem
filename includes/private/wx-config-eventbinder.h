#ifndef __WX_CONFIG_EVENTBINDER_H
#define __WX_CONFIG_EVENTBINDER_H

#ifdef __cplusplus
extern "C" {
#endif
void *wx_config_eventbinder(void *hdlg, void (*selectedPageCallback)(void *hdlg, int selectedPage));
void wx_config_destroyeventbinder(void *eventBinder);
#ifdef __cplusplus
}
#endif

#endif /* __WX_CONFIG_EVENTBINDER_H */