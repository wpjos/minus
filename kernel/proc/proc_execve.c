#include "proc_execve.h"
#include "loader.h"
#include "task.h"
#include "sched.h"
#include "vspace.h"
#include "mmu.h"
#include "memory.h"
#include "page.h"
#include "buddy.h"
#include "mm.h"
#include "vfs.h"
#include "string.h"
#include "errno.h"
#include "stat.h"
#include "fcntl.h"
#include "printk.h"
#include "pt_regs.h"
#include "entry-common.h"
#include "uaccess.h"

extern void ret_to_user(void);

static int proc_setup_std_fds(struct task_struct *task)
{
	int fd;

	for (fd = 0; fd <= 2; fd++) {
		struct file *old = fget(task->files, fd);
		struct file *f = NULL;
		int ret;

		ret = vfs_open("/dev/console", O_RDWR, 0, &f);
		if (ret < 0) {
			if (old)
				vfs_close(old);
			return ret;
		}
		fd_install(task->files, fd, f);
		if (old)
			vfs_close(old);
	}
	return 0;
}

static void proc_setup_entry(struct task_struct *task,
			    uintptr_t entry, uintptr_t stack_top)
{
	struct pt_regs *regs;

	regs = (struct pt_regs *)(task->vspace->kstack_top - sizeof(*regs));
	memset(regs, 0, sizeof(*regs));
	regs->elr = entry;
	regs->sp_el0 = stack_top;
	regs->spsr = 0;

	task->thread.sp = (uint64_t)regs;
	task->thread.lr = (uint64_t)ret_to_user;
}

static int proc_setup_task(struct task_struct *task)
{
	int ret;
	uintptr_t entry;
	uintptr_t stack_top;

	ret = proc_load_elf(task->name, task, &entry, &stack_top);
	if (ret < 0)
		return ret;

	ret = proc_setup_std_fds(task);
	if (ret < 0)
		return ret;

	proc_setup_entry(task, entry, stack_top);

	return ret;
}

int proc_spawn(const char *filename, char *const argv[], char *const envp[])
{
	struct task_struct *task;
	int ret;

	(void)argv;
	(void)envp;

	if (!filename)
		return -EINVAL;

	task = task_alloc(filename);
	if (!task)
		return -ENOMEM;

	ret = proc_setup_task(task);
	if (ret != 0) {
		task_free(task);
		return ret;
	}
	sched_enqueue(task);
	return 0;
}

int proc_execve(const char *filename, char *const argv[], char *const envp[])
{
	struct task_struct *old_task = current;
	struct task_struct *new_task;
	int ret;

	(void)argv;
	(void)envp;

	if (!filename)
		return -EINVAL;

	new_task = task_alloc(filename);
	if (!new_task)
		return -ENOMEM;

	/* Preserve the PID across exec. */
	new_task->pid = old_task->pid;

	ret = proc_setup_task(new_task);
	if (ret != 0) {
		task_free(new_task);
		return ret;
	}
	sched_enqueue(new_task);
	old_task->state = TASK_DEAD;
	schedule();

	return 0;
}
