#include "namei.h"
#include "file.h"
#include "string.h"
#include "mm.h"
#include "errno.h"
#include "task.h"
#include "stat.h"

struct dentry *lookup_one_len(const char *name, struct dentry *base, int len)
{
	struct dentry *dentry;
	char tmp[64];

	if (!base || !name)
		return NULL;

	if (len >= (int)sizeof(tmp))
		len = (int)sizeof(tmp) - 1;
	memcpy(tmp, name, len);
	tmp[len] = '\0';

	dentry = d_lookup(base, tmp);
	if (dentry)
		return dentry;

	dentry = d_alloc(base, tmp);
	if (!dentry)
		return NULL;

	if (base->d_inode && base->d_inode->i_op &&
	    base->d_inode->i_op->lookup) {
		struct dentry *found;

		found = base->d_inode->i_op->lookup(base->d_inode, dentry);
		if (!found) {
			dput(dentry);
			return NULL;
		}
		if (found != dentry)
			dput(dentry);
		return found;
	}

	return dentry;
}

static int follow_path(const char *path, struct path *path_out)
{
	struct dentry *dentry;
	const char *p;
	char comp[64];
	int i;

	if (!path || !path_out)
		return -EINVAL;

	if (path[0] == '/')
		dentry = root_dentry;
	else
		dentry = current->files ? current->files->fd_array[0] ?
			current->files->fd_array[0]->f_dentry : root_dentry
			: root_dentry;

	if (!dentry)
		return -ENOENT;

	dget(dentry);
	path_out->dentry = dentry;
	path_out->mnt = root_mnt;

	p = path;
	while (*p == '/')
		p++;

	while (*p) {
		struct dentry *next;

		i = 0;
		while (*p && *p != '/' && i < (int)sizeof(comp) - 1)
			comp[i++] = *p++;
		comp[i] = '\0';

		while (*p == '/')
			p++;

		if (i == 0)
			continue;

		if (!S_ISDIR(dentry->d_inode->i_mode)) {
			dput(dentry);
			return -ENOTDIR;
		}

		next = lookup_one_len(comp, dentry, i);
		if (!next) {
			dput(dentry);
			return -ENOENT;
		}

		dput(dentry);
		dentry = next;
	}

	path_out->dentry = dentry;
	return 0;
}

int vfs_path_lookup(const char *path, struct path *path_out)
{
	return follow_path(path, path_out);
}

int split_last_component(const char *pathname,
			 char *parent, size_t parent_size,
			 char *name, size_t name_size)
{
	const char *last;
	size_t len;

	last = strrchr(pathname, '/');
	if (!last)
		return -EINVAL;

	len = (size_t)(last - pathname);
	if (len >= parent_size)
		len = parent_size - 1;
	memcpy(parent, pathname, len);
	parent[len] = '\0';
	if (len == 0) {
		parent[0] = '/';
		parent[1] = '\0';
	}

	last++;
	len = strlen(last);
	if (len >= name_size)
		len = name_size - 1;
	memcpy(name, last, len);
	name[len] = '\0';
	return 0;
}

int vfs_do_unlink(const char *pathname)
{
	char parent_path[256];
	char name[64];
	struct path p;
	struct dentry *dentry;
	int ret;

	ret = split_last_component(pathname, parent_path, sizeof(parent_path),
				   name, sizeof(name));
	if (ret < 0)
		return ret;

	ret = vfs_path_lookup(parent_path, &p);
	if (ret < 0)
		return ret;

	if (!S_ISDIR(p.dentry->d_inode->i_mode)) {
		dput(p.dentry);
		return -ENOTDIR;
	}

	dentry = lookup_one_len(name, p.dentry, strlen(name));
	if (!dentry) {
		dput(p.dentry);
		return -ENOENT;
	}

	if (!dentry->d_inode) {
		ret = -ENOENT;
	} else if (S_ISDIR(dentry->d_inode->i_mode)) {
		ret = -EISDIR;
	} else if (!p.dentry->d_inode->i_op ||
		   !p.dentry->d_inode->i_op->unlink) {
		ret = -EPERM;
	} else {
		ret = p.dentry->d_inode->i_op->unlink(p.dentry->d_inode, dentry);
	}

	dput(dentry);
	dput(p.dentry);
	return ret;
}

int vfs_do_mkdir(const char *pathname, uint16_t mode)
{
	char parent_path[256];
	char name[64];
	struct path p;
	struct dentry *dentry;
	int ret;

	ret = split_last_component(pathname, parent_path, sizeof(parent_path),
				   name, sizeof(name));
	if (ret < 0)
		return ret;

	ret = vfs_path_lookup(parent_path, &p);
	if (ret < 0)
		return ret;

	if (!S_ISDIR(p.dentry->d_inode->i_mode)) {
		dput(p.dentry);
		return -ENOTDIR;
	}

	dentry = d_alloc(p.dentry, name);
	if (!dentry) {
		dput(p.dentry);
		return -ENOMEM;
	}

	if (!p.dentry->d_inode->i_op ||
	    !p.dentry->d_inode->i_op->mkdir) {
		ret = -EPERM;
	} else {
		ret = p.dentry->d_inode->i_op->mkdir(p.dentry->d_inode, dentry,
						     mode | S_IFDIR);
	}

	dput(dentry);
	dput(p.dentry);
	return ret;
}

int vfs_do_rmdir(const char *pathname)
{
	char parent_path[256];
	char name[64];
	struct path p;
	struct dentry *dentry;
	int ret;

	ret = split_last_component(pathname, parent_path, sizeof(parent_path),
				   name, sizeof(name));
	if (ret < 0)
		return ret;

	ret = vfs_path_lookup(parent_path, &p);
	if (ret < 0)
		return ret;

	if (!S_ISDIR(p.dentry->d_inode->i_mode)) {
		dput(p.dentry);
		return -ENOTDIR;
	}

	dentry = lookup_one_len(name, p.dentry, strlen(name));
	if (!dentry) {
		dput(p.dentry);
		return -ENOENT;
	}

	if (!dentry->d_inode) {
		ret = -ENOENT;
	} else if (!S_ISDIR(dentry->d_inode->i_mode)) {
		ret = -ENOTDIR;
	} else if (!p.dentry->d_inode->i_op ||
		   !p.dentry->d_inode->i_op->rmdir) {
		ret = -EPERM;
	} else {
		ret = p.dentry->d_inode->i_op->rmdir(p.dentry->d_inode, dentry);
	}

	dput(dentry);
	dput(p.dentry);
	return ret;
}
