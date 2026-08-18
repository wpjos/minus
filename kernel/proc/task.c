#include "task.h"
#include "sched.h"
#include "mm.h"
#include "vspace.h"
#include "pt_regs.h"
#include "mmu.h"
#include "printk.h"
#include "string.h"
#include "vfs.h"

static struct task_struct init_task = {
	.thread		= { 0 },
	.se = {
		.run_node	= DLIST_NODE_INIT(init_task.se.run_node),
		.time_slice	= SCHED_SLICE_TICKS,
		.priority	= SCHED_PRIO_IDLE,
	},
	.state		= TASK_RUNNING,
	.pid		= 0,
	.name		= "idle",
	.vspace		= NULL,
	.files		= NULL,
	.pt_regs	= NULL,
};

static int next_pid = 1;

struct task_struct *current = &init_task;

static void task_init_identity(struct task_struct *task, const char *name)
{
	task->pid = next_pid++;
	strncpy(task->name, name, sizeof(task->name) - 1);
	task->state = TASK_INIT;
	dlist_init(&task->se.run_node);
	task->se.priority = SCHED_PRIO_DEFAULT;
}

/*
 * Allocate a task and all per-task resources.  Each sub-resource has its own
 * allocation helper so failures can be unwound cleanly.
 */
struct task_struct *task_alloc(const char *name)
{
	struct task_struct *task;

	task = kzalloc(sizeof(struct task_struct));
	if (!task)
		return NULL;

	task->vspace = vspace_alloc();
	if (!task->vspace)
		goto fail_task;

	task->files = alloc_files_struct();
	if (!task->files)
		goto fail_vspace;

	task_init_identity(task, name);
	return task;

fail_vspace:
	vspace_free(task->vspace);
fail_task:
	kfree(task);
	return NULL;
}

void task_free(struct task_struct *task)
{
	if (!task)
		return;

	vspace_free(task->vspace);
	kfree(task->files);
	kfree(task);
}
