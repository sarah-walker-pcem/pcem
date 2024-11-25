#ifndef _VIEWER_VOODOO_H_
#define _VIEWER_VOODOO_H_

#ifdef __cplusplus
extern "C" {
#endif

void voodoo_viewer_swap_buffer(void *v, void *param);
void voodoo_viewer_queue_triangle(void *v, void *param);
void voodoo_viewer_begin_strip(void *v, void *param);
void voodoo_viewer_end_strip(void *v, void *param);
void voodoo_viewer_use_texture(void *v, void *param);

#ifdef __cplusplus
}
#endif

#endif