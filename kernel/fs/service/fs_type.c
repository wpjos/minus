#include "fs_type.h"
#include "string.h"
#include "errno.h"

/*
 * Filesystem type registry.
 *
 * The VFS core keeps a single linked list of struct file_system_type.
 * Backends (kernel/fs/backend/) register themselves from their
 * SUBSYS_LEVEL_FS_BACKEND init via register_filesystem(); mount.c looks
 * types up via get_fs_type().  Everything else in the kernel goes through
 * the fs service table and never touches this file.
 */

static struct file_system_type *g_fs_types;

int register_filesystem(struct file_system_type *fs)
{
	struct file_system_type **p;

	if (!fs || !fs->name || !fs->mount)
		return -EINVAL;

	for (p = &g_fs_types; *p; p = &(*p)->next) {
		if (strcmp((*p)->name, fs->name) == 0)
			return -EEXIST;
	}

	fs->next = NULL;
	*p = fs;
	return 0;
}

struct file_system_type *get_fs_type(const char *name)
{
	struct file_system_type *fs;

	for (fs = g_fs_types; fs; fs = fs->next) {
		if (strcmp(fs->name, name) == 0)
			return fs;
	}
	return NULL;
}
