#include "dentry.h"
#include "inode.h"
#include "string.h"
#include "mm.h"
#include "slab.h"

static struct kmem_cache *g_dentry_cache;

void dentry_cache_init(void)
{
	g_dentry_cache = kmem_cache_create("dentry", sizeof(struct dentry), 8);
}

struct dentry *d_alloc(struct dentry *parent, const char *name)
{
	struct dentry *dentry;
	size_t len;

	dentry = (struct dentry *)kmem_cache_alloc(g_dentry_cache);
	if (!dentry)
		return NULL;

	memset(dentry, 0, sizeof(*dentry));
	dlist_init(&dentry->d_child);
	dlist_init(&dentry->d_subdirs);
	dentry->d_parent = parent ? parent : dentry;
	dentry->d_count = 1;

	if (name) {
		len = strlen(name);
		if (len >= sizeof(dentry->d_name))
			len = sizeof(dentry->d_name) - 1;
		memcpy(dentry->d_name, name, len);
		dentry->d_name[len] = '\0';
	}

	if (parent)
		dlist_add(&parent->d_subdirs, &dentry->d_child);

	return dentry;
}

void d_instantiate(struct dentry *dentry, struct inode *inode)
{
	if (!dentry || !inode)
		return;
	dentry->d_inode = inode;
	dentry->d_sb = inode->i_sb;
	inode->i_count++;
}

struct dentry *d_lookup(struct dentry *parent, const char *name)
{
	struct dentry *dentry;

	if (!parent || !name)
		return NULL;

	dlist_for_each_entry(dentry, &parent->d_subdirs, d_child) {
		if (strcmp(dentry->d_name, name) == 0) {
			dentry->d_count++;
			return dentry;
		}
	}
	return NULL;
}

void dget(struct dentry *dentry)
{
	if (dentry)
		dentry->d_count++;
}

void dput(struct dentry *dentry)
{
	if (!dentry)
		return;

	dentry->d_count--;
	if (dentry->d_count <= 0) {
		if (dentry->d_inode) {
			iput(dentry->d_inode);
			dentry->d_inode = NULL;
		}
		dlist_del(&dentry->d_child);
		kmem_cache_free(g_dentry_cache, dentry);
	}
}
