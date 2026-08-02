#include "buffer.h"
#include "super.h"
#include "mm.h"
#include "string.h"
#include "page.h"
#include "buddy.h"
#include "bitops.h"

static struct rb_tree g_bcache_tree;
static struct dlist_node g_bcache_lru;

static int bcache_cmp(struct rb_node *n1, struct rb_node *n2)
{
	struct buffer_head *bh1 = container_of(n1, struct buffer_head, b_rbnode);
	struct buffer_head *bh2 = container_of(n2, struct buffer_head, b_rbnode);
	uintptr_t bdev1 = (uintptr_t)bh1->b_bdev;
	uintptr_t bdev2 = (uintptr_t)bh2->b_bdev;

	if (bdev1 != bdev2)
		return (bdev1 < bdev2) ? -1 : 1;
	if (bh1->b_blocknr == bh2->b_blocknr)
		return 0;
	return (bh1->b_blocknr < bh2->b_blocknr) ? -1 : 1;
}

void buffer_cache_init(void)
{
	g_bcache_tree.comp = bcache_cmp;
	g_bcache_tree.root = NULL;
	dlist_init(&g_bcache_lru);
}

static struct buffer_head *bcache_find(struct block_device *bdev,
				       uint64_t block)
{
	struct buffer_head key_bh;
	struct rb_node *node;

	memset(&key_bh, 0, sizeof(key_bh));
	key_bh.b_bdev = bdev;
	key_bh.b_blocknr = block;

	node = g_bcache_tree.root;
	while (node) {
		struct buffer_head *bh = container_of(node, struct buffer_head, b_rbnode);
		int cmp = bcache_cmp(&key_bh.b_rbnode, node);
		if (cmp == 0)
			return bh;
		node = (cmp < 0) ? node->left : node->right;
	}
	return NULL;
}

static struct buffer_head *alloc_buffer(struct block_device *bdev,
					uint64_t block, uint32_t size)
{
	struct buffer_head *bh;
	struct page *page;

	bh = (struct buffer_head *)kmalloc(sizeof(*bh));
	if (!bh)
		return NULL;

	page = buddy_alloc_pages(size);
	if (!page) {
		kfree(bh);
		return NULL;
	}

	memset(bh, 0, sizeof(*bh));
	bh->b_blocknr = block;
	bh->b_size = size;
	bh->b_ref_count = 1;
	bh->b_data = page_to_virt(page);
	bh->b_bdev = bdev;
	bh->b_dirty = 0;
	dlist_init(&bh->b_lru);

	return bh;
}

static void free_buffer(struct buffer_head *bh)
{
	struct page *page;

	if (!bh)
		return;

	if (bh->b_data) {
		page = virt_to_page(bh->b_data);
		buddy_free_pages(page);
	}
	kfree(bh);
}

struct buffer_head *bread(struct block_device *bdev, uint64_t block)
{
	struct buffer_head *bh;
	int ret;

	bh = bcache_find(bdev, block);
	if (bh) {
		bh->b_ref_count++;
		return bh;
	}

	bh = alloc_buffer(bdev, block, bdev->bd_block_size);
	if (!bh)
		return NULL;

	ret = bdev_read_blocks(bdev, block, 1, bh->b_data);
	if (ret < 0) {
		free_buffer(bh);
		return NULL;
	}

	rb_insert(&g_bcache_tree, &bh->b_rbnode);
	dlist_add_tail(&g_bcache_lru, &bh->b_lru);
	return bh;
}

struct buffer_head *sb_bread(struct super_block *sb, uint64_t block)
{
	if (!sb || !sb->s_bdev)
		return NULL;
	return bread(sb->s_bdev, block);
}

void brelse(struct buffer_head *bh)
{
	if (!bh)
		return;

	bh->b_ref_count--;
	if (bh->b_ref_count < 0)
		bh->b_ref_count = 0;
}

void mark_buffer_dirty(struct buffer_head *bh)
{
	if (bh)
		bh->b_dirty = 1;
}

int sync_dirty_buffers(struct block_device *bdev)
{
	struct dlist_node *node;
	struct buffer_head *bh;
	int ret;
	int err = 0;

	node = g_bcache_lru.next;
	while (node != &g_bcache_lru) {
		struct dlist_node *next = node->next;
		bh = container_of(node, struct buffer_head, b_lru);

		if (bdev && bh->b_bdev != bdev)
			goto next;

		if (bh->b_dirty) {
			ret = bdev_write_blocks(bh->b_bdev,
						bh->b_blocknr, 1,
						bh->b_data);
			if (ret < 0)
				err = ret;
			else
				bh->b_dirty = 0;
		}
next:
		node = next;
	}
	return err;
}
