#include "ext4.h"
#include "string.h"
#include "mm.h"
#include "slab.h"
#include "module.h"
#include "errno.h"
#include "bitops.h"

struct kmem_cache *g_ext4_inode_cache;

static int ext4_check_superblock(struct ext4_super_block *es)
{
	if (le16_to_cpu(es->s_magic) != EXT4_SUPER_MAGIC) {
		return -1;
	}

	if (le32_to_cpu(es->s_feature_incompat) & EXT4_FEATURE_INCOMPAT_64BIT) {
		return -1;
	}

	if (!(le32_to_cpu(es->s_feature_incompat) &
	      EXT4_FEATURE_INCOMPAT_EXTENTS)) {
		return -1;
	}

	return 0;
}

static struct ext4_sb_info *ext4_alloc_sbi(struct ext4_super_block *es)
{
	struct ext4_sb_info *sbi;
	uint32_t groups_count;
	uint32_t desc_per_block;
	uint32_t block_size;

	sbi = (struct ext4_sb_info *)kmalloc(sizeof(*sbi));
	if (!sbi)
		return NULL;
	memset(sbi, 0, sizeof(*sbi));

	sbi->s_es = es;
	sbi->s_inodes_per_group = le32_to_cpu(es->s_inodes_per_group);
	sbi->s_blocks_per_group = le32_to_cpu(es->s_blocks_per_group);
	sbi->s_first_data_block = le32_to_cpu(es->s_first_data_block);
	sbi->s_log_block_size = le32_to_cpu(es->s_log_block_size);
	sbi->s_inode_size = le16_to_cpu(es->s_inode_size);
	sbi->s_first_ino = le32_to_cpu(es->s_first_ino);

	block_size = 1024 << sbi->s_log_block_size;
	desc_per_block = block_size / sizeof(struct ext4_group_desc);
	sbi->s_desc_per_block = desc_per_block ? desc_per_block : 1;

	groups_count = (le32_to_cpu(es->s_blocks_count_lo) -
			sbi->s_first_data_block +
			sbi->s_blocks_per_group - 1) /
		       sbi->s_blocks_per_group;
	sbi->s_groups_count = groups_count;

	return sbi;
}

static int ext4_fill_super(struct super_block *sb)
{
	struct ext4_sb_info *sbi;
	struct buffer_head *bh;
	struct ext4_super_block *es;
	struct inode *root_inode;
	struct dentry *root_dentry;
	uint32_t block_size;
	uint32_t gd_blocks;
	uint32_t superblock_block;
	uint32_t superblock_off;
	int ret;
	uint32_t i;
	char super_buf[EXT4_SUPERBLOCK_SIZE];

	/*
	 * Read the superblock directly from the block device using its native
	 * block size. The superblock lives at byte 1024; for a 512-byte sector
	 * device we need sectors 2 and 3.
	 */
	ret = bdev_read_blocks(sb->s_bdev,
			       EXT4_SUPERBLOCK_OFFSET / sb->s_bdev->bd_block_size,
			       EXT4_SUPERBLOCK_SIZE / sb->s_bdev->bd_block_size,
			       super_buf);
	if (ret < 0)
		return -1;

	es = (struct ext4_super_block *)(super_buf + (EXT4_SUPERBLOCK_OFFSET % sb->s_bdev->bd_block_size));
	block_size = 1024 << le32_to_cpu(es->s_log_block_size);

	superblock_block = EXT4_SUPERBLOCK_OFFSET / block_size;
	superblock_off = EXT4_SUPERBLOCK_OFFSET % block_size;

	/* Now set the filesystem block size and re-read via the buffer cache. */
	sb->s_bdev->bd_block_size = block_size;
	sb->s_bdev->bd_block_size_bits = 10 + le32_to_cpu(es->s_log_block_size);
	sb->s_blocksize = block_size;
	sb->s_blocksize_bits = 10 + le32_to_cpu(es->s_log_block_size);

	bh = sb_bread(sb, superblock_block);
	if (!bh)
		return -1;

	es = (struct ext4_super_block *)(bh->b_data + superblock_off);
	ret = ext4_check_superblock(es);
	if (ret < 0)
		return ret;

	sbi = ext4_alloc_sbi(es);
	if (!sbi)
		return -1;

	sb->s_blocksize = block_size;
	sb->s_blocksize_bits = 10 + sbi->s_log_block_size;
	sb->s_fs_info = sbi;

	gd_blocks = (sbi->s_groups_count + sbi->s_desc_per_block - 1) /
		    sbi->s_desc_per_block;
	sbi->s_group_desc = (struct buffer_head **)kmalloc(
		gd_blocks * sizeof(struct buffer_head *));
	if (!sbi->s_group_desc)
		return -1;

	for (i = 0; i < gd_blocks; i++) {
		uint32_t blk = sbi->s_first_data_block + 1 + i;
		sbi->s_group_desc[i] = sb_bread(sb, blk);
		if (!sbi->s_group_desc[i])
			return -1;
	}

	g_ext4_inode_cache = kmem_cache_create("ext4_inode_info",
					       sizeof(struct ext4_inode_info), 8);

	root_inode = ext4_iget(sb, EXT4_ROOT_INO);
	if (!root_inode)
		return -1;

	root_dentry = d_alloc(NULL, "/");
	if (!root_dentry)
		return -1;
	d_instantiate(root_dentry, root_inode);
	sb->s_root = root_dentry;

	return 0;
}

struct inode *ext4_alloc_inode_stub(struct super_block *sb)
{
	struct inode *inode;
	struct ext4_inode_info *ei;

	(void)sb;
	inode = (struct inode *)kmalloc(sizeof(*inode));
	if (!inode)
		return NULL;
	memset(inode, 0, sizeof(*inode));

	ei = (struct ext4_inode_info *)kmem_cache_alloc(g_ext4_inode_cache);
	if (!ei) {
		kfree(inode);
		return NULL;
	}
	memset(ei, 0, sizeof(*ei));
	inode->i_private = ei;
	return inode;
}

static void ext4_destroy_inode(struct inode *inode)
{
	struct ext4_inode_info *ei = inode->i_private;

	if (ei)
		kmem_cache_free(g_ext4_inode_cache, ei);
	kfree(inode);
}

static struct super_operations ext4_sops = {
	.alloc_inode = ext4_alloc_inode_stub,
	.destroy_inode = ext4_destroy_inode,
	.write_inode = ext4_write_inode,
};

static struct super_block *ext4_mount(struct file_system_type *fs,
				      const char *dev_name, const char *data)
{
	struct super_block *sb;
	struct block_device *bdev;

	(void)data;

	bdev = bdev_get_by_name(dev_name);
	if (!bdev)
		return NULL;

	sb = sb_alloc(fs);
	if (!sb) {
		bdev_put(bdev);
		return NULL;
	}
	sb->s_bdev = bdev;
	sb->s_dev = bdev->bd_dev;
	sb->s_op = &ext4_sops;

	if (ext4_fill_super(sb) < 0) {
		bdev_put(bdev);
		kfree(sb);
		return NULL;
	}

	sb_add(sb);
	return sb;
}

static void ext4_kill_sb(struct super_block *sb)
{
	if (sb->s_bdev)
		bdev_put(sb->s_bdev);
}

static struct file_system_type ext4_fs_type = {
	.name = "ext4",
	.mount = ext4_mount,
	.kill_sb = ext4_kill_sb,
};

static void ext4_init_once(void)
{
	register_filesystem(&ext4_fs_type);
}
module_register(ext4, MODULE_LEVEL_LOW, ext4_init_once);
