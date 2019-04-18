#include "ibm.h"

#include "timer.h"

uint64_t TIMER_USEC;
uint32_t timer_target;

/*Enabled timers are stored in a linked list, with the first timer to expire at
  the head.*/
static pc_timer_t *timer_head = NULL;

void timer_enable(pc_timer_t *timer)
{
	pc_timer_t *timer_node = timer_head;

//	pclog("timer->enable %p %i\n", timer, timer->enabled);
	if (timer->enabled)
		timer_disable(timer);

	if (timer->next || timer->prev)
		fatal("timer_enable - timer->next\n");

	timer->enabled = 1;

	/*List currently empty - add to head*/
	if (!timer_head)
	{
		timer_head = timer;
		timer->next = timer->prev = NULL;
        	timer_target = timer_head->ts_integer;
		return;
	}

	timer_node = timer_head;

	while (1)
	{
		/*Timer expires before timer_node. Add to list in front of timer_node*/
		if (TIMER_LESS_THAN(timer, timer_node))
		{
			timer->next = timer_node;
			timer->prev = timer_node->prev;
			timer_node->prev = timer;
			if (timer->prev)
				timer->prev->next = timer;
			else
			{
				timer_head = timer;
				timer_target = timer_head->ts_integer;
                        }
			return;
		}

		/*timer_node is last in the list. Add timer to end of list*/
		if (!timer_node->next)
		{
			timer_node->next = timer;
			timer->prev = timer_node;
			return;
		}

		timer_node = timer_node->next;
	}
}
void timer_disable(pc_timer_t *timer)
{
//	pclog("timer->disable %p\n", timer);
	if (!timer->enabled)
		return;

	if (!timer->next && !timer->prev && timer != timer_head)
		fatal("timer_disable - !timer->next\n");

	timer->enabled = 0;

	if (timer->prev)
		timer->prev->next = timer->next;
	else
		timer_head = timer->next;
	if (timer->next)
		timer->next->prev = timer->prev;
	timer->prev = timer->next = NULL;
}
static void timer_remove_head()
{
	if (timer_head)
	{
		pc_timer_t *timer = timer_head;
//		pclog("timer_remove_head %p %p\n", timer_head, timer_head->next);
		timer_head = timer->next;
		timer_head->prev = NULL;
		timer->next = timer->prev = NULL;
		timer->enabled = 0;
	}
}

void timer_process()
{
	if (!timer_head)
		return;

	while (1)
	{
		pc_timer_t *timer = timer_head;

		if (!TIMER_LESS_THAN_VAL(timer, (uint32_t)tsc))
			break;

		timer_remove_head();
		timer->callback(timer->p);
	}

	timer_target = timer_head->ts_integer;
}

void timer_reset()
{
	pclog("timer_reset\n");
	timer_target = 0;
	tsc = 0;
	timer_head = NULL;
}

void timer_add(pc_timer_t *timer, void (*callback)(void *p), void *p, int start_timer)
{
        memset(timer, 0, sizeof(pc_timer_t));
        
	timer->callback = callback;
	timer->p = p;
	timer->enabled = 0;
	timer->prev = timer->next = NULL;
	if (start_timer)
		timer_set_delay_u64(timer, 0);
}
