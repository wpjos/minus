#ifndef __FSBLOCK_BUFFER_H__
#define __FSBLOCK_BUFFER_H__

/*
 * Buffer cache - bridge between filesystems and block devices.
 *
 * Architecture:
 *   Filesystems (ext4, ...)
 *       |  bread(), sb_bread(), brelse(), mark_buffer_dirty()
 *       v
 *   Buffer Cache (here)  -- caches block I/O, tracks dirty state
 *       |
 *       v  bdev_read_blocks(), bdev_write_blocks()
 *   Block Devices (kernel/driver/block/)
 */

#include "types.h"
#include "dlist.h"
#include "rbtree.h"
#include "blkdev.h"

struct super_block;

struct buffer_head {
	uint64_t		b_blocknr;
	uint32_t		b_size;
	int			b_ref_count;
	int			b_dirty;
	void			*b_data;
	struct dlist_node	b_lru;
	struct block_device	*b_bdev;
	struct rb_node		b_rbnode;
};

struct buffer_head *bread(struct block_device *bdev, uint64_t block);
struct buffer_head *sb_bread(struct super_block *sb, uint64_t block);
void brelse(struct buffer_head *bh);
void mark_buffer_dirty(struct buffer_head *bh);
int sync_dirty_buffers(struct block_device *bdev);

void buffer_cache_init(void);

#endif /* __FSBLOCK_BUFFER_H__ */
