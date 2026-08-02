#include "inode.h"
#include "super.h"
#include "string.h"
#include "mm.h"
#include "slab.h"

static struct kmem_cache *g_inode_cache;
static struct rb_tree g_inode_tree;
static struct dlist_node g_dirty_inodes;

static int inode_cmp(struct rb_node *n1, struct rb_node *n2)
{
	struct inode *i1 = container_of(n1, struct inode, i_rbnode);
	struct inode *i2 = container_of(n2, struct inode, i_rbnode);

	if (i1->i_sb != i2->i_sb)
		return (i1->i_sb > i2->i_sb) ? 1 : -1;
	if (i1->i_ino == i2->i_ino)
		return 0;
	return (i1->i_ino > i2->i_ino) ? 1 : -1;
}

void inode_cache_init(void)
{
	g_inode_cache = kmem_cache_create("inode", sizeof(struct inode), 8);
	g_inode_tree.comp = inode_cmp;
	g_inode_tree.root = NULL;
	dlist_init(&g_dirty_inodes);
}

static struct inode *inode_find(struct super_block *sb, unsigned long ino)
{
	struct rb_node *node = g_inode_tree.root;
	struct inode key;

	memset(&key, 0, sizeof(key));
	key.i_sb = sb;
	key.i_ino = ino;

	while (node) {
		struct inode *inode = container_of(node, struct inode, i_rbnode);
		int cmp = inode_cmp(&key.i_rbnode, node);
		if (cmp == 0)
			return inode;
		node = (cmp < 0) ? node->left : node->right;
	}
	return NULL;
}

struct inode *new_inode(struct super_block *sb)
{
	struct inode *inode;

	if (sb->s_op && sb->s_op->alloc_inode)
		inode = sb->s_op->alloc_inode(sb);
	else
		inode = (struct inode *)kmem_cache_alloc(g_inode_cache);

	if (!inode)
		return NULL;
	memset(inode, 0, sizeof(*inode));
	dlist_init(&inode->i_dentry);
	dlist_init(&inode->i_dirty_list);
	inode->i_sb = sb;
	inode->i_count = 1;
	return inode;
}

struct inode *iget(struct super_block *sb, unsigned long ino)
{
	struct inode *inode;

	inode = inode_find(sb, ino);
	if (inode) {
		inode->i_count++;
		return inode;
	}

	inode = new_inode(sb);
	if (!inode)
		return NULL;

	inode->i_ino = ino;
	rb_insert(&g_inode_tree, &inode->i_rbnode);
	return inode;
}

void insert_inode_hash(struct inode *inode)
{
	if (!inode)
		return;
	rb_insert(&g_inode_tree, &inode->i_rbnode);
}

void iput(struct inode *inode)
{
	if (!inode)
		return;

	inode->i_count--;
	if (inode->i_count <= 0) {
		if (inode->i_dirty)
			write_inode_now(inode);
		rb_delete(&g_inode_tree, &inode->i_rbnode);
		if (inode->i_sb->s_op && inode->i_sb->s_op->destroy_inode)
			inode->i_sb->s_op->destroy_inode(inode);
		else
			kmem_cache_free(g_inode_cache, inode);
	}
}

void mark_inode_dirty(struct inode *inode)
{
	if (!inode)
		return;
	if (!inode->i_dirty) {
		inode->i_dirty = 1;
		dlist_add(&g_dirty_inodes, &inode->i_dirty_list);
	}
}

int write_inode_now(struct inode *inode)
{
	int ret = 0;

	if (!inode)
		return -1;

	if (inode->i_sb->s_op && inode->i_sb->s_op->write_inode)
		ret = inode->i_sb->s_op->write_inode(inode);

	if (ret == 0) {
		inode->i_dirty = 0;
		dlist_del(&inode->i_dirty_list);
	}
	return ret;
}

void sync_inodes(void)
{
	struct dlist_node *node;
	struct inode *inode;

	node = g_dirty_inodes.next;
	while (node != &g_dirty_inodes) {
		struct dlist_node *next = node->next;
		inode = container_of(node, struct inode, i_dirty_list);
		write_inode_now(inode);
		node = next;
	}
}
