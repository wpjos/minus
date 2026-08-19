#include "file.h"
#include "string.h"
#include "mm.h"
#include "errno.h"
#include "cap.h"
#include "switch_notify.h"
#include "vfs.h"
#include "dirent.h"
#include "dentry.h"
#include "namei.h"
#include "stat.h"

/*
 * Per-CPU mirror of the running task's open-file table, fed by core's
 * switch notification bus.  Plain statics on this single-CPU build;
 * become per-CPU variables under SMP.  Early-boot context (idle task)
 * arrives as a NULL envelope and mirrors as NULL/CAP_NULL.
 *
 * The bus carries the switch envelope prepared by proc, so this callback
 * never touches task_struct and never calls into proc: the files_cap
 * slot is fs's own, resolved here against fs's own files_struct objects -
 * no per-syscall resolve, no service round-trip.
 */
static struct files_struct *cur_files;

static void fdtable_switch_notify(const struct switch_envelope *prev,
				  const struct switch_envelope *next)
{
	cap_t files_cap;

	(void)prev;
	files_cap = next ? next->files_cap : CAP_NULL;
	cur_files = files_from_cap(files_cap);
}

struct files_struct *fs_current_files(void)
{
	return cur_files;
}

int fdtable_init(void)
{
	return switch_notifier_register(fdtable_switch_notify);
}

struct files_struct *alloc_files_struct(void)
{
	struct files_struct *files;

	files = (struct files_struct *)kmalloc(sizeof(*files));
	if (!files)
		return NULL;
	memset(files, 0, sizeof(*files));
	files->count = 1;

	files->fs_ctx = (struct fs_context *)kmalloc(sizeof(*files->fs_ctx));
	if (!files->fs_ctx) {
		kfree(files);
		return NULL;
	}
	memset(files->fs_ctx, 0, sizeof(*files->fs_ctx));
	return files;
}

void free_files_struct(struct files_struct *files)
{
	if (!files)
		return;
	if (files->fs_ctx) {
		if (files->fs_ctx->cwd_dentry)
			dput(files->fs_ctx->cwd_dentry);
		kfree(files->fs_ctx);
	}
	kfree(files);
}

int get_unused_fd(struct files_struct *files)
{
	int i;

	if (!files)
		return -EBADF;

	for (i = 0; i < NR_OPEN_DEFAULT; i++) {
		if (!files->fd_array[i])
			return i;
	}
	return -EMFILE;
}

void put_unused_fd(struct files_struct *files, int fd)
{
	if (!files || fd < 0 || fd >= NR_OPEN_DEFAULT)
		return;
	files->fd_array[fd] = NULL;
}

struct file *fget(struct files_struct *files, int fd)
{
	if (!files || fd < 0 || fd >= NR_OPEN_DEFAULT)
		return NULL;
	return files->fd_array[fd];
}

void fd_install(struct files_struct *files, int fd, struct file *file)
{
	if (!files || fd < 0 || fd >= NR_OPEN_DEFAULT)
		return;
	files->fd_array[fd] = file;
}

/*
 * FD layer: file operations scoped to an fd of the current task's
 * fdtable (the per-CPU mirror above).  The in-module sysif layer and
 * proc's loader call these directly with kernel pointers; cross-module
 * callers that must name a specific fdtable stay on the fs service
 * table's capability ops instead.  A NULL mirror (early boot) yields
 * -EBADF.
 */

int fd_openat(const char *pathname, int flags, uint16_t mode)
{
	struct files_struct *files = cur_files;
	struct file *file;
	int fd;
	long ret;

	if (!files)
		return -EBADF;

	ret = vfs_open(pathname, flags, mode, &file);
	if (ret < 0)
		return ret;

	fd = get_unused_fd(files);
	if (fd < 0) {
		vfs_close(file);
		return fd;
	}

	fd_install(files, fd, file);
	return fd;
}

int fd_close(unsigned int fd)
{
	struct files_struct *files = cur_files;
	struct file *file;

	if (!files)
		return -EBADF;

	file = fget(files, fd);
	if (!file)
		return -EBADF;

	put_unused_fd(files, fd);
	vfs_close(file);
	return 0;
}

ssize_t fd_read(unsigned int fd, void *buf, size_t count)
{
	struct files_struct *files = cur_files;
	struct file *file;

	if (!files)
		return -EBADF;

	file = fget(files, fd);
	if (!file)
		return -EBADF;

	return vfs_read(file, buf, count, NULL);
}

ssize_t fd_write(unsigned int fd, const void *buf, size_t count)
{
	struct files_struct *files = cur_files;
	struct file *file;

	if (!files)
		return -EBADF;

	file = fget(files, fd);
	if (!file)
		return -EBADF;

	return vfs_write(file, buf, count, NULL);
}

long fd_lseek(unsigned int fd, long offset, int whence)
{
	struct files_struct *files = cur_files;
	struct file *file;

	if (!files)
		return -EBADF;

	file = fget(files, fd);
	if (!file)
		return -EBADF;

	return (long)vfs_llseek(file, (loff_t)offset, whence);
}

long fd_fstat(unsigned int fd, struct stat *st)
{
	struct files_struct *files = cur_files;
	struct file *file;

	if (!files)
		return -EBADF;

	file = fget(files, fd);
	if (!file)
		return -EBADF;

	return vfs_fstat(file, st);
}

struct getdents_ctx {
	struct dir_context ctx;
	char *buf;
	size_t count;
	size_t pos;
};

static long filldir(struct dir_context *ctx, const char *name, int namlen,
		    loff_t off, uint64_t ino, unsigned int d_type)
{
	struct getdents_ctx *g = container_of(ctx, struct getdents_ctx, ctx);
	struct dirent64_s de;
	size_t reclen;

	reclen = sizeof(struct dirent64_s) + namlen + 1;
	reclen = (reclen + 7) & ~7;

	if (g->pos + reclen > g->count)
		return 1;

	memset(&de, 0, sizeof(de));
	de.d_ino = ino;
	de.d_off = off;
	de.d_reclen = reclen;
	de.d_type = d_type;

	/* Header, name, then NUL + alignment padding, all zeroed. */
	memcpy(g->buf + g->pos, &de, sizeof(de));
	memcpy(g->buf + g->pos + sizeof(de), name, namlen);
	memset(g->buf + g->pos + sizeof(de) + namlen, 0,
	       reclen - sizeof(de) - (size_t)namlen);

	g->pos += reclen;
	return 0;
}

long fd_getdents64(unsigned int fd, struct dirent64_s *buf, size_t count)
{
	struct files_struct *files = cur_files;
	struct file *file;
	struct getdents_ctx ctx;
	long ret;

	if (!files)
		return -EBADF;

	file = fget(files, fd);
	if (!file)
		return -EBADF;

	memset(&ctx, 0, sizeof(ctx));
	ctx.ctx.actor = filldir;
	ctx.buf = (char *)buf;
	ctx.count = count;
	ctx.pos = 0;

	ret = vfs_readdir(file, &ctx.ctx);
	return ret == 0 ? (long)ctx.pos : ret;
}

long fd_ioctl(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	struct files_struct *files = cur_files;
	struct file *file;

	if (!files)
		return -EBADF;

	file = fget(files, fd);
	if (!file)
		return -EBADF;

	if (file->f_op && file->f_op->ioctl)
		return file->f_op->ioctl(file, cmd, arg);
	return -ENOTTY;
}

/*
 * Change the current task's cwd.  Relative @path resolves against the
 * existing cwd (follow_path), matching POSIX chdir semantics.
 */
long fd_chdir(const char *path)
{
	struct files_struct *files = cur_files;
	struct path p;
	int ret;

	if (!files)
		return -EBADF;

	ret = vfs_path_lookup(path, &p);
	if (ret < 0)
		return ret;

	if (!p.dentry->d_inode || !S_ISDIR(p.dentry->d_inode->i_mode)) {
		dput(p.dentry);
		return -ENOTDIR;
	}

	/* The lookup reference on p.dentry transfers into the cwd slot. */
	if (files->fs_ctx->cwd_dentry)
		dput(files->fs_ctx->cwd_dentry);
	files->fs_ctx->cwd_dentry = p.dentry;
	files->fs_ctx->cwd_mnt = p.mnt;
	return 0;
}

long fd_mmap(unsigned int fd, uint64_t *uva, size_t length, loff_t offset)
{
	struct files_struct *files = cur_files;
	struct file *file;

	if (!files)
		return -EBADF;

	file = fget(files, fd);
	if (!file)
		return -EBADF;

	return vfs_file_mmap(file, uva, length, offset);
}
