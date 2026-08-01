#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#include "pt_regs.h"

#define SYS_WRITE	0
#define SYS_EXIT	1
#define SYS_GETPID	2
#define SYS_YIELD	3
#define SYS_EXECVE	4
#define NR_SYSCALLS	5

void do_syscall(struct pt_regs *regs);

#endif /* __SYSCALL_H__ */
