#ifndef __WAIT_H__
#define __WAIT_H__

#include "types.h"
#include "irqflags.h"
#include "cap.h"
#include "thread.h"

/*
 * Single-waiter wait queue head.  Only one thread waits on this queue
 * at a time, which is sufficient for the console UART in this kernel.
 */
struct wait_queue_head {
	cap_t waiter;
};

#define DECLARE_WAIT_QUEUE_HEAD(name) \
	struct wait_queue_head name = { CAP_NULL }

/*
 * wait_event - sleep until @condition becomes true.
 *
 * The whole sleep protocol lives here and in thread_block_current():
 * the condition is checked and the thread marked blocked inside one
 * IRQ-off window, so a wake_up can never be lost between the two.
 * schedule() returns with IRQs enabled in whoever is resumed, and the
 * loop re-disables them before re-checking, so the entry state does
 * not matter (a syscall entry runs masked, an IRQ path already does).
 */
#define wait_event(wq, condition)					\
	do {								\
		for (;;) {						\
			local_irq_disable();				\
			if (condition) {				\
				(wq).waiter = CAP_NULL;			\
				local_irq_enable();			\
				break;					\
			}						\
			(wq).waiter = thread_current_cap();		\
			thread_block_current();				\
			schedule();					\
		}							\
	} while (0)

/*
 * wake_up - wake the thread sleeping on @wq, if any.
 */
#define wake_up(wq)							\
	do {								\
		uint32_t __flags;					\
		cap_t __t;						\
		local_irq_save(__flags);				\
		__t = (wq).waiter;					\
		(wq).waiter = CAP_NULL;					\
		if (__t != CAP_NULL)					\
			thread_wake(__t);				\
		local_irq_restore(__flags);				\
	} while (0)

#endif /* __WAIT_H__ */
