#ifndef __SYSCALL_DISPATCH_H__
#define __SYSCALL_DISPATCH_H__

#include "types.h"
#include "syscall.h"
#include "pt_regs.h"

/* Syscall handler prototype: six uint64_t args, returns long. */
typedef long (*syscall_fn_t)(uint64_t, uint64_t, uint64_t,
			     uint64_t, uint64_t, uint64_t);

struct syscall_entry {
	uint64_t	nr;
	syscall_fn_t	fn;
};

/*
 * Register a syscall handler.  The cast to syscall_fn_t avoids warnings for
 * handlers with typed signatures; the AArch64 calling convention passes the
 * first six integer/pointer arguments in the same registers regardless of the
 * exact parameter types.
 */
#define syscall_register(_nr, _fn)						\
	static const struct syscall_entry __sc_##_fn				\
		__attribute__((__used__, __section__(".syscalls.init"))) = {	\
			.nr = (_nr),						\
			.fn = (syscall_fn_t)(_fn),					\
		}

/*
 * The dispatch table is built by a SUBSYS_LEVEL_SYSCALL init routine
 * (see kernel/subsys/syscall_dispatch.c); nothing else needs to call it
 * explicitly.
 */

/* Dispatch one syscall from el0_sync(). */
void do_syscall(struct pt_regs *regs);

#endif /* __SYSCALL_DISPATCH_H__ */
