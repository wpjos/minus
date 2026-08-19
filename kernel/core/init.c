#include "module.h"
#include "subsys.h"
#include "printk.h"
#include "mmu.h"
#include "irq.h"
#include "cmdline.h"
#include "string.h"
#include "fs_service.h"
#include "cap.h"
#include "proc_service.h"
#include "thread.h"

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
		if (fs_call(mount_root, args.root_device, fstype) == 0) {
			printk("rootfs mounted: %s (%s)\n", args.root_device, fstype);
		} else {
			printk("rootfs mount failed: %s (%s)\n", args.root_device, fstype);
		}
	} else {
		printk("no root= in bootargs, trying /dev/vda\n");
		if (fs_call(mount_root, "/dev/vda", "ext4") == 0) {
			printk("rootfs mounted: /dev/vda (ext4)\n");
		} else {
			printk("rootfs mount failed: /dev/vda (ext4), trying /dev/mmcblk0p2\n");
			if (fs_call(mount_root, "/dev/mmcblk0p2", "ext4") == 0) {
				printk("rootfs mounted: /dev/mmcblk0p2 (ext4)\n");
			} else {
				printk("rootfs mount failed: /dev/mmcblk0p2 (ext4), trying /dev/mmcblk0\n");
				if (fs_call(mount_root, "/dev/mmcblk0", "ext4") == 0)
					printk("rootfs mounted: /dev/mmcblk0 (ext4)\n");
				else
					printk("rootfs mount failed: /dev/mmcblk0 (ext4)\n");
			}
		}
	}

	if (proc_call(spawn, "/bin/shell", NULL, NULL) == 0)
		printk("shell spawned\n");
	else
		printk("shell spawn failed\n");

	irq_unmask();

	/*
	 * This is the idle thread (core's statically allocated kthread):
	 * it continuously offers the CPU to the scheduler and waits for an
	 * interrupt when there is no runnable work.  A real init task will
	 * later be created and call execve() to start the first user
	 * program.
	 */
	while (1) {
		schedule();
		__asm__ volatile("wfi");
	}
	return 0;
}
