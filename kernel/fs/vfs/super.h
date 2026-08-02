#ifndef __VFS_SUPER_H__
#define __VFS_SUPER_H__

#include "types.h"
#include "dlist.h"

/* Forward declarations */
struct block_device;
struct dentry;
struct inode;
struct file_system_type;

/* In-memory superblock. */
struct super_block {
	dev_t			s_dev;
	struct file_system_type		*s_type;
	struct dentry			*s_root;
	struct block_device		*s_bdev;
	unsigned long			s_blocksize;
	unsigned char			s_blocksize_bits;
	struct super_operations		*s_op;
	void				*s_fs_info;
	int				s_count;
	struct dlist_node		s_list;
};

/* Superblock operation vector. */
struct super_operations {
	struct inode *(*alloc_inode)(struct super_block *sb);
	void          (*destroy_inode)(struct inode *);
	int           (*write_inode)(struct inode *);
	void          (*put_super)(struct super_block *sb);
};

/* Superblock management. */
struct super_block *sb_alloc(struct file_system_type *type);
void sb_add(struct super_block *sb);

#endif /* __VFS_SUPER_H__ */
