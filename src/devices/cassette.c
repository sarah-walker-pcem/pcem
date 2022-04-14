/************************************************************************

    PCem: IBM 5150 Cassette support

    Copyright (C) 2019  John Elliott <seasip.webmaster@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*************************************************************************/

#include <stdlib.h>
#include "ibm.h"
#include "cpu.h"
#include "x86.h"
#include "device.h"
#include "pzx.h"

char cassettefn[256];

typedef struct cassette_t 
{
	uint8_t motor;		/* Motor status */
	pzxfile_t pzx;
	int cycles_last;	/* Cycle count at last cassette poll */

} cassette_t;

extern device_t cassette_device;


static cassette_t *st_cas;



#define CAS_LOG(x) pclog x
// #define CAS_LOG(x) 


/* The PCem CPU uses IBM cycles (4.77MHz). PZX uses Spectrum cycles (3.5MHz)
 * so scale accordingly. */
static int32_t pzx_cycles(int32_t pc)
{
	double d = pc;

	return (int32_t)(((d * 3.5) / 4.772728) + 0.5);
}

void cassette_eject(void)
{
	if (st_cas->pzx.input)
	{
		pzx_close(&st_cas->pzx);
	}
	cassettefn[0] = 0;
}

void cassette_load(const char *filename)
{
	FILE *fp;
	unsigned char magic[8];

	if (!filename) return;

	fp = fopen(filename, "rb");
	if (!fp)
	{
		/* Warn user? */
		CAS_LOG(("Failed to open cassette input %s\n", filename));
		return;
	}
	memset(magic, 0, sizeof(magic));
	fread(magic, 1, sizeof(magic), fp);	

	/* Check for PZX signature. In due course support could be added for
	 * other formats like TZX */
	if (!memcmp(magic, "PZXT", 4))
	{
		const char *result;

		result = pzx_open(&st_cas->pzx, fp);

		if (result)
		{
			CAS_LOG(("Failed to open %s as PZX: %s\n",
				filename, result));
			fclose(fp);
			return;			
		}
		strcpy(cassettefn, filename);
	}
}


uint8_t cassette_input(void)
{
	int ticks;

	/* While motor is off, result is loopback */
	if (!st_cas->motor)
	{
		return ppispeakon;
	}
	/* If there is no tapefile open don't try to extract data */
	if (st_cas->pzx.input == NULL) return 0;
	/* Otherwise see how many ticks there have been since the last input */
	if (st_cas->cycles_last == -1) 
	{
		st_cas->cycles_last = cycles;
	}
	if (cycles <= st_cas->cycles_last)
	{
		ticks = (st_cas->cycles_last - cycles);
	}
	else
	{
		ticks = (st_cas->cycles_last + (cpu_get_speed() / 100) - cycles);
	}
	st_cas->cycles_last = cycles;

	return pzx_advance(&st_cas->pzx, pzx_cycles(ticks));	
}



void cassette_set_motor(uint8_t on)
{
	if (on && !st_cas->motor)
	{
		CAS_LOG(("Start cassette motor\n"));
		st_cas->cycles_last = -1;
	}
	if (st_cas->motor && !on)
	{
		CAS_LOG(("Stop cassette motor\n"));
		st_cas->cycles_last = -1;
	}
	st_cas->motor = on;
}


static void *cassette_init(void)
{
	cassette_t *cas = malloc(sizeof(cassette_t));

	memset(cas, 0, sizeof(cassette_t));
	pzx_init(&cas->pzx);

	st_cas = cas;
	return cas;
}


static void cassette_close(void *p)
{
	cassette_t *cas = (cassette_t *)p;

	pzx_close(&cas->pzx);

	free(cas);
}


device_t cassette_device = 
{
	"Cassette",
	0,
	cassette_init,
	cassette_close,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

