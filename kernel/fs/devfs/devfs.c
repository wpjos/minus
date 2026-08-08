#include "fs_type.h"
#include "super.h"
#include "inode.h"
#include "dentry.h"
#include "file.h"
#include "string.h"
#include "mm.h"
#include "errno.h"
#include "stat.h"
#include "dirent.h"
#include "uart.h"
#include "module.h"

static struct file_operations console_fops;

static ssize_t console_read(struct file *file, char *buf, size_t len, loff_t *pos)
{
	size_t i;

	(void)file;
	(void)pos;

	for (i = 0; i < len; i++)
		buf[i] = uart_getc();

	return (ssize_t)len;
}

static ssize_t console_write(struct file *file, const char *buf, size_t len,
			     loff_t *pos)
{
	size_t i;

	(void)file;
	(void)pos;

	for (i = 0; i < len; i++)
		uart_putc(buf[i]);

	return (ssize_t)len;
}

static struct file_operations console_fops = {
	.read = console_read,
	.write = console_write,
};

static struct inode *devfs_new_console_inode(struct super_block *sb)
{
	struct inode *inode;

	inode = new_inode(sb);
	if (!inode)
		return NULL;

	inode->i_ino = 2;
	inode->i_mode = S_IFCHR | 0666;
	inode->i_nlink = 1;
	inode->i_uid = 0;
	inode->i_gid = 0;
	inode->i_size = 0;
	inode->i_fop = &console_fops;
	return inode;
}

static struct dentry *devfs_lookup(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode;

	(void)dir;

	if (strcmp(dentry->d_name, "console") != 0)
		return NULL;

	inode = devfs_new_console_inode(dir->i_sb);
	if (!inode)
		return NULL;

	d_instantiate(dentry, inode);
	return dentry;
}

static long devfs_iterate(struct file *file, struct dir_context *ctx)
{
	static const struct {
		const char *name;
		uint64_t ino;
		unsigned int d_type;
	} entries[] = {
		{ ".", 1, DT_DIR },
		{ "..", 1, DT_DIR },
		{ "console", 2, DT_CHR },
	};
	size_t i;
	loff_t pos;

	(void)file;

	pos = ctx->pos;
	if (pos < 0)
		return -EINVAL;

	for (i = (size_t)pos; i < sizeof(entries) / sizeof(entries[0]); i++) {
		if (ctx->actor(ctx, entries[i].name, strlen(entries[i].name),
			       (loff_t)(i + 1), entries[i].ino,
			       entries[i].d_type) != 0)
			break;
		ctx->pos = (loff_t)(i + 1);
	}

	return 0;
}

static struct inode_operations devfs_dir_inode_operations = {
	.lookup = devfs_lookup,
};

static struct file_operations devfs_dir_operations = {
	.iterate = devfs_iterate,
};

static struct super_block *devfs_mount(struct file_system_type *fs,
				       const char *dev_name, const char *data)
{
	struct super_block *sb;
	struct inode *root_inode;
	struct dentry *root_dentry;

	(void)dev_name;
	(void)data;

	sb = sb_alloc(fs);
	if (!sb)
		return NULL;

	sb->s_dev = 0;
	sb->s_blocksize = 4096;

	root_inode = new_inode(sb);
	if (!root_inode)
		goto fail_sb;

	root_inode->i_ino = 1;
	root_inode->i_mode = S_IFDIR | 0755;
	root_inode->i_nlink = 2;
	root_inode->i_uid = 0;
	root_inode->i_gid = 0;
	root_inode->i_size = 0;
	root_inode->i_op = &devfs_dir_inode_operations;
	root_inode->i_fop = &devfs_dir_operations;

	root_dentry = d_alloc(NULL, "/");
	if (!root_dentry)
		goto fail_inode;

	d_instantiate(root_dentry, root_inode);
	sb->s_root = root_dentry;
	sb_add(sb);
	return sb;

fail_inode:
	iput(root_inode);
fail_sb:
	kfree(sb);
	return NULL;
}

static void devfs_kill_sb(struct super_block *sb)
{
	(void)sb;
}

static struct file_system_type devfs_fs_type = {
	.name = "devfs",
	.mount = devfs_mount,
	.kill_sb = devfs_kill_sb,
};

static void devfs_init_once(void)
{
	register_filesystem(&devfs_fs_type);
}
module_register(devfs, MODULE_LEVEL_LOW, devfs_init_once);
