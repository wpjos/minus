#include "vfs.h"
#include "file.h"
#include "mm.h"
#include "printk.h"
#include "fs_service.h"
#include "cap.h"
#include "string.h"
#include "errno.h"
#include "stat.h"
#include "fcntl.h"
#include "dirent.h"
#include "svc.h"
#include "subsys.h"

/*
 * fs_service implementation: the single FS service table.
 *
 * The table is the cross-module boundary: other modules (proc's task
 * lifecycle, init's rootfs setup, mm's mmap) reach the filesystem only
 * through it, naming fdtables by capability.  Current-fdtable file
 * operations live one layer down in fdtable.c (fd_*), which the in-module
 * sysif layer and proc's loader call directly through the per-CPU fdtable
 * mirror - no service table, no capability round-trip.  All pointers here
 * are kernel pointers; user copying lives in sysif/.
 */

/*
 * O(1) files_cap -> files_struct lookup.  Files created through
 * files_create carry CAP_R_CTL; callers may hold weaker caps later.
 */
struct files_struct *files_from_cap(cap_t files_cap)
{
	return (struct files_struct *)cap_resolve(files_cap,
						  cap_rights(CAP_R_CTL));
}

/*
 * Fdtable lifecycle: proc creates/destroys the per-task fdtable through
 * these hooks.  files_destroy consumes the capability.
 */
static cap_t fs_files_create(void)
{
	struct files_struct *files;
	cap_t cap;

	files = alloc_files_struct();
	if (!files)
		return CAP_NULL;

	cap = cap_alloc(files, cap_rights(CAP_R_CTL));
	if (cap == CAP_NULL) {
		free_files_struct(files);
		return CAP_NULL;
	}
	return cap;
}

static void fs_files_destroy(cap_t files_cap)
{
	struct files_struct *files;

	files = files_from_cap(files_cap);
	if (!files)
		return;

	cap_free(files_cap);
	free_files_struct(files);
}

/*
 * Setup stdin/stdout/stderr for a new task by opening /dev/console and
 * installing it into the fdtable.  All work is done locally in FS so proc
 * only makes one cross-module call.
 */
static long fs_setup_std_fds(cap_t files_cap)
{
	struct files_struct *files;
	struct file *file;
	int fd;
	int i;
	long ret;

	files = files_from_cap(files_cap);
	if (!files)
		return -EBADF;

	for (i = 0; i <= 2; i++) {
		ret = vfs_open("/dev/console", O_RDWR, 0, &file);
		if (ret < 0)
			goto fail;

		fd = get_unused_fd(files);
		if (fd < 0) {
			vfs_close(file);
			ret = fd;
			goto fail;
		}

		fd_install(files, fd, file);
	}
	return 0;

fail:
	for (i = 0; i <= 2; i++) {
		file = fget(files, i);
		if (file) {
			put_unused_fd(files, i);
			vfs_close(file);
		}
	}
	return ret;
}

/*
 * Mount the root filesystem, then complete the initial namespace.  /dev
 * and devfs are FS-internal setup: nothing outside FS should orchestrate
 * them, so they happen here as part of mounting root.
 */
static long fs_mount_root(const char *source, const char *fstype)
{
	long ret;

	ret = vfs_mount(source, fstype, "/");
	if (ret < 0)
		return ret;

	if (vfs_mkdir("/dev", 0755) == 0 && vfs_mount("none", "devfs", "/dev") == 0)
		printk("devfs mounted\n");

	return 0;
}

/*
 * Read a whole file into kernel memory.  Context-free by design: no
 * fdtable, no fd, no cwd - used by proc's ELF loader, which may run in
 * the context of a task with no fdtable at all (e.g. idle at bring-up).
 */
#define FS_LOAD_MAX (16 * 1024 * 1024)

static long fs_load_file(const char *path, void **buf_out, size_t *size_out)
{
	struct stat st;
	struct file *file;
	char *buf;
	size_t total = 0;

	if (vfs_open(path, O_RDONLY, 0, &file) < 0)
		return -EIO;

	if (vfs_fstat(file, &st) < 0) {
		vfs_close(file);
		return -EIO;
	}
	if (st.st_size == 0 || st.st_size > FS_LOAD_MAX) {
		vfs_close(file);
		return -ENOEXEC;
	}

	buf = (char *)kmalloc(st.st_size);
	if (!buf) {
		vfs_close(file);
		return -ENOMEM;
	}

	while (total < st.st_size) {
		ssize_t n = vfs_read(file, buf + total, st.st_size - total, NULL);
		if (n <= 0)
			break;
		total += n;
	}

	vfs_close(file);

	if (total != st.st_size) {
		kfree(buf);
		return -EIO;
	}

	*buf_out = buf;
	*size_out = st.st_size;
	return 0;
}

const struct fs_service g_fs_service = {
	.files_create = fs_files_create,
	.files_destroy = fs_files_destroy,
	.setup_std_fds = fs_setup_std_fds,
	.mount_root = fs_mount_root,
	.load_file = fs_load_file,
};

const struct fs_service *fs_service(void)
{
	static const struct fs_service *tbl;

	if (!tbl)
		tbl = (const struct fs_service *)svc_lookup("fs");
	return tbl;
}

/*
 * FS subsystem registration: init the VFS core and publish the service
 * table.  Kept here, next to the table, so vfs.c stays pure VFS mechanics
 * and knows nothing about the service layer.
 */
static int fs_subsys_init(void)
{
	int ret;

	vfs_init();

	/* Subscribe the fdtable mirror to core's switch notification bus. */
	ret = fdtable_init();
	if (ret)
		return ret;

	svc_register("fs", &g_fs_service);
	return 0;
}

subsys_register(fs, SUBSYS_LEVEL_FS, fs_subsys_init);
