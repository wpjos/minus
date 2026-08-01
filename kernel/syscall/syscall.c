#include "syscall.h"
#include "task.h"
#include "sched.h"
#include "printk.h"
#include "string.h"
#include "page.h"
#include "memory.h"
#include "uaccess.h"

#define SYS_WRITE_BUF_SIZE	128

static long sys_write(unsigned int fd, const char *buf, size_t count)
{
	char kbuf[SYS_WRITE_BUF_SIZE];
	size_t done = 0;

	if (fd != 1)		/* only stdout supported for now */
		return -1;

	while (done < count) {
		size_t chunk = count - done;
		size_t copy;
		long ret;

		if (chunk > sizeof(kbuf) - 1)
			chunk = sizeof(kbuf) - 1;

		ret = copy_from_user(kbuf, buf + done, chunk);
		copy = chunk - (size_t)ret;
		if (copy == 0)
			return -1;

		kbuf[copy] = '\0';
		printk("%s", kbuf);

		done += copy;
		if (ret != 0)
			break;
	}

	return (long)done;
}

static long sys_exit(int code)
{
	(void)code;
	current->state = TASK_DEAD;
	schedule();
	/* An exited task never comes back. */
	while (1)
		;
	return 0;
}

static long sys_yield(void)
{
	schedule();
	return 0;
}

static long sys_execve(const char *filename, char *const argv[],
		       char *const envp[])
{
	(void)filename;
	(void)argv;
	(void)envp;
	/* Real ELF loader not implemented yet. */
	return -1;
}

static long sys_getpid(void)
{
	return current->pid;
}

typedef long (*syscall_fn_t)(uint64_t, uint64_t, uint64_t, uint64_t,
			     uint64_t, uint64_t);

static syscall_fn_t sys_call_table[NR_SYSCALLS] = {
	[SYS_WRITE] = (syscall_fn_t)sys_write,
	[SYS_EXIT] = (syscall_fn_t)sys_exit,
	[SYS_GETPID] = (syscall_fn_t)sys_getpid,
	[SYS_YIELD] = (syscall_fn_t)sys_yield,
	[SYS_EXECVE] = (syscall_fn_t)sys_execve,
};

void do_syscall(struct pt_regs *regs)
{
	uint64_t nr = regs->x[8];
	syscall_fn_t fn;
	long ret;

	if (nr >= NR_SYSCALLS) {
		regs->x[0] = (uint64_t)-1;
		return;
	}

	fn = sys_call_table[nr];
	ret = fn(regs->x[0], regs->x[1], regs->x[2],
		 regs->x[3], regs->x[4], regs->x[5]);
	regs->x[0] = (uint64_t)ret;
}
