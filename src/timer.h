extern int timer_start;

#define timer_start_period(cycles)                      \
        timer_start = cycles;

#define timer_end_period(cycles) 			\
	do 						\
	{						\
                int diff = timer_start - cycles;        \
		timer_count -= diff;			\
                timer_start = cycles;                   \
		if (timer_count <= 0)			\
		{					\
			timer_process();		\
			timer_update_outstanding();	\
		}					\
	} while (0)

#define timer_clock()                                   \
	do 						\
	{						\
                int diff = timer_start - cycles;        \
		timer_count -= diff;			\
                timer_start = cycles;                   \
		timer_process();		        \
        	timer_update_outstanding();	        \
	} while (0)

void timer_process();
void timer_update_outstanding();
void timer_reset();
int timer_add(void (*callback)(void *priv), int *count, int *enable, void *priv);
void timer_set_callback(int timer, void (*callback)(void *priv));

#define TIMER_ALWAYS_ENABLED &timer_one

extern int timer_count;
extern int timer_one;

#define TIMER_SHIFT 6

extern int TIMER_USEC;
