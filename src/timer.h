#ifndef _TIMER_H_
#define _TIMER_H_

#include "cpu.h"

/*Timers are based on the CPU Time Stamp Counter. Timer timestamps are in a
  32:32 fixed point format, with the integer part compared against the TSC. The
  fractional part is used when advancing the timestamp to ensure a more accurate
  period.
  
  As the timer only stores 32 bits of integer timestamp, and the TSC is 64 bits,
  the timer period can only be at most 0x7fffffff CPU cycles. To allow room for
  (optimistic) CPU frequency growth, timer period must be at most 1 second.

  When a timer callback is called, the timer has been disabled. If the timer is
  to repeat, the callback must call timer_advance_u64(). This is a change from
  the old timer API.*/
typedef struct pc_timer_t
{
	uint32_t ts_integer;
	uint32_t ts_frac;
	int enabled;

	void (*callback)(void *p);
	void *p;

	struct pc_timer_t *prev, *next;
} pc_timer_t;

/*Timestamp of nearest enabled timer. CPU emulation must call timer_process()
  when TSC matches or exceeds this.*/
extern uint32_t timer_target;

/*Enable timer, without updating timestamp*/
void timer_enable(pc_timer_t *timer);
/*Disable timer*/
void timer_disable(pc_timer_t *timer);

/*Process any pending timers*/
void timer_process();

/*Reset timer system*/
void timer_reset();

/*Add new timer. If start_timer is set, timer will be enabled with a zero
  timestamp - this is useful for permanently enabled timers*/
void timer_add(pc_timer_t *timer, void (*callback)(void *p), void *p, int start_timer);

/*1us in 32:32 format*/
extern uint64_t TIMER_USEC;

/*True if timer a expires before timer b*/
#define TIMER_LESS_THAN(a, b) ((int32_t)((a)->ts_integer - (b)->ts_integer) <= 0)
/*True if timer a expires before 32 bit integer timestamp b*/
#define TIMER_LESS_THAN_VAL(a, b) ((int32_t)((a)->ts_integer - (b)) <= 0)
/*True if 32 bit integer timestamp a expires before 32 bit integer timestamp b*/
#define TIMER_VAL_LESS_THAN_VAL(a, b) ((int32_t)((a) - (b)) <= 0)

/*Advance timer by delay, specified in 32:32 format. This should be used to
  resume a recurring timer in a callback routine*/
static inline void timer_advance_u64(pc_timer_t *timer, uint64_t delay)
{
	uint32_t int_delay = delay >> 32;
	uint32_t frac_delay = delay & 0xffffffff;

	if ((frac_delay + timer->ts_frac) < frac_delay)
		timer->ts_integer++;
	timer->ts_frac += frac_delay;
	timer->ts_integer += int_delay;

	timer_enable(timer);
}

/*Set a timer to the given delay, specified in 32:32 format. This should be used
  when starting a timer*/
static inline void timer_set_delay_u64(pc_timer_t *timer, uint64_t delay)
{
	uint32_t int_delay = delay >> 32;
	uint32_t frac_delay = delay & 0xffffffff;

	timer->ts_frac = frac_delay;
	timer->ts_integer = int_delay + (uint32_t)tsc;

	timer_enable(timer);
}

/*True if timer currently enabled*/
static inline int timer_is_enabled(pc_timer_t *timer)
{
	return timer->enabled;
}

/*Return integer timestamp of timer*/
static inline uint32_t timer_get_ts_int(pc_timer_t *timer)
{
	return timer->ts_integer;
}

/*Return remaining time before timer expires, in us. If the timer has already
  expired then return 0*/
static inline uint32_t timer_get_remaining_us(pc_timer_t *timer)
{
	if (timer->enabled)
	{
		int64_t remaining = (((uint64_t)timer->ts_integer << 32) | timer->ts_frac) - (tsc << 32);

		if (remaining < 0)
			return 0;
		return remaining / TIMER_USEC;
	}

	return 0;
}

/*Return remaining time before timer expires, in 32:32 timestamp format. If the
  timer has already expired then return 0*/
static inline uint64_t timer_get_remaining_u64(pc_timer_t *timer)
{
	if (timer->enabled)
	{
		int64_t remaining = (((uint64_t)timer->ts_integer << 32) | timer->ts_frac) - (tsc << 32);

		if (remaining < 0)
			return 0;
		return remaining;
	}

	return 0;
}

/*Set timer callback function*/
static inline void timer_set_callback(pc_timer_t *timer, void (*callback)(void *p))
{
        timer->callback = callback;
}
/*Set timer private data*/
static inline void timer_set_p(pc_timer_t *timer, void *p)
{
        timer->p = p;
}

#endif /*_TIMER_H_*/
