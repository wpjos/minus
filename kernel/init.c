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
#include "loader.h"

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

	/* VFS self-test: exercise the new public VFS helpers. */
	{
		struct file *f;
		char buf[32];
		ssize_t n;

		f = vfs_open("/vfstest.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644);
		if (f) {
			n = vfs_write(f, "vfs ok\n", 7, NULL);
			if (n == 7)
				printk("vfs write test passed\n");
			vfs_close(f);
		} else {
			printk("vfs open (create) test failed\n");
		}

		f = vfs_open("/vfstest.txt", O_RDONLY, 0);
		if (f) {
			n = vfs_read(f, buf, sizeof(buf), NULL);
			if (n == 7)
				printk("vfs read test passed\n");
			vfs_close(f);
		}

		if (vfs_mkdir("/vfstestdir", 0755) == 0)
			printk("vfs mkdir test passed\n");
		if (vfs_rmdir("/vfstestdir") == 0)
			printk("vfs rmdir test passed\n");
		if (vfs_unlink("/vfstest.txt") == 0)
			printk("vfs unlink test passed\n");
	}

	run_user_init("/bin/shell");

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
