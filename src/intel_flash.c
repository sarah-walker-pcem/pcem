#include <stdlib.h>
#include "ibm.h"
#include "device.h"
#include "mem.h"

enum
{
        CMD_READ_ARRAY = 0xff,
        CMD_IID = 0x90,
        CMD_READ_STATUS = 0x70,
        CMD_CLEAR_STATUS = 0x50,
        CMD_ERASE_SETUP = 0x20,
        CMD_ERASE_CONFIRM = 0xd0,
        CMD_ERASE_SUSPEND = 0xb0,
        CMD_PROGRAM_SETUP = 0x40
};

typedef struct flash_t
{
        uint8_t command, status;
        mem_mapping_t read_mapping, write_mapping;
} flash_t;

static flash_t flash;

static uint8_t flash_read(uint32_t addr, void *p)
{
        flash_t *flash = (flash_t *)p;
//        pclog("flash_read : addr=%08x command=%02x %04x:%08x\n", addr, flash->command, CS, pc);
        switch (flash->command)
        {
                case CMD_IID:
                if (addr & 1)
                        return 0x94;
                return 0x89;
                
                default:
                return flash->status;
        }
}

static void flash_write(uint32_t addr, uint8_t val, void *p)
{
        flash_t *flash = (flash_t *)p;
//        pclog("flash_write : addr=%08x val=%02x command=%02x %04x:%08x\n", addr, val, flash->command, CS, pc);        
        switch (flash->command)
        {
                case CMD_ERASE_SETUP:
                if (val == CMD_ERASE_CONFIRM)
                {
//                        pclog("flash_write: erase %05x\n", addr & 0x1ffff);
                        if ((addr & 0x1f000) == 0x0d000)
                                memset(&rom[0x0d000], 0xff, 0x1000);
                        if ((addr & 0x1f000) == 0x0c000)
                                memset(&rom[0x0c000], 0xff, 0x1000);
                        if ((addr & 0x1f000) < 0x0c000 || (addr & 0x1f000) >= 0x10000)
                        {
                                memset(rom, 0xff, 0xc000);
                                memset(&rom[0x10000], 0xff, 0x10000);
                        }
                        flash->status = 0x80;
                }
                flash->command = CMD_READ_STATUS;
                break;
                
                case CMD_PROGRAM_SETUP:
//                pclog("flash_write: program %05x %02x\n", addr & 0x1ffff, val);
                if ((addr & 0x1e000) != 0x0e000)
                        rom[addr & 0x1ffff] = val;
                flash->command = CMD_READ_STATUS;
                flash->status = 0x80;
                break;
                
                default:
                flash->command = val;
                switch (val)
                {
                        case CMD_CLEAR_STATUS:
                        flash->status = 0;
                        break;
                                
                        case CMD_IID:
                        case CMD_READ_STATUS:
                        mem_mapping_disable(&bios_mapping[0]);
                        mem_mapping_disable(&bios_mapping[1]);
                        mem_mapping_disable(&bios_mapping[2]);
                        mem_mapping_disable(&bios_mapping[3]);
                        mem_mapping_disable(&bios_mapping[4]);
                        mem_mapping_disable(&bios_mapping[5]);
                        mem_mapping_disable(&bios_mapping[6]);
                        mem_mapping_disable(&bios_mapping[7]);
                        mem_mapping_enable(&flash->read_mapping);                        
                        break;
                        
                        case CMD_READ_ARRAY:
                        mem_mapping_enable(&bios_mapping[0]);
                        mem_mapping_enable(&bios_mapping[1]);
                        mem_mapping_enable(&bios_mapping[2]);
                        mem_mapping_enable(&bios_mapping[3]);
                        mem_mapping_enable(&bios_mapping[4]);
                        mem_mapping_enable(&bios_mapping[5]);
                        mem_mapping_enable(&bios_mapping[6]);
                        mem_mapping_enable(&bios_mapping[7]);
                        mem_mapping_disable(&flash->read_mapping);
                        break;
                }
        }
}

void *intel_flash_init()
{
        FILE *f;
        flash_t *flash = malloc(sizeof(flash_t));
        memset(flash, 0, sizeof(flash_t));

        mem_mapping_add(&flash->read_mapping,
                    0xe0000, 
                    0x20000,
                    flash_read, NULL, NULL,
                    NULL, NULL, NULL,
                    NULL, MEM_MAPPING_EXTERNAL, (void *)flash);
        mem_mapping_add(&flash->write_mapping,
                    0xe0000, 
                    0x20000,
                    NULL, NULL, NULL,
                    flash_write, NULL, NULL,
                    NULL, MEM_MAPPING_EXTERNAL, (void *)flash);
        mem_mapping_disable(&flash->read_mapping);
        flash->command = CMD_READ_ARRAY;
        flash->status = 0;
        
        memset(&rom[0xc000], 0, 0x2000);
        
        f = romfopen("roms/endeavor/oemlogo.bin", "rb");
        if (f)
        {
                fread(&rom[0xc000], 0x1000, 1, f);
                fclose(f);
        }
        f = romfopen("roms/endeavor/ecsd.bin", "rb");
        if (f)
        {
                fread(&rom[0xd000], 0x1000, 1, f);
                fclose(f);
        }
        
        return flash;
}

void intel_flash_close(void *p)
{
        FILE *f;
        flash_t *flash = (flash_t *)p;

        f = romfopen("roms/endeavor/oemlogo.bin", "wb");
        fwrite(&rom[0xc000], 0x1000, 1, f);
        fclose(f);
        f = romfopen("roms/endeavor/ecsd.bin", "wb");
        fwrite(&rom[0xd000], 0x1000, 1, f);
        fclose(f);

        free(flash);
}

device_t intel_flash_device =
{
        "Intel 28F001BXT Flash BIOS",
        0,
        intel_flash_init,
        intel_flash_close,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
};
