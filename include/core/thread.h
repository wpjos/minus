#ifndef __CORE_THREAD_H__
#define __CORE_THREAD_H__

#include "types.h"
#include "cap.h"

/*
 * Core thread API - execution threads and scheduling.
 *
 * Threads (kthread) are core's own objects, like caps and the switch
 * bus: the scheduler, the context switcher and thread lifecycle are one
 * module inside the microkernel core, in both the monolithic and the
 * micro-kernel build (every L4-family system keeps scheduling in the
 * kernel).  There is deliberately NO service table here: proc is this
 * API's only real client, and in a micro-kernel build this header is the
 * shape of the thread-create/exit system calls.
 *
 * Threads are named externally only by capability.  The per-subsystem
 * caps a thread carries (vspace, fdtable, owner) ride inside its switch
 * envelope (service/switch_notify.h) and are mirrored to their owning
 * servers on every context switch; nobody asks proc who is running.
 */

/* Scheduling priorities visible at this boundary.  Lower runs first. */
#define SCHED_PRIO_DEFAULT	20

struct thread_create_args {
	const char	*name;		/* debug label, copied */
	int		prio;		/* SCHED_PRIO_* or driver-defined */
	uintptr_t	entry;		/* EL0 entry PC (ret_to_user start) */
	uintptr_t	sp;		/* EL0 stack pointer */
	cap_t		vspace_cap;	/* slot owned by mm, CAP_NULL = none */
	cap_t		files_cap;	/* slot owned by fs, CAP_NULL = none */
	cap_t		owner_cap;	/* slot owned by the creator (proc) */
};

/*
 * Create a thread: kernel stack and first-run frame built, not yet
 * runnable.  Returns its capability or CAP_NULL.  Make it runnable with
 * thread_wake().
 */
cap_t thread_create(const struct thread_create_args *args);

/* Make @thread runnable (first run, or wake from block).  Idempotent. */
void thread_wake(cap_t thread);

/*
 * Mark the current thread blocked.  Must be called with local IRQs off
 * and the wakeup condition already re-checked under that same IRQ-off
 * window - this is the sleeping half of the wait protocol, see
 * core/wait.h.  Follow with schedule().
 */
void thread_block_current(void);

/* Give up the CPU; run another thread chosen by the scheduler. */
void schedule(void);

/* schedule() if the scheduler requested a reschedule. */
void schedule_if_needed(void);

/*
 * Terminate the current thread.  Its kthread (stack included) is freed
 * after the switch by the next context; the creator is notified through
 * the exit notifier below so it can release the resources named in the
 * thread's envelope.  Never returns.
 */
void thread_exit_current(void) __attribute__((noreturn));

/*
 * Terminate the current thread and activate @replacement in one
 * transaction (execve-style image swap: the old image must leave the
 * run queue atomically with the new one entering it).  Never returns.
 */
void thread_replace_current(cap_t replacement) __attribute__((noreturn));

/* Capability of the thread currently running on this CPU. */
cap_t thread_current_cap(void);

/*
 * Timer tick hook for the clock driver: account the current thread's
 * time slice and request a reschedule when it expires.
 */
void thread_tick(void);

/*
 * Exit notification: called with the owner cap of a thread that just
 * died, in the context of the next thread, local IRQs off.  Must not
 * block.  Boot-time registration, one subscriber (proc).
 */
void thread_exit_notify_register(void (*fn)(cap_t owner_cap));

#endif /* __CORE_THREAD_H__ */
