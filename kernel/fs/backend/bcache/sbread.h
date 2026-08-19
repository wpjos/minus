#ifndef __FS_BCACHE_SBREAD_H__
#define __FS_BCACHE_SBREAD_H__

/*
 * Bridge between the VFS core (service/) and the buffer cache (bcache/).
 *
 * sb_bread() is the convenience every block-based backend uses to read
 * one filesystem block through a superblock's device.  It lives here -
 * not in bcache.h - because it needs struct super_block from service/
 * and the dependency direction must stay backend -> service, never the
 * other way.  bcache.h itself remains free of VFS types.
 */

#include "bcache.h"
#include "super.h"

static inline struct buffer_head *sb_bread(struct super_block *sb,
					   uint64_t block)
{
	if (!sb || !sb->s_bdev)
		return NULL;
	return bread(sb->s_bdev, block);
}

#endif /* __FS_BCACHE_SBREAD_H__ */
