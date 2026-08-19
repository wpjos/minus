#ifndef __VFS_H__
#define __VFS_H__

/*
 * Public VFS interface.
 *
 * This is the only header the rest of the kernel (syscall layer, init,
 * task management, etc.) should include to use filesystem services.
 * The concrete VFS data structures live in kernel/fs/fs.h and the
 * kernel/fs/vfs/ directory, which are private to the filesystem
 * implementation.
 */

#include "types.h"
#include "stat.h"

struct files_struct;
struct file;
struct dir_context;

/* Subsystem init. */
void vfs_init(void);

/* Mount management. */
int vfs_mount(const char *dev_name, const char *fs_name,
	      const char *dir_name);
int vfs_umount(const char *dir_name);

/* Per-task file descriptor table. */
struct files_struct *alloc_files_struct(void);
int get_unused_fd(struct files_struct *files);
void put_unused_fd(struct files_struct *files, int fd);
struct file *fget(struct files_struct *files, int fd);
void fd_install(struct files_struct *files, int fd, struct file *file);

/* File operations. */
int vfs_open(const char *pathname, int flags, uint16_t mode,
	     struct file **file);
void vfs_close(struct file *filp);
ssize_t vfs_read(struct file *filp, char *buf, size_t len, loff_t *pos);
ssize_t vfs_write(struct file *filp, const char *buf, size_t len, loff_t *pos);
loff_t vfs_llseek(struct file *filp, loff_t offset, int whence);
long vfs_readdir(struct file *filp, struct dir_context *ctx);

/* Path/name/stat operations. */
int vfs_stat(const char *pathname, struct stat *st);
int vfs_fstat(struct file *filp, struct stat *st);
int vfs_unlink(const char *pathname);
int vfs_mkdir(const char *pathname, uint16_t mode);
int vfs_rmdir(const char *pathname);

#endif /* __VFS_H__ */
