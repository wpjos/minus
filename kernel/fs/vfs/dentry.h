#ifndef __VFS_DENTRY_H__
#define __VFS_DENTRY_H__

#include "inode.h"
#include "vfs.h"

struct super_block;
struct vfsmount;

/* Directory cache entry. */
struct dentry {
	struct dentry		*d_parent;
	struct dlist_node	d_child;
	struct dlist_node	d_subdirs;
	struct inode		*d_inode;
	struct super_block	*d_sb;
	char			d_name[64];
	int			d_count;
};

/* Path lookup result. */
struct path {
	struct dentry	*dentry;
	struct vfsmount	*mnt;
};

/* Path lookup. */
int path_lookup(const char *path, struct path *path_out);
struct dentry *lookup_one_len(const char *name, struct dentry *base, int len);

/* Dentry cache management. */
void dentry_cache_init(void);
struct dentry *d_alloc(struct dentry *parent, const char *name);
struct dentry *d_lookup(struct dentry *parent, const char *name);
void d_instantiate(struct dentry *dentry, struct inode *inode);
void dget(struct dentry *dentry);
void dput(struct dentry *dentry);

#endif /* __VFS_DENTRY_H__ */
