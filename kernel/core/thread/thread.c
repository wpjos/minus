#include "thread_priv.h"
#include "subsys.h"
#include "mm.h"
#include "memory.h"
#include "pt_regs.h"
#include "string.h"
#include "printk.h"
#include "irqflags.h"

/*
 * Thread lifecycle and the context switch, owned by core.
 *
 * The switch is the one place in the kernel where one C frame hands the
 * CPU to a different C frame, so it has one rule and one helper:
 *
 *   RULE - after arch_context_switch() "returns" we execute as the
 *   resumed thread, on ITS stack, inside ITS previous call of this same
 *   function.  No stack local assigned before the switch may be read
 *   after it.  The one value that must cross the switch - the thread
 *   that was just descheduled - travels through the per-CPU
 *   last_switched instead.  finish_switch() is noinline so its frame is
 *   built fresh on the new stack; there is nothing stale to misuse.
 *
 * Interrupt convention: schedule() may be entered with local IRQs in
 * any state; it disables them across the decision and the switch, and
 * returns with them ENABLED, in whoever is resumed.  No DAIF state is
 * saved across the switch - that is what makes the rule above safe.
 * Exception return paths re-establish their own state from SPSR.
 */

extern void ret_to_user(void);

/* Currently running thread on this CPU.  Starts as the boot idle thread. */
struct kthread *current_thread;

/* The thread this CPU just switched away from (crosses the switch). */
static struct kthread *last_switched;

/* Registered by proc: release a dying thread's owner resources. */
static void (*exit_notify_fn)(cap_t owner_cap);

/*
 * The idle thread: statically allocated, runs on the boot stack until
 * the first switch saves a context into it.  Its envelope is empty -
 * every slot CAP_NULL - so subscribers mirror the "no task" state.
 * Its scheduling context is initialized by thread_subsys_init().
 */
static struct kthread idle_kthread = {
	.cap	= CAP_NULL,
	.env	= { CAP_NULL, CAP_NULL, CAP_NULL, CAP_NULL },
	.kstack	= NULL,
	.name	= "idle",
};

static void thread_free(struct kthread *t)
{
	if (t->cap != CAP_NULL)
		cap_free(t->cap);
	kfree_pages(t->kstack);
	kfree(t);
}

cap_t thread_create(const struct thread_create_args *args)
{
	struct kthread *t;
	struct pt_regs *regs;

	t = kzalloc(sizeof(*t));
	if (!t)
		return CAP_NULL;

	t->kstack = kzalloc_pages(PAGE_SIZE);
	if (!t->kstack) {
		kfree(t);
		return CAP_NULL;
	}

	t->cap = cap_alloc(t, cap_rights(CAP_R_CTL | CAP_R_READ));
	if (t->cap == CAP_NULL) {
		kfree_pages(t->kstack);
		kfree(t);
		return CAP_NULL;
	}

	/*
	 * First-run frame: pt_regs at the top of the kernel stack, the
	 * switcher resuming at ret_to_user which pops it into EL0.
	 */
	regs = (struct pt_regs *)((char *)t->kstack + PAGE_SIZE - sizeof(*regs));
	regs->elr = args->entry;
	regs->sp_el0 = args->sp;
	regs->spsr = 0;
	t->ti.sp = (uint64_t)regs;
	t->ti.lr = (uint64_t)ret_to_user;

	strncpy(t->name, args->name ? args->name : "?", sizeof(t->name) - 1);
	sched_entity_init(&t->sched, args->prio);

	t->env.thread_cap = t->cap;
	t->env.owner_cap = args->owner_cap;
	t->env.files_cap = args->files_cap;
	t->env.vspace_cap = args->vspace_cap;

	return t->cap;
}

/*
 * Post-switch bookkeeping: retire the thread this CPU switched away
 * from.  Runs in the resumed thread's context, on its stack, with IRQs
 * off.  The exit notify callback must not block (core/thread.h).
 */
static void __attribute__((noinline)) finish_switch(struct kthread *prev)
{
	if (!prev)
		return;
	if (prev->sched.state == SCHED_DEAD) {
		if (exit_notify_fn && prev->env.owner_cap != CAP_NULL)
			exit_notify_fn(prev->env.owner_cap);
		thread_free(prev);
	}
}

/*
 * Pick and switch.  Called with local IRQs off.  When the switch
 * happens, control "returns" in the resumed thread's own previous
 * switch_away() - which then finishes ITS predecessor, re-enables
 * IRQs and unwinds its own callers.  The rule above applies.
 */
static void switch_away(void)
{
	struct kthread *prev = current_thread;
	struct kthread *next = sched_pick_next();

	if (next == prev)
		return;

	last_switched = prev;
	current_thread = next;
	arch_context_switch(&prev->ti, &next->ti, &prev->env, &next->env);

	finish_switch(last_switched);
}

void schedule(void)
{
	local_irq_disable();
	switch_away();
	local_irq_enable();
}

void schedule_if_needed(void)
{
	if (sched_need_resched())
		schedule();
}

void thread_wake(cap_t thread_cap)
{
	struct kthread *t;

	t = (struct kthread *)cap_resolve(thread_cap, cap_rights(CAP_R_CTL));
	if (t)
		sched_wake(t);
}

void thread_block_current(void)
{
	current_thread->sched.state = SCHED_BLOCKED;
}

void thread_replace_current(cap_t replacement)
{
	struct kthread *rep = NULL;

	local_irq_disable();

	if (replacement != CAP_NULL) {
		rep = (struct kthread *)cap_resolve(replacement,
						    cap_rights(CAP_R_CTL));
		if (rep)
			sched_enqueue_ready(rep);
	}

	sched_mark_dead(current_thread);
	switch_away();

	/* The idle thread is always queued, so we never come back. */
	while (1)
		__asm__ volatile("wfi");
}

void thread_exit_current(void)
{
	thread_replace_current(CAP_NULL);
}

cap_t thread_current_cap(void)
{
	return current_thread->cap;
}

void thread_tick(void)
{
	uint32_t flags;

	local_irq_save(flags);
	sched_tick();
	local_irq_restore(flags);
}

void thread_exit_notify_register(void (*fn)(cap_t owner_cap))
{
	exit_notify_fn = fn;
}

static int thread_subsys_init(void)
{
	sched_init();
	current_thread = &idle_kthread;
	sched_set_idle(&idle_kthread);
	return 0;
}

subsys_register(thread, SUBSYS_LEVEL_SCHED, thread_subsys_init);
