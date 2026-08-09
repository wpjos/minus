#ifndef __WAIT_H__
#define __WAIT_H__

#include "types.h"
#include "irqflags.h"
#include "task.h"
#include "sched.h"

/*
 * Single-waiter wait queue head.  Only one task waits on this queue at a
 * time, which is sufficient for the console UART in this kernel.
 */
struct wait_queue_head {
	struct task_struct *task;
};

#define DECLARE_WAIT_QUEUE_HEAD(name) \
	struct wait_queue_head name = { NULL }

/*
 * wait_event - sleep until @condition becomes true.
 *
 * The condition is checked with local IRQs disabled to avoid the lost-wakeup
 * race where the wake_up happens between the condition check and registering
 * the waiter.
 */
#define wait_event(wq, condition)                           \
	do {                                                    \
		uint32_t __flags;                                   \
		for (;;) {                                          \
			local_irq_save(__flags);                        \
			if (condition) {                                \
				local_irq_restore(__flags);                 \
				break;                                      \
			}                                               \
			(wq).task = current;                            \
			current->state = TASK_BLOCKED;                  \
			local_irq_restore(__flags);                     \
			schedule();                                     \
		}                                                   \
	} while (0)

/*
 * wake_up - wake the task sleeping on @wq, if any.
 */
#define wake_up(wq)                                         \
	do {                                                    \
		uint32_t __flags;                                   \
		struct task_struct *__w;                            \
		local_irq_save(__flags);                            \
		__w = (wq).task;                                    \
		if (__w) {                                          \
			(wq).task = NULL;                               \
			sched_wake_up(__w);                             \
		}                                                   \
		local_irq_restore(__flags);                         \
	} while (0)

#endif /* __WAIT_H__ */
