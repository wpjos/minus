#ifndef __PROC_SERVICE_H__
#define __PROC_SERVICE_H__

#include "types.h"

/*
 * Proc / process subsystem service interface.
 *
 * proc owns process identity and image lifecycle; core owns the threads
 * that execute them (core/thread.h - a kernel-internal API, not a
 * service).  This table is the movable-server boundary: in a
 * micro-kernel build these two entry points become IPC stubs, while
 * thread create/exit turn into system calls.
 *
 * Current-process state is not served here: proc mirrors it per CPU
 * from core's switch notification bus (service/switch_notify.h), so no
 * syscall pays a service round-trip to learn who is running.
 */
struct proc_service {
	/* Create a process from @filename and make it runnable. */
	int (*spawn)(const char *filename, char *const argv[],
		     char *const envp[]);

	/*
	 * Replace the calling process' image with @filename.  Never
	 * returns to the old image; returns negative errno only when the
	 * new image could not be built (then the old image lives on).
	 */
	int (*execve)(const char *filename, char *const argv[],
		      char *const envp[]);
};

const struct proc_service *proc_service(void);

/* Convenience wrapper: proc_call(fn, args...) -> proc_service()->fn(args...). */
#define proc_call(fn, ...) proc_service()->fn(__VA_ARGS__)

#endif /* __PROC_SERVICE_H__ */
