#include "file.h"
#include "vfs.h"
#include "mm_service.h"
#include "syscall_dispatch.h"
#include "string.h"
#include "errno.h"

/*
 * Syscall stubs for the FS.
 *
 * sysif/ is inside the FS module, so these handlers speak kernel pointers
 * only: the fd layer (fdtable.c) and the VFS core are called directly -
 * no service table, no capability round-trip.  The current task's fdtable
 * comes from the per-CPU mirror fed by core's switch notification bus.
 * Bouncing user paths/buffers through small kernel stack buffers happens
 * here and only here; everything below works on kernel addresses.
 */

#define SYS_PATH_LEN	256
#define SYS_IO_BOUNCE	256
#define SYS_DENTS_BOUNCE 1024

static long copy_path_from_user(char *dst, const char *src)
{
	if (mm_call(strncpy_from_user, dst, (user_addr_t)(uintptr_t)src,
		    SYS_PATH_LEN) != 0)
		return -EFAULT;
	return 0;
}

static long sys_openat(int dirfd, const char *pathname, int flags,
		       uint16_t mode)
{
	char path[SYS_PATH_LEN];
	long ret;

	(void)dirfd;

	ret = copy_path_from_user(path, pathname);
	if (ret < 0)
		return ret;

	return fd_openat(path, flags, mode);
}

static long sys_close(unsigned int fd)
{
	return fd_close(fd);
}

static long sys_read(unsigned int fd, user_addr_t buf, size_t count)
{
	char kbuf[SYS_IO_BOUNCE];
	size_t done = 0;

	if (count == 0)
		return 0;

	while (done < count) {
		size_t chunk = count - done;
		ssize_t ret;

		if (chunk > sizeof(kbuf))
			chunk = sizeof(kbuf);

		ret = fd_read(fd, kbuf, chunk);
		if (ret < 0)
			return done ? (long)done : ret;
		if (ret == 0)
			break;

		if (mm_call(copy_to_user,
			    (user_addr_t)((uintptr_t)buf + done),
			    kbuf, (size_t)ret) != 0)
			return done ? (long)done : -EFAULT;

		done += (size_t)ret;
		if ((size_t)ret < chunk)
			break;
	}

	return (long)done;
}

static long sys_write(unsigned int fd, user_addr_t buf, size_t count)
{
	char kbuf[SYS_IO_BOUNCE];
	size_t done = 0;

	if (count == 0)
		return 0;

	while (done < count) {
		size_t chunk = count - done;
		ssize_t ret;

		if (chunk > sizeof(kbuf))
			chunk = sizeof(kbuf);

		if (mm_call(copy_from_user, kbuf,
			    (user_addr_t)((uintptr_t)buf + done),
			    chunk) != 0)
			return done ? (long)done : -EFAULT;

		ret = fd_write(fd, kbuf, chunk);
		if (ret < 0)
			return done ? (long)done : ret;

		done += (size_t)ret;
		if ((size_t)ret < chunk)
			break;
	}

	return (long)done;
}

static long sys_lseek(unsigned int fd, long offset, int whence)
{
	return fd_lseek(fd, offset, whence);
}

static long sys_newfstatat(int dirfd, const char *pathname,
			   user_addr_t statbuf, int flags)
{
	char path[SYS_PATH_LEN];
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

	if (mm_call(copy_to_user, statbuf, &st, sizeof(st)) != 0)
		return -EFAULT;
	return 0;
}

static long sys_fstat(unsigned int fd, user_addr_t statbuf)
{
	struct stat st;
	long ret;

	ret = fd_fstat(fd, &st);
	if (ret < 0)
		return ret;

	if (mm_call(copy_to_user, statbuf, &st, sizeof(st)) != 0)
		return -EFAULT;
	return 0;
}

static long sys_unlinkat(int dirfd, const char *pathname, int flags)
{
	char path[SYS_PATH_LEN];
	long ret;

	(void)dirfd;
	(void)flags;

	ret = copy_path_from_user(path, pathname);
	if (ret < 0)
		return ret;

	return vfs_unlink(path);
}

static long sys_mkdirat(int dirfd, const char *pathname, uint16_t mode)
{
	char path[SYS_PATH_LEN];
	long ret;

	(void)dirfd;

	ret = copy_path_from_user(path, pathname);
	if (ret < 0)
		return ret;

	return vfs_mkdir(path, mode);
}

static long sys_getdents64(unsigned int fd, user_addr_t buf, unsigned int count)
{
	char kbuf[SYS_DENTS_BOUNCE];
	long ret;

	if (count > sizeof(kbuf))
		count = sizeof(kbuf);

	ret = fd_getdents64(fd,
			    (struct dirent64_s *)kbuf, count);
	if (ret <= 0)
		return ret;

	if (mm_call(copy_to_user, buf, kbuf, (size_t)ret) != 0)
		return -EFAULT;
	return ret;
}

static long sys_chdir(const char *pathname)
{
	char path[SYS_PATH_LEN];
	long ret;

	ret = copy_path_from_user(path, pathname);
	if (ret < 0)
		return ret;
	return fd_chdir(path);
}

static long sys_ioctl(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	return fd_ioctl(fd, cmd, arg);
}

/*
 * File/device-backed mmap is an FS syscall: it names an fd, so it lives
 * here on the fd layer.  Anonymous mapping will be MM's own syscall when
 * it arrives.  All scalar/return-value passing; no user buffers involved.
 */
static long sys_mmap(void *addr, size_t length, int prot, int flags,
		     unsigned int fd, long offset)
{
	uint64_t uva;
	long ret;

	(void)addr;
	(void)prot;
	(void)flags;

	ret = fd_mmap(fd, &uva, length, (loff_t)offset);
	if (ret < 0)
		 return ret;
	return (long)uva;
}

syscall_register(SYS_OPENAT, sys_openat);
syscall_register(SYS_CLOSE, sys_close);
syscall_register(SYS_READ, sys_read);
syscall_register(SYS_WRITE, sys_write);
syscall_register(SYS_LSEEK, sys_lseek);
syscall_register(SYS_NEWFSTATAT, sys_newfstatat);
syscall_register(SYS_FSTAT, sys_fstat);
syscall_register(SYS_UNLINKAT, sys_unlinkat);
syscall_register(SYS_MKDIRAT, sys_mkdirat);
syscall_register(SYS_GETDENTS64, sys_getdents64);
syscall_register(SYS_CHDIR, sys_chdir);
syscall_register(SYS_IOCTL, sys_ioctl);
syscall_register(SYS_MMAP, sys_mmap);
