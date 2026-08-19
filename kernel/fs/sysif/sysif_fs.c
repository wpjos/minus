#include "vfs.h"
#include "sysif_fs.h"
#include "task.h"
#include "uaccess.h"
#include "string.h"
#include "errno.h"
#include "stat.h"
#include "fcntl.h"
#include "dirent.h"
#include "file.h"

#define PATH_LEN 256

static long copy_path_from_user(char *dst, const char *src)
{
	long ret;

	ret = strncpy_from_user(dst, src, PATH_LEN);
	if (ret != 0)
		return -EFAULT;
	return 0;
}

long sys_openat(int dirfd, const char *pathname, int flags, uint16_t mode)
{
	char path[PATH_LEN];
	struct file *file;
	int fd;
	long ret;

	(void)dirfd;

	ret = copy_path_from_user(path, pathname);
	if (ret < 0)
		return ret;

	ret = vfs_open(path, flags, mode, &file);
	if (ret < 0)
		return ret;

	fd = get_unused_fd(current->files);
	if (fd < 0) {
		vfs_close(file);
		return fd;
	}

	fd_install(current->files, fd, file);
	return fd;
}

long sys_close(unsigned int fd)
{
	struct file *file;

	file = fget(current->files, fd);
	if (!file)
		return -EBADF;

	put_unused_fd(current->files, fd);
	vfs_close(file);
	return 0;
}

long sys_read(unsigned int fd, char *buf, size_t count)
{
	struct file *file;
	char kbuf[256];
	size_t done = 0;

	file = fget(current->files, fd);
	if (!file)
		return -EBADF;

	while (done < count) {
		size_t chunk = count - done;
		long ret;
		long cpy;

		if (chunk > sizeof(kbuf))
			chunk = sizeof(kbuf);

		ret = vfs_read(file, kbuf, chunk, NULL);
		if (ret < 0)
			return done ? (long)done : ret;
		if (ret == 0)
			break;

		cpy = copy_to_user(buf + done, kbuf, (size_t)ret);
		if (cpy != 0)
			return done ? (long)done : -EFAULT;

		done += (size_t)ret;
		if ((size_t)ret < chunk)
			break;
	}

	return (long)done;
}

long sys_write(unsigned int fd, const char *buf, size_t count)
{
	struct file *file;
	char kbuf[256];
	size_t done = 0;

	file = fget(current->files, fd);
	if (!file)
		return -EBADF;

	while (done < count) {
		size_t chunk = count - done;
		long ret;

		if (chunk > sizeof(kbuf))
			chunk = sizeof(kbuf);

		if (copy_from_user(kbuf, buf + done, chunk) != 0)
			return done ? (long)done : -EFAULT;

		ret = vfs_write(file, kbuf, chunk, NULL);
		if (ret < 0)
			return done ? (long)done : ret;

		done += (size_t)ret;
		if ((size_t)ret < chunk)
			break;
	}

	return (long)done;
}

long sys_lseek(unsigned int fd, long offset, int whence)
{
	struct file *file;

	file = fget(current->files, fd);
	if (!file)
		return -EBADF;

	return (long)vfs_llseek(file, (loff_t)offset, whence);
}

long sys_newfstatat(int dirfd, const char *pathname, struct stat *statbuf,
		    int flags)
{
	char path[PATH_LEN];
	struct stat st;
	long ret;

	(void)dirfd;
	(void)flags;

	ret = copy_path_from_user(path, pathname);
	if (ret < 0)
		return ret;

	ret = vfs_stat(path, &st);
	if (ret < 0)
		return ret;

	if (copy_to_user(statbuf, &st, sizeof(st)) != 0)
		return -EFAULT;
	return 0;
}

long sys_fstat(unsigned int fd, struct stat *statbuf)
{
	struct file *file;
	struct stat st;
	long ret;

	file = fget(current->files, fd);
	if (!file)
		return -EBADF;

	ret = vfs_fstat(file, &st);
	if (ret < 0)
		return ret;

	if (copy_to_user(statbuf, &st, sizeof(st)) != 0)
		return -EFAULT;
	return 0;
}

long sys_unlinkat(int dirfd, const char *pathname, int flags)
{
	char path[PATH_LEN];
	long ret;

	(void)dirfd;
	(void)flags;

	ret = copy_path_from_user(path, pathname);
	if (ret < 0)
		return ret;

	return vfs_unlink(path);
}

long sys_mkdirat(int dirfd, const char *pathname, uint16_t mode)
{
	char path[PATH_LEN];
	long ret;

	(void)dirfd;

	ret = copy_path_from_user(path, pathname);
	if (ret < 0)
		return ret;

	return vfs_mkdir(path, mode);
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
	char rec[128];

	reclen = sizeof(struct dirent64_s) + namlen + 1;
	reclen = (reclen + 7) & ~7;

	if (g->pos + reclen > g->count)
		return 1;

	if (reclen > sizeof(rec))
		return -EINVAL;

	memset(&de, 0, sizeof(de));
	de.d_ino = ino;
	de.d_off = off;
	de.d_reclen = reclen;
	de.d_type = d_type;

	memset(rec, 0, reclen);
	memcpy(rec, &de, sizeof(de));
	memcpy(rec + sizeof(de), name, namlen);
	rec[sizeof(de) + namlen] = '\0';

	if (copy_to_user(g->buf + g->pos, rec, reclen) != 0)
		return -EFAULT;

	g->pos += reclen;
	return 0;
}

long sys_getdents64(unsigned int fd, char *buf, unsigned int count)
{
	struct file *file;
	struct getdents_ctx ctx;
	long ret;

	file = fget(current->files, fd);
	if (!file)
		return -EBADF;

	memset(&ctx, 0, sizeof(ctx));
	ctx.ctx.actor = filldir;
	ctx.buf = buf;
	ctx.count = count;
	ctx.pos = 0;

	ret = vfs_readdir(file, &ctx.ctx);
	return ret == 0 ? (long)ctx.pos : ret;
}

long sys_ioctl(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	struct file *file;

	file = fget(current->files, fd);
	if (!file)
		return -EBADF;

	if (file->f_op && file->f_op->ioctl)
		return file->f_op->ioctl(file, cmd, arg);

	return -ENOTTY;
}

long sys_mmap(void *addr, size_t length, int prot, int flags,
	      unsigned int fd, long offset)
{
	struct file *file;
	void *uva;
	long ret;

	(void)addr;
	(void)prot;
	(void)flags;

	file = fget(current->files, fd);
	if (!file)
		return -EBADF;

	if (!file->f_op || !file->f_op->mmap)
		return -ENODEV;

	ret = file->f_op->mmap(file, &uva, length, (loff_t)offset);
	if (ret < 0)
		return ret;

	return (long)uva;
}
