#ifndef __VFS_NAMEI_H__
#define __VFS_NAMEI_H__

#include "dentry.h"
#include "mount.h"

extern struct dentry *root_dentry;
extern struct vfsmount *root_mnt;

int vfs_path_lookup(const char *pathname, struct path *path);
int split_last_component(const char *pathname,
			 char *parent, size_t parent_size,
			 char *name, size_t name_size);
int vfs_do_mount(const char *dev_name, const char *fs_name,
		 const char *dir_name);
int vfs_do_umount(const char *dir_name);
int vfs_do_unlink(const char *pathname);
int vfs_do_mkdir(const char *pathname, uint16_t mode);
int vfs_do_rmdir(const char *pathname);

#endif /* __VFS_NAMEI_H__ */
