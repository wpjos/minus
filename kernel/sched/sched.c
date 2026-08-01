#include "sched.h"
#include "task.h"
#include "mmu.h"
#include "memory.h"
#include "irqflags.h"
#include "printk.h"

/* Number of timer ticks a task may run before being preempted. */
#define SCHED_SLICE_TICKS	10

struct task_struct *current;
static struct dlist_node run_queue;
static struct dlist_node dead_tasks;
static struct task_struct *idle_task;

void sched_init(struct task_struct *idle_task_arg)
{
	dlist_init(&run_queue);
	dlist_init(&dead_tasks);
	current = idle_task_arg;
	idle_task = idle_task_arg;
}

void sched_enqueue(struct task_struct *task)
{
	uint32_t flags;

	local_irq_save(flags);
	task->state = TASK_RUNNING;
	task->se.time_slice = SCHED_SLICE_TICKS;
	dlist_add_tail(&run_queue, &task->se.run_node);
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
	if (dlist_empty(&run_queue))
		return NULL;
	return dlist_first_entry(&run_queue, struct task_struct, se.run_node);
}

void scheduler_tick(void)
{
	/* Idle has no time slice; rely on new tasks or explicit yields. */
	if (current->state == TASK_IDLE)
		return;

	if (current->se.time_slice > 0)
		current->se.time_slice--;

	/* Time slice exhausted: preempt directly from the timer handler. */
	if (current->se.time_slice == 0)
		schedule();
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

	next = sched_pick_next();
	if (!next)
		next = idle_task;

	if (prev == next) {
		local_irq_restore(flags);
		return;
	}

	sched_dequeue(next);

	/* Put the outgoing task back if it is still runnable. */
	if (prev->state == TASK_RUNNING)
		sched_enqueue(prev);

	/* Switch user page tables, or clear TTBR0 when entering idle/kernel mode. */
	if (next->mm) {
		if (next->mm != prev->mm)
			mmu_switch_pgd(TTBR0_EL1,
				       __VA_PA__((uint64_t)next->mm->pgd));
	} else if (prev->mm) {
		mmu_clear_ttbr0();
	}

	/*
	 * switch_to() returns the task we switched away from.  After the stack
	 * switch we are running on 'next' context, so it is safe to free 'prev'
	 * if it has exited - but we only queue it here and release memory after
	 * re-enabling interrupts.
	 */
	prev = switch_to(prev, next);
	finish_task_switch(prev);

	local_irq_restore(flags);

	/* Now that we are outside the scheduler critical section, free zombies. */
	release_dead_tasks();
}
