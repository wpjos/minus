#ifndef __PROCESS_H__
#define __PROCESS_H__

/*
 * proc module-internal declarations.
 *
 * A process is the user-visible image: identity plus resources.  It has
 * exactly one execution thread (a core kthread) whose envelope carries
 * this process' owner cap, so proc's switch mirror always knows which
 * process is current without asking anyone.  Everything in this header
 * is private to kernel/proc/; other modules see proc only through
 * include/proc/proc_service.h.
 */

#include "types.h"
#include "cap.h"

#define PROC_NAME_LEN	32

/* Process lifecycle, owned by proc (core owns the thread lifecycle). */
enum process_state {
	PROC_ALIVE,
	PROC_DEAD,		/* thread exited, resources being released */
};

struct process {
	enum process_state	state;
	int			pid;
	char			name[PROC_NAME_LEN];
	cap_t			self;	/* owner slot in the thread envelope */
	cap_t			thread;	/* core kthread running this image */
	cap_t			vspace;	/* mm address space */
	cap_t			files;	/* fs fdtable */
};

/* The process whose thread is current on this CPU (NULL early boot). */
struct process *proc_current_process(void);

/* Allocate a zeroed process with pid and self cap. */
int process_alloc(const char *name, struct process **out);

/* Release every resource the image owns (after its thread died). */
void process_free(struct process *p);

int proc_spawn(const char *filename, char *const argv[], char *const envp[]);
int proc_execve(const char *filename, char *const argv[], char *const envp[]);

#endif /* __PROCESS_H__ */
