#include <SDL2/SDL.h>
#include "video.h"
#include "wx-sdl2.h"
#include "wx-sdl2-video.h"
#include "ibm.h"
#include "device.h"
#include "x86_ops.h"
#include "mem.h"
#include "codegen.h"
#include "cpu.h"
#include "model.h"
#include "fdd.h"
#include "hdd.h"
#include "ide.h"
#include "cdrom-image.h"
#include "scsi_zip.h"
#include "codegen_allocator.h"
#include "wx-common.h"

drive_info_t drive_info[10];

int status_is_open = 0;

extern int sreadlnum, swritelnum, segareads, segawrites, scycles_lost;
extern int render_fps, fps;

extern uint64_t main_time;
extern uint64_t render_time;
static uint64_t status_time;

drive_info_t* get_machine_info(char* s, int* num_drive_info) {
        int drive, i;
        int pos = 0;
        sprintf(s,
                        "Model: %s\n"
                        "CPU: %s",
                        model_getname(),
                        models[model].cpu[cpu_manufacturer].cpus[cpu].name);
        if (emulation_state == EMULATION_RUNNING)
                sprintf(s+strlen(s), "\nEmulation speed: %d%%", fps);
        else if (emulation_state == EMULATION_PAUSED)
                strcat(s, "\nEmulation is paused.");
        else
                strcat(s, "\nEmulation is not running.");

        for (i = 0; i < 2; ++i)
        {
                drive = 'A'+i;
                if (fdd_get_type(i) > 0)
                {
                        strcpy(drive_info[pos].fn, discfns[i]);
                        drive_info[pos].enabled = strlen(drive_info[pos].fn) > 0;
                        drive_info[pos].drive = i;
                        drive_info[pos].drive_letter = drive;
                        drive_info[pos].type = DRIVE_TYPE_FDD;
                        drive_info[pos].readflash = readflash_get(READFLASH_FDC, i);
                        readflash_clear(READFLASH_FDC, i);
                        pos++;
                }
        }
        int num_hdds = hdd_controller_current_is_mfm() ? 2 : (hdd_controller_current_is_scsi() ? 7 : 4);
        for (i = 0; i < num_hdds; ++i)
        {
                drive = 'C'+i;
                if (!hdd_controller_current_is_mfm() && i == cdrom_channel)
                {
                        if (cdrom_drive < 0)
                                continue;
                        if (cdrom_drive == CDROM_IMAGE)
                        {
                                if (!strlen(image_path))
                                        continue;
                                strcpy(drive_info[pos].fn, image_path);
                        }
                        else
                                strcpy(drive_info[pos].fn, "");
                        drive_info[pos].enabled = strlen(drive_info[pos].fn) > 0;
                        drive_info[pos].drive = i;
                        drive_info[pos].drive_letter = drive;
                        drive_info[pos].type = DRIVE_TYPE_CDROM;
                        drive_info[pos].readflash = readflash_get(READFLASH_HDC, i);
                        readflash_clear(READFLASH_HDC, i);
                        pos++;
                }
                else if (!hdd_controller_current_is_mfm() && i == zip_channel)
                {
                        strcpy(drive_info[pos].fn, "");
                        drive_info[pos].enabled = zip_loaded();
                        drive_info[pos].drive = i;
                        drive_info[pos].drive_letter = drive;
                        drive_info[pos].type = DRIVE_TYPE_FDD;
                        drive_info[pos].readflash = readflash_get(READFLASH_HDC, i);
                        readflash_clear(READFLASH_HDC, i);
                        pos++;
                }
                else if (strlen(ide_fn[i]) > 0)
                {
                        strcpy(drive_info[pos].fn, ide_fn[i]);
                        drive_info[pos].enabled = 1;
                        drive_info[pos].drive = i;
                        drive_info[pos].drive_letter = drive;
                        drive_info[pos].type = DRIVE_TYPE_HDD;
                        drive_info[pos].readflash = readflash_get(READFLASH_HDC, i);
                        readflash_clear(READFLASH_HDC, i);
                        pos++;
                }
        }
        *num_drive_info = pos;
        return drive_info;
}

int get_status(char* machine, char* device)
{
//        if (!status_is_open) {
//                return 0;
//        }
//        char device_s[4096];
        uint64_t new_time = timer_read();
        uint64_t status_diff = new_time - status_time;
        status_time = new_time;
        sprintf(machine,
                "CPU speed : %f MIPS\n"
                "FPU speed : %f MFLOPS\n\n"

        /*                        "Cache misses (read) : %i/sec\n"
                "Cache misses (write) : %i/sec\n\n"*/

                "Video throughput (read) : %i bytes/sec\n"
                "Video throughput (write) : %i bytes/sec\n\n"
                "Effective clockspeed : %iHz\n\n"
                "Timer 0 frequency : %fHz\n\n"
                "CPU time : %f%% (%f%%)\n"
                "Render time : %f%% (%f%%)\n"
                "Renderer: %s\n"
                "Render FPS: %d\n"
                "\n"

                "New blocks : %i\nOld blocks : %i\nRecompiled speed : %f MIPS\nAverage size : %f\n"
                "Flushes : %i\nEvicted : %i\nReused : %i\nRemoved : %i\nReal speed : %f MIPS\nMem blocks used : %i (%g MB)"
        //                        "\nFully recompiled ins %% : %f%%"
                ,mips,
                flops,
        /*#ifndef DYNAREC
                sreadlnum,
                swritelnum,
        #endif*/
                segareads,
                segawrites,
                cpu_get_speed() - scycles_lost,
                pit_timer0_freq(),
                ((double)main_time * 100.0) / status_diff,
                ((double)main_time * 100.0) / timer_freq,
                ((double)render_time * 100.0) / status_diff,
                ((double)render_time * 100.0) / timer_freq,
                current_render_driver_name,
                render_fps

                , cpu_new_blocks_latched, cpu_recomp_blocks_latched, (double)cpu_recomp_ins_latched / 1000000.0, (double)cpu_recomp_ins_latched/cpu_recomp_blocks_latched,
                cpu_recomp_flushes_latched, cpu_recomp_evicted_latched,
                cpu_recomp_reuse_latched, cpu_recomp_removed_latched,

                ((double)cpu_recomp_ins_latched / 1000000.0) / ((double)main_time / timer_freq),
                codegen_allocator_usage,
                (double)(codegen_allocator_usage * MEM_BLOCK_SIZE) / (1024.0*1024.0)
        //                        ((double)cpu_recomp_full_ins_latched / (double)cpu_recomp_ins_latched) * 100.0
        //                        cpu_reps_latched, cpu_notreps_latched
        );
        main_time = 0;
        render_time = 0;
        /*#ifndef DYNAREC
        device_add_status_info(device_s, 4096);
        #endif*/

//        device_s[0] = 0;
        device[0] = 0;
        device_add_status_info(device, 4096);

        return 1;
}
