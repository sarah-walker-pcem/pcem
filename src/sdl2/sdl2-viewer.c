#include <stddef.h>
#include "viewer.h"
#include "viewer_voodoo.h"

viewer_t viewer_font = {NULL};
viewer_t viewer_palette = {NULL};
viewer_t viewer_palette_16 = {NULL};
viewer_t viewer_voodoo = {NULL};
viewer_t viewer_vram = {NULL};

void viewer_reset() {}
void viewer_add(char *title, viewer_t *viewer, void *p) {}
void viewer_open(void *hwnd, int id) {}
void viewer_remove(void *viewer) {}
void viewer_update(viewer_t *viewer, void *p) {}
void viewer_call(viewer_t *viewer, void *p, void (*func)(void *v, void *param), void *param) {}
void viewer_close_all() {}
void viewer_notify_pause() {}
void viewer_notify_resume() {}
void update_viewers_menu(void *menu) {}

void voodoo_viewer_swap_buffer(void *v, void *param) {}
void voodoo_viewer_queue_triangle(void *v, void *param) {}
void voodoo_viewer_begin_strip(void *v, void *param) {}
void voodoo_viewer_end_strip(void *v, void *param) {}
void voodoo_viewer_use_texture(void *v, void *param) {}
