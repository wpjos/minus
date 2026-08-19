#include "sysif_proc.h"
#include "proc_execve.h"
#include "task.h"
#include "sched.h"
#include "errno.h"
#include "printk.h"
#include "uaccess.h"
#include "syscall_dispatch.h"

long sys_execve(const char *filename, char *const argv[],
		       char *const envp[])
{
	char path[256];
	long ret;

	ret = strncpy_from_user(path, filename, sizeof(path));
	if (ret != 0)
		return -EFAULT;
	path[sizeof(path) - 1] = '\0';

	printk("sys_execve: %s\n", path);
	return proc_execve(path, argv, envp);
}

long sys_getpid(void)
{
	return current->pid;
}

long sys_exit(int code)
{
	(void)code;
	current->state = TASK_DEAD;
	schedule();
	/* An exited task never comes back. */
	while (1)
		;
	return 0;
}

syscall_register(SYS_GETPID, sys_getpid, "proc");
syscall_register(SYS_EXECVE, sys_execve, "proc");
syscall_register(SYS_EXIT, sys_exit, "proc");

