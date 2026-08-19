#ifndef __VFS_MOUNT_H__
#define __VFS_MOUNT_H__

#include "super.h"
#include "dentry.h"

/* Mounted filesystem instance. */
struct vfsmount {
	struct dentry	*mnt_root;
	struct super_block *mnt_sb;
	struct dentry	*mnt_mountpoint;
	struct vfsmount	*mnt_parent;
	char		mnt_devname[64];
	struct dlist_node mnt_list;
};

int kern_mount(struct file_system_type *fs, const char *dev_name,
	       struct vfsmount **out);

void vfs_mount_init(void);

#endif /* __VFS_MOUNT_H__ */
