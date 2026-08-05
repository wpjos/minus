#include "ext4.h"
#include "string.h"
#include "mm.h"
#include "errno.h"
#include "stat.h"
#include "bitops.h"

static struct ext4_dir_entry_2 *ext4_next_entry(struct ext4_dir_entry_2 *de)
{
	return (struct ext4_dir_entry_2 *)((char *)de +
					   le16_to_cpu(de->rec_len));
}

struct dentry *ext4_lookup(struct inode *dir, struct dentry *dentry)
{
	struct buffer_head *bh;
	struct ext4_dir_entry_2 *de;
	uint32_t block;
	uint32_t block_size = dir->i_sb->s_blocksize;
	uint32_t nr_blocks = (dir->i_size + block_size - 1) / block_size;
	uint32_t bidx;

	for (bidx = 0; bidx < nr_blocks; bidx++) {
		if (ext4_get_block(dir, bidx, &block) != 0)
			return NULL;
		bh = sb_bread(dir->i_sb, block);
		if (!bh)
			return NULL;

		de = (struct ext4_dir_entry_2 *)bh->b_data;
		while ((char *)de < (char *)bh->b_data + block_size) {
			if (de->inode != 0) {
				if (strlen(dentry->d_name) == de->name_len &&
				    strncmp(dentry->d_name, de->name, de->name_len) == 0) {
					struct inode *inode;

					inode = ext4_iget(dir->i_sb, le32_to_cpu(de->inode));
					brelse(bh);
					if (!inode)
						return NULL;
					d_instantiate(dentry, inode);
					return dentry;
				}
			}
			de = ext4_next_entry(de);
		}
		brelse(bh);
	}

	return NULL;
}

int ext4_dir_is_empty(struct inode *inode)
{
	struct buffer_head *bh;
	struct ext4_dir_entry_2 *de;
	uint32_t block;
	int count = 0;

	if (ext4_get_block(inode, 0, &block) != 0)
		return -EIO;

	bh = sb_bread(inode->i_sb, block);
	if (!bh)
		return -EIO;

	de = (struct ext4_dir_entry_2 *)bh->b_data;
	while ((char *)de < (char *)bh->b_data + inode->i_sb->s_blocksize) {
		if (de->inode != 0)
			count++;
		de = ext4_next_entry(de);
	}
	brelse(bh);
	return count <= 2 ? 1 : 0;
}

int ext4_add_entry(struct inode *dir, const char *name, int len,
		   uint32_t ino, uint8_t file_type)
{
	struct buffer_head *bh;
	struct ext4_dir_entry_2 *de;
	struct ext4_dir_entry_2 *prev;
	uint32_t block;
	uint32_t block_size = dir->i_sb->s_blocksize;
	uint16_t rec_len;
	uint16_t needed;
	uint32_t bidx = 0;

	needed = sizeof(struct ext4_dir_entry_2) + ((len + 3) & ~3);

	while (bidx * block_size < dir->i_size) {
		if (ext4_get_block(dir, bidx, &block) != 0)
			return -EIO;

		bh = sb_bread(dir->i_sb, block);
		if (!bh)
			return -EIO;

		de = (struct ext4_dir_entry_2 *)bh->b_data;
		prev = NULL;
		while ((char *)de < (char *)bh->b_data + block_size) {
			rec_len = le16_to_cpu(de->rec_len);
			if (de->inode == 0 && prev == NULL) {
				/* First unused entry at start of block. */
				if (rec_len >= needed) {
					goto insert;
				}
			} else if (de->inode == 0) {
				/* Unreachable in normal layout, but handle. */
			} else {
				uint16_t actual = sizeof(struct ext4_dir_entry_2) +
						((de->name_len + 3) & ~3);
				uint16_t free_space = rec_len - actual;
				if (free_space >= needed) {
					struct ext4_dir_entry_2 *new_de;

					new_de = (struct ext4_dir_entry_2 *)((char *)de + actual);
					new_de->rec_len = cpu_to_le16(free_space);
					de->rec_len = cpu_to_le16(actual);
					de = new_de;
					goto insert;
				}
			}
			prev = de;
			de = ext4_next_entry(de);
		}
		brelse(bh);
		bidx++;
	}

	/* Need to allocate a new directory block. */
	{
		uint32_t pblock;
		if (ext4_new_block(dir, &pblock) != 0)
			return -ENOSPC;
		if (ext4_set_block(dir, bidx, pblock) != 0)
			return -ENOSPC;
		bh = sb_bread(dir->i_sb, pblock);
		if (!bh)
			return -EIO;
		memset(bh->b_data, 0, block_size);
		de = (struct ext4_dir_entry_2 *)bh->b_data;
		de->rec_len = cpu_to_le16(block_size);
		if (dir->i_size == 0)
			dir->i_size = block_size;
		else
			dir->i_size += block_size;
		mark_inode_dirty(dir);
	}

insert:
	de->inode = cpu_to_le32(ino);
	de->name_len = len;
	de->file_type = file_type;
	memcpy(de->name, name, len);
	mark_buffer_dirty(bh);
	brelse(bh);
	return 0;
}

int ext4_delete_entry(struct inode *dir, const char *name, int len)
{
	struct buffer_head *bh;
	struct ext4_dir_entry_2 *de;
	struct ext4_dir_entry_2 *prev;
	uint32_t block;
	uint32_t block_size = dir->i_sb->s_blocksize;
	uint32_t bidx = 0;

	while (bidx * block_size < dir->i_size) {
		if (ext4_get_block(dir, bidx, &block) != 0)
			return -EIO;

		bh = sb_bread(dir->i_sb, block);
		if (!bh)
			return -EIO;

		de = (struct ext4_dir_entry_2 *)bh->b_data;
		prev = NULL;
		while ((char *)de < (char *)bh->b_data + block_size) {
			if (de->inode != 0 &&
			    de->name_len == len &&
			    strncmp(de->name, name, len) == 0) {
				if (prev) {
					prev->rec_len = cpu_to_le16(
						le16_to_cpu(prev->rec_len) +
						le16_to_cpu(de->rec_len));
				} else {
					de->inode = 0;
				}
				mark_buffer_dirty(bh);
				brelse(bh);
				return 0;
			}
			prev = de;
			de = ext4_next_entry(de);
		}
		brelse(bh);
		bidx++;
	}
	return -ENOENT;
}

int ext4_make_empty(struct inode *inode, struct inode *parent)
{
	struct buffer_head *bh;
	struct ext4_dir_entry_2 *de;
	uint32_t block;
	uint32_t block_size = inode->i_sb->s_blocksize;
	uint32_t rec_len;

	if (ext4_new_block(inode, &block) != 0)
		return -ENOSPC;
	if (ext4_set_block(inode, 0, block) != 0)
		return -ENOSPC;

	bh = sb_bread(inode->i_sb, block);
	if (!bh)
		return -EIO;
	memset(bh->b_data, 0, block_size);

	de = (struct ext4_dir_entry_2 *)bh->b_data;
	de->inode = cpu_to_le32(inode->i_ino);
	de->name_len = 1;
	de->file_type = EXT4_FT_DIR;
	de->name[0] = '.';
	rec_len = sizeof(struct ext4_dir_entry_2) + 3;
	rec_len = (rec_len + 3) & ~3;
	de->rec_len = cpu_to_le16(rec_len);

	de = (struct ext4_dir_entry_2 *)((char *)de + rec_len);
	de->inode = cpu_to_le32(parent->i_ino);
	de->name_len = 2;
	de->file_type = EXT4_FT_DIR;
	de->name[0] = '.';
	de->name[1] = '.';
	de->rec_len = cpu_to_le16(block_size - rec_len);

	inode->i_size = block_size;
	mark_buffer_dirty(bh);
	brelse(bh);
	mark_inode_dirty(inode);
	return 0;
}

struct file_operations ext4_dir_operations = { };
