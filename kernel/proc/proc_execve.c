#include "proc_execve.h"
#include "process.h"
#include "loader.h"
#include "mm_service.h"
#include "fs_service.h"
#include "thread.h"
#include "string.h"
#include "errno.h"
#include "printk.h"

/*
 * Image activation: spawn (new process, run it) and execve (replace the
 * current process' image).  Both build the image the same way and then
 * differ only in how the thread enters the run queue - spawn hands a
 * fresh thread to the scheduler, execve swaps the current thread for a
 * new one atomically (thread_replace_current) so the old image can
 * never be scheduled again.
 */

static int proc_setup_process(struct process *p, const char *filename,
			      uintptr_t *entry, uintptr_t *usp)
{
	int ret;

	if (!filename)
		return -EINVAL;

	p->vspace = mm_call(vspace_create);
	if (p->vspace == CAP_NULL)
		return -ENOMEM;

	p->files = fs_call(files_create);
	if (p->files == CAP_NULL) {
		mm_call(vspace_destroy, p->vspace);
		p->vspace = CAP_NULL;
		return -ENOMEM;
	}

	ret = proc_load_elf(filename, p->vspace, entry, usp);
	if (ret < 0)
		return ret;

	ret = (int)fs_call(setup_std_fds, p->files);
	if (ret < 0)
		return ret;

	return 0;
}

/*
 * Create the core thread that executes @p's image.  The caps handed in
 * here ride in the thread's switch envelope from its first switch on.
 */
static int process_start_thread(struct process *p, uintptr_t entry,
				uintptr_t usp)
{
	struct thread_create_args args = {
		.name		= p->name,
		.prio		= SCHED_PRIO_DEFAULT,
		.entry		= entry,
		.sp		= usp,
		.vspace_cap	= p->vspace,
		.files_cap	= p->files,
		.owner_cap	= p->self,
	};

	p->thread = thread_create(&args);
	if (p->thread == CAP_NULL)
		return -ENOMEM;

	thread_wake(p->thread);
	return 0;
}

int proc_spawn(const char *filename, char *const argv[], char *const envp[])
{
	struct process *p;
	uintptr_t entry, usp;
	int ret;

	(void)argv;
	(void)envp;

	ret = process_alloc(filename, &p);
	if (ret)
		return ret;

	ret = proc_setup_process(p, filename, &entry, &usp);
	if (ret)
		goto fail;

	ret = process_start_thread(p, entry, usp);
	if (ret)
		goto fail;

	return 0;

fail:
	process_free(p);
	return ret;
}

int proc_execve(const char *filename, char *const argv[], char *const envp[])
{
	struct process *old = proc_current_process();
	struct process *new;
	uintptr_t entry, usp;
	int ret;

	(void)argv;
	(void)envp;

	ret = process_alloc(filename, &new);
	if (ret)
		return ret;

	ret = proc_setup_process(new, filename, &entry, &usp);
	if (ret)
		goto fail;

	/* The image replaces the old one; the PID survives exec. */
	new->pid = old ? old->pid : new->pid;

	ret = process_start_thread(new, entry, usp);
	if (ret)
		goto fail;

	/*
	 * Atomically retire this thread and enter the new one; control
	 * continues in the new image and never returns here.  The old
	 * process is released by proc's exit notifier after the switch.
	 */
	thread_replace_current(new->thread);

fail:
	process_free(new);
	return ret;
}
