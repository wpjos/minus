#ifndef __SWITCH_NOTIFY_H__
#define __SWITCH_NOTIFY_H__

#include "types.h"
#include "cap.h"

/*
 * Context-switch notification bus, provided by core (the microkernel).
 *
 * Servers that need per-CPU mirrors of current-thread state (fdtable,
 * address space, process identity, ...) subscribe with a callback
 * instead of asking proc who is running on every syscall.
 *
 * The bus uses event-carried state transfer: the notification payload
 * is an envelope assembled by core when the thread is created (from the
 * caps its creator passed to thread_create()) and delivered by the
 * architecture switcher at the exact switch moment - before any of the
 * new thread's state is restored, so no syscall or IRQ in the new
 * context can observe a half-updated mirror.  Subscribers read their
 * slot straight out of the envelope and resolve it against their own
 * objects; nobody calls back into proc on this path.
 *
 * The envelope speaks capabilities only - the same boundary discipline
 * as the service tables.  Slot semantics belong to the owning server:
 * files_cap is defined by fs, vspace_cap by mm, owner_cap by proc; core
 * just transports them.
 *
 * Callbacks run on the new thread's stack with IRQs off and must stay
 * cheap (a resolve plus a store, at most a TTBR0 flip); they must not
 * block or allocate.  A NULL envelope means "no thread" (early boot /
 * idle context): mirrors reset to their empty state.
 *
 * Registration is boot-time only.  The callback is invoked once
 * immediately with (NULL, NULL) so mirrors start from a defined state.
 *
 * SMP note: mirrors are plain statics today (single CPU).  When current
 * becomes per-CPU, turn each mirror into a per-CPU variable; each CPU's
 * switch publishes its own envelope, so the update path is unchanged.
 */
struct switch_envelope {
	cap_t thread_cap;	/* kthread identity, owned by core */
	cap_t owner_cap;	/* process identity, slot owned by proc */
	cap_t files_cap;	/* fdtable, slot owned by fs */
	cap_t vspace_cap;	/* address space, slot owned by mm */
};

typedef void (*switch_notify_fn)(const struct switch_envelope *prev,
				 const struct switch_envelope *next);

/* Boot-time only.  Returns 0 on success, -ENOMEM if the table is full. */
int switch_notifier_register(switch_notify_fn fn);

/*
 * Publish a completed context switch: @prev_env describes the task that
 * ran before, @next_env the task running now.  IRQs are off.  Either may
 * be NULL to mean "no task".
 */
void switch_notify(const struct switch_envelope *prev,
		   const struct switch_envelope *next);

/*
 * Architecture switch hook (called from arch/switch.S).  Defined as a
 * separate symbol so the asm layer has a stable entry point; today it is
 * just switch_notify().  Runs in the new task's context after its stack
 * is established, before its registers are restored - so every switch
 * path (first run, preemption, exit) publishes exactly once, and no
 * switch caller needs to know about the bus.  Placed here rather than in
 * the switcher's schedule(): a fresh task resumes at ret_to_user and
 * never returns into schedule(), so a post-switch publish there never
 * fires for the first switch.
 */
void switch_finish(const struct switch_envelope *prev,
		   const struct switch_envelope *next);

#endif /* __SWITCH_NOTIFY_H__ */
