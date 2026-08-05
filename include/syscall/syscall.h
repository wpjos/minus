#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#include "sysif_fs.h"

#include "pt_regs.h"

/* ARM64 syscall numbers for the implemented syscalls. */
#define SYS_MKDIRAT	34
#define SYS_UNLINKAT	35
#define SYS_OPENAT	56
#define SYS_CLOSE	57
#define SYS_LSEEK	62
#define SYS_READ	63
#define SYS_WRITE	64
#define SYS_NEWFSTATAT	76
#define SYS_FSTAT	77
#define SYS_EXIT	93
#define SYS_YIELD	124	/* sched_yield */
#define SYS_GETPID	172
#define SYS_EXECVE	221

void do_syscall(struct pt_regs *regs);

#endif /* __SYSCALL_H__ */
