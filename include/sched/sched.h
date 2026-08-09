#ifndef __SCHED_H__
#define __SCHED_H__

#include "types.h"
#include "dlist.h"

/*
 * Scheduling entity.
 * The scheduler manipulates this structure only; it does not care about
 * the rest of the task state.  Embedded inside struct task_struct.
 */
struct sched_entity {
	struct dlist_node run_node;
	unsigned int time_slice;	/* ticks remaining in this quantum */
};

/* Forward declaration: execution entity lives in task.h. */
struct task_struct;

/* Currently running task on this CPU. */
extern struct task_struct *current;

void sched_init(struct task_struct *idle_task);
void schedule(void);
void scheduler_tick(void);
void finish_task_switch(struct task_struct *prev);
void schedule_if_needed(void);

/* Add/remove tasks from the run queue. */
void sched_enqueue(struct task_struct *task);
void sched_dequeue(struct task_struct *task);
struct task_struct *sched_pick_next(void);

/* Wake a blocked task from interrupt or task context. */
void sched_wake_up(struct task_struct *task);

/* Architecture context switch (bottom half in switch.S). */
extern struct task_struct *__switch_to(struct task_struct *prev,
				       struct task_struct *next);

/*
 * switch_to() wraps the raw context switch with a normal C call to __switch_to
 * so the compiler handles the caller-saved register clobbers.  An empty volatile
 * asm with a callee-saved clobber list tells the compiler that __switch_to also
 * replaces x19-x30/sp with next's context.
 */
static inline __attribute__((always_inline)) struct task_struct *
switch_to(struct task_struct *prev, struct task_struct *next)
{
	struct task_struct *last;

	last = __switch_to(prev, next);

	__asm__ __volatile__("" ::: "x19", "x20", "x21", "x22", "x23", "x24",
			      "x25", "x26", "x27", "x28", "x29", "x30",
			      "memory");

	return last;
}

#endif /* __SCHED_H__ */
