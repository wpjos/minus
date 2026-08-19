#ifndef __FS_BCACHE_H__
#define __FS_BCACHE_H__

/*
 * Buffer cache - shared block cache for block-based filesystem backends.
 *
 * Architecture:
 *   Filesystem backends (backend/ext4fs/)
 *       |  sb_bread() convenience (inline in sbread.h)
 *       |  bread(), brelse(), mark_buffer_dirty()
 *       v
 *   Buffer Cache (here)  -- caches block I/O, tracks dirty state
 *       |
 *       v  bdev_read_blocks(), bdev_write_blocks()
 *   Block Devices (kernel/driver/block/)
 *
 * This layer knows nothing about VFS types (super_block, inode, ...);
 * it operates purely on (struct block_device *, blocknr) pairs, so the
 * dependency direction is one-way: backend -> bcache -> drivers.
 * Self-initializing: registered as a SUBSYS_LEVEL_BLOCK init routine,
 * so filesystem backends can rely on it being ready at their own init.
 *
 * Note: a page cache (file-data caching tied to address spaces and mmap)
 * is a memory-management concern and would live under mm/, not here.
 */

#include "types.h"
#include "dlist.h"
#include "rbtree.h"
#include "blkdev.h"

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
void brelse(struct buffer_head *bh);
void mark_buffer_dirty(struct buffer_head *bh);
int sync_dirty_buffers(struct block_device *bdev);

#endif /* __FS_BCACHE_H__ */
