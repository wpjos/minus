#include "process.h"
#include "proc_execve.h"
#include "proc_service.h"
#include "mm.h"
#include "mm_service.h"
#include "fs_service.h"
#include "thread.h"
#include "switch_notify.h"
#include "cap.h"
#include "subsys.h"
#include "svc.h"
#include "string.h"
#include "errno.h"
#include "printk.h"

/*
 * proc: the process server.
 *
 * proc owns process identity and resources (pid, name, address space,
 * fdtable); core owns the thread that executes the image.  The two meet
 * in exactly two places:
 *
 *   - thread_create(): proc hands core the image's entry point and the
 *     caps that will ride in the thread's switch envelope (owner slot
 *     included), and core owns them from there on.
 *   - the exit notifier: when a thread dies, core calls back with the
 *     owner cap so proc releases the image's resources.  This is the
 *     only "callback" in the system, and it is proc-registered - core
 *     has no compile-time knowledge of processes.
 *
 * The current process is a mirror fed by core's switch bus, like mm's
 * and fs's mirrors: nobody pays a service round-trip to learn who is
 * running.
 */

static struct process *current_process;
static int next_pid = 1;

static struct process *process_from_cap(cap_t owner_cap)
{
	if (owner_cap == CAP_NULL)
		return NULL;
	return (struct process *)cap_resolve(owner_cap, cap_rights(CAP_R_READ));
}

static void proc_switch_notify(const struct switch_envelope *prev,
			       const struct switch_envelope *next)
{
	(void)prev;
	current_process = process_from_cap(next ? next->owner_cap : CAP_NULL);
}

struct process *proc_current_process(void)
{
	return current_process;
}

int process_alloc(const char *name, struct process **out)
{
	struct process *p;

	p = kzalloc(sizeof(*p));
	if (!p)
		return -ENOMEM;

	p->self = cap_alloc(p, cap_rights(CAP_R_CTL | CAP_R_READ));
	if (p->self == CAP_NULL) {
		kfree(p);
		return -ENOMEM;
	}

	p->pid = next_pid++;
	strncpy(p->name, name ? name : "?", sizeof(p->name) - 1);
	*out = p;
	return 0;
}

/* Release every resource the image owns.  Called after its thread died. */
void process_free(struct process *p)
{
	if (!p)
		return;

	if (p->files != CAP_NULL)
		fs_call(files_destroy, p->files);
	if (p->vspace != CAP_NULL)
		mm_call(vspace_destroy, p->vspace);
	if (p->self != CAP_NULL)
		cap_free(p->self);
	kfree(p);
}

/* Exit notifier: runs in the next thread's context, IRQs off, no blocking. */
static void proc_thread_exit(cap_t owner_cap)
{
	process_free(process_from_cap(owner_cap));
}

static const struct proc_service g_proc_service = {
	.spawn = proc_spawn,
	.execve = proc_execve,
};

static int proc_subsys_init(void)
{
	int ret;

	/* Mirror the current process from core's switch bus. */
	ret = switch_notifier_register(proc_switch_notify);
	if (ret)
		return ret;

	/* Reap dying images: core hands back the owner cap post-switch. */
	thread_exit_notify_register(proc_thread_exit);

	svc_register("proc", &g_proc_service);
	return 0;
}

subsys_register(proc, SUBSYS_LEVEL_PROC, proc_subsys_init);

const struct proc_service *proc_service(void)
{
	static const struct proc_service *tbl;

	if (!tbl)
		tbl = (const struct proc_service *)svc_lookup("proc");
	return tbl;
}
