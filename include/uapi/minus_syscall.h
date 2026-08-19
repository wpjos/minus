#ifndef __MINUS_SYSCALL_H__
#define __MINUS_SYSCALL_H__

/*
 * AArch64 syscall numbers for the Minus kernel.
 *
 * This header is shared between the kernel and user-space libc.  User-space
 * code may rely on these values, so changing them is an ABI break.
 */

#define SYS_GETDENTS64	61
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

#define SYS_IOCTL	29
#define SYS_MMAP	222

#endif /* __MINUS_SYSCALL_H__ */
