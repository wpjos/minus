#include "ext4.h"
#include "string.h"
#include "mm.h"
#include "errno.h"
#include "buffer.h"

static ssize_t ext4_file_read(struct file *filp, char *buf, size_t len, loff_t *pos)
{
	struct inode *inode = filp->f_dentry->d_inode;
	struct super_block *sb = inode->i_sb;
	uint32_t block_size = sb->s_blocksize;
	ssize_t total = 0;
	loff_t offset = *pos;

	if (offset >= inode->i_size)
		return 0;

	if (offset + len > inode->i_size)
		len = (size_t)(inode->i_size - offset);

	while (len > 0) {
		uint32_t iblock = (uint32_t)(offset / block_size);
		uint32_t pblock;
		uint32_t off_in_block = (uint32_t)(offset % block_size);
		uint32_t to_copy;
		struct buffer_head *bh;

		if (ext4_get_block(inode, iblock, &pblock) != 0)
			return total ? total : -EIO;

		bh = sb_bread(sb, pblock);
		if (!bh)
			return total ? total : -EIO;

		to_copy = block_size - off_in_block;
		if (to_copy > len)
			to_copy = (uint32_t)len;

		memcpy(buf, (char *)bh->b_data + off_in_block, to_copy);
		brelse(bh);

		buf += to_copy;
		offset += to_copy;
		len -= to_copy;
		total += to_copy;
	}

	*pos = offset;
	return total;
}

static ssize_t ext4_file_write(struct file *filp, const char *buf, size_t len,
			       loff_t *pos)
{
	struct inode *inode = filp->f_dentry->d_inode;
	struct super_block *sb = inode->i_sb;
	uint32_t block_size = sb->s_blocksize;
	ssize_t total = 0;
	loff_t offset = *pos;

	while (len > 0) {
		uint32_t iblock = (uint32_t)(offset / block_size);
		uint32_t pblock;
		uint32_t off_in_block = (uint32_t)(offset % block_size);
		uint32_t to_copy;
		struct buffer_head *bh;

		if (ext4_get_block(inode, iblock, &pblock) != 0) {
			if (ext4_new_block(inode, &pblock) != 0)
				return total ? total : -ENOSPC;
			if (ext4_set_block(inode, iblock, pblock) != 0)
				return total ? total : -ENOSPC;
		}

		bh = sb_bread(sb, pblock);
		if (!bh)
			return total ? total : -EIO;

		to_copy = block_size - off_in_block;
		if (to_copy > len)
			to_copy = (uint32_t)len;

		memcpy((char *)bh->b_data + off_in_block, buf, to_copy);
		mark_buffer_dirty(bh);
		brelse(bh);

		buf += to_copy;
		offset += to_copy;
		len -= to_copy;
		total += to_copy;
	}

	if (offset > inode->i_size) {
		inode->i_size = offset;
		mark_inode_dirty(inode);
	}

	*pos = offset;
	return total;
}

struct inode_operations ext4_file_inode_operations = { };

struct file_operations ext4_file_operations = {
	.read = ext4_file_read,
	.write = ext4_file_write,
};
