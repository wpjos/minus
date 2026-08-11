#include "task.h"
#include "sched.h"
#include "mm.h"
#include "vma.h"
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
	init_task.state = TASK_IDLE;
	strncpy(init_task.name, "idle", sizeof(init_task.name) - 1);
	dlist_init(&init_task.se.run_node);
	init_task.mm = NULL;
	init_task.files = alloc_files_struct();
}

struct task_struct *task_alloc(const char *name)
{
	struct task_struct *task;

	task = kzalloc(sizeof(*task));
	if (!task)
		return NULL;

	task->mm = vma_alloc_init();
	if (!task->mm) {
		kfree(task);
		return NULL;
	}

	task->files = alloc_files_struct();
	if (!task->files) {
		kfree(task->mm);
		kfree(task);
		return NULL;
	}

	task->pid = next_pid++;
	strncpy(task->name, name, sizeof(task->name) - 1);
	task->state = TASK_RUNNING;
	dlist_init(&task->se.run_node);

	return task;
}

static void free_task_pages(struct task_struct *task)
{
	if (task->mm) {
		vma_free_all(task->mm);
		kfree(task->mm);
	}
	if (task->files)
		kfree(task->files);
	kfree(task);
}

void task_free(struct task_struct *task)
{
	free_task_pages(task);
}

struct task_struct *task_idle_task(void)
{
	return &init_task;
}
