#include "syscall.h"
#include "task.h"
#include "sched.h"
#include "errno.h"
#include "fs_call.h"

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

struct syscall_entry {
	uint64_t	nr;
	syscall_fn_t	fn;
};

static const struct syscall_entry sys_call_table[] = {
	{ SYS_MKDIRAT,		(syscall_fn_t)sys_mkdirat },
	{ SYS_UNLINKAT,		(syscall_fn_t)sys_unlinkat },
	{ SYS_OPENAT,		(syscall_fn_t)sys_openat },
	{ SYS_CLOSE,		(syscall_fn_t)sys_close },
	{ SYS_LSEEK,		(syscall_fn_t)sys_lseek },
	{ SYS_READ,		(syscall_fn_t)sys_read },
	{ SYS_WRITE,		(syscall_fn_t)sys_write },
	{ SYS_NEWFSTATAT,	(syscall_fn_t)sys_newfstatat },
	{ SYS_FSTAT,		(syscall_fn_t)sys_fstat },
	{ SYS_EXIT,		(syscall_fn_t)sys_exit },
	{ SYS_YIELD,		(syscall_fn_t)sys_yield },
	{ SYS_GETPID,		(syscall_fn_t)sys_getpid },
	{ SYS_EXECVE,		(syscall_fn_t)sys_execve },
};

#define NR_SYSCALLS	(sizeof(sys_call_table) / sizeof(sys_call_table[0]))

void do_syscall(struct pt_regs *regs)
{
	uint64_t nr = regs->x[8];
	size_t i;

	for (i = 0; i < NR_SYSCALLS; i++) {
		if (sys_call_table[i].nr == nr) {
			syscall_fn_t syscall_fn = sys_call_table[i].fn;
			regs->x[0] = syscall_fn(regs->x[0], regs->x[1],
						regs->x[2], regs->x[3],
						regs->x[4], regs->x[5]);
			return;
		}
	}

	regs->x[0] = (uint64_t)-ENOSYS;
}
