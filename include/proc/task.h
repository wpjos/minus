#ifndef __TASK_H__
#define __TASK_H__

#include "types.h"
#include "sched.h"
#include "pt_regs.h"

struct files_struct;
struct vspace;

#define TASK_NAME_LEN	32

#define TASK_RUNNING	0
#define TASK_BLOCKED	1
#define TASK_ZOMBIE	2
#define TASK_DEAD	3
#define TASK_IDLE	4		/* idle task: runnable but not on run queue */

/* Saved FP/SIMD context.  AArch64 has 32 128-bit vector registers. */
struct fpsimd_state {
	__uint128_t v[32];
	uint32_t fpsr;
	uint32_t fpcr;
};

/*
 * Saved callee-saved kernel context. Must be first member of task_struct
 * because switch_to receives a task_struct *.
 */
struct thread_info {
	uint64_t x19;
	uint64_t x20;
	uint64_t x21;
	uint64_t x22;
	uint64_t x23;
	uint64_t x24;
	uint64_t x25;
	uint64_t x26;
	uint64_t x27;
	uint64_t x28;
	uint64_t x29;
	uint64_t lr;		/* x30 */
	uint64_t sp;		/* SP_EL1 at switch point */
	struct fpsimd_state fpstate;	/* FP/SIMD context */
};

struct task_struct {
	struct thread_info thread;	/* must be first */
	struct sched_entity se;		/* scheduling entity */
	uint64_t state;
	int need_resched;		/* set by scheduler_tick, cleared in schedule */
	int pid;
	char name[TASK_NAME_LEN];
	struct vspace *vspace;
	struct files_struct *files;
	struct pt_regs *pt_regs;	/* syscall register frame */
};

void task_init(void);

/* Allocate a bare task structure (used for both user and kernel tasks). */
struct task_struct *task_alloc(const char *name);

void task_free(struct task_struct *task);

/* idle task setup for the scheduler. */
struct task_struct *task_idle_task(void);

#endif /* __TASK_H__ */
