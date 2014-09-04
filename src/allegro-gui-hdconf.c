#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _GNU_SOURCE

#include <stdio.h>
#include "ibm.h"
#include "ide.h"
#include "allegro-main.h"
#include "allegro-gui.h"

static char hd_path[2][260];
static char hd_sectors[2][10];
static char hd_heads[2][10];
static char hd_cylinders[2][10];
static char hd_size[2][20];

static char hd_path_new[260];
static char hd_sectors_new[10];
static char hd_heads_new[10];
static char hd_cylinders_new[10];
static char hd_size_new[20];

static PcemHDC hdc_new[2];

static DIALOG hdparams_dialog[]=
{
        {d_shadow_box_proc, 0, 0, 194*2,86,0,0xffffff,0,0,     0,0,0,0,0}, // 0

        {d_button_proc, 126,  66, 50, 14, 0, 0xffffff, 0, D_EXIT, 0, 0, "OK",     0, 0}, // 1
        {d_button_proc, 196,  66, 50, 16, 0, 0xffffff, 0, D_EXIT, 0, 0, "Cancel", 0, 0}, // 2

        {d_text_proc,   7*2,   6,  170,   10, 0, 0xffffff, 0, 0, 0, 0, "Initial settings are based on file size", 0, 0},

        {d_text_proc,   7*2,  22,  27, 10, 0, 0xffffff, 0, 0, 0, 0, "Sectors:", 0, 0},
        {d_text_proc,  63*2,  22,  29,  8, 0, 0xffffff, 0, 0, 0, 0, "Heads:", 0, 0},
        {d_text_proc, 120*2,  22,  28, 12, 0, 0xffffff, 0, 0, 0, 0, "Cylinders:", 0, 0},
        {d_edit_proc,  44*2,  22,  16*2, 12, 0, 0xffffff, 0, 0, 2, 0, hd_sectors_new, 0, 0},
        {d_edit_proc,  92*2,  22,  16*2, 12, 0, 0xffffff, 0, 0, 3, 0, hd_heads_new, 0, 0},
        {d_edit_proc, 168*2,  22,  24*2, 12, 0, 0xffffff, 0, 0, 5, 0, hd_cylinders_new, 0, 0},
        {d_text_proc,   7*2,  54, 136, 12, 0, 0xffffff, 0, 0, 0, 0, hd_size_new, 0, 0},

        {0,0,0,0,0,0,0,0,0,0,0,NULL,NULL,NULL}
};

static int hdconf_open(int msg, DIALOG *d, int c)
{
        int drv = d->d2;
        int ret = d_button_proc(msg, d, c);
        
        if (ret == D_EXIT)
        {
                char fn[260];
                int xsize = SCREEN_W - 32, ysize = SCREEN_H - 64;
                
                strcpy(fn, hd_path[drv]);
                ret = file_select_ex("Please choose a disc image", fn, "IMG", 260, xsize, ysize);
                if (ret)
                {
                        uint64_t sz;
                        FILE *f = fopen64(fn, "rb");
                        if (!f)
                        {
                                return D_REDRAW;
                        }
                        fseeko64(f, -1, SEEK_END);
                        sz = ftello64(f) + 1;
                        fclose(f);
                        sprintf(hd_sectors_new, "63");
                        sprintf(hd_heads_new, "16");
                        sprintf(hd_cylinders_new, "%i", (int)((sz / 512) / 16) / 63);

                        while (1)
                        {
                                position_dialog(hdparams_dialog, SCREEN_W/2 - 186, SCREEN_H/2 - 86/2);
        
                                ret = popup_dialog(hdparams_dialog, 1);

                                position_dialog(hdparams_dialog, -(SCREEN_W/2 - 186), -(SCREEN_H/2 - 86/2));
                        
                                if (ret == 1)
                                {
                                        int spt, hpc, cyl;
                                        sscanf(hd_sectors_new, "%i", &spt);
                                        sscanf(hd_heads_new, "%i", &hpc);
                                        sscanf(hd_cylinders_new, "%i", &cyl);
                                        
                                        if (spt > 63)
                                        {
                                                alert("Drive has too many sectors (maximum is 63)", NULL, NULL, "OK", NULL, 0, 0);
                                                continue;
                                        }
                                        if (hpc > 128)
                                        {
                                                alert("Drive has too many heads (maximum is 128)", NULL, NULL, "OK", NULL, 0, 0);
                                                continue;
                                        }
                                        if (cyl > 16383)
                                        {
                                                alert("Drive has too many cylinders (maximum is 16383)", NULL, NULL, "OK", NULL, 0, 0);
                                                continue;
                                        }
                                        
                                        hdc_new[drv].spt = spt;
                                        hdc_new[drv].hpc = hpc;
                                        hdc_new[drv].tracks = cyl;

                                        strcpy(hd_path[drv], fn);
                                        sprintf(hd_sectors[drv], "%i", hdc_new[drv].spt);
                                        sprintf(hd_heads[drv], "%i", hdc_new[drv].hpc);
                                        sprintf(hd_cylinders[drv], "%i", hdc_new[drv].tracks);
                                        sprintf(hd_size[drv], "Size : %imb", (((((uint64_t)hdc_new[drv].tracks*(uint64_t)hdc_new[drv].hpc)*(uint64_t)hdc_new[drv].spt)*512)/1024)/1024);

                                        return D_REDRAW;
                                }

                                if (ret == 2)
                                        break;
                        }
                }

                return D_REDRAW;
        }

        return ret;
}

static int hdconf_new_file(int msg, DIALOG *d, int c)
{
        int ret = d_button_proc(msg, d, c);
        
        if (ret == D_EXIT)
        {        
                char fn[260];
                int xsize = SCREEN_W - 32, ysize = SCREEN_H - 64;
                
                strcpy(fn, hd_path_new);
                ret = file_select_ex("Please choose a disc image", fn, "IMG", 260, xsize, ysize);
                if (ret)
                        strcpy(hd_path_new, fn);
                
                return D_REDRAW;
        }
        
        return ret;
}

static DIALOG hdnew_dialog[]=
{
        {d_shadow_box_proc, 0, 0, 194*2,86,0,0xffffff,0,0,     0,0,0,0,0}, // 0

        {d_button_proc, 126,  66, 50, 14, 0, 0xffffff, 0, D_EXIT, 0, 0, "OK",     0, 0}, // 1
        {d_button_proc, 196,  66, 50, 16, 0, 0xffffff, 0, D_EXIT, 0, 0, "Cancel", 0, 0}, // 2

        {d_edit_proc,   7*2,   6,  136*2, 10, 0, 0xffffff, 0, 0, 0, 0, hd_path_new, 0, 0},
        {hdconf_new_file, 143*2, 6,   16*2, 14, 0, 0xffffff, 0, D_EXIT, 0, 0, "...", 0, 0},
        
        {d_text_proc,   7*2,  22,  27, 10, 0, 0xffffff, 0, 0, 0, 0, "Sectors:", 0, 0},
        {d_text_proc,  63*2,  22,  29,  8, 0, 0xffffff, 0, 0, 0, 0, "Heads:", 0, 0},
        {d_text_proc, 120*2,  22,  28, 12, 0, 0xffffff, 0, 0, 0, 0, "Cylinders:", 0, 0},
        {d_edit_proc,  44*2,  22,  16*2, 12, 0, 0xffffff, 0, 0, 2, 0, hd_sectors_new, 0, 0},
        {d_edit_proc,  92*2,  22,  16*2, 12, 0, 0xffffff, 0, 0, 3, 0, hd_heads_new, 0, 0},
        {d_edit_proc, 168*2,  22,  24*2, 12, 0, 0xffffff, 0, 0, 5, 0, hd_cylinders_new, 0, 0},
//        {d_text_proc,   7*2,  54, 136, 12, 0, -1, 0, 0, 0, 0, hd_size_new, 0, 0},

        {0,0,0,0,0,0,0,0,0,0,0,NULL,NULL,NULL}
};

static int create_hd(char *fn, int cyl, int hpc, int spt)
{
	int c;
	int e;
	uint8_t buf[512];
	FILE *f = fopen64(hd_path_new, "wb");
	e = errno;
	if (!f)
	{
		alert("Can't open file for write", NULL, NULL, "OK", NULL, 0, 0);
		return -1;
	}
	memset(buf, 0, 512);
	for (c = 0; c < (cyl * hpc * spt); c++)
	{
		fwrite(buf, 512, 1, f);
	}
	fclose(f);
}

static int hdconf_new(int msg, DIALOG *d, int c)
{
        int drv = d->d2;
        int ret = d_button_proc(msg, d, c);
        
        if (ret == D_EXIT)
        {
                sprintf(hd_sectors_new, "63");
                sprintf(hd_heads_new, "16");
                sprintf(hd_cylinders_new, "511");
                strcpy(hd_path_new, "");

                while (1)
                {
                        position_dialog(hdnew_dialog, SCREEN_W/2 - 186, SCREEN_H/2 - 86/2);
        
                        ret = popup_dialog(hdnew_dialog, 1);

                        position_dialog(hdnew_dialog, -(SCREEN_W/2 - 186), -(SCREEN_H/2 - 86/2));
                
                        if (ret == 1)
                        {
                                int spt, hpc, cyl;
                                int c, d;
                                FILE *f;
				uint8_t *buf;
                                
                                sscanf(hd_sectors_new, "%i", &spt);
                                sscanf(hd_heads_new, "%i", &hpc);
                                sscanf(hd_cylinders_new, "%i", &cyl);
                        
                                if (spt > 63)
                                {
                                        alert("Drive has too many sectors (maximum is 63)", NULL, NULL, "OK", NULL, 0, 0);
                                        continue;
                                }
                                if (hpc > 128)
                                {
                                        alert("Drive has too many heads (maximum is 128)", NULL, NULL, "OK", NULL, 0, 0);
                                        continue;
                                }
                                if (cyl > 16383)
                                {
                                        alert("Drive has too many cylinders (maximum is 16383)", NULL, NULL, "OK", NULL, 0, 0);
                                        continue;
                                }
				if (create_hd(hd_path_new, cyl, hpc, spt))
					return D_REDRAW;

                                alert("Remember to partition and format the new drive", NULL, NULL, "OK", NULL, 0, 0);
                                
                                hdc_new[drv].spt = spt;
                                hdc_new[drv].hpc = hpc;
                                hdc_new[drv].tracks = cyl;

                                strcpy(hd_path[drv], hd_path_new);
                                sprintf(hd_sectors[drv], "%i", hdc_new[drv].spt);
                                sprintf(hd_heads[drv], "%i", hdc_new[drv].hpc);
                                sprintf(hd_cylinders[drv], "%i", hdc_new[drv].tracks);
                                sprintf(hd_size[drv], "Size : %imb", (((((uint64_t)hdc_new[drv].tracks*(uint64_t)hdc_new[drv].hpc)*(uint64_t)hdc_new[drv].spt)*512)/1024)/1024);

                                return D_REDRAW;
                        }
                        
                        if (ret == 2)
                                break;
                }
                
                return D_REDRAW;
        }
        
        return ret;
}

static int hdconf_eject(int msg, DIALOG *d, int c)
{
        int drv = d->d2;
        int ret = d_button_proc(msg, d, c);
        
        if (ret == D_EXIT)
        {
                hdc_new[drv].spt = 0;
                hdc_new[drv].hpc = 0;
                hdc_new[drv].tracks = 0;                
                strcpy(hd_path[drv], "");
                sprintf(hd_sectors[drv], "%i", hdc_new[drv].spt);
                sprintf(hd_heads[drv], "%i", hdc_new[drv].hpc);
                sprintf(hd_cylinders[drv], "%i", hdc_new[drv].tracks);
                sprintf(hd_size[drv], "Size : %imb", (((((uint64_t)hdc_new[drv].tracks*(uint64_t)hdc_new[drv].hpc)*(uint64_t)hdc_new[drv].spt)*512)/1024)/1024);
                
                return D_REDRAW;
        }

        return ret;
}

static DIALOG hdconf_dialog[]=
{
        {d_shadow_box_proc, 0, 0, 210*2,172,0,0xffffff,0,0,     0,0,0,0,0}, // 0

        {d_button_proc, 150,  152, 50, 14, 0, 0xffffff, 0, D_EXIT, 0, 0, "OK",     0, 0}, // 1
        {d_button_proc, 220,  152, 50, 16, 0, 0xffffff, 0, D_EXIT, 0, 0, "Cancel", 0, 0}, // 2

        {d_text_proc,   7*2,   6,  27,   10, 0, 0xffffff, 0, 0, 0, 0, "C:", 0, 0},
        {d_edit_proc,   7*2,   22, 136*2, 12, 0, 0xffffff, 0, D_DISABLED, 0, 0, hd_path[0], 0, 0},
        {hdconf_open,   143*2, 22, 16*2, 14, 0, 0xffffff, 0, D_EXIT, 0, 0, "...", 0, 0},
        {hdconf_new,    159*2, 22, 24*2, 14, 0, 0xffffff, 0, D_EXIT, 0, 0, "New", 0, 0},
        {hdconf_eject,  183*2, 22, 24*2, 14, 0, 0xffffff, 0, D_EXIT, 0, 0, "Eject", 0, 0},

        {d_text_proc,   7*2,  38,  27, 10, 0, 0xffffff, 0, 0, 0, 0, "Sectors:", 0, 0},
        {d_text_proc,  63*2,  38,  29,  8, 0, 0xffffff, 0, 0, 0, 0, "Heads:", 0, 0},
        {d_text_proc, 120*2,  38,  28, 12, 0, 0xffffff, 0, 0, 0, 0, "Cylinders:", 0, 0},
        {d_edit_proc,  44*2,  38,  16*2, 12, 0, 0xffffff, 0, D_DISABLED, 0, 0, hd_sectors[0], 0, 0},
        {d_edit_proc,  92*2,  38,  16*2, 12, 0, 0xffffff, 0, D_DISABLED, 0, 0, hd_heads[0], 0, 0},
        {d_edit_proc, 168*2,  38,  24*2, 12, 0, 0xffffff, 0, D_DISABLED, 0, 0, hd_cylinders[0], 0, 0},
        {d_text_proc,   7*2,  54, 136, 12, 0, 0xffffff, 0, 0, 0, 0, hd_size[0], 0, 0},

        {d_text_proc,   7*2,  76,  27, 10, 0, 0xffffff, 0, 0, 0, 0, "D:", 0, 0},
        {d_edit_proc,   7*2,   92, 136*2, 12, 0, 0xffffff, 0, D_DISABLED, 0, 0, hd_path[1], 0, 0},
        {hdconf_open,   143*2, 92, 16*2, 14, 0, 0xffffff, 0, D_EXIT, 0, 1, "...", 0, 0},
        {hdconf_new,    159*2, 92, 24*2, 14, 0, 0xffffff, 0, D_EXIT, 0, 1, "New", 0, 0},
        {hdconf_eject,  183*2, 92, 24*2, 14, 0, 0xffffff, 0, D_EXIT, 0, 1, "Eject", 0, 0},

        {d_edit_proc,  44*2, 108,  16*2, 12, 0, 0xffffff, 0, D_DISABLED, 0, 0, hd_sectors[1], 0, 0},
        {d_edit_proc,  92*2, 108,  16*2, 12, 0, 0xffffff, 0, D_DISABLED, 0, 0, hd_heads[1], 0, 0},
        {d_edit_proc, 168*2, 108,  24*2, 12, 0, 0xffffff, 0, D_DISABLED, 0, 0, hd_cylinders[1], 0, 0},
        {d_text_proc,   7*2, 108,  27, 10, 0, 0xffffff, 0, 0, 0, 0, "Sectors:", 0, 0},
        {d_text_proc,  63*2, 108,  29,  8, 0, 0xffffff, 0, 0, 0, 0, "Heads:", 0, 0},
        {d_text_proc, 120*2, 108,  32, 12, 0, 0xffffff, 0, 0, 0, 0, "Cylinders:", 0, 0},
        {d_text_proc,   7*2, 124, 136, 12, 0, 0xffffff, 0, 0, 0, 0, hd_size[1], 0, 0},

        {0,0,0,0,0,0,0,0,0,0,0,NULL,NULL,NULL}
};

int disc_hdconf()
{
        int c;
        int changed=0;

        hdc_new[0] = hdc[0];
        hdc_new[1] = hdc[1];
        strcpy(hd_path[0], ide_fn[0]);
        strcpy(hd_path[1], ide_fn[1]);
        sprintf(hd_sectors[0], "%i", hdc[0].spt);
        sprintf(hd_sectors[1], "%i", hdc[1].spt);
        sprintf(hd_heads[0], "%i", hdc[0].hpc);
        sprintf(hd_heads[1], "%i", hdc[1].hpc);
        sprintf(hd_cylinders[0], "%i", hdc[0].tracks);
        sprintf(hd_cylinders[1], "%i", hdc[1].tracks);
        sprintf(hd_size[0], "Size : %imb", (((((uint64_t)hdc[0].tracks*(uint64_t)hdc[0].hpc)*(uint64_t)hdc[0].spt)*512)/1024)/1024);
        sprintf(hd_size[1], "Size : %imb", (((((uint64_t)hdc[1].tracks*(uint64_t)hdc[1].hpc)*(uint64_t)hdc[1].spt)*512)/1024)/1024);

        while (1)
        {
                position_dialog(hdconf_dialog, SCREEN_W/2 - 210, SCREEN_H/2 - 172/2);
        
                c = popup_dialog(hdconf_dialog, 1);

                position_dialog(hdconf_dialog, -(SCREEN_W/2 - 210), -(SCREEN_H/2 - 172/2));

                if (c == 1)
                {
                        if (alert("This will reset PCem!", "Okay to continue?", NULL, "OK", "Cancel", 0, 0) == 1)
                        {
                                hdc[0] = hdc_new[0];
                                hdc[1] = hdc_new[1];
                
                                strcpy(ide_fn[0], hd_path[0]);
                                strcpy(ide_fn[1], hd_path[1]);

                                saveconfig();
                                                                                
                                resetpchard();
                                
                                return D_O_K;
                        }
                }
                if (c == 2)
                        return D_O_K;
        }
        
        return D_O_K;
}
