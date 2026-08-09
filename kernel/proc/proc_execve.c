#include "proc_execve.h"
#include "loader.h"
#include "task.h"
#include "sched.h"
#include "vma.h"
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

static int proc_setup_task_memory(struct task_struct *task)
{
	struct mm_struct *mm = task->mm;
	void *kstack;

	if (!mm)
		return -EINVAL;

	mm->pgd = (uint64_t *)kzalloc_pages(PAGE_SIZE);
	if (!mm->pgd)
		return -ENOMEM;

	kstack = kzalloc_pages(PAGE_SIZE);
	if (!kstack) {
		kfree_pages(mm->pgd);
		mm->pgd = NULL;
		return -ENOMEM;
	}

	mm->kstack_top = (uint64_t)kstack + PAGE_SIZE;
	return 0;
}

static int proc_setup_std_fds(struct task_struct *task)
{
	int fd;

	for (fd = 0; fd <= 2; fd++) {
		struct file *old = fget(task->files, fd);
		struct file *f = vfs_open("/dev/console", O_RDWR, 0);
		if (!f) {
			if (old)
				vfs_close(old);
			return -ENODEV;
		}
		fd_install(task->files, fd, f);
		if (old)
			vfs_close(old);
	}
	return 0;
}

static void proc_setup_task_regs(struct task_struct *task,
				 uintptr_t entry, uintptr_t stack_top)
{
	struct pt_regs *regs;

	regs = (struct pt_regs *)(task->mm->kstack_top - sizeof(*regs));
	memset(regs, 0, sizeof(*regs));
	regs->elr = entry;
	regs->sp_el0 = stack_top;
	regs->spsr = 0;

	task->thread.sp = (uint64_t)regs;
	task->thread.lr = (uint64_t)ret_to_user;
}

int proc_execve(const char *filename, char *const argv[], char *const envp[])
{
	struct task_struct *old_task = current;
	struct task_struct *new_task;
	uintptr_t entry;
	uintptr_t stack_top;
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

	ret = proc_setup_task_memory(new_task);
	if (ret < 0)
		goto fail_task;

	ret = proc_load_elf(filename, new_task, &entry, &stack_top);
	if (ret < 0)
		goto fail_task;

	ret = proc_setup_std_fds(new_task);
	if (ret < 0)
		goto fail_task;

	proc_setup_task_regs(new_task, entry, stack_top);

	/*
	 * The idle task lives in static BSS and must never be freed.
	 * Normal user tasks are replaced by the new image and released later.
	 */
	if (old_task->state != TASK_IDLE)
		old_task->state = TASK_DEAD;
	sched_enqueue(new_task);
	schedule();

	/* Never reached: the old task is dead and the new task takes over. */
	return 0;

fail_task:
	task_free(new_task);
	return ret;
}
