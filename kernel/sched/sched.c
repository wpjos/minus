#include "sched.h"
#include "task.h"
#include "vspace.h"
#include "mmu.h"
#include "memory.h"
#include "irqflags.h"

/* Per-priority FIFO run queues.  Index 0 is highest priority. */
static struct dlist_node run_queue[SCHED_PRIO_MAX];
static struct dlist_node dead_tasks = DLIST_NODE_INIT(dead_tasks);

static int runqueue_empty(void)
{
	unsigned int prio;

	for (prio = 0; prio < SCHED_PRIO_MAX; prio++)
		if (!dlist_empty(&run_queue[prio]))
			return 0;
	return 1;
}

void sched_init(void)
{
	unsigned int prio;

	for (prio = 0; prio < SCHED_PRIO_MAX; prio++)
		dlist_init(&run_queue[prio]);
}

void sched_enqueue(struct task_struct *task)
{
	uint32_t flags;
	unsigned int prio;

	local_irq_save(flags);

	/*
	 * If the task is already linked to a queue (run queue, zombie list,
	 * etc.), do not enqueue it again.  This makes it safe to wake a task
	 * from interrupt context even if the task is concurrently being
	 * enqueued by schedule().
	 */
	if (task->se.run_node.next != &task->se.run_node) {
		local_irq_restore(flags);
		return;
	}

	prio = task->se.priority;
	task->state = TASK_READY;
	task->se.time_slice = SCHED_SLICE_TICKS;
	dlist_add_tail(&run_queue[prio], &task->se.run_node);
	local_irq_restore(flags);
}

void sched_wake_up(struct task_struct *task)
{
	uint32_t flags;

	if (!task)
		return;

	local_irq_save(flags);
	sched_enqueue(task);
	local_irq_restore(flags);
}

void sched_dequeue(struct task_struct *task)
{
	uint32_t flags;

	local_irq_save(flags);
	/* Defensive: silently accept tasks already outside the run queue. */
	if (task->se.run_node.next == &task->se.run_node)
		goto out;
	dlist_del(&task->se.run_node);
out:
	local_irq_restore(flags);
}

struct task_struct *sched_pick_next(void)
{
	unsigned int prio;

	for (prio = 0; prio < SCHED_PRIO_MAX; prio++) {
		if (!dlist_empty(&run_queue[prio]))
			return dlist_first_entry(&run_queue[prio],
						 struct task_struct, se.run_node);
	}
	return NULL;
}

void scheduler_tick(void)
{
	/* No point preempting if there is no other runnable task. */
	if (runqueue_empty())
		return;

	if (current->se.time_slice > 0) {
		current->se.time_slice--;
	}

	/* Time slice exhausted: request reschedule, but do not call schedule()
	 * from interrupt context.  The actual switch happens when returning to
	 * user space (el0_sync / el0_irq).
	 */
	if (current->se.time_slice == 0)
		current->need_resched = 1;
}

/*
 * Move an exited task to the zombie list.  The actual memory is released
 * outside the scheduler critical section (after interrupts are re-enabled)
 * to keep the scheduling path short and deterministic.
 */
static void make_task_zombie(struct task_struct *task)
{
	task->state = TASK_ZOMBIE;
	dlist_add_tail(&dead_tasks, &task->se.run_node);
}

/*
 * Release every task on the zombie list.  Must be called with interrupts
 * enabled and no scheduler spinlock held.
 */
static void release_dead_tasks(void)
{
	struct task_struct *task;
	struct dlist_node *node;

	while (!dlist_empty(&dead_tasks)) {
		node = dead_tasks.next;
		dlist_del(node);
		task = container_of(node, struct task_struct, se.run_node);
		task_free(task);
	}
}

/*
 * finish_task_switch - scheduler housekeeping executed on the incoming task
 * after a context switch.  If the outgoing task exited, queue it for delayed
 * release instead of freeing memory inside the scheduler critical section.
 */
void finish_task_switch(struct task_struct *prev)
{
	if (prev && prev->state == TASK_DEAD)
		make_task_zombie(prev);
}

void schedule(void)
{
	struct task_struct *prev = current;
	struct task_struct *next;
	uint32_t flags;

	local_irq_save(flags);

	/*
	 * Take prev out of the run queue and, if it is still runnable, put it
	 * at the tail of its priority queue.  This guarantees round-robin among
	 * tasks of the same priority.
	 */
	sched_dequeue(prev);
	if (prev->state == TASK_RUNNING)
		sched_enqueue(prev);

	next = sched_pick_next();
	sched_dequeue(next);
	next->state = TASK_RUNNING;
	next->need_resched = 0;

	if (prev == next) {
		local_irq_restore(flags);
		return;
	}

	switch_vspace(prev->vspace, next->vspace);
	prev = switch_to(prev, next);
	finish_task_switch(prev);

	local_irq_restore(flags);

	/* Now that we are outside the scheduler critical section, free zombies. */
	release_dead_tasks();
}

/*
 * schedule_if_needed - invoke the scheduler if a reschedule was requested.
 * Call this only when about to return to user space (el0_sync / el0_irq).
 */
void schedule_if_needed(void)
{
	if (current->need_resched)
		schedule();
}
