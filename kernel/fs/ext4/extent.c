#include "ext4.h"
#include "string.h"
#include "endian.h"
#include "errno.h"
#include "buffer.h"
#include "bitops.h"

int ext4_get_block(struct inode *inode, uint32_t iblock, uint64_t *pblock)
{
	struct ext4_inode_info *ei = inode->i_private;
	struct ext4_extent_header *eh;
	struct ext4_extent *ex;
	uint32_t i;
	uint32_t n;

	if (!ei)
		return -EINVAL;

	eh = (struct ext4_extent_header *)ei->i_data;
	if (le16_to_cpu(eh->eh_magic) != EXT4_EXTENT_HEADER_MAGIC) {
		*pblock = 0;
		return -EIO;
	}

	n = le16_to_cpu(eh->eh_entries);
	ex = (struct ext4_extent *)((uint8_t *)ei->i_data +
				    sizeof(struct ext4_extent_header));

	for (i = 0; i < n; i++) {
		uint32_t start = le32_to_cpu(ex[i].ee_block);
		uint32_t len = le16_to_cpu(ex[i].ee_len);
		uint64_t phys = ((uint64_t)le16_to_cpu(ex[i].ee_start_hi) << 32) |
			      le32_to_cpu(ex[i].ee_start_lo);

		if (iblock >= start && iblock < start + len) {
			*pblock = phys + (iblock - start);
			return 0;
		}
	}

	*pblock = 0;
	return -ENOSPC;
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

static int ext4_find_free_block(struct super_block *sb, uint64_t *pblock)
{
	struct ext4_sb_info *sbi = sb->s_fs_info;
	uint32_t group;

	for (group = 0; group < sbi->s_groups_count; group++) {
		struct buffer_head *bh;
		unsigned long *bitmap;
		uint32_t idx;

		bh = ext4_read_bitmap(sb, group, 0);
		if (!bh)
			continue;

		bitmap = (unsigned long *)bh->b_data;
		idx = find_first_zero_bit(bitmap, sbi->s_blocks_per_group);
		if (idx < sbi->s_blocks_per_group) {
			set_bit(idx, bitmap);
			mark_buffer_dirty(bh);
			brelse(bh);
			*pblock = (uint64_t)group * sbi->s_blocks_per_group +
				 idx + sbi->s_first_data_block;
			return 0;
		}
		brelse(bh);
	}
	return -ENOSPC;
}

int ext4_new_block(struct inode *inode, uint64_t *pblock)
{
	return ext4_find_free_block(inode->i_sb, pblock);
}

int ext4_set_block(struct inode *inode, uint32_t iblock, uint64_t pblock)
{
	struct ext4_inode_info *ei = inode->i_private;
	struct ext4_extent_header *eh;
	struct ext4_extent *ex;
	uint32_t n;

	if (!ei)
		return -EINVAL;

	eh = (struct ext4_extent_header *)ei->i_data;
	if (le16_to_cpu(eh->eh_magic) != EXT4_EXTENT_HEADER_MAGIC) {
		eh->eh_magic = cpu_to_le16(EXT4_EXTENT_HEADER_MAGIC);
		eh->eh_entries = 0;
		eh->eh_max = cpu_to_le16((sizeof(ei->i_data) -
				    sizeof(struct ext4_extent_header)) /
				   sizeof(struct ext4_extent));
		eh->eh_depth = 0;
	}

	n = le16_to_cpu(eh->eh_entries);
	ex = (struct ext4_extent *)((uint8_t *)ei->i_data +
				    sizeof(struct ext4_extent_header));

	/* Try to append to the last extent if contiguous. */
	if (n > 0) {
		struct ext4_extent *last = &ex[n - 1];
		uint32_t last_start = le32_to_cpu(last->ee_block);
		uint32_t last_len = le16_to_cpu(last->ee_len);
		uint64_t last_phys = ((uint64_t)le16_to_cpu(last->ee_start_hi) << 32) |
				     le32_to_cpu(last->ee_start_lo);

		if (iblock == last_start + last_len &&
		    pblock == last_phys + last_len &&
		    last_len < 32768) {
			last->ee_len = cpu_to_le16((uint16_t)(last_len + 1));
			goto out;
		}
	}

	if (n >= le16_to_cpu(eh->eh_max))
		return -ENOSPC;

	ex[n].ee_block = cpu_to_le32(iblock);
	ex[n].ee_len = cpu_to_le16(1);
	ex[n].ee_start_lo = cpu_to_le32(pblock & 0xffffffff);
	ex[n].ee_start_hi = cpu_to_le16((uint16_t)(pblock >> 16));
	eh->eh_entries = cpu_to_le16(n + 1);

out:
	inode->i_blocks += (inode->i_sb->s_blocksize / 512);
	mark_inode_dirty(inode);
	return 0;
}

int ext4_truncate_blocks(struct inode *inode)
{
	struct ext4_inode_info *ei = inode->i_private;

	if (!ei)
		return -EINVAL;

	memset(ei->i_data, 0, sizeof(ei->i_data));
	inode->i_size = 0;
	inode->i_blocks = 0;
	mark_inode_dirty(inode);
	return 0;
}
