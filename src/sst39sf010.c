#include <stdlib.h>
#include "ibm.h"
#include "device.h"
#include "mem.h"
#include "sst39sf010.h"

typedef struct sst_t
{
        int command_state;
        int id_mode;
        int erase;
        int dirty;
        
        char flash_path[1024];
        uint8_t data[0x20000];
} sst_t;

#define SST_CHIP_ERASE    0x10
#define SST_SECTOR_ERASE  0x30
#define SST_ERASE         0x80
#define SST_SET_ID_MODE   0x90
#define SST_BYTE_PROGRAM  0xa0
#define SST_CLEAR_ID_MODE 0xf0

static void set_id_mode(sst_t *sst);
static void clear_id_mode(sst_t *sst);

static void sst_new_command(sst_t *sst, uint8_t val)
{
        switch (val)
        {
                case SST_CHIP_ERASE:
                if (sst->erase)
                {
                        memset(rom, 0xff, 0x20000);
                        memset(sst->data, 0xff, 0x20000);
                }
                sst->command_state = 0;
                sst->erase = 0;
                break;
                
                case SST_ERASE:
                sst->command_state = 0;
                sst->erase = 1;
                break;
                
                case SST_SET_ID_MODE:
                if (!sst->id_mode)
                        set_id_mode(sst);
                sst->command_state = 0;
                sst->erase = 0;
                break;
                
                case SST_BYTE_PROGRAM:
                sst->command_state = 3;
                sst->erase = 0;
                break;

                case SST_CLEAR_ID_MODE:
                if (sst->id_mode)
                        clear_id_mode(sst);
                sst->command_state = 0;
                sst->erase = 0;
                break;

                default:
                sst->command_state = 0;
                sst->erase = 0;
        }
}

static void sst_sector_erase(sst_t *sst, uint32_t addr)
{
//        pclog("SST sector erase %08x\n", addr);
        memset(&rom[addr & 0x1f000], 0xff, 4096);
        memset(&sst->data[addr & 0x1f000], 0xff, 4096);
        sst->dirty = 1;
}

static uint8_t sst_read_id(uint32_t addr, void *p)
{
        if ((addr & 0xffff) == 0)
                return 0xbf; /*SST*/
        else if ((addr & 0xffff) == 1)
                return 0xb5; /*39SF010*/
        else
                return 0xff;
//                fatal("sst_read_id: addr=%08x\n", addr);
}

static void sst_write(uint32_t addr, uint8_t val, void *p)
{
        sst_t *sst = (sst_t *)p;

//        pclog("sst_write: addr=%08x val=%02x state=%i\n", addr, val, sst->command_state);
        switch (sst->command_state)
        {
                case 0:
                if (val == 0xf0)
                {
                        if (sst->id_mode)
                                clear_id_mode(sst);
                }
                else if ((addr & 0xffff) == 0x5555 && val == 0xaa)
                        sst->command_state = 1;
                else
                        sst->command_state = 0;
                break;
                case 1:
                if ((addr & 0xffff) == 0x2aaa && val == 0x55)
                        sst->command_state = 2;
                else
                        sst->command_state = 0;
                break;
                case 2:
                if ((addr & 0xffff) == 0x5555)
                        sst_new_command(sst, val);
                else if (val == SST_SECTOR_ERASE && sst->erase)
                {
                        sst_sector_erase(sst, addr);
                        sst->command_state = 0;
                }
                else
                        sst->command_state = 0;
                break;
                case 3:
//                pclog("Byte program %08x %02x\n", addr, val);
                rom[addr & 0x1ffff] = val;
                sst->data[addr & 0x1ffff] = val;
                sst->command_state = 0;
                sst->dirty = 1;
                break;
        }
}

static void set_id_mode(sst_t *sst)
{
        int c;

	for (c = 0; c < 8; c++)
	{
                mem_mapping_set_handler(&bios_mapping[c],
                                        sst_read_id, NULL, NULL,
                                        sst_write, NULL, NULL);
                mem_mapping_set_p(&bios_mapping[c], sst);
                mem_mapping_set_handler(&bios_high_mapping[c],
                                        sst_read_id, NULL, NULL,
                                        sst_write, NULL, NULL);
                mem_mapping_set_p(&bios_high_mapping[c], sst);
	}
	sst->id_mode = 1;
}

static void clear_id_mode(sst_t *sst)
{
        int c;

	for (c = 0; c < 8; c++)
	{
                mem_mapping_set_handler(&bios_mapping[c],
                                        mem_read_bios, mem_read_biosw, mem_read_biosl,
                                        sst_write, NULL, NULL);
                mem_mapping_set_p(&bios_mapping[c], sst);
                mem_mapping_set_handler(&bios_high_mapping[c],
                                        mem_read_bios, mem_read_biosw, mem_read_biosl,
                                        sst_write, NULL, NULL);
                mem_mapping_set_p(&bios_high_mapping[c], sst);
	}
	sst->id_mode = 0;
}

static void *sst_39sf010_init()
{
        FILE *f;
        sst_t *sst = malloc(sizeof(sst_t));
        memset(sst, 0, sizeof(sst_t));

	switch(romset)
	{
		case ROM_FIC_VA503P:
		strcpy(sst->flash_path, "fic_va503p/");
		break;
		default:
                fatal("sst_39sf010_init on unsupported ROM set %i\n", romset);
	}
	strcat(sst->flash_path, "flash.bin");
        f = romfopen(sst->flash_path, "rb");
        if (f)
        {
                fread(rom, 0x20000, 1, f);
                fclose(f);
        }
        memcpy(sst->data, rom, 0x20000);

        clear_id_mode(sst);
	
	return sst;
}

static void sst_39sf010_close(void *p)
{
        sst_t *sst = (sst_t *)p;
        
        if (sst->dirty)
        {
                FILE *f = romfopen(sst->flash_path, "wb");
                fwrite(sst->data, 0x20000, 1, f);
                fclose(f);
        }

        free(sst);
}

device_t sst_39sf010_device =
{
        "SST 39SF010 Flash BIOS",
        0,
        sst_39sf010_init,
        sst_39sf010_close,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
};
