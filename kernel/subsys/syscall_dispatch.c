#include "syscall_dispatch.h"
#include "task.h"
#include "errno.h"

extern struct syscall_entry __syscalls_start[];
extern struct syscall_entry __syscalls_end[];

void do_syscall(struct pt_regs *regs)
{
	uint64_t nr = regs->x[8];
	struct syscall_entry *e;
	long ret = -ENOSYS;

	current->pt_regs = regs;

	for (e = __syscalls_start; e < __syscalls_end; e++) {
		if (e->nr != nr)
			continue;

		ret = e->fn(regs->x[0], regs->x[1], regs->x[2],
			     regs->x[3], regs->x[4], regs->x[5]);
		if (ret != -ENOSYS)
			break;
	}

	regs->x[0] = (uint64_t)ret;
}
