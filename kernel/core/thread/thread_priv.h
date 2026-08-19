#ifndef __THREAD_PRIV_H__
#define __THREAD_PRIV_H__

/*
 * Private shared declarations of core's thread/sched module.
 *
 * struct kthread and struct sched_entity are core-internal: they are
 * shared between thread.c (lifecycle + switch) and sched.c (runqueue
 * policy) and must not leak into include/ - outside this directory
 * threads are named by capability only.
 */

#include "types.h"
#include "dlist.h"
#include "cap.h"
#include "thread_info.h"	/* include/arch: saved register context */
#include "switch_notify.h"	/* include/service: switch_envelope */
#include "thread.h"		/* include/core: public thread API */

/* Scheduling states - private to the scheduler. */
enum sched_state {
	SCHED_READY,		/* on a run queue */
	SCHED_RUNNING,		/* current on this CPU, not on any run queue */
	SCHED_BLOCKED,		/* waiting for an event */
	SCHED_DEAD,		/* exited, awaiting post-switch free */
};

/* Per-thread scheduling context, owned by sched.c. */
struct sched_entity {
	struct dlist_node	run_node;	/* run queue linkage */
	int			prio;
	unsigned int		slice;		/* remaining tick budget */
	enum sched_state		state;
};

/* Execution thread - core's fundamental schedulable object. */
struct kthread {
	struct thread_info	ti;		/* must be first (switch.S) */
	struct sched_entity	sched;
	cap_t			cap;		/* self handle */
	struct switch_envelope	env;		/* caps published on every switch */
	void			*kstack;	/* PAGE_SIZE kernel stack */
	char			name[16];	/* debug label */
};

/* Currently running thread on this CPU. */
extern struct kthread *current_thread;

/*
 * Architecture context switch, provided by core/arch (switch.S).
 *
 * Switches from @prev's context to @next's: saves prev's callee-saved
 * registers and kernel stack, publishes the switch through the
 * notification bus with the two envelopes, then resumes next.  When it
 * "returns", execution continues in next's context, on next's stack,
 * inside next's own previous call - so callers may not touch any stack
 * local assigned before the call; only globals / per-CPU state survive
 * the switch (see thread.c's finish_switch for the pattern).
 *
 * Callers run with local IRQs off and must have established the new
 * current (current_thread = next) before calling.
 */
void arch_context_switch(struct thread_info *prev, struct thread_info *next,
			 const struct switch_envelope *prev_env,
			 const struct switch_envelope *next_env);

/* --- sched.c policy entry points; all called with local IRQs off --- */

void sched_init(void);
void sched_set_idle(struct kthread *idle);
void sched_entity_init(struct sched_entity *se, int prio);
void sched_enqueue_ready(struct kthread *t);
void sched_wake(struct kthread *t);
void sched_mark_dead(struct kthread *t);
struct kthread *sched_pick_next(void);
int sched_need_resched(void);
void sched_tick(void);

#endif /* __THREAD_PRIV_H__ */
