#include "thread_priv.h"
#include "subsys.h"

/*
 * Priority FIFO scheduler - pure run-queue policy.
 *
 * This is core-internal code, not a service: scheduling stays in the
 * kernel in both the monolithic and the micro-kernel build (the
 * recursive-scheduling problem is why every L4-family system keeps it
 * here).  Everything it touches is a kthread; thread.c owns lifecycle
 * and the switch itself.
 *
 * Invariant: a SCHED_RUNNING thread is never on a run queue; it sits at
 * the head of its CPU.  pick_next() re-queues it round-robin if it is
 * still runnable, so a running thread only leaves the queues it was
 * never in.  The idle thread is permanently queued at SCHED_PRIO_IDLE,
 * so pick_next() always has someone to return.
 */

#define SCHED_SLICE_TICKS	10	/* timer ticks per time slice */
#define SCHED_PRIO_MAX		32	/* priority levels, lower = higher */
#define SCHED_PRIO_IDLE		(SCHED_PRIO_MAX - 1)

/* Per-priority FIFO run queues.  Index 0 is highest priority. */
static struct dlist_node run_queue[SCHED_PRIO_MAX];

/* Set when the current thread's slice expired and a switch is due. */
static int need_resched_flag;

/* The permanently-queued idle thread (see thread.c for its lifetime). */
static struct kthread *idle_thread;

void sched_init(void)
{
	int prio;

	for (prio = 0; prio < SCHED_PRIO_MAX; prio++)
		dlist_init(&run_queue[prio]);
	need_resched_flag = 0;
	idle_thread = NULL;
}

/* Remember the idle thread and put it on its queue, where it stays. */
void sched_set_idle(struct kthread *idle)
{
	idle_thread = idle;
	sched_entity_init(&idle->sched, SCHED_PRIO_IDLE);
	idle->sched.state = SCHED_READY;
	sched_enqueue_ready(idle);
}

void sched_entity_init(struct sched_entity *se, int prio)
{
	se->prio = prio;
	se->slice = SCHED_SLICE_TICKS;
	se->state = SCHED_BLOCKED;	/* not yet runnable */
	dlist_init(&se->run_node);
}

static int queued(const struct sched_entity *se)
{
	return se->run_node.next != &se->run_node;
}

void sched_enqueue_ready(struct kthread *t)
{
	if (queued(&t->sched))
		return;
	t->sched.state = SCHED_READY;
	dlist_add_tail(&run_queue[t->sched.prio], &t->sched.run_node);
}

void sched_wake(struct kthread *t)
{
	/*
	 * A RUNNING target re-checks its condition before blocking, a
	 * READY one is already queued - only BLOCKED needs the queue.
	 */
	if (t == current_thread || t->sched.state != SCHED_BLOCKED)
		return;
	sched_enqueue_ready(t);
}

void sched_mark_dead(struct kthread *t)
{
	if (queued(&t->sched)) {
		dlist_del(&t->sched.run_node);
		dlist_init(&t->sched.run_node);
	}
	t->sched.state = SCHED_DEAD;
}

static struct kthread *pick_highest(void)
{
	int prio;

	for (prio = 0; prio < SCHED_PRIO_MAX; prio++) {
		if (!dlist_empty(&run_queue[prio]))
			return dlist_first_entry(&run_queue[prio],
						 struct kthread, sched.run_node);
	}
	return NULL;
}

/*
 * Select the next thread to run.  The current thread, if still runnable,
 * is re-queued first so it competes round-robin with its priority class
 * (and stays SCHED_READY on its queue when it loses to a higher
 * priority).  Never returns NULL: the idle thread is always queued.
 * Called with local IRQs off.
 */
struct kthread *sched_pick_next(void)
{
	struct kthread *cur = current_thread;
	struct kthread *next;

	need_resched_flag = 0;

	if (cur && cur->sched.state == SCHED_RUNNING)
		sched_enqueue_ready(cur);

	next = pick_highest();
	dlist_del(&next->sched.run_node);
	dlist_init(&next->sched.run_node);
	next->sched.slice = SCHED_SLICE_TICKS;
	next->sched.state = SCHED_RUNNING;

	return next;
}

int sched_need_resched(void)
{
	return need_resched_flag;
}

/* Timer tick: charge the current thread's slice.  IRQs are off. */
void sched_tick(void)
{
	struct kthread *cur = current_thread;
	struct kthread *head;

	if (!cur || cur == idle_thread)
		return;

	/* No point preempting when nobody else is runnable. */
	head = pick_highest();
	if (!head || head == cur)
		return;

	if (cur->sched.slice > 0)
		cur->sched.slice--;
	if (cur->sched.slice == 0)
		need_resched_flag = 1;
}
