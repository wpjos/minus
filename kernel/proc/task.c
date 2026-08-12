#include "task.h"
#include "sched.h"
#include "mm.h"
#include "vspace.h"
#include "pt_regs.h"
#include "mmu.h"
#include "printk.h"
#include "string.h"
#include "vfs.h"

static struct task_struct init_task;
static int next_pid = 1;

void task_init(void)
{
	memset(&init_task, 0, sizeof(init_task));
	init_task.pid = 0;
	init_task.state = TASK_RUNNING;
	strncpy(init_task.name, "idle", sizeof(init_task.name) - 1);
	dlist_init(&init_task.se.run_node);
	init_task.vspace = NULL;
	init_task.files = alloc_files_struct();
}

static void task_init_identity(struct task_struct *task, const char *name)
{
	task->pid = next_pid++;
	strncpy(task->name, name, sizeof(task->name) - 1);
	task->state = TASK_INIT;
	dlist_init(&task->se.run_node);
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

struct task_struct *task_idle_task(void)
{
	return &init_task;
}
