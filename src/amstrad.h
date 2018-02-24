#include "mouse.h"

void amstrad_init();

extern mouse_t mouse_amstrad;
extern int amstrad_latch;

enum
{
        AMSTRAD_NOLATCH,
        AMSTRAD_SW9,
        AMSTRAD_SW10
};
