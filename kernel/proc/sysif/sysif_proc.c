#include "process.h"
#include "proc_execve.h"
#include "mm_service.h"
#include "thread.h"
#include "string.h"
#include "errno.h"
#include "printk.h"
#include "syscall_dispatch.h"

/*
 * Syscall stubs for proc.
 *
 * sysif/ is inside the proc module, so these handlers call proc's
 * implementation directly (proc_current_process, proc_execve) - the
 * proc_service table is the cross-module boundary, not an in-module
 * detour.  core's thread API (thread_exit_current, schedule) is called
 * directly too: servers sit on top of core, never beside it.
 */

long sys_execve(const char *filename, char *const argv[],
		char *const envp[])
{
	char path[256];
	long ret;

	ret = mm_call(strncpy_from_user, path, (user_addr_t)(uintptr_t)filename,
		      sizeof(path));
	if (ret != 0)
		return -EFAULT;
	path[sizeof(path) - 1] = '\0';

	printk("sys_execve: %s\n", path);
	return proc_execve(path, argv, envp);
}

long sys_getpid(void)
{
	struct process *p = proc_current_process();

	return p ? p->pid : 0;
}

long sys_exit(int code)
{
	thread_exit_current();
}

long sys_yield(void)
{
	schedule();
	return 0;
}

syscall_register(SYS_GETPID, sys_getpid);
syscall_register(SYS_EXECVE, sys_execve);
syscall_register(SYS_EXIT, sys_exit);
syscall_register(SYS_YIELD, sys_yield);
