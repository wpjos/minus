#ifndef __VFS_FS_TYPE_H__
#define __VFS_FS_TYPE_H__

#include "super.h"

/* Registered filesystem type. */
struct file_system_type {
	const char *name;
	struct file_system_type *next;
	struct super_block *(*mount)(struct file_system_type *, const char *dev_name,
				     const char *data);
	void (*kill_sb)(struct super_block *);
};

/* Filesystem type registration and lookup. */
int register_filesystem(struct file_system_type *fs);
struct file_system_type *get_fs_type(const char *name);

#endif /* __VFS_FS_TYPE_H__ */
