#include "module.h"
#include "printk.h"
#include "mm.h"
#include "irq.h"
#include "task.h"
#include "sched.h"
#include "syscall.h"

const char logo[] = "hello minus!!!\n";

int start_kernel(void)
{
	mm_init();
	/* Install exception vectors before any driver may trigger a fault */
	irq_init();

	/*
	 * Set up the scheduler before probing devices, because drivers such as
	 * the generic timer may call schedule() from their interrupt handlers.
	 */
	task_init();
	sched_init(task_idle_task());

	/* Register all drivers (triggers match & probe) */
	module_init();

	printk("%s\n", &logo[0]);

	/* Enable interrupts once the interrupt controller is ready */
	irq_unmask();

	/*
	 * The idle task continuously offers the CPU to the scheduler and waits
	 * for an interrupt when there is no runnable work.  A real init task
	 * will later be created and call execve() to start the first user
	 * program.
	 */
	while (1) {
		schedule();
		__asm__ volatile("wfi");
	}
	return 0;
}
