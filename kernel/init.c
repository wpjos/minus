#include "module.h"
#include "printk.h"
#include "mm.h"
#include "irq.h"
#include "task.h"
#include "sched.h"
#include "vfs.h"
#include "boot/cmdline.h"
#include "string.h"
#include "fcntl.h"
#include "proc_execve.h"

const char logo[] = "hello minus!!!\n";

int start_kernel(void)
{
	struct bootargs args;

	mm_init();
	irq_init();

	task_init();
	sched_init(task_idle_task());

	vfs_init();
	module_init();

	printk("%s\n", &logo[0]);

	if (parse_bootargs(&args) == 0 && args.root_device[0]) {
		const char *fstype = args.root_fstype[0] ? args.root_fstype : "ext4";
		vfs_mount(args.root_device, fstype, "/");
	} else {
		vfs_mount("/dev/vda", "ext4", "/");
	}

	/* Create /dev and mount a minimal devfs so /dev/console is available. */
	if (vfs_mkdir("/dev", 0755) == 0)
		printk("vfs /dev created\n");
	if (vfs_mount("none", "devfs", "/dev") == 0)
		printk("devfs mounted\n");

	proc_execve("/bin/shell", NULL, NULL);

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
