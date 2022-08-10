#include <stdlib.h>
#include "ibm.h"
#include "device.h"
#include "mem.h"

#define FLASH_2MBIT 4
#define FLASH_IS_BXB 2
#define FLASH_INVERT 1

#define BLOCK_MAIN 0
#define BLOCK_DATA1 1
#define BLOCK_DATA2 2
#define BLOCK_BOOT 3
#define BLOCK_MAIN2 4 /*2Mbit devices only*/

enum {
        CMD_READ_ARRAY = 0xff,
        CMD_IID = 0x90,
        CMD_READ_STATUS = 0x70,
        CMD_CLEAR_STATUS = 0x50,
        CMD_ERASE_SETUP = 0x20,
        CMD_ERASE_CONFIRM = 0xd0,
        CMD_ERASE_SUSPEND = 0xb0,
        CMD_PROGRAM_SETUP = 0x40
};

typedef struct flash_t {
        uint8_t command, status;
        uint16_t flash_id;
        uint8_t type;
        int invert_high_pin;
        mem_mapping_t mapping[8], mapping_h[10];
        uint32_t block_start[5], block_end[5], block_len[5];
        uint8_t array[256 * 1024];
        uint32_t addr_mask;
} flash_t;

static char flash_path[1024];

static uint8_t flash_read(uint32_t addr, void *p) {
        flash_t *flash = (flash_t *)p;
        if (flash->invert_high_pin)
                addr ^= 0x10000;
        // pclog("flash_read : addr=%08x command=%02x %04x:%08x\n", addr, flash->command, CS, cpu_state.pc);
        addr &= flash->addr_mask;
        switch (flash->command) {
        case CMD_READ_ARRAY:
        default:
                return flash->array[addr];

        case CMD_IID:
                if (addr & 1)
                        return flash->flash_id;
                return 0x89;

        case CMD_READ_STATUS:
                return flash->status;
        }
}

static uint16_t flash_readw(uint32_t addr, void *p) {
        flash_t *flash = (flash_t *)p;
        addr &= flash->addr_mask;
        if (flash->invert_high_pin)
                addr ^= 0x10000;
        switch (flash->command) {
        case CMD_READ_ARRAY:
        default:
                return *(uint16_t *)&(flash->array[addr]);

        case CMD_IID:
                return 0x89 | (flash->flash_id << 8);

        case CMD_READ_STATUS:
                return flash->status;
        }
}

static uint32_t flash_readl(uint32_t addr, void *p) {
        flash_t *flash = (flash_t *)p;
        addr &= flash->addr_mask;
        if (flash->invert_high_pin)
                addr ^= 0x10000;
        return *(uint32_t *)&(flash->array[addr]);
}

static void flash_write(uint32_t addr, uint8_t val, void *p) {
        flash_t *flash = (flash_t *)p;
        int i;
        //        pclog("flash_write : addr=%08x val=%02x command=%02x %04x:%08x\n", addr, val, flash->command, CS, cpu_state.pc);

        if (flash->invert_high_pin)
                addr ^= 0x10000;
        addr &= flash->addr_mask;

        switch (flash->command) {
        case CMD_ERASE_SETUP:
                if (val == CMD_ERASE_CONFIRM) {
                        // pclog("flash_write: erase %05x\n", addr);

                        for (i = 0; i < 3; i++) {
                                if ((addr >= flash->block_start[i]) && (addr <= flash->block_end[i]))
                                        memset(&(flash->array[flash->block_start[i]]), 0xff, flash->block_len[i]);
                        }

                        flash->status = 0x80;
                }
                flash->command = CMD_READ_STATUS;
                break;

        case CMD_PROGRAM_SETUP:
                // pclog("flash_write: program %05x %02x\n", addr, val);
                if ((addr & 0x3e000) != (flash->block_start[3] & 0x3e000))
                        flash->array[addr] = val;
                flash->command = CMD_READ_STATUS;
                flash->status = 0x80;
                break;

        default:
                flash->command = val;
                switch (val) {
                case CMD_CLEAR_STATUS:
                        flash->status = 0;
                        break;
                }
        }
}

static void intel_flash_add_mappings(flash_t *flash) {
        int i = 0;

        for (i = 0; i <= 7; i++) {
                mem_mapping_add(&(flash->mapping[i]), 0xe0000 + (i << 14), 0x04000, flash_read, flash_readw, flash_readl,
                                flash_write, mem_write_nullw, mem_write_nulll,
                                flash->array + (((i << 14) + 0x20000) & flash->addr_mask), MEM_MAPPING_EXTERNAL, (void *)flash);
                mem_mapping_add(&(flash->mapping_h[i]), 0xfffe0000 + (i << 14), 0x04000, flash_read, flash_readw, flash_readl,
                                flash_write, mem_write_nullw, mem_write_nulll,
                                flash->array + (((i << 14) + 0x20000) & flash->addr_mask), 0, (void *)flash);
        }

        if (flash->type & FLASH_2MBIT) {
                mem_mapping_add(&flash->mapping_h[8], 0xfffc0000, 0x20000, flash_read, flash_readw, flash_readl, flash_write,
                                mem_write_nullw, mem_write_nulll, flash->array, 0, (void *)flash);
        }
}

/* This is for boards which invert the high pin - the flash->array pointers need to pointer invertedly in order for INTERNAL
 * writes to go to the right part of the array. */
static void intel_flash_add_mappings_inverted(flash_t *flash) {
        int i = 0;

        for (i = 0; i <= 7; i++) {
                mem_mapping_add(&(flash->mapping[i]), 0xe0000 + (i << 14), 0x04000, flash_read, flash_readw, flash_readl,
                                flash_write, mem_write_nullw, mem_write_nulll,
                                flash->array + ((((i << 14) ^ 0x10000) + 0x20000) & flash->addr_mask), MEM_MAPPING_EXTERNAL,
                                (void *)flash);
                mem_mapping_add(&(flash->mapping_h[i]), 0xfffe0000 + (i << 14), 0x04000, flash_read, flash_readw, flash_readl,
                                flash_write, mem_write_nullw, mem_write_nulll,
                                flash->array + ((((i << 14) ^ 0x10000) + 0x20000) & flash->addr_mask), 0, (void *)flash);
        }

        if (flash->type & FLASH_2MBIT) {
                mem_mapping_add(&flash->mapping_h[8], 0xfffc0000, 0x10000, flash_read, flash_readw, flash_readl, flash_write,
                                mem_write_nullw, mem_write_nulll, flash->array + 0x10000, 0, (void *)flash);
                mem_mapping_add(&flash->mapping_h[9], 0xfffd0000, 0x10000, flash_read, flash_readw, flash_readl, flash_write,
                                mem_write_nullw, mem_write_nulll, flash->array, 0, (void *)flash);
        }
}

void *intel_flash_init(uint8_t type, uint8_t flash_id) {
        FILE *f;
        flash_t *flash = malloc(sizeof(flash_t));
        memset(flash, 0, sizeof(flash_t));
        char fpath[1024];
        int i;

        flash->type = type;

        switch (romset) {
        case ROM_REVENGE:
                strcpy(flash_path, "revenge/");
                break;
        case ROM_PB520R:
                strcpy(flash_path, "pb520r/");
                break;
        case ROM_ENDEAVOR:
                strcpy(flash_path, "endeavor/");
                break;
        case ROM_PB570:
                strcpy(flash_path, "pb570/");
                break;
        case ROM_ZAPPA:
                strcpy(flash_path, "zappa/");
                break;
        case ROM_P55VA:
                strcpy(flash_path, "p55va/");
                break;
        case ROM_P55TVP4:
                strcpy(flash_path, "p55tvp4/");
                break;
        case ROM_430VX:
                strcpy(flash_path, "430vx/");
                break;
        case ROM_P55T2P4:
                strcpy(flash_path, "p55t2p4/");
                break;
        case ROM_ITAUTEC_INFOWAYM:
                strcpy(flash_path, "infowaym/");
                break;
        case ROM_VS440FX:
                strcpy(flash_path, "vs440fx/");
                break;
        case ROM_GA686BX:
                strcpy(flash_path, "ga686bx/");
                break;

        default:
                fatal("intel_flash_init on unsupported ROM set %i\n", romset);
        }
        // pclog("Flash init: Path is: %s\n", flash_path);

        flash->flash_id = flash_id;

        flash->addr_mask = (type & FLASH_2MBIT) ? 0x3ffff : 0x1ffff;

        //	flash->flash_id = (type & FLASH_IS_BXB) ? 0x95 : 0x94;
        flash->invert_high_pin = (type & FLASH_INVERT);

        if (type & FLASH_2MBIT) {
                flash->block_len[BLOCK_MAIN] = 0x20000;
                flash->block_len[BLOCK_MAIN2] = 0x18000;
                flash->block_len[BLOCK_DATA1] = 0x02000;
                flash->block_len[BLOCK_DATA2] = 0x02000;
                flash->block_len[BLOCK_BOOT] = 0x04000;

                if (type & FLASH_IS_BXB) /* 28F001BX-B */
                {
                        flash->block_start[BLOCK_MAIN] = 0x20000; /* MAIN BLOCK */
                        flash->block_end[BLOCK_MAIN] = 0x3ffff;
                        flash->block_start[BLOCK_MAIN2] = 0x08000; /* MAIN BLOCK */
                        flash->block_end[BLOCK_MAIN2] = 0x1ffff;
                        flash->block_start[BLOCK_DATA1] = 0x04000; /* DATA AREA 1 BLOCK */
                        flash->block_end[BLOCK_DATA1] = 0x05fff;
                        flash->block_start[BLOCK_DATA2] = 0x06000; /* DATA AREA 2 BLOCK */
                        flash->block_end[BLOCK_DATA2] = 0x07fff;
                        flash->block_start[BLOCK_BOOT] = 0x00000; /* BOOT BLOCK */
                        flash->block_end[BLOCK_BOOT] = 0x03fff;
                } else /* 28F001BX-T */
                {
                        flash->block_start[BLOCK_MAIN] = 0x00000; /* MAIN BLOCK */
                        flash->block_end[BLOCK_MAIN] = 0x1ffff;
                        flash->block_start[BLOCK_MAIN2] = 0x20000; /* MAIN BLOCK */
                        flash->block_end[BLOCK_MAIN2] = 0x37fff;
                        flash->block_start[BLOCK_DATA1] = 0x38000; /* DATA AREA 1 BLOCK */
                        flash->block_end[BLOCK_DATA1] = 0x39fff;
                        flash->block_start[BLOCK_DATA2] = 0x3a000; /* DATA AREA 2 BLOCK */
                        flash->block_end[BLOCK_DATA2] = 0x3bfff;
                        flash->block_start[BLOCK_BOOT] = 0x3c000; /* BOOT BLOCK */
                        flash->block_end[BLOCK_BOOT] = 0x3ffff;
                }
        } else {
                /* The block lengths are the same both flash types. */
                flash->block_len[BLOCK_MAIN] = 0x1c000;
                flash->block_len[BLOCK_DATA1] = 0x01000;
                flash->block_len[BLOCK_DATA2] = 0x01000;
                flash->block_len[BLOCK_BOOT] = 0x02000;

                if (type & FLASH_IS_BXB) /* 28F001BX-B */
                {
                        flash->block_start[BLOCK_MAIN] = 0x04000; /* MAIN BLOCK */
                        flash->block_end[BLOCK_MAIN] = 0x1ffff;
                        flash->block_start[BLOCK_DATA1] = 0x03000; /* DATA AREA 1 BLOCK */
                        flash->block_end[BLOCK_DATA1] = 0x03fff;
                        flash->block_start[BLOCK_DATA2] = 0x04000; /* DATA AREA 2 BLOCK */
                        flash->block_end[BLOCK_DATA2] = 0x04fff;
                        flash->block_start[BLOCK_BOOT] = 0x00000; /* BOOT BLOCK */
                        flash->block_end[BLOCK_BOOT] = 0x01fff;
                } else /* 28F001BX-T */
                {
                        flash->block_start[BLOCK_MAIN] = 0x00000; /* MAIN BLOCK */
                        flash->block_end[BLOCK_MAIN] = 0x1bfff;
                        flash->block_start[BLOCK_DATA1] = 0x1c000; /* DATA AREA 1 BLOCK */
                        flash->block_end[BLOCK_DATA1] = 0x1cfff;
                        flash->block_start[BLOCK_DATA2] = 0x1d000; /* DATA AREA 2 BLOCK */
                        flash->block_end[BLOCK_DATA2] = 0x1dfff;
                        flash->block_start[BLOCK_BOOT] = 0x1e000; /* BOOT BLOCK */
                        flash->block_end[BLOCK_BOOT] = 0x1ffff;
                }
        }

        for (i = 0; i < 8; i++) {
                mem_mapping_disable(&bios_mapping[i]);
                mem_mapping_disable(&bios_high_mapping[i]);
        }

        if (flash->invert_high_pin) {
                memcpy(flash->array, rom + 65536, 65536);
                memcpy(flash->array + 65536, rom, 65536);
                if (type & FLASH_2MBIT) {
                        memcpy(flash->array + 0x20000, rom + 0x30000, 65536);
                        memcpy(flash->array + 0x30000, rom + 0x20000, 65536);
                }
        } else {
                memcpy(flash->array, rom, 131072);
                if (type & FLASH_2MBIT)
                        memcpy(flash->array + 0x20000, rom + 0x20000, 131072);
        }

        if (flash->invert_high_pin) {
                intel_flash_add_mappings_inverted(flash);
        } else {
                intel_flash_add_mappings(flash);
        }

        flash->command = CMD_READ_ARRAY;
        flash->status = 0;

        strcpy(fpath, flash_path);
        strcat(fpath, "flash.bin");
        f = romfopen(fpath, "rb");
        if (f) {
                fread(&(flash->array[flash->block_start[BLOCK_MAIN]]), flash->block_len[BLOCK_MAIN], 1, f);
                if (type & FLASH_2MBIT)
                        fread(&(flash->array[flash->block_start[BLOCK_MAIN2]]), flash->block_len[BLOCK_MAIN2], 1, f);
                fread(&(flash->array[flash->block_start[BLOCK_DATA1]]), flash->block_len[BLOCK_DATA1], 1, f);
                fread(&(flash->array[flash->block_start[BLOCK_DATA2]]), flash->block_len[BLOCK_DATA2], 1, f);
                fclose(f);
        }

        return flash;
}

/* For AMI BIOS'es - Intel 28F001BXT with high address pin inverted. */
void *intel_flash_bxt_ami_init() { return intel_flash_init(FLASH_INVERT, 0x94); }

/* For Award BIOS'es - Intel 28F001BXT with high address pin not inverted. */
void *intel_flash_bxt_init() { return intel_flash_init(0, 0x94); }

/* For Acer BIOS'es - Intel 28F001BXB. */
void *intel_flash_bxb_init() { return intel_flash_init(FLASH_IS_BXB, 0x95); }

static void *intel_flash_28fb200bxt_init(void) { return intel_flash_init(FLASH_INVERT | FLASH_2MBIT, 0x74); }

static void *intel_flash_28f002bc_init(void) { return intel_flash_init(FLASH_INVERT | FLASH_2MBIT, 0x7c); }

void intel_flash_close(void *p) {
        FILE *f;
        flash_t *flash = (flash_t *)p;

        char fpath[1024];

        strcpy(fpath, flash_path);
        strcat(fpath, "flash.bin");
        f = romfopen(fpath, "wb");
        fwrite(&(flash->array[flash->block_start[BLOCK_MAIN]]), flash->block_len[BLOCK_MAIN], 1, f);
        if (flash->type & FLASH_2MBIT)
                fwrite(&(flash->array[flash->block_start[BLOCK_MAIN2]]), flash->block_len[BLOCK_MAIN2], 1, f);
        fwrite(&(flash->array[flash->block_start[BLOCK_DATA1]]), flash->block_len[BLOCK_DATA1], 1, f);
        fwrite(&(flash->array[flash->block_start[BLOCK_DATA2]]), flash->block_len[BLOCK_DATA2], 1, f);
        fclose(f);

        free(flash);
}

device_t intel_flash_bxt_ami_device = {
        "Intel 28F001BXT Flash BIOS", 0, intel_flash_bxt_ami_init, intel_flash_close, NULL, NULL, NULL, NULL, NULL};

device_t intel_flash_bxt_device = {
        "Intel 28F001BXT Flash BIOS", 0, intel_flash_bxt_init, intel_flash_close, NULL, NULL, NULL, NULL, NULL};

device_t intel_flash_bxb_device = {
        "Intel 28F001BXB Flash BIOS", 0, intel_flash_bxb_init, intel_flash_close, NULL, NULL, NULL, NULL, NULL};

device_t intel_flash_28fb200bxt_device = {
        "Intel 28FB200BX-T Flash BIOS", 0, intel_flash_28fb200bxt_init, intel_flash_close, NULL, NULL, NULL, NULL, NULL};

device_t intel_flash_28f002bc_device = {
        "Intel 28F002BC Flash BIOS", 0, intel_flash_28f002bc_init, intel_flash_close, NULL, NULL, NULL, NULL, NULL};
