#include "fs_type.h"
#include "super.h"
#include "mount.h"
#include "namei.h"
#include "string.h"
#include "mm.h"
#include "errno.h"

static struct dlist_node g_mounts;

struct dentry *root_dentry = NULL;
struct vfsmount *root_mnt = NULL;

struct vfsmount *kern_mount(struct file_system_type *fs, const char *dev_name)
{
	struct super_block *sb;
	struct vfsmount *mnt;

	sb = fs->mount(fs, dev_name, NULL);
	if (!sb)
		return NULL;

	mnt = (struct vfsmount *)kmalloc(sizeof(*mnt));
	if (!mnt) {
		fs->kill_sb(sb);
		return NULL;
	}
	memset(mnt, 0, sizeof(*mnt));
	mnt->mnt_sb = sb;
	mnt->mnt_root = sb->s_root;
	dget(sb->s_root);

	dlist_init(&mnt->mnt_list);
	dlist_add(&g_mounts, &mnt->mnt_list);

	return mnt;
}

int vfs_do_mount(const char *dev_name, const char *fs_name,
		 const char *dir_name)
{
	struct file_system_type *fs;
	struct vfsmount *mnt;

	fs = get_fs_type(fs_name);
	if (!fs) {
		return -ENODEV;
	}

	mnt = kern_mount(fs, dev_name);
	if (!mnt) {
		return -EIO;
	}

	strncpy(mnt->mnt_devname, dev_name, sizeof(mnt->mnt_devname) - 1);

	if (strcmp(dir_name, "/") == 0) {
		root_mnt = mnt;
		root_dentry = mnt->mnt_root;
		return 0;
	}

	/* Non-root mounts are not wired yet; roll back. */
	dlist_del(&mnt->mnt_list);
	if (mnt->mnt_sb && mnt->mnt_sb->s_type->kill_sb)
		mnt->mnt_sb->s_type->kill_sb(mnt->mnt_sb);
	dput(mnt->mnt_root);
	kfree(mnt);
	return -ENOSYS;
}

int vfs_do_umount(const char *dir_name)
{
	struct vfsmount *mnt;

	dlist_for_each_entry(mnt, &g_mounts, mnt_list) {
		bool is_root = (mnt == root_mnt) && (strcmp(dir_name, "/") == 0);
		bool match_name = strcmp(mnt->mnt_devname, dir_name) == 0;

		if (!is_root && !match_name)
			continue;

		if (mnt == root_mnt) {
			root_mnt = NULL;
			root_dentry = NULL;
		}

		dlist_del(&mnt->mnt_list);
		if (mnt->mnt_sb && mnt->mnt_sb->s_type->kill_sb)
			mnt->mnt_sb->s_type->kill_sb(mnt->mnt_sb);
		dput(mnt->mnt_root);
		kfree(mnt);
		return 0;
	}

	return -EINVAL;
}

void vfs_mount_init(void)
{
	dlist_init(&g_mounts);
}
