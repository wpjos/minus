#include "ext4.h"
#include "string.h"
#include "mm.h"
#include "slab.h"
#include "stat.h"
#include "errno.h"
#include "buffer.h"
#include "bitops.h"

static uint32_t ext4_inode_block(struct ext4_sb_info *sbi, uint32_t group,
				 uint32_t offset_in_group,
				 uint32_t *off_in_block)
{
	struct ext4_group_desc *gdp;
	uint32_t inode_table;
	uint32_t inode_size;
	uint32_t block_size;
	uint32_t raw_off;

	gdp = (struct ext4_group_desc *)sbi->s_group_desc[group]->b_data;
	inode_table = le32_to_cpu(gdp->bg_inode_table_lo);
	inode_size = sbi->s_inode_size;
	block_size = 1024 << sbi->s_log_block_size;

	raw_off = offset_in_group * inode_size;
	*off_in_block = raw_off % block_size;
	return inode_table + raw_off / block_size;
}

static void ext4_raw_inode_to_inode(struct inode *inode,
				    struct ext4_inode *raw)
{
	struct ext4_inode_info *ei = inode->i_private;

	inode->i_mode = le16_to_cpu(raw->i_mode);
	inode->i_nlink = le16_to_cpu(raw->i_links_count);
	inode->i_uid = le16_to_cpu(raw->i_uid);
	inode->i_gid = le16_to_cpu(raw->i_gid);
	inode->i_size = ((uint64_t)le32_to_cpu(raw->i_size_high) << 32) |
			le32_to_cpu(raw->i_size_lo);
	inode->i_blocks = le32_to_cpu(raw->i_blocks_lo);

	memcpy(&ei->raw_inode, raw, sizeof(*raw));
	memcpy(ei->i_data, raw->i_block, sizeof(raw->i_block));

	if (S_ISDIR(inode->i_mode) || S_ISLNK(inode->i_mode)) {
		inode->i_op = &ext4_dir_inode_operations;
		inode->i_fop = &ext4_dir_operations;
	} else {
		inode->i_op = &ext4_file_inode_operations;
		inode->i_fop = &ext4_file_operations;
	}
}

static void ext4_inode_to_raw_inode(struct inode *inode,
				    struct ext4_inode *raw)
{
	struct ext4_inode_info *ei = inode->i_private;

	memset(raw, 0, sizeof(*raw));
	raw->i_mode = cpu_to_le16(inode->i_mode);
	raw->i_uid = cpu_to_le16((uint16_t)inode->i_uid);
	raw->i_gid = cpu_to_le16((uint16_t)inode->i_gid);
	raw->i_size_lo = cpu_to_le32((uint32_t)(inode->i_size & 0xffffffff));
	raw->i_size_high = cpu_to_le32((uint32_t)(inode->i_size >> 32));
	raw->i_links_count = cpu_to_le16((uint16_t)inode->i_nlink);
	raw->i_blocks_lo = cpu_to_le32((uint32_t)inode->i_blocks);
	memcpy(raw->i_block, ei->i_data, sizeof(raw->i_block));
}

struct inode *ext4_iget(struct super_block *sb, unsigned long ino)
{
	struct ext4_sb_info *sbi = sb->s_fs_info;
	struct inode *inode;
	struct ext4_inode_info *ei;
	struct buffer_head *bh;
	uint32_t group;
	uint32_t offset_in_group;
	uint32_t block;
	uint32_t off;
	struct ext4_inode *raw;

	inode = iget(sb, ino);
	if (!inode)
		return NULL;

	/* If already populated, just return. */
	if (inode->i_mode != 0)
		return inode;

	ei = inode->i_private;
	if (!ei) {
		ei = (struct ext4_inode_info *)kmem_cache_alloc(g_ext4_inode_cache);
		if (!ei) {
			iput(inode);
			return NULL;
		}
		memset(ei, 0, sizeof(*ei));
		inode->i_private = ei;
	}

	group = (ino - 1) / sbi->s_inodes_per_group;
	offset_in_group = (ino - 1) % sbi->s_inodes_per_group;

	block = ext4_inode_block(sbi, group, offset_in_group, &off);
	bh = sb_bread(sb, block);
	if (!bh) {
		iput(inode);
		return NULL;
	}

	raw = (struct ext4_inode *)(bh->b_data + off);
	ext4_raw_inode_to_inode(inode, raw);
	brelse(bh);

	return inode;
}

int ext4_write_inode(struct inode *inode)
{
	struct super_block *sb = inode->i_sb;
	struct ext4_sb_info *sbi = sb->s_fs_info;
	struct buffer_head *bh;
	uint32_t group;
	uint32_t offset_in_group;
	uint32_t block;
	uint32_t off;
	struct ext4_inode *raw;

	group = (inode->i_ino - 1) / sbi->s_inodes_per_group;
	offset_in_group = (inode->i_ino - 1) % sbi->s_inodes_per_group;

	block = ext4_inode_block(sbi, group, offset_in_group, &off);
	bh = sb_bread(sb, block);
	if (!bh)
		return -EIO;

	raw = (struct ext4_inode *)(bh->b_data + off);
	ext4_inode_to_raw_inode(inode, raw);
	mark_buffer_dirty(bh);
	brelse(bh);
	return 0;
}

static struct buffer_head *ext4_read_bitmap(struct super_block *sb,
					    uint32_t group, int inode_bitmap)
{
	struct ext4_sb_info *sbi = sb->s_fs_info;
	struct ext4_group_desc *gdp;
	uint32_t block;

	gdp = (struct ext4_group_desc *)sbi->s_group_desc[group]->b_data;
	if (inode_bitmap)
		block = le32_to_cpu(gdp->bg_inode_bitmap_lo);
	else
		block = le32_to_cpu(gdp->bg_block_bitmap_lo);

	return sb_bread(sb, block);
}

static int ext4_find_free_inode(struct super_block *sb, uint32_t *ino)
{
	struct ext4_sb_info *sbi = sb->s_fs_info;
	uint32_t group;

	for (group = 0; group < sbi->s_groups_count; group++) {
		struct buffer_head *bh;
		unsigned long *bitmap;
		uint32_t idx;

		bh = ext4_read_bitmap(sb, group, 1);
		if (!bh)
			continue;

		bitmap = (unsigned long *)bh->b_data;
		idx = find_first_zero_bit(bitmap, sbi->s_inodes_per_group);
		if (idx < sbi->s_inodes_per_group) {
			set_bit(idx, bitmap);
			mark_buffer_dirty(bh);
			brelse(bh);
			*ino = group * sbi->s_inodes_per_group + idx + 1;
			return 0;
		}
		brelse(bh);
	}
	return -ENOSPC;
}

int ext4_alloc_inode(struct super_block *sb, struct inode *inode)
{
	struct ext4_inode_info *ei;
	uint32_t ino;
	int ret;

	ret = ext4_find_free_inode(sb, &ino);
	if (ret < 0)
		return ret;

	inode->i_ino = ino;
	ei = (struct ext4_inode_info *)kmem_cache_alloc(g_ext4_inode_cache);
	if (!ei)
		return -ENOMEM;
	memset(ei, 0, sizeof(*ei));
	inode->i_private = ei;
	return 0;
}
