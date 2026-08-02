#ifndef __EXT4_H__
#define __EXT4_H__

#include "types.h"
#include "endian.h"
#include "super.h"
#include "fs_type.h"
#include "mount.h"
#include "inode.h"
#include "dentry.h"
#include "file.h"
#include "buffer.h"

/* Inode numbers */
#define EXT4_ROOT_INO		2

/* Magic */
#define EXT4_SUPER_MAGIC	0xEF53

/* Superblock offset */
#define EXT4_SUPERBLOCK_OFFSET	1024
#define EXT4_SUPERBLOCK_SIZE	1024

/* Inode size default */
#define EXT4_INODE_SIZE		128

/* ext4_inode.i_mode values */
#define EXT4_S_IFIFO		0x1000
#define EXT4_S_IFCHR		0x2000
#define EXT4_S_IFDIR		0x4000
#define EXT4_S_IFBLK		0x6000
#define EXT4_S_IFREG		0x8000
#define EXT4_S_IFLNK		0xA000
#define EXT4_S_IFSOCK		0xC000
#define EXT4_S_IFMT		0xF000

/* Feature incompat bits we care about */
#define EXT4_FEATURE_INCOMPAT_FILETYPE	0x0002
#define EXT4_FEATURE_INCOMPAT_EXTENTS	0x0040
#define EXT4_FEATURE_INCOMPAT_64BIT	0x0080

/* Feature compat bits we may see */
#define EXT4_FEATURE_COMPAT_HAS_JOURNAL	0x0004

/* Directory entry file types */
#define EXT4_FT_UNKNOWN		0
#define EXT4_FT_REG_FILE	1
#define EXT4_FT_DIR		2
#define EXT4_FT_CHRDEV		3
#define EXT4_FT_BLKDEV		4
#define EXT4_FT_FIFO		5
#define EXT4_FT_SOCK		6
#define EXT4_FT_SYMLINK		7

/* Extent constants */
#define EXT4_EXTENT_HEADER_MAGIC	0xF30A
#define EXT4_EXTENT_TAIL_OFFSET		(sizeof(struct ext4_extent_header))

/* i_block layout when extents are used */
struct ext4_extent_header {
	uint16_t	eh_magic;
	uint16_t	eh_entries;
	uint16_t	eh_max;
	uint16_t	eh_depth;
	uint32_t	eh_generation;
};

struct ext4_extent {
	uint32_t	ee_block;
	uint16_t	ee_len;
	uint16_t	ee_start_hi;
	uint32_t	ee_start_lo;
};

struct ext4_extent_idx {
	uint32_t	ei_block;
	uint32_t	ei_leaf_lo;
	uint16_t	ei_leaf_hi;
	uint16_t	ei_unused;
};

struct ext4_extent_tail {
	uint32_t	et_checksum;
};

/* On-disk superblock (partial) */
struct ext4_super_block {
	uint32_t	s_inodes_count;
	uint32_t	s_blocks_count_lo;
	uint32_t	s_r_blocks_count_lo;
	uint32_t	s_free_blocks_count_lo;
	uint32_t	s_free_inodes_count;
	uint32_t	s_first_data_block;
	uint32_t	s_log_block_size;
	uint32_t	s_log_cluster_size;
	uint32_t	s_blocks_per_group;
	uint32_t	s_clusters_per_group;
	uint32_t	s_inodes_per_group;
	uint32_t	s_mtime;
	uint32_t	s_wtime;
	uint16_t	s_mnt_count;
	uint16_t	s_max_mnt_count;
	uint16_t	s_magic;
	uint16_t	s_state;
	uint16_t	s_errors;
	uint16_t	s_minor_rev_level;
	uint32_t	s_lastcheck;
	uint32_t	s_checkinterval;
	uint32_t	s_creator_os;
	uint32_t	s_rev_level;
	uint16_t	s_def_resuid;
	uint16_t	s_def_resgid;
	uint32_t	s_first_ino;
	uint16_t	s_inode_size;
	uint16_t	s_block_group_nr;
	uint32_t	s_feature_compat;
	uint32_t	s_feature_incompat;
	uint32_t	s_feature_ro_compat;
	uint8_t		s_uuid[16];
	char		s_volume_name[16];
	char		s_last_mounted[64];
	uint32_t	s_algorithm_usage_bitmap;
	uint8_t		s_prealloc_blocks;
	uint8_t		s_prealloc_dir_blocks;
	uint16_t	s_reserved_gdt_blocks;
	uint8_t		s_journal_uuid[16];
	uint32_t	s_journal_inum;
	uint32_t	s_journal_dev;
	uint32_t	s_last_orphan;
	uint32_t	s_hash_seed[4];
	uint8_t		s_def_hash_version;
	uint8_t		s_reserved_char_pad;
	uint16_t	s_desc_size;
	uint32_t	s_default_mount_opts;
	uint32_t	s_first_meta_bg;
	uint32_t	s_mkfs_time;
	uint32_t	s_jnl_blocks[17];
	uint32_t	s_blocks_count_hi;
	uint32_t	s_r_blocks_count_hi;
	uint32_t	s_free_blocks_count_hi;
	uint16_t	s_min_extra_isize;
	uint16_t	s_want_extra_isize;
	uint32_t	s_flags;
	uint16_t	s_raid_stride;
	uint16_t	s_mmp_interval;
	uint64_t	s_mmp_block;
	uint32_t	s_raid_stripe_width;
	uint8_t		s_log_groups_per_flex;
	uint8_t		s_checksum_type;
	uint16_t	s_reserved_pad;
	uint64_t	s_kbytes_written;
	uint32_t	s_snapshot_inum;
	uint32_t	s_snapshot_id;
	uint64_t	s_snapshot_r_blocks_count;
	uint32_t	s_snapshot_list;
	uint32_t	s_error_count;
	uint32_t	s_first_error_time;
	uint32_t	s_first_error_ino;
	uint64_t	s_first_error_block;
	uint8_t		s_first_error_func[32];
	uint32_t	s_first_error_line;
	uint32_t	s_last_error_time;
	uint32_t	s_last_error_ino;
	uint32_t	s_last_error_line;
	uint64_t	s_last_error_block;
	uint8_t		s_last_error_func[32];
	uint8_t		s_mount_opts[64];
	uint32_t	s_usr_quota_inum;
	uint32_t	s_reserved_gdt;
	uint32_t	s_backup_bgs[2];
	uint8_t		s_encrypt_algos[4];
	uint8_t		s_encrypt_pw_salt[16];
	uint32_t	s_lpf_ino;
	uint32_t	s_prj_quota_inum;
	uint32_t	s_checksum_seed;
	uint8_t		s_reserved[98];
	uint32_t	s_checksum;
};

/* Group descriptor without 64bit support */
struct ext4_group_desc {
	uint32_t	bg_block_bitmap_lo;
	uint32_t	bg_inode_bitmap_lo;
	uint32_t	bg_inode_table_lo;
	uint16_t	bg_free_blocks_count_lo;
	uint16_t	bg_free_inodes_count_lo;
	uint16_t	bg_used_dirs_count_lo;
	uint16_t	bg_flags;
	uint32_t	bg_exclude_bitmap_lo;
	uint16_t	bg_block_bitmap_csum_lo;
	uint16_t	bg_inode_bitmap_csum_lo;
	uint16_t	bg_itable_unused_lo;
	uint16_t	bg_checksum;
};

/* On-disk inode */
struct ext4_inode {
	uint16_t	i_mode;
	uint16_t	i_uid;
	uint32_t	i_size_lo;
	uint32_t	i_atime;
	uint32_t	i_ctime;
	uint32_t	i_mtime;
	uint32_t	i_dtime;
	uint16_t	i_gid;
	uint16_t	i_links_count;
	uint32_t	i_blocks_lo;
	uint32_t	i_flags;
	uint32_t	i_osd1;
	uint32_t	i_block[15];
	uint32_t	i_generation;
	uint32_t	i_file_acl_lo;
	uint32_t	i_size_high;
	uint32_t	i_obso_faddr;
	uint16_t	i_osd2[12];
};

/* Directory entry */
struct ext4_dir_entry_2 {
	uint32_t	inode;
	uint16_t	rec_len;
	uint8_t		name_len;
	uint8_t		file_type;
	char		name[];
};

/* In-memory superblock info */
struct ext4_sb_info {
	struct ext4_super_block *s_es;
	struct buffer_head	**s_group_desc;
	uint32_t		s_inodes_per_group;
	uint32_t		s_blocks_per_group;
	uint32_t		s_desc_per_block;
	uint32_t		s_groups_count;
	uint32_t		s_first_data_block;
	uint32_t		s_log_block_size;
	uint32_t		s_inode_size;
	uint32_t		s_first_ino;
};

extern struct kmem_cache *g_ext4_inode_cache;

extern struct inode_operations ext4_dir_inode_operations;
extern struct file_operations ext4_dir_operations;
extern struct inode_operations ext4_file_inode_operations;
extern struct file_operations ext4_file_operations;

/* In-memory inode info */
struct ext4_inode_info {
	struct ext4_inode raw_inode;
	uint32_t i_data[15];
};

/* super.c (internal to super.c) */

/* inode.c */
struct inode *ext4_iget(struct super_block *sb, unsigned long ino);
int ext4_write_inode(struct inode *inode);
int ext4_alloc_inode(struct super_block *sb, struct inode *inode);

/* extent.c */
int ext4_get_block(struct inode *inode, uint32_t iblock, uint32_t *pblock);
int ext4_new_block(struct inode *inode, uint32_t *pblock);
int ext4_set_block(struct inode *inode, uint32_t iblock, uint32_t pblock);
int ext4_truncate_blocks(struct inode *inode);

/* dir.c */
struct dentry *ext4_lookup(struct inode *dir, struct dentry *dentry);
int ext4_add_entry(struct inode *dir, const char *name, int len,
		   uint32_t ino, uint8_t file_type);
int ext4_delete_entry(struct inode *dir, const char *name, int len);
int ext4_make_empty(struct inode *inode, struct inode *parent);
int ext4_dir_is_empty(struct inode *inode);

/* file.c (operations tables only; read/write are static) */

/* namei.c */
int ext4_create(struct inode *dir, struct dentry *dentry, uint16_t mode);
int ext4_mkdir(struct inode *dir, struct dentry *dentry, uint16_t mode);
int ext4_unlink(struct inode *dir, struct dentry *dentry);
int ext4_rmdir(struct inode *dir, struct dentry *dentry);

#endif /* __EXT4_H__ */
