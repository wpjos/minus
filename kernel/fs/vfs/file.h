#ifndef __VFS_FILE_H__
#define __VFS_FILE_H__

#include "types.h"
#include "dlist.h"

struct vfsmount;
struct dentry;
struct inode;

/* Per-task open-file table. */
#define NR_OPEN_DEFAULT 16

struct files_struct {
	struct file *fd_array[NR_OPEN_DEFAULT];
	int count;
};

/* In-memory file handle. */
struct file {
	struct dentry			*f_dentry;
	struct vfsmount			*f_vfsmnt;
	const struct file_operations	*f_op;
	loff_t				f_pos;
	unsigned int			f_flags;
	int				f_count;
	void				*f_private;
};

/* File operation vector (defined by concrete filesystems). */
struct file_operations {
	int     (*open)(struct inode *, struct file *);
	int     (*release)(struct inode *, struct file *);
	ssize_t (*read)(struct file *, char *buf, size_t len, loff_t *pos);
	ssize_t (*write)(struct file *, const char *buf, size_t len, loff_t *pos);
	loff_t  (*llseek)(struct file *, loff_t offset, int whence);
};

/* VFS file helpers (public declarations are in include/fs/vfs.h). */
struct file *dentry_open(struct dentry *dentry, struct vfsmount *mnt,
			 int flags);

/* File descriptor table. */
struct files_struct *alloc_files_struct(void);
int get_unused_fd(struct files_struct *files);
void put_unused_fd(struct files_struct *files, int fd);
struct file *fget(struct files_struct *files, int fd);
void fd_install(struct files_struct *files, int fd, struct file *file);

#endif /* __VFS_FILE_H__ */
