#include "fb.h"
#include "minus_fb.h"
#include "file.h"
#include "inode.h"
#include "super.h"
#include "task.h"
#include "vma.h"
#include "uaccess.h"
#include "errno.h"
#include "page.h"
#include "stat.h"
#include "string.h"

static int fb0_open(struct inode *inode, struct file *file)
{
	struct fb_info *info;

	(void)inode;

	info = fb_get_info();
	if (!info)
		return -ENODEV;

	file->f_private = info;
	return 0;
}

static int fb0_release(struct inode *inode, struct file *file)
{
	(void)inode;
	file->f_private = NULL;
	return 0;
}

static long fb0_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct fb_info *info = (struct fb_info *)file->f_private;
	struct fb_info_req req;

	if (!info)
		return -ENODEV;

	switch (cmd) {
	case FBIOGET_INFO:
		memset(&req, 0, sizeof(req));
		req.width = info->width;
		req.height = info->height;
		req.stride = info->stride;
		req.format = info->format;
		req.size = info->screen_size;
		if (copy_to_user((void *)arg, &req, sizeof(req)) != 0)
			return -EFAULT;
		return 0;
	case FBIO_FLUSH:
		if (info->fbops && info->fbops->fb_flush)
			return info->fbops->fb_flush(info, 0, 0,
						    info->width, info->height);
		return -ENODEV;
	default:
		return -ENOTTY;
	}
}

static long fb0_mmap(struct file *file, void **addr, size_t length,
		     loff_t offset)
{
	struct fb_info *info = (struct fb_info *)file->f_private;
	struct mm_struct *mm = current->mm;
	uint64_t uva;
	size_t size;
	int ret;

	(void)offset;

	if (!info || !mm || !mm->pgd)
		return -EINVAL;

	size = PAGE_ALIGN(info->screen_size);
	if (length < size)
		return -EINVAL;

	ret = vma_map_contig_phys(mm, info->phys_base, size,
				  VM_READ | VM_WRITE, &uva);
	if (ret < 0)
		return ret;

	*addr = (void *)uva;
	return 0;
}

static struct file_operations fb0_fops = {
	.open    = fb0_open,
	.release = fb0_release,
	.ioctl   = fb0_ioctl,
	.mmap    = fb0_mmap,
};

struct inode *devfs_new_fb0_inode(struct super_block *sb)
{
	struct inode *inode;

	inode = new_inode(sb);
	if (!inode)
		return NULL;

	inode->i_ino = 3;
	inode->i_mode = S_IFCHR | 0666;
	inode->i_nlink = 1;
	inode->i_uid = 0;
	inode->i_gid = 0;
	inode->i_size = 0;
	inode->i_fop = &fb0_fops;
	return inode;
}
