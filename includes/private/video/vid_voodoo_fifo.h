#ifndef _VID_VOODOO_FIFO_H_
#define _VID_VOODOO_FIFO_H_
void voodoo_wake_fifo_thread(voodoo_t *voodoo);
void voodoo_wake_fifo_thread_now(voodoo_t *voodoo);
void voodoo_wake_timer(void *p);
void voodoo_queue_command(voodoo_t *voodoo, uint32_t addr_type, uint32_t val);
void voodoo_flush(voodoo_t *voodoo);
void voodoo_wake_fifo_threads(voodoo_set_t *set, voodoo_t *voodoo);
void voodoo_wait_for_swap_complete(voodoo_t *voodoo);
void voodoo_fifo_thread(void *param);


#endif /* _VID_VOODOO_FIFO_H_ */
