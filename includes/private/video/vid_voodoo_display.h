#ifndef _VID_VOODOO_DISPLAY_H_
#define _VID_VOODOO_DISPLAY_H_
void voodoo_update_ncc(voodoo_t *voodoo, int tmu);
void voodoo_pixelclock_update(voodoo_t *voodoo);
void voodoo_generate_filter_v1(voodoo_t *voodoo);
void voodoo_generate_filter_v2(voodoo_t *voodoo);
void voodoo_threshold_check(voodoo_t *voodoo);
void voodoo_callback(void *p);


#endif /* _VID_VOODOO_DISPLAY_H_ */
