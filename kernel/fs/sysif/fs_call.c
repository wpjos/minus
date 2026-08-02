#include "vfs.h"
#include "fs_call.h"
#include "task.h"
#include "uaccess.h"
#include "string.h"
#include "errno.h"
#include "stat.h"
#include "fcntl.h"
#include "printk.h"

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

	file = vfs_open(path, flags, mode);
	if (!file)
		return -ENOENT;

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

	/* fd 1 is a temporary console hook until a real stdout device is wired. */
	if (fd == 1) {
		while (done < count) {
			size_t chunk = count - done;
			long ret;
			size_t copy;

			if (chunk > sizeof(kbuf) - 1)
				chunk = sizeof(kbuf) - 1;

			ret = copy_from_user(kbuf, buf + done, chunk);
			copy = chunk - (size_t)ret;
			if (copy == 0)
				return done ? (long)done : -EFAULT;

			kbuf[copy] = '\0';
			printk("%s", kbuf);

			done += copy;
			if (ret != 0)
				break;
		}
		return (long)done;
	}

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
