#ifndef _MODEL_H_
#define _MODEL_H_

#include <pcem/devices.h>

#define MODEL_AT      1
#define MODEL_PS2     2
#define MODEL_AMSTRAD 4
#define MODEL_OLIM24  8
#define MODEL_HAS_IDE 0x10
#define MODEL_MCA     0x20
#define MODEL_PCI     0x40

/*Machine has no integrated graphics*/
#define MODEL_GFX_NONE       0x000
/*Machine has integrated graphics that can not be disabled*/
#define MODEL_GFX_FIXED      0x100
/*Machine has integrated graphics that can be disabled by jumpers or switches*/
#define MODEL_GFX_DISABLE_HW 0x200
/*Machine has integrated graphics that can be disabled through software*/
#define MODEL_GFX_DISABLE_SW 0x300
#define MODEL_GFX_MASK       0x300

extern MODEL *models[ROM_MAX];

extern int model;

int model_count();
int model_getromset();
int model_getromset_from_model(int model);
int model_getmodel(int romset);
char *model_getname();
char *model_get_internal_name();
int model_get_model_from_internal_name(char *s);
void model_init();
void model_init_builtin();
int model_has_fixed_gfx(int model);
int model_has_optional_gfx(int model);


#endif /* _MODEL_H_ */
