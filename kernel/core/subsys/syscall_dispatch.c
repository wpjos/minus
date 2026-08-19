#include "syscall_dispatch.h"
#include "subsys.h"
#include "string.h"
#include "errno.h"
#include "printk.h"

#define SC_TABLE_SIZE 256

extern struct syscall_entry __syscalls_start[];
extern struct syscall_entry __syscalls_end[];

static syscall_fn_t g_syscall_table[SC_TABLE_SIZE];

static int syscall_dispatch_subsys_init(void)
{
	const struct syscall_entry *e;

	memset(g_syscall_table, 0, sizeof(g_syscall_table));
	for (e = __syscalls_start; e < __syscalls_end; e++) {
		if (e->nr >= SC_TABLE_SIZE) {
			printk("syscall %llu out of range\n", e->nr);
			return -EINVAL;
		}
		if (g_syscall_table[e->nr]) {
			printk("duplicate syscall %llu\n", e->nr);
			return -EINVAL;
		}
		g_syscall_table[e->nr] = e->fn;
	}
	return 0;
}

subsys_register(syscall, SUBSYS_LEVEL_SYSCALL,
		syscall_dispatch_subsys_init);

void do_syscall(struct pt_regs *regs)
{
	uint64_t nr = regs->x[8];
	syscall_fn_t fn;

	if (nr >= SC_TABLE_SIZE || !g_syscall_table[nr]) {
		regs->x[0] = (uint64_t)-ENOSYS;
		return;
	}

	fn = g_syscall_table[nr];
	regs->x[0] = (uint64_t)fn(regs->x[0], regs->x[1], regs->x[2],
				   regs->x[3], regs->x[4], regs->x[5]);
}
