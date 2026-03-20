#include "ibm.h"

#include "timer.h"

uint64_t TIMER_USEC;
uint32_t timer_target;

/*Enabled timers are stored in a linked list, with the first timer to expire at
  the head.*/
static pc_timer_t *timer_head = NULL;

/*All timers ever registered via timer_add are tracked here so we can
  safely disable them all on reset, even if the caller forgot to call
  timer_disable before freeing the containing struct.*/
#define MAX_TIMERS 256
static pc_timer_t *all_timers[MAX_TIMERS];
static int num_timers = 0;

static inline int timer_valid(pc_timer_t *timer) {
        return timer && timer->magic == TIMER_MAGIC;
}

void timer_enable(pc_timer_t *timer) {
        pc_timer_t *timer_node;

        if (!timer_valid(timer))
                return;

        if (timer->enabled)
                timer_disable(timer);

        if (timer->next || timer->prev)
                fatal("timer_enable - timer->next\n");

        timer->enabled = 1;

        /*List currently empty - add to head*/
        if (!timer_head) {
                timer_head = timer;
                timer->next = timer->prev = NULL;
                timer_target = timer_head->ts_integer;
                return;
        }

        timer_node = timer_head;

        while (1) {
                if (!timer_valid(timer_node)) {
                        /* Corrupted list - truncate here */
                        pclog("timer_enable: corrupted node %p in list, truncating\n", timer_node);
                        /* Find the previous valid node and cap the list */
                        if (timer_node == timer_head) {
                                timer_head = timer;
                                timer->next = timer->prev = NULL;
                                timer_target = timer->ts_integer;
                        } else {
                                /* The prev pointer of the corrupt node is unreliable,
                                   so just append to head */
                                timer->next = timer_head;
                                timer->prev = NULL;
                                timer_head->prev = timer;
                                timer_head = timer;
                                timer_target = timer->ts_integer;
                        }
                        return;
                }

                /*Timer expires before timer_node. Add to list in front of timer_node*/
                if (TIMER_LESS_THAN(timer, timer_node)) {
                        timer->next = timer_node;
                        timer->prev = timer_node->prev;
                        timer_node->prev = timer;
                        if (timer->prev)
                                timer->prev->next = timer;
                        else {
                                timer_head = timer;
                                timer_target = timer_head->ts_integer;
                        }
                        return;
                }

                /*timer_node is last in the list. Add timer to end of list*/
                if (!timer_node->next) {
                        timer_node->next = timer;
                        timer->prev = timer_node;
                        return;
                }

                timer_node = timer_node->next;
        }
}
void timer_disable(pc_timer_t *timer) {
        if (!timer_valid(timer))
                return;

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
static void timer_remove_head() {
        if (timer_head) {
                pc_timer_t *timer = timer_head;
                timer_head = timer->next;
                if (timer_head) {
                        if (!timer_valid(timer_head)) {
                                pclog("timer_remove_head: next node %p is corrupted, clearing list\n", timer_head);
                                timer_head = NULL;
                        } else {
                                timer_head->prev = NULL;
                        }
                }
                timer->next = timer->prev = NULL;
                timer->enabled = 0;
        }
}

void timer_process() {
        while (timer_head) {
                pc_timer_t *timer = timer_head;

                if (!timer_valid(timer)) {
                        pclog("timer_process: head %p is corrupted, clearing list\n", timer);
                        timer_head = NULL;
                        break;
                }

                if (!TIMER_LESS_THAN_VAL(timer, (uint32_t)tsc))
                        break;

                timer_remove_head();
                timer->callback(timer->p);
        }

        if (timer_head)
                timer_target = timer_head->ts_integer;
}

void timer_reset() {
        int i;

        pclog("timer_reset\n");

        /* Disable every registered timer so their next/prev pointers are
           cleaned up before device_close_all() frees the containing structs. */
        for (i = 0; i < num_timers; i++) {
                pc_timer_t *t = all_timers[i];
                if (t && t->magic == TIMER_MAGIC) {
                        t->enabled = 0;
                        t->prev = t->next = NULL;
                        t->magic = 0; /* Invalidate so freed memory is detectable */
                }
        }

        timer_target = 0;
        tsc = 0;
        timer_head = NULL;
        num_timers = 0;
}

void timer_add(pc_timer_t *timer, void (*callback)(void *p), void *p, int start_timer) {
        /* If this timer is still in the active list, disable it first */
        if (timer->magic == TIMER_MAGIC && timer->enabled)
                timer_disable(timer);

        memset(timer, 0, sizeof(pc_timer_t));

        timer->magic = TIMER_MAGIC;
        timer->callback = callback;
        timer->p = p;
        timer->enabled = 0;
        timer->prev = timer->next = NULL;

        /* Track this timer for cleanup on reset */
        if (num_timers < MAX_TIMERS)
                all_timers[num_timers++] = timer;

        if (start_timer)
                timer_set_delay_u64(timer, 0);
}
