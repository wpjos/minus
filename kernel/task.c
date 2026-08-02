#include "task.h"
#include "sched.h"
#include "mm.h"
#include "mm/vma.h"
#include "mm/page.h"
#include "mm/buddy.h"
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

	task = kmalloc(sizeof(*task));
	if (!task)
		return NULL;
	memset(task, 0, sizeof(*task));

	task->mm = kmalloc(sizeof(*task->mm));
	if (!task->mm) {
		kfree(task);
		return NULL;
	}
	memset(task->mm, 0, sizeof(*task->mm));
	dlist_init(&task->mm->vma_list);

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
	vma_init_mm(task->mm);

	return task;
}

static void free_task_pages(struct task_struct *task)
{
	if (task->mm) {
		vma_free_all(task->mm);

		if (task->mm->pgd)
			kfree_pages(task->mm->pgd);
		if (task->mm->kstack_top)
			buddy_free_pages(virt_to_page(task->mm->kstack_top - PAGE_SIZE));
		kfree(task->mm);
	}
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
