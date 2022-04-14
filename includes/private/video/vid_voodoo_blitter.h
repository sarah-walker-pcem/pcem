#ifndef _VID_VOODOO_BLITTER_H_
#define _VID_VOODOO_BLITTER_H_
void voodoo_v2_blit_start(voodoo_t *voodoo);
void voodoo_v2_blit_data(voodoo_t *voodoo, uint32_t data);
void voodoo_fastfill(voodoo_t *voodoo, voodoo_params_t *params);


#endif /* _VID_VOODOO_BLITTER_H_ */
