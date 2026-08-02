#include "file.h"
#include "string.h"
#include "mm.h"
#include "errno.h"

struct files_struct *alloc_files_struct(void)
{
	struct files_struct *files;

	files = (struct files_struct *)kmalloc(sizeof(*files));
	if (!files)
		return NULL;
	memset(files, 0, sizeof(*files));
	files->count = 1;
	return files;
}

int get_unused_fd(struct files_struct *files)
{
	int i;

	if (!files)
		return -EBADF;

	for (i = 0; i < NR_OPEN_DEFAULT; i++) {
		if (!files->fd_array[i])
			return i;
	}
	return -EMFILE;
}

void put_unused_fd(struct files_struct *files, int fd)
{
	if (!files || fd < 0 || fd >= NR_OPEN_DEFAULT)
		return;
	files->fd_array[fd] = NULL;
}

struct file *fget(struct files_struct *files, int fd)
{
	if (!files || fd < 0 || fd >= NR_OPEN_DEFAULT)
		return NULL;
	return files->fd_array[fd];
}

void fd_install(struct files_struct *files, int fd, struct file *file)
{
	if (!files || fd < 0 || fd >= NR_OPEN_DEFAULT)
		return;
	files->fd_array[fd] = file;
}
