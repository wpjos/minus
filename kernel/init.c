#include "module.h"
#include "subsys.h"
#include "printk.h"
#include "mm.h"
#include "irq.h"
#include "mmu.h"
#include "task.h"
#include "sched.h"
#include "vfs.h"
#include "cmdline.h"
#include "string.h"
#include "fcntl.h"
#include "proc_execve.h"

const char logo[] = "hello minus!!!\n";

int start_kernel(void)
{
	struct bootargs args;

	if (subsys_init_all() < 0) {
		printk("subsys_init_all failed\n");
		while (1)
			;
	}

	module_init();
	mmu_clear_ttbr0();

	printk("%s\n", &logo[0]);

	/* Parse firmware bootargs; if none or no root=, fall back to block devices. */
	if (parse_bootargs(&args) == 0 && args.root_device[0]) {
		const char *fstype = args.root_fstype[0] ? args.root_fstype : "ext4";
		if (vfs_mount(args.root_device, fstype, "/") == 0) {
			printk("rootfs mounted: %s (%s)\n", args.root_device, fstype);
		} else {
			printk("rootfs mount failed: %s (%s)\n", args.root_device, fstype);
		}
	} else {
		printk("no root= in bootargs, trying /dev/vda\n");
		if (vfs_mount("/dev/vda", "ext4", "/") == 0) {
			printk("rootfs mounted: /dev/vda (ext4)\n");
		} else {
			printk("rootfs mount failed: /dev/vda (ext4), trying /dev/mmcblk0p2\n");
			if (vfs_mount("/dev/mmcblk0p2", "ext4", "/") == 0) {
				printk("rootfs mounted: /dev/mmcblk0p2 (ext4)\n");
			} else {
				printk("rootfs mount failed: /dev/mmcblk0p2 (ext4), trying /dev/mmcblk0\n");
				if (vfs_mount("/dev/mmcblk0", "ext4", "/") == 0)
					printk("rootfs mounted: /dev/mmcblk0 (ext4)\n");
				else
					printk("rootfs mount failed: /dev/mmcblk0 (ext4)\n");
			}
		}
	}

	/* Create /dev and mount a minimal devfs so /dev/console is available. */
	if (vfs_mkdir("/dev", 0755) == 0)
		printk("vfs /dev created\n");
	if (vfs_mount("none", "devfs", "/dev") == 0)
		printk("devfs mounted\n");

	if (proc_spawn("/bin/shell", NULL, NULL) == 0)
		printk("shell spawned\n");
	else
		printk("shell spawn failed\n");

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
