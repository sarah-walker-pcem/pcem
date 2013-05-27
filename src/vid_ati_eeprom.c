#include "ibm.h"
#include "vid_ati_eeprom.h"

static uint16_t eeprom[256];
enum
{
        EEPROM_IDLE,
        EEPROM_WAIT,
        EEPROM_OPCODE,
        EEPROM_INPUT,
        EEPROM_OUTPUT
};

enum
{
        EEPROM_OP_EW    = 4,
        EEPROM_OP_WRITE = 5,
        EEPROM_OP_READ  = 6,
        EEPROM_OP_ERASE = 7,
        
        EEPROM_OP_WRALMAIN = -1
};

enum
{
        EEPROM_OP_EWDS = 0,
        EEPROM_OP_WRAL = 1,
        EEPROM_OP_ERAL = 2,
        EEPROM_OP_EWEN = 3
};

static int oldclk, oldena;
static int eeprom_opcode, eeprom_state, eeprom_count, eeprom_out;
static int eeprom_wp;
static uint32_t eeprom_dat;
static int eeprom_type;

static char eeprom_fn[256] = {0};

void ati_eeprom_load(char *fn, int type)
{
        FILE *f;
        eeprom_type = type;
        strcpy(eeprom_fn, fn);
        f = romfopen(eeprom_fn, "rb");
        if (!f)
        {
                memset(eeprom, 0, type ? 512 : 128);
                return;
        }
        fread(eeprom, 1, type ? 512 : 128, f);
        fclose(f);
}

void ati_eeprom_save()
{
        FILE *f = romfopen(eeprom_fn, "wb");
        if (!f) return;
        fwrite(eeprom, 1, eeprom_type ? 512 : 128, f);
        fclose(f);
}

void ati_eeprom_write(int ena, int clk, int dat)
{
        int c;
        pclog("EEPROM write %i %i %i\n", ena, clk, dat);
        if (!ena)
        {
                eeprom_out = 1;
        }
        if (clk && !oldclk)
        {
                if (ena && !oldena)
                {
                        eeprom_state = EEPROM_WAIT;
                        eeprom_opcode = 0;
                        eeprom_count = 3;
                        eeprom_out = 1;
                }
                else if (ena)
                {
                        pclog("EEPROM receive %i %i %i\n", ena, clk, dat);
                        switch (eeprom_state)
                        {
                                case EEPROM_WAIT:
                                if (!dat)
                                        break;
                                eeprom_state = EEPROM_OPCODE;
                                /* fall through */
                                case EEPROM_OPCODE:
                                eeprom_opcode = (eeprom_opcode << 1) | (dat ? 1 : 0);
                                eeprom_count--;
                                if (!eeprom_count)
                                {
                                        pclog("EEPROM opcode - %i\n", eeprom_opcode);
                                        switch (eeprom_opcode)
                                        {
                                                case EEPROM_OP_WRITE:
                                                eeprom_count = eeprom_type ? 24 : 22;
                                                eeprom_state = EEPROM_INPUT;
                                                eeprom_dat = 0;
                                                break;
                                                case EEPROM_OP_READ:
                                                eeprom_count = eeprom_type ? 8 : 6;
                                                eeprom_state = EEPROM_INPUT;
                                                eeprom_dat = 0;
                                                break;
                                                case EEPROM_OP_EW:
                                                eeprom_count = 2;
                                                eeprom_state = EEPROM_INPUT;
                                                eeprom_dat = 0;
                                                break;
                                                case EEPROM_OP_ERASE:
                                                eeprom_count = eeprom_type ? 8 : 6;
                                                eeprom_state = EEPROM_INPUT;
                                                eeprom_dat = 0;
                                                break;
                                        }
                                }
                                break;
                                
                                case EEPROM_INPUT:
                                eeprom_dat = (eeprom_dat << 1) | (dat ? 1 : 0);
                                eeprom_count--;
                                if (!eeprom_count)
                                {
                                        pclog("EEPROM dat - %02X\n", eeprom_dat);
                                        switch (eeprom_opcode)
                                        {
                                                case EEPROM_OP_WRITE:
                                                pclog("EEPROM_OP_WRITE addr %02X eeprom_dat %04X\n", (eeprom_dat >> 16) & (eeprom_type ? 255 : 63), eeprom_dat & 0xffff);
                                                if (!eeprom_wp)
                                                {
                                                        eeprom[(eeprom_dat >> 16) & (eeprom_type ? 255 : 63)] = eeprom_dat & 0xffff;
                                                        ati_eeprom_save();
                                                }
                                                eeprom_state = EEPROM_IDLE;
                                                eeprom_out = 1;
                                                break;

                                                case EEPROM_OP_READ:
                                                eeprom_count = 17;
                                                eeprom_state = EEPROM_OUTPUT;
                                                eeprom_dat = eeprom[eeprom_dat];
                                                pclog("Trigger EEPROM_OUTPUT %04X\n", eeprom_dat);
                                                break;
                                                case EEPROM_OP_EW:
                                                pclog("EEPROM_OP_EW %i\n", eeprom_dat);
                                                switch (eeprom_dat)
                                                {
                                                        case EEPROM_OP_EWDS:
                                                        eeprom_wp = 1;
                                                        break;
                                                        case EEPROM_OP_WRAL:
                                                        opcode = EEPROM_OP_WRALMAIN;
                                                        eeprom_count = 20;
                                                        break;
                                                        case EEPROM_OP_ERAL:
                                                        if (!eeprom_wp)
                                                        {
                                                                memset(eeprom, 0xff, 128);
                                                                ati_eeprom_save();
                                                        }
                                                        break;
                                                        case EEPROM_OP_EWEN:
                                                        eeprom_wp = 0;
                                                        break;
                                                }
                                                eeprom_state = EEPROM_IDLE;
                                                eeprom_out = 1;
                                                break;

                                                case EEPROM_OP_ERASE:
                                                pclog("EEPROM_OP_ERASE %i\n", eeprom_dat);
                                                if (!eeprom_wp)
                                                {
                                                        eeprom[eeprom_dat] = 0xffff;
                                                        ati_eeprom_save();
                                                }
                                                eeprom_state = EEPROM_IDLE;
                                                eeprom_out = 1;
                                                break;

                                                case EEPROM_OP_WRALMAIN:
                                                pclog("EEPROM_OP_WRAL %04X\n", eeprom_dat);
                                                if (!eeprom_wp)
                                                {
                                                        for (c = 0; c < 256; c++)
                                                                eeprom[c] = eeprom_dat;
                                                        ati_eeprom_save();
                                                }
                                                eeprom_state = EEPROM_IDLE;
                                                eeprom_out = 1;
                                                break;
                                        }
                                }
                                break;
                        }
                }                
                oldena = ena;
        }
        else if (!clk && oldclk)
        {
                if (ena)
                {
                        switch (eeprom_state)
                        {
                                case EEPROM_OUTPUT:
                                eeprom_out = (eeprom_dat & 0x10000) ? 1 : 0;
                                eeprom_dat <<= 1;
                                pclog("EEPROM_OUTPUT - data %i\n", eeprom_out);                                
                                eeprom_count--;
                                if (!eeprom_count)
                                {
                                        pclog("EEPROM_OUTPUT complete\n");
                                        eeprom_state = EEPROM_IDLE;
                                }
                                break;
                        }
                }
        }
        oldclk = clk;
}

int ati_eeprom_read()
{
        return eeprom_out;
}

