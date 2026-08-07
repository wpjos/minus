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
	ssize_t ret = 0;
	ssize_t total = 0;
	loff_t offset = *pos;

	if (offset >= inode->i_size)
		return 0;

	if (offset + len > inode->i_size)
		len = (size_t)(inode->i_size - offset);

	while (len > 0) {
		uint32_t iblock = (uint32_t)(offset / block_size);
		uint64_t pblock;
		uint32_t off_in_block = (uint32_t)(offset % block_size);
		uint32_t to_copy = block_size - off_in_block;
		if (to_copy > len) {
			to_copy = (uint32_t)len;
		}
		ret = ext4_get_block(inode, iblock, &pblock);
		if (ret == -ENOSPC) {
			memset(buf, 0, to_copy);
			goto cont;
		}
		if (ret != 0) {
			ret = -EIO;
			break;
		}
		struct buffer_head *bh = sb_bread(sb, pblock);
		if (!bh) {
			ret = -EIO;
			break;
		}
		memcpy(buf, (char *)bh->b_data + off_in_block, to_copy);
		brelse(bh);
cont:
		buf += to_copy;
		offset += to_copy;
		len -= to_copy;
		total += to_copy;
	}

	if (total > 0)
		*pos = offset;

	return total ? total : ret;
}

static ssize_t ext4_file_write(struct file *filp, const char *buf, size_t len,
			       loff_t *pos)
{
	struct inode *inode = filp->f_dentry->d_inode;
	struct super_block *sb = inode->i_sb;
	uint32_t block_size = sb->s_blocksize;
	ssize_t ret = 0;
	ssize_t total = 0;
	loff_t offset = *pos;

	while (len > 0) {
		uint32_t iblock = (uint32_t)(offset / block_size);
		uint64_t pblock;
		uint32_t off_in_block = (uint32_t)(offset % block_size);
		uint32_t to_copy;
		struct buffer_head *bh;

		if (ext4_get_block(inode, iblock, &pblock) != 0) {
			if (ext4_new_block(inode, &pblock) != 0) {
				ret = -ENOSPC;
				break;
			}
			if (ext4_set_block(inode, iblock, pblock) != 0) {
				ret = -ENOSPC;
				break;
			}
		}

		bh = sb_bread(sb, pblock);
		if (!bh) {
			ret = -EIO;
			break;
		}

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

	if (total > 0) {
		*pos = offset;
		if (offset > inode->i_size) {
			inode->i_size = offset;
			mark_inode_dirty(inode);
		}
	}

	return total ? total : ret;
}

struct inode_operations ext4_file_inode_operations = { };

struct file_operations ext4_file_operations = {
	.read = ext4_file_read,
	.write = ext4_file_write,
};
