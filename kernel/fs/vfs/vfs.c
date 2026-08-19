#include "fs_type.h"
#include "super.h"
#include "inode.h"
#include "dentry.h"
#include "file.h"
#include "mount.h"
#include "buffer.h"
#include "namei.h"
#include "string.h"
#include "mm.h"
#include "errno.h"
#include "fcntl.h"
#include "stat.h"
#include "subsys.h"

static struct file_system_type *g_fs_types;
static struct dlist_node g_superblocks;

void vfs_init(void)
{
	g_fs_types = NULL;
	dlist_init(&g_superblocks);
	vfs_mount_init();
	inode_cache_init();
	dentry_cache_init();
	buffer_cache_init();
}

int register_filesystem(struct file_system_type *fs)
{
	struct file_system_type **p;

	if (!fs || !fs->name || !fs->mount)
		return -EINVAL;

	for (p = &g_fs_types; *p; p = &(*p)->next) {
		if (strcmp((*p)->name, fs->name) == 0)
			return -EEXIST;
	}

	fs->next = NULL;
	*p = fs;
	return 0;
}

struct file_system_type *get_fs_type(const char *name)
{
	struct file_system_type *fs;

	for (fs = g_fs_types; fs; fs = fs->next) {
		if (strcmp(fs->name, name) == 0)
			return fs;
	}
	return NULL;
}

void sb_add(struct super_block *sb)
{
	dlist_add(&g_superblocks, &sb->s_list);
}

struct super_block *sb_alloc(struct file_system_type *type)
{
	struct super_block *sb;

	sb = (struct super_block *)kmalloc(sizeof(*sb));
	if (!sb)
		return NULL;
	memset(sb, 0, sizeof(*sb));
	sb->s_type = type;
	sb->s_count = 1;
	return sb;
}

/* File handle helpers. */

int dentry_open(struct dentry *dentry, struct vfsmount *mnt,
		int flags, struct file **out)
{
	struct file *file;
	struct inode *inode;
	int ret;

	if (!dentry || !out)
		return -EINVAL;

	dget(dentry);
	inode = dentry->d_inode;
	if (!inode) {
		dput(dentry);
		return -ENOENT;
	}

	file = (struct file *)kmalloc(sizeof(*file));
	if (!file) {
		dput(dentry);
		return -ENOMEM;
	}
	memset(file, 0, sizeof(*file));

	file->f_dentry = dentry;
	file->f_vfsmnt = mnt;
	file->f_flags = flags;
	file->f_pos = 0;
	file->f_count = 1;
	file->f_op = inode->i_fop;

	if (file->f_op && file->f_op->open) {
		ret = file->f_op->open(inode, file);
		if (ret < 0) {
			kfree(file);
			dput(dentry);
			return ret;
		}
	}

	*out = file;
	return 0;
}

static int vfs_create(const char *pathname, int flags, uint16_t mode,
		      struct file **out)
{
	char parent_path[256];
	char name[64];
	struct path p;
	struct dentry *dentry;
	struct file *file = NULL;
	int ret;

	if (!out)
		return -EINVAL;

	ret = split_last_component(pathname, parent_path, sizeof(parent_path),
				   name, sizeof(name));
	if (ret < 0)
		return ret;

	ret = vfs_path_lookup(parent_path, &p);
	if (ret < 0)
		return ret;

	if (!S_ISDIR(p.dentry->d_inode->i_mode)) {
		dput(p.dentry);
		return -ENOTDIR;
	}

	dentry = d_alloc(p.dentry, name);
	if (!dentry) {
		dput(p.dentry);
		return -ENOMEM;
	}

	if (dentry->d_inode) {
		dput(dentry);
		dput(p.dentry);
		return -EEXIST;
	}

	if (!p.dentry->d_inode->i_op ||
	    !p.dentry->d_inode->i_op->create) {
		dput(dentry);
		dput(p.dentry);
		return -EPERM;
	}

	ret = p.dentry->d_inode->i_op->create(p.dentry->d_inode, dentry, mode);
	if (ret < 0) {
		dput(dentry);
		dput(p.dentry);
		return ret;
	}

	ret = dentry_open(dentry, p.mnt, flags, &file);
	dput(dentry);
	dput(p.dentry);
	if (ret < 0)
		return ret;

	*out = file;
	return 0;
}

int vfs_open(const char *path, int flags, uint16_t mode, struct file **out)
{
	struct path p;
	struct file *file = NULL;
	int ret;

	if (!out)
		return -EINVAL;

	ret = vfs_path_lookup(path, &p);
	if (ret < 0) {
		if ((flags & O_CREAT) && ret == -ENOENT)
			return vfs_create(path, flags, mode, out);
		return ret;
	}

	if ((flags & O_DIRECTORY) && !S_ISDIR(p.dentry->d_inode->i_mode)) {
		dput(p.dentry);
		return -ENOTDIR;
	}

	if ((flags & O_TRUNC) && S_ISREG(p.dentry->d_inode->i_mode)) {
		p.dentry->d_inode->i_size = 0;
		mark_inode_dirty(p.dentry->d_inode);
	}

	ret = dentry_open(p.dentry, p.mnt, flags, &file);
	dput(p.dentry);	/* balance the ref taken by vfs_path_lookup */
	if (ret < 0)
		return ret;

	*out = file;
	return 0;
}

void vfs_close(struct file *filp)
{
	if (!filp)
		return;

	filp->f_count--;
	if (filp->f_count <= 0) {
		if (filp->f_op && filp->f_op->release)
			filp->f_op->release(filp->f_dentry->d_inode, filp);
		dput(filp->f_dentry);
		kfree(filp);
	}
}

ssize_t vfs_read(struct file *file, char *buf, size_t count, loff_t *pos)
{
	if (!file || !file->f_op || !file->f_op->read)
		return -EINVAL;
	return file->f_op->read(file, buf, count, pos ? pos : &file->f_pos);
}

ssize_t vfs_write(struct file *file, const char *buf, size_t count, loff_t *pos)
{
	if (!file || !file->f_op || !file->f_op->write)
		return -EINVAL;
	return file->f_op->write(file, buf, count, pos ? pos : &file->f_pos);
}

loff_t vfs_llseek(struct file *file, loff_t offset, int whence)
{
	loff_t newpos;

	if (!file)
		return -EINVAL;

	if (file->f_op && file->f_op->llseek)
		return file->f_op->llseek(file, offset, whence);

	switch (whence) {
	case 0:
		newpos = offset;
		break;
	case 1:
		newpos = file->f_pos + offset;
		break;
	case 2:
		newpos = file->f_dentry->d_inode->i_size + offset;
		break;
	default:
		return -EINVAL;
	}

	if (newpos < 0)
		newpos = 0;
	file->f_pos = newpos;
	return newpos;
}

long vfs_readdir(struct file *file, struct dir_context *ctx)
{
	long ret;

	if (!file || !file->f_op || !file->f_op->iterate)
		return -EINVAL;

	ctx->pos = file->f_pos;
	ret = file->f_op->iterate(file, ctx);
	if (ret == 0)
		file->f_pos = ctx->pos;
	return ret;
}

static void vfs_fill_stat(struct inode *inode, struct stat *st)
{
	memset(st, 0, sizeof(*st));
	st->st_dev = inode->i_sb->s_dev;
	st->st_ino = (uint32_t)inode->i_ino;
	st->st_mode = inode->i_mode;
	st->st_nlink = (uint16_t)inode->i_nlink;
	st->st_uid = (uint16_t)inode->i_uid;
	st->st_gid = (uint16_t)inode->i_gid;
	st->st_size = inode->i_size;
	st->st_blksize = inode->i_sb->s_blocksize;
	st->st_blocks = inode->i_blocks;
}

int vfs_stat(const char *pathname, struct stat *st)
{
	struct path p;
	int ret;

	ret = vfs_path_lookup(pathname, &p);
	if (ret < 0)
		return ret;

	if (!p.dentry->d_inode) {
		dput(p.dentry);
		return -ENOENT;
	}

	vfs_fill_stat(p.dentry->d_inode, st);
	dput(p.dentry);
	return 0;
}

int vfs_fstat(struct file *filp, struct stat *st)
{
	if (!filp || !filp->f_dentry || !filp->f_dentry->d_inode)
		return -EBADF;
	vfs_fill_stat(filp->f_dentry->d_inode, st);
	return 0;
}

/* Entry points backed by per-area helpers. */

int vfs_mount(const char *dev_name, const char *fs_name,
	      const char *dir_name)
{
	return vfs_do_mount(dev_name, fs_name, dir_name);
}

int vfs_umount(const char *dir_name)
{
	return vfs_do_umount(dir_name);
}

int vfs_unlink(const char *pathname)
{
	return vfs_do_unlink(pathname);
}

int vfs_mkdir(const char *pathname, uint16_t mode)
{
	return vfs_do_mkdir(pathname, mode);
}

int vfs_rmdir(const char *pathname)
{
	return vfs_do_rmdir(pathname);
}

static int fs_subsys_init(void)
{
	vfs_init();
	return 0;
}

static const struct subsys_ops fs_subsys_ops = {
	.name = "fs",
	.level = SUBSYS_LEVEL_FS,
	.init = fs_subsys_init,
};
subsys_register(fs, &fs_subsys_ops);
