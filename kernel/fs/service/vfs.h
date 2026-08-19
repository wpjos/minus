#ifndef __VFS_H__
#define __VFS_H__

/*
 * VFS core interface - private to the FS module (kernel/fs/).
 *
 * The public cross-module surface is include/fs/fs_service.h (the
 * capability-addressed service table); current-task file operations are
 * the fd layer in fdtable.c (file.h).  Everything declared here is VFS
 * mechanics for the FS module itself: service layer, sysif layer and
 * filesystem backends.
 */

#include "types.h"
#include "stat.h"

struct file;
struct dir_context;

/* Subsystem init. */
void vfs_init(void);

/* Mount management. */
int vfs_mount(const char *dev_name, const char *fs_name,
	      const char *dir_name);
int vfs_umount(const char *dir_name);

/* File operations. */
int vfs_open(const char *pathname, int flags, uint16_t mode,
	     struct file **file);
void vfs_close(struct file *filp);
ssize_t vfs_read(struct file *filp, char *buf, size_t len, loff_t *pos);
ssize_t vfs_write(struct file *filp, const char *buf, size_t len, loff_t *pos);
loff_t vfs_llseek(struct file *filp, loff_t offset, int whence);
long vfs_readdir(struct file *filp, struct dir_context *ctx);
long vfs_file_mmap(struct file *filp, uint64_t *addr, size_t length,
		   loff_t offset);

/* Path/name/stat operations. */
int vfs_stat(const char *pathname, struct stat *st);
int vfs_fstat(struct file *filp, struct stat *st);
int vfs_unlink(const char *pathname);
int vfs_mkdir(const char *pathname, uint16_t mode);
int vfs_rmdir(const char *pathname);

#endif /* __VFS_H__ */
