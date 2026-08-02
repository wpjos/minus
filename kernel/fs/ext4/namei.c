#include "ext4.h"
#include "string.h"
#include "mm.h"
#include "errno.h"
#include "stat.h"
#include "buffer.h"

int ext4_create(struct inode *dir, struct dentry *dentry, uint16_t mode)
{
	struct inode *inode;
	struct ext4_inode_info *ei;
	struct ext4_extent_header *eh;
	int ret;

	inode = new_inode(dir->i_sb);
	if (!inode)
		return -ENOMEM;

	ret = ext4_alloc_inode(dir->i_sb, inode);
	if (ret < 0) {
		iput(inode);
		return ret;
	}

	inode->i_mode = (mode & ~S_IFMT) | S_IFREG;
	inode->i_nlink = 1;
	inode->i_uid = 0;
	inode->i_gid = 0;
	inode->i_size = 0;
	inode->i_blocks = 0;
	inode->i_op = &ext4_file_inode_operations;
	inode->i_fop = &ext4_file_operations;

	insert_inode_hash(inode);

	ei = inode->i_private;
	eh = (struct ext4_extent_header *)ei->i_data;
	eh->eh_magic = cpu_to_le16(EXT4_EXTENT_HEADER_MAGIC);
	eh->eh_entries = 0;
	eh->eh_max = cpu_to_le16((sizeof(ei->i_data) -
				    sizeof(struct ext4_extent_header)) /
				   sizeof(struct ext4_extent));
	eh->eh_depth = 0;

	ret = ext4_add_entry(dir, dentry->d_name, strlen(dentry->d_name),
			     inode->i_ino, EXT4_FT_REG_FILE);
	if (ret < 0) {
		iput(inode);
		return ret;
	}

	mark_inode_dirty(inode);
	d_instantiate(dentry, inode);
	return 0;
}

int ext4_mkdir(struct inode *dir, struct dentry *dentry, uint16_t mode)
{
	struct inode *inode;
	struct ext4_inode_info *ei;
	struct ext4_extent_header *eh;
	int ret;

	inode = new_inode(dir->i_sb);
	if (!inode)
		return -ENOMEM;

	ret = ext4_alloc_inode(dir->i_sb, inode);
	if (ret < 0) {
		iput(inode);
		return ret;
	}

	inode->i_mode = (mode & ~S_IFMT) | S_IFDIR;
	inode->i_nlink = 2;
	inode->i_uid = 0;
	inode->i_gid = 0;
	inode->i_size = 0;
	inode->i_blocks = 0;
	inode->i_op = &ext4_dir_inode_operations;
	inode->i_fop = &ext4_dir_operations;

	insert_inode_hash(inode);

	ei = inode->i_private;
	eh = (struct ext4_extent_header *)ei->i_data;
	eh->eh_magic = cpu_to_le16(EXT4_EXTENT_HEADER_MAGIC);
	eh->eh_entries = 0;
	eh->eh_max = cpu_to_le16((sizeof(ei->i_data) -
				    sizeof(struct ext4_extent_header)) /
				   sizeof(struct ext4_extent));
	eh->eh_depth = 0;

	ret = ext4_make_empty(inode, dir);
	if (ret < 0) {
		iput(inode);
		return ret;
	}

	ret = ext4_add_entry(dir, dentry->d_name, strlen(dentry->d_name),
			     inode->i_ino, EXT4_FT_DIR);
	if (ret < 0) {
		iput(inode);
		return ret;
	}

	dir->i_nlink++;
	mark_inode_dirty(dir);
	mark_inode_dirty(inode);
	d_instantiate(dentry, inode);
	return 0;
}

int ext4_unlink(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = dentry->d_inode;
	int ret;

	if (!inode)
		return -ENOENT;

	ret = ext4_delete_entry(dir, dentry->d_name, strlen(dentry->d_name));
	if (ret < 0)
		return ret;

	inode->i_nlink--;
	if (inode->i_nlink == 0)
		ext4_truncate_blocks(inode);
	mark_inode_dirty(inode);
	return 0;
}

int ext4_rmdir(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = dentry->d_inode;
	int ret;

	if (!inode)
		return -ENOENT;

	if (!S_ISDIR(inode->i_mode))
		return -ENOTDIR;

	if (ext4_dir_is_empty(inode) <= 0)
		return -ENOTEMPTY;

	ret = ext4_delete_entry(dir, dentry->d_name, strlen(dentry->d_name));
	if (ret < 0)
		return ret;

	inode->i_nlink = 0;
	ext4_truncate_blocks(inode);
	mark_inode_dirty(inode);

	dir->i_nlink--;
	mark_inode_dirty(dir);
	return 0;
}

struct inode_operations ext4_dir_inode_operations = {
	.lookup = ext4_lookup,
	.create = ext4_create,
	.mkdir = ext4_mkdir,
	.unlink = ext4_unlink,
	.rmdir = ext4_rmdir,
};
