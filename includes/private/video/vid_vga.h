#ifndef _VID_VGA_H_
#define _VID_VGA_H_
extern device_t vga_device;
extern device_t ps1vga_device;

void vga_disable(void *p);
void vga_enable(void *p);

struct svga_t;
extern struct svga_t *mb_vga;


#endif /* _VID_VGA_H_ */
