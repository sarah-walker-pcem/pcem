#ifndef QT_JOYSTICKCONFIG_H_
#define QT_JOYSTICKCONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
void joystickconfig_open(void *hwnd, int joy_nr, int type);

#ifdef __cplusplus
}
#endif

#endif /* QT_JOYSTICKCONFIG_H_ */
