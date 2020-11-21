#include <math.h>
#include <stddef.h>
#include "ibm.h"
#include "device.h"
#include "mem.h"
#include "thread.h"
#include "video.h"
#include "vid_svga.h"
#include "vid_voodoo.h"
#include "vid_voodoo_common.h"
#include "vid_voodoo_banshee_blitter.h"
#include "vid_voodoo_fb.h"
#include "vid_voodoo_fifo.h"
#include "vid_voodoo_reg.h"
#include "vid_voodoo_regs.h"
#include "vid_voodoo_render.h"
#include "vid_voodoo_texture.h"

#define WAKE_DELAY (TIMER_USEC * 100)
void voodoo_wake_fifo_thread(voodoo_t *voodoo)
{
        if (!timer_is_enabled(&voodoo->wake_timer))
        {
                /*Don't wake FIFO thread immediately - if we do that it will probably
                  process one word and go back to sleep, requiring it to be woken on
                  almost every write. Instead, wait a short while so that the CPU
                  emulation writes more data so we have more batched-up work.*/
		timer_set_delay_u64(&voodoo->wake_timer, WAKE_DELAY);
        }
}

void voodoo_wake_fifo_thread_now(voodoo_t *voodoo)
{
        thread_set_event(voodoo->wake_fifo_thread); /*Wake up FIFO thread if moving from idle*/
}

void voodoo_wake_timer(void *p)
{
        voodoo_t *voodoo = (voodoo_t *)p;

        thread_set_event(voodoo->wake_fifo_thread); /*Wake up FIFO thread if moving from idle*/
}

void voodoo_queue_command(voodoo_t *voodoo, uint32_t addr_type, uint32_t val)
{
        fifo_entry_t *fifo = &voodoo->fifo[voodoo->fifo_write_idx & FIFO_MASK];

        while (FIFO_FULL)
        {
                thread_reset_event(voodoo->fifo_not_full_event);
                if (FIFO_FULL)
                {
                        thread_wait_event(voodoo->fifo_not_full_event, 1); /*Wait for room in ringbuffer*/
                        if (FIFO_FULL)
                                voodoo_wake_fifo_thread_now(voodoo);
                }
        }

        fifo->val = val;
        fifo->addr_type = addr_type;

        voodoo->fifo_write_idx++;

        if (FIFO_ENTRIES > 0xe000)
                voodoo_wake_fifo_thread(voodoo);
}

void voodoo_flush(voodoo_t *voodoo)
{
        voodoo->flush = 1;
        while (!FIFO_EMPTY)
        {
                voodoo_wake_fifo_thread_now(voodoo);
                thread_wait_event(voodoo->fifo_not_full_event, 1);
        }
        voodoo_wait_for_render_thread_idle(voodoo);
        voodoo->flush = 0;
}

void voodoo_wake_fifo_threads(voodoo_set_t *set, voodoo_t *voodoo)
{
        voodoo_wake_fifo_thread(voodoo);
        if (SLI_ENABLED && voodoo->type != VOODOO_2 && set->voodoos[0] == voodoo)
                voodoo_wake_fifo_thread(set->voodoos[1]);
}

void voodoo_wait_for_swap_complete(voodoo_t *voodoo)
{
        while (voodoo->swap_pending)
        {
                thread_wait_event(voodoo->wake_fifo_thread, -1);
                thread_reset_event(voodoo->wake_fifo_thread);

                thread_lock_mutex(voodoo->swap_mutex);
                if ((voodoo->swap_pending && voodoo->flush) || FIFO_FULL)
                {
                        /*Main thread is waiting for FIFO to empty, so skip vsync wait and just swap*/
                        memset(voodoo->dirty_line, 1, sizeof(voodoo->dirty_line));
                        voodoo->front_offset = voodoo->params.front_offset;
                        if (voodoo->swap_count > 0)
                                voodoo->swap_count--;
                        voodoo->swap_pending = 0;
                        thread_unlock_mutex(voodoo->swap_mutex);
                        break;
                }
                else
                        thread_unlock_mutex(voodoo->swap_mutex);
        }
}


static uint32_t cmdfifo_get(voodoo_t *voodoo)
{
        uint32_t val;

        if (!voodoo->cmdfifo_in_sub)
        {
                while (voodoo->cmdfifo_depth_rd == voodoo->cmdfifo_depth_wr)
                {
                        thread_wait_event(voodoo->wake_fifo_thread, -1);
                        thread_reset_event(voodoo->wake_fifo_thread);
                }
        }

        val = *(uint32_t *)&voodoo->fb_mem[voodoo->cmdfifo_rp & voodoo->fb_mask];

        if (!voodoo->cmdfifo_in_sub)
                voodoo->cmdfifo_depth_rd++;
        voodoo->cmdfifo_rp += 4;

//        pclog("  CMDFIFO get %08x\n", val);
        return val;
}

static inline float cmdfifo_get_f(voodoo_t *voodoo)
{
        union
        {
                uint32_t i;
                float f;
        } tempif;

        tempif.i = cmdfifo_get(voodoo);
        return tempif.f;
}

enum
{
        CMDFIFO3_PC_MASK_RGB   = (1 << 10),
        CMDFIFO3_PC_MASK_ALPHA = (1 << 11),
        CMDFIFO3_PC_MASK_Z     = (1 << 12),
        CMDFIFO3_PC_MASK_Wb    = (1 << 13),
        CMDFIFO3_PC_MASK_W0    = (1 << 14),
        CMDFIFO3_PC_MASK_S0_T0 = (1 << 15),
        CMDFIFO3_PC_MASK_W1    = (1 << 16),
        CMDFIFO3_PC_MASK_S1_T1 = (1 << 17),

        CMDFIFO3_PC = (1 << 28)
};

void voodoo_fifo_thread(void *param)
{
        voodoo_t *voodoo = (voodoo_t *)param;

        while (1)
        {
                thread_set_event(voodoo->fifo_not_full_event);
                thread_wait_event(voodoo->wake_fifo_thread, -1);
                thread_reset_event(voodoo->wake_fifo_thread);
                voodoo->voodoo_busy = 1;
                while (!FIFO_EMPTY)
                {
                        uint64_t start_time = timer_read();
                        uint64_t end_time;
                        fifo_entry_t *fifo = &voodoo->fifo[voodoo->fifo_read_idx & FIFO_MASK];

                        switch (fifo->addr_type & FIFO_TYPE)
                        {
                                case FIFO_WRITEL_REG:
                                while ((fifo->addr_type & FIFO_TYPE) == FIFO_WRITEL_REG)
                                {
                                        voodoo_reg_writel(fifo->addr_type & FIFO_ADDR, fifo->val, voodoo);
                                        fifo->addr_type = FIFO_INVALID;
                                        voodoo->fifo_read_idx++;
                                        if (FIFO_EMPTY)
                                                break;
                                        fifo = &voodoo->fifo[voodoo->fifo_read_idx & FIFO_MASK];
                                }
                                break;
                                case FIFO_WRITEW_FB:
                                voodoo_wait_for_render_thread_idle(voodoo);
                                while ((fifo->addr_type & FIFO_TYPE) == FIFO_WRITEW_FB)
                                {
                                        voodoo_fb_writew(fifo->addr_type & FIFO_ADDR, fifo->val, voodoo);
                                        fifo->addr_type = FIFO_INVALID;
                                        voodoo->fifo_read_idx++;
                                        if (FIFO_EMPTY)
                                                break;
                                        fifo = &voodoo->fifo[voodoo->fifo_read_idx & FIFO_MASK];
                                }
                                break;
                                case FIFO_WRITEL_FB:
                                voodoo_wait_for_render_thread_idle(voodoo);
                                while ((fifo->addr_type & FIFO_TYPE) == FIFO_WRITEL_FB)
                                {
                                        voodoo_fb_writel(fifo->addr_type & FIFO_ADDR, fifo->val, voodoo);
                                        fifo->addr_type = FIFO_INVALID;
                                        voodoo->fifo_read_idx++;
                                        if (FIFO_EMPTY)
                                                break;
                                        fifo = &voodoo->fifo[voodoo->fifo_read_idx & FIFO_MASK];
                                }
                                break;
                                case FIFO_WRITEL_TEX:
                                while ((fifo->addr_type & FIFO_TYPE) == FIFO_WRITEL_TEX)
                                {
                                        if (!(fifo->addr_type & 0x400000))
                                                voodoo_tex_writel(fifo->addr_type & FIFO_ADDR, fifo->val, voodoo);
                                        fifo->addr_type = FIFO_INVALID;
                                        voodoo->fifo_read_idx++;
                                        if (FIFO_EMPTY)
                                                break;
                                        fifo = &voodoo->fifo[voodoo->fifo_read_idx & FIFO_MASK];
                                }
                                break;
                                case FIFO_WRITEL_2DREG:
                                while ((fifo->addr_type & FIFO_TYPE) == FIFO_WRITEL_2DREG)
                                {
                                        voodoo_2d_reg_writel(voodoo, fifo->addr_type & FIFO_ADDR, fifo->val);
                                        fifo->addr_type = FIFO_INVALID;
                                        voodoo->fifo_read_idx++;
                                        if (FIFO_EMPTY)
                                                break;
                                        fifo = &voodoo->fifo[voodoo->fifo_read_idx & FIFO_MASK];
                                }
                                break;

                                default:
                                fatal("Unknown fifo entry %08x\n", fifo->addr_type);
                        }

                        if (FIFO_ENTRIES > 0xe000)
                                thread_set_event(voodoo->fifo_not_full_event);

                        end_time = timer_read();
                        voodoo->time += end_time - start_time;
                }

                while (voodoo->cmdfifo_enabled && (voodoo->cmdfifo_depth_rd != voodoo->cmdfifo_depth_wr || voodoo->cmdfifo_in_sub))
                {
                        uint64_t start_time = timer_read();
                        uint64_t end_time;
                        uint32_t header = cmdfifo_get(voodoo);
                        uint32_t addr;
                        uint32_t mask;
                        int smode;
                        int num;
                        int num_verticies;
                        int v_num;

//                        pclog(" CMDFIFO header %08x at %08x\n", header, voodoo->cmdfifo_rp);

                        switch (header & 7)
                        {
                                case 0:
//                                pclog("CMDFIFO0\n");
                                switch ((header >> 3) & 7)
                                {
                                        case 0: /*NOP*/
                                        break;

                                        case 1: /*JSR*/
//                                        pclog("JSR %08x\n", (header >> 4) & 0xfffffc);
                                        voodoo->cmdfifo_ret_addr = voodoo->cmdfifo_rp;
                                        voodoo->cmdfifo_rp = (header >> 4) & 0xfffffc;
                                        voodoo->cmdfifo_in_sub = 1;
                                        break;

                                        case 2: /*RET*/
                                        voodoo->cmdfifo_rp = voodoo->cmdfifo_ret_addr;
                                        voodoo->cmdfifo_in_sub = 0;
                                        break;

                                        case 3: /*JMP local frame buffer*/
                                        voodoo->cmdfifo_rp = (header >> 4) & 0xfffffc;
//                                        pclog("JMP to %08x %04x\n", voodoo->cmdfifo_rp, header);
                                        break;

                                        default:
                                        fatal("Bad CMDFIFO0 %08x\n", header);
                                }
                                break;

                                case 1:
                                num = header >> 16;
                                addr = (header & 0x7ff8) >> 1;
//                                pclog("CMDFIFO1 addr=%08x\n",addr);
                                while (num--)
                                {
                                        uint32_t val = cmdfifo_get(voodoo);
                                        if ((addr & (1 << 13)) && voodoo->type >= VOODOO_BANSHEE)
                                        {
//                                                if (voodoo->type != VOODOO_BANSHEE)
//                                                        fatal("CMDFIFO1: Not Banshee\n");
//                                                pclog("CMDFIFO1: write %08x %08x\n", addr, val);
                                                voodoo_2d_reg_writel(voodoo, addr, val);
                                        }
                                        else
                                        {
                                                if ((addr & 0x3ff) == SST_triangleCMD || (addr & 0x3ff) == SST_ftriangleCMD ||
                                                    (addr & 0x3ff) == SST_fastfillCMD || (addr & 0x3ff) == SST_nopCMD)
                                                        voodoo->cmd_written_fifo++;

                                                if (voodoo->type >= VOODOO_BANSHEE && (addr & 0x3ff) == SST_swapbufferCMD)
                                                        voodoo->cmd_written_fifo++;
                                                voodoo_reg_writel(addr, val, voodoo);
                                        }

                                        if (header & (1 << 15))
                                                addr += 4;
                                }
                                break;

                                case 2:
                                if (voodoo->type < VOODOO_BANSHEE)
                                        fatal("CMDFIFO2: Not Banshee\n");
                                mask = (header >> 3);
                                addr = 8;
                                while (mask)
                                {
                                        if (mask & 1)
                                        {
                                                uint32_t val = cmdfifo_get(voodoo);

                                                voodoo_2d_reg_writel(voodoo, addr, val);
                                        }

                                        addr += 4;
                                        mask >>= 1;
                                }
                                break;
                                
                                case 3:
                                num = (header >> 29) & 7;
                                mask = header;//(header >> 10) & 0xff;
                                smode = (header >> 22) & 0xf;
                                voodoo_reg_writel(SST_sSetupMode, ((header >> 10) & 0xff) | (smode << 16), voodoo);
                                num_verticies = (header >> 6) & 0xf;
                                v_num = 0;
                                if (((header >> 3) & 7) == 2)
                                        v_num = 1;
//                                pclog("CMDFIFO3: num=%i verts=%i mask=%02x\n", num, num_verticies, (header >> 10) & 0xff);
//                                pclog("CMDFIFO3 %02x %i\n", (header >> 10), (header >> 3) & 7);

                                while (num_verticies--)
                                {
                                        voodoo->verts[3].sVx = cmdfifo_get_f(voodoo);
                                        voodoo->verts[3].sVy = cmdfifo_get_f(voodoo);
                                        if (mask & CMDFIFO3_PC_MASK_RGB)
                                        {
                                                if (header & CMDFIFO3_PC)
                                                {
                                                        uint32_t val = cmdfifo_get(voodoo);
                                                        voodoo->verts[3].sBlue  = (float)(val & 0xff);
                                                        voodoo->verts[3].sGreen = (float)((val >> 8) & 0xff);
                                                        voodoo->verts[3].sRed   = (float)((val >> 16) & 0xff);
                                                        voodoo->verts[3].sAlpha = (float)((val >> 24) & 0xff);
                                                }
                                                else
                                                {
                                                        voodoo->verts[3].sRed = cmdfifo_get_f(voodoo);
                                                        voodoo->verts[3].sGreen = cmdfifo_get_f(voodoo);
                                                        voodoo->verts[3].sBlue = cmdfifo_get_f(voodoo);
                                                }
                                        }
                                        if ((mask & CMDFIFO3_PC_MASK_ALPHA) && !(header & CMDFIFO3_PC))
                                                voodoo->verts[3].sAlpha = cmdfifo_get_f(voodoo);
                                        if (mask & CMDFIFO3_PC_MASK_Z)
                                                voodoo->verts[3].sVz = cmdfifo_get_f(voodoo);
                                        if (mask & CMDFIFO3_PC_MASK_Wb)
                                                voodoo->verts[3].sWb = cmdfifo_get_f(voodoo);
                                        if (mask & CMDFIFO3_PC_MASK_W0)
                                                voodoo->verts[3].sW0 = cmdfifo_get_f(voodoo);
                                        if (mask & CMDFIFO3_PC_MASK_S0_T0)
                                        {
                                                voodoo->verts[3].sS0 = cmdfifo_get_f(voodoo);
                                                voodoo->verts[3].sT0 = cmdfifo_get_f(voodoo);
                                        }
                                        if (mask & CMDFIFO3_PC_MASK_W1)
                                                voodoo->verts[3].sW1 = cmdfifo_get_f(voodoo);
                                        if (mask & CMDFIFO3_PC_MASK_S1_T1)
                                        {
                                                voodoo->verts[3].sS1 = cmdfifo_get_f(voodoo);
                                                voodoo->verts[3].sT1 = cmdfifo_get_f(voodoo);
                                        }
                                        if (v_num)
                                                voodoo_reg_writel(SST_sDrawTriCMD, 0, voodoo);
                                        else
                                                voodoo_reg_writel(SST_sBeginTriCMD, 0, voodoo);
                                        v_num++;
                                        if (v_num == 3 && ((header >> 3) & 7) == 0)
                                                v_num = 0;
                                }
                                break;

                                case 4:
                                num = (header >> 29) & 7;
                                mask = (header >> 15) & 0x3fff;
                                addr = (header & 0x7ff8) >> 1;
//                                pclog("CMDFIFO4 addr=%08x\n",addr);
                                while (mask)
                                {
                                        if (mask & 1)
                                        {
                                                uint32_t val = cmdfifo_get(voodoo);

                                                if ((addr & (1 << 13)) && voodoo->type >= VOODOO_BANSHEE)
                                                {
                                                        if (voodoo->type < VOODOO_BANSHEE)
                                                                fatal("CMDFIFO1: Not Banshee\n");
//                                                pclog("CMDFIFO1: write %08x %08x\n", addr, val);
                                                        voodoo_2d_reg_writel(voodoo, addr, val);
                                                }
                                                else
                                                {
                                                        if ((addr & 0x3ff) == SST_triangleCMD || (addr & 0x3ff) == SST_ftriangleCMD ||
                                                            (addr & 0x3ff) == SST_fastfillCMD || (addr & 0x3ff) == SST_nopCMD)
                                                                voodoo->cmd_written_fifo++;

                                                        if (voodoo->type >= VOODOO_BANSHEE && (addr & 0x3ff) == SST_swapbufferCMD)
                                                                voodoo->cmd_written_fifo++;
                                                        voodoo_reg_writel(addr, val, voodoo);
                                                }
                                        }

                                        addr += 4;
                                        mask >>= 1;
                                }
                                while (num--)
                                        cmdfifo_get(voodoo);
                                break;

                                case 5:
//                                if (header & 0x3fc00000)
//                                        fatal("CMDFIFO packet 5 has byte disables set %08x\n", header);
                                num = (header >> 3) & 0x7ffff;
                                addr = cmdfifo_get(voodoo) & 0xffffff;
                                if (!num)
                                        num = 1;
//                                pclog("CMDFIFO5 addr=%08x num=%i\n", addr, num);
                                switch (header >> 30)
                                {
                                        case 0: /*Linear framebuffer (Banshee)*/
                                        if (voodoo->texture_present[0][(addr & voodoo->texture_mask) >> TEX_DIRTY_SHIFT])
                                        {
//                                                pclog("texture_present at %08x %i\n", addr, (addr & voodoo->texture_mask) >> TEX_DIRTY_SHIFT);
                                                flush_texture_cache(voodoo, addr & voodoo->texture_mask, 0);
                                        }
                                        if (voodoo->texture_present[1][(addr & voodoo->texture_mask) >> TEX_DIRTY_SHIFT])
                                        {
//                                                pclog("texture_present at %08x %i\n", addr, (addr & voodoo->texture_mask) >> TEX_DIRTY_SHIFT);
                                                flush_texture_cache(voodoo, addr & voodoo->texture_mask, 1);
                                        }
                                        while (num--)
                                        {
                                                uint32_t val = cmdfifo_get(voodoo);
                                                if (addr <= voodoo->fb_mask)
                                                        *(uint32_t *)&voodoo->fb_mem[addr] = val;
                                                addr += 4;
                                        }
                                        break;
                                        case 2: /*Framebuffer*/
                                        while (num--)
                                        {
                                                uint32_t val = cmdfifo_get(voodoo);
                                                voodoo_fb_writel(addr, val, voodoo);
                                                addr += 4;
                                        }
                                        break;
                                        case 3: /*Texture*/
                                        while (num--)
                                        {
                                                uint32_t val = cmdfifo_get(voodoo);
                                                voodoo_tex_writel(addr, val, voodoo);
                                                addr += 4;
                                        }
                                        break;

                                        default:
                                        fatal("CMDFIFO packet 5 bad space %08x %08x\n", header, voodoo->cmdfifo_rp);
                                }
                                break;

                                default:
                                fatal("Bad CMDFIFO packet %08x %08x\n", header, voodoo->cmdfifo_rp);
                        }

                        end_time = timer_read();
                        voodoo->time += end_time - start_time;
                }
                voodoo->voodoo_busy = 0;
        }
}
