#ifndef __VFS_INODE_H__
#define __VFS_INODE_H__

#include "types.h"
#include "dlist.h"
#include "rbtree.h"

struct block_device;
struct super_block;
struct dentry;
struct file_operations;

/* Address space embedded in each inode. */
struct address_space {
	struct rb_tree		page_tree;
	struct block_device	*host_bdev;
	struct dlist_node	buffers;
};

/* In-memory inode. */
struct inode {
	unsigned long			i_ino;
	uint16_t			i_mode;
	unsigned int			i_nlink;
	uint32_t			i_uid;
	uint32_t			i_gid;
	uint64_t			i_size;
	uint64_t			i_blocks;
	struct super_block		*i_sb;
	const struct inode_operations	*i_op;
	const struct file_operations	*i_fop;
	struct rb_node			i_rbnode;	/* inode cache tree node */
	struct address_space		i_data;
	struct dlist_node		i_dentry;	/* dentries attached to this inode */
	struct dlist_node		i_dirty_list;	/* dirty inode list */
	int				i_count;
	int				i_dirty;
	void				*i_private;
};

struct inode_operations {
	int (*lookup)(struct inode *dir, struct dentry *dentry,
		      struct dentry **found);
	int (*create)(struct inode *dir, struct dentry *dentry, uint16_t mode);
	int (*mkdir)(struct inode *dir, struct dentry *dentry, uint16_t mode);
	int (*unlink)(struct inode *dir, struct dentry *dentry);
	int (*rmdir)(struct inode *dir, struct dentry *dentry);
};

/* Inode cache management. */
void inode_cache_init(void);
struct inode *iget(struct super_block *sb, unsigned long ino);
struct inode *new_inode(struct super_block *sb);
void insert_inode_hash(struct inode *inode);
void iput(struct inode *inode);
void mark_inode_dirty(struct inode *inode);
int write_inode_now(struct inode *inode);
void sync_inodes(void);

#endif /* __VFS_INODE_H__ */
