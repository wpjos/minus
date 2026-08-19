#ifndef __VFS_FILE_H__
#define __VFS_FILE_H__

#include "types.h"
#include "dlist.h"
#include "cap.h"

struct vfsmount;
struct dentry;
struct inode;

#define NR_OPEN_DEFAULT 16

/*
 * Per-task filesystem context: cwd, and later root/umask.  This is
 * Linux's fs_struct, kept separate from the fd table because the two
 * have independent sharing semantics: clone(CLONE_FILES) shares the fd
 * table, clone(CLONE_FS) shares the fs context, and pthreads share both
 * (chdir() in one thread is visible process-wide).  A task may still
 * unshare one while keeping the other.
 */
struct fs_context {
	struct dentry	*cwd_dentry;	/* referenced; NULL -> fs root */
	struct vfsmount	*cwd_mnt;
};

/* Per-task open-file table. */
struct files_struct {
	struct file *fd_array[NR_OPEN_DEFAULT];
	int count;			/* tasks sharing this table */
	struct fs_context *fs_ctx;	/* fs context (see above) */
};

/* Directory iteration context. */
struct dir_context {
	long (*actor)(struct dir_context *ctx, const char *name, int namlen,
		      loff_t pos, uint64_t ino, unsigned int d_type);
	loff_t pos;
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
	long    (*iterate)(struct file *, struct dir_context *);
	long    (*ioctl)(struct file *, unsigned int cmd, unsigned long arg);
	long    (*mmap)(struct file *, uint64_t *addr, size_t length,
			loff_t offset);
};

/* VFS file helpers (the vfs_* entry points are declared in vfs.h). */
int dentry_open(struct dentry *dentry, struct vfsmount *mnt,
		int flags, struct file **out);

/* File descriptor table. */
struct files_struct *alloc_files_struct(void);
void free_files_struct(struct files_struct *files);
int get_unused_fd(struct files_struct *files);
void put_unused_fd(struct files_struct *files, int fd);
struct file *fget(struct files_struct *files, int fd);
void fd_install(struct files_struct *files, int fd, struct file *file);

/*
 * Current task's open-file table, mirrored per-CPU through core's switch
 * notification bus (fdtable_init() subscribes at fs bring-up).  NULL in
 * early-boot context (before the first real task runs).
 */
struct files_struct *fs_current_files(void);
struct files_struct *files_from_cap(cap_t files_cap);
int fdtable_init(void);

/*
 * FD layer: file operations scoped to an fd of the current task's
 * fdtable (via the per-CPU mirror).  Called directly by the in-module
 * sysif layer and proc's loader; cross-module callers that must name a
 * specific fdtable use the fs service table's capability ops instead.
 * A NULL mirror (early boot) yields -EBADF.
 */
struct stat;
struct dirent64_s;
int fd_openat(const char *pathname, int flags, uint16_t mode);
int fd_close(unsigned int fd);
ssize_t fd_read(unsigned int fd, void *buf, size_t count);
ssize_t fd_write(unsigned int fd, const void *buf, size_t count);
long fd_lseek(unsigned int fd, long offset, int whence);
long fd_fstat(unsigned int fd, struct stat *st);
long fd_getdents64(unsigned int fd, struct dirent64_s *buf, size_t count);
long fd_ioctl(unsigned int fd, unsigned int cmd, unsigned long arg);
long fd_mmap(unsigned int fd, uint64_t *uva, size_t length, loff_t offset);
long fd_chdir(const char *path);


#endif /* __VFS_FILE_H__ */
