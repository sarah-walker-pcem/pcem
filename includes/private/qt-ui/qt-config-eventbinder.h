#ifndef QT_CONFIG_EVENTBINDER_H_
#define QT_CONFIG_EVENTBINDER_H_

#ifdef __cplusplus
extern "C" {
#endif
void *wx_config_eventbinder(void *hdlg, void (*selectedPageCallback)(void *hdlg, int selectedPage));
void wx_config_destroyeventbinder(void *eventBinder);
#ifdef __cplusplus
}
#endif

#endif /* QT_CONFIG_EVENTBINDER_H_ */
