#ifndef SRC_WX_DEVICECONFIG_H_
#define SRC_WX_DEVICECONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
void joystickconfig_open(void *hwnd, int joy_nr, int type);

#ifdef __cplusplus
}
#endif

#endif /* SRC_WX_DEVICECONFIG_H_ */
