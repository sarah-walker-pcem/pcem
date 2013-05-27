#include "ibm.h"

/*#include "sound_opl.h"
#include "adlibgold.h"
#include "sound_pas16.h"
#include "sound_sb.h"
#include "sound_sb_dsp.h"
#include "sound_wss.h"*/
#include "timer.h"

#define TIMERS_MAX 32

int TIMER_USEC;

static struct
{
	int present;
	void (*callback)(void *priv);
	void *priv;
	int *enable;
	int *count;
} timers[TIMERS_MAX];

int timers_present = 0;
int timer_one = 1;
	
int timer_count = 0, timer_latch = 0;

void timer_process()
{
	int c;
	
	/*Get actual elapsed time*/
	timer_latch -= timer_count;

	for (c = 0; c < timers_present; c++)
	{
//		pclog("timer_process : %i enable %i %X\n", c, timers[c].enable, *timers[c].enable);
		if (*timers[c].enable)
		{
			*timers[c].count = *timers[c].count - (timer_latch << TIMER_SHIFT);
			if (*timers[c].count <= 0)
			{
				timers[c].callback(timers[c].priv);
			}
		}
	}
}

void timer_update_outstanding()
{
	int c;
	timer_latch = 0x7fffffff;
	for (c = 0; c < timers_present; c++)
	{
		if (*timers[c].enable && *timers[c].count < timer_latch)
			timer_latch = *timers[c].count;
	}
	if (spktime < timer_latch)	timer_latch = spktime;
	//if (soundtime < timer_latch)	timer_latch = soundtime;
	//if (gustime   < timer_latch)	timer_latch = gustime;
	//if (gustime2  < timer_latch)	timer_latch = gustime2;
	//if (vidtime   < timer_latch)	timer_latch = vidtime;
	timer_count = timer_latch = (timer_latch + ((1 << TIMER_SHIFT) - 1)) >> TIMER_SHIFT;
}

void timer_reset()
{
	pclog("timer_reset\n");
	timers_present = 0;
	timer_latch = timer_count = 0;
//	timer_process();
}

int timer_add(void (*callback)(void *priv), int *count, int *enable, void *priv)
{
	if (timers_present < TIMERS_MAX)
	{
		pclog("timer_add : adding timer %i\n", timers_present);
		timers[timers_present].present = 1;
		timers[timers_present].callback = callback;
		timers[timers_present].priv = priv;
		timers[timers_present].count = count;
		timers[timers_present].enable = enable;
		timers_present++;
		return timers_present - 1;
	}
	return -1;
}

void timer_set_callback(int timer, void (*callback)(void *priv))
{
	timers[timer].callback = callback;
}
