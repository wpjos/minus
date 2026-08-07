#include "loader.h"
#include "elf.h"
#include "task.h"
#include "sched.h"
#include "vma.h"
#include "mmu.h"
#include "memory.h"
#include "page.h"
#include "buddy.h"
#include "mm.h"
#include "vfs.h"
#include "string.h"
#include "errno.h"
#include "stat.h"
#include "fcntl.h"
#include "cache.h"
#include "printk.h"
#include "pt_regs.h"
#include "entry-common.h"

#define MAX_ELF_SIZE (16 * 1024 * 1024)

extern void ret_to_user(void);

static uint64_t prot_to_attr(uint32_t flags)
{
	if (flags & VM_WRITE) {
		if (flags & VM_EXEC) {
			return PTE_ATTR_NORMAL | PTE_SH_INNER |
			       PTE_AP_RW_ANY | PTE_AF | PTE_PXN;
		}
		return MMU_REGION_USER_STACK;
	}

	if (flags & VM_EXEC)
		return MMU_REGION_USER_CODE;

	return MMU_REGION_USER_RO;
}

static int map_user_page(struct mm_struct *mm, uintptr_t uva,
			 struct page *page, uint32_t flags)
{
	struct vm_area_struct *vma;
	uint64_t attr = prot_to_attr(flags);
	uint64_t pa = page_to_phy(page);
	void *kvaddr = page_to_virt(page);

	vma = (struct vm_area_struct *)kmalloc(sizeof(*vma));
	if (!vma)
		return -ENOMEM;

	memset(vma, 0, sizeof(*vma));
	vma->start = uva;
	vma->end = uva + PAGE_SIZE;
	vma->flags = flags;
	vma->page = page;

	mmu_map(mm->pgd, uva, pa, PAGE_SIZE, attr);
	flush_dcache_icache_range(kvaddr, PAGE_SIZE);

	dlist_add_tail(&mm->vma_list, &vma->node);
	return 0;
}

static void unload_segment_pages(struct mm_struct *mm, uintptr_t vstart,
				 uintptr_t vend)
{
	struct dlist_node *node, *next;
	struct vm_area_struct *vma;

	node = mm->vma_list.next;
	while (node != &mm->vma_list) {
		next = node->next;
		vma = container_of(node, struct vm_area_struct, node);
		if (vma->start >= vstart && vma->start < vend) {
			dlist_del(&vma->node);
			if (vma->page)
				buddy_free_pages(vma->page);
			kfree(vma);
		}
		node = next;
	}
}

static int load_elf_segment(struct task_struct *task,
			    struct elf64_phdr *ph, const char *elf_buf,
			    size_t elf_size)
{
	struct mm_struct *mm = task->mm;
	uintptr_t vstart = ALIGN_DOWN(ph->p_vaddr, PAGE_SIZE);
	uintptr_t vend = ALIGN_UP(ph->p_vaddr + ph->p_memsz, PAGE_SIZE);
	uintptr_t uva;
	uint32_t flags = VM_READ;

	if (ph->p_filesz > ph->p_memsz)
		return -ENOEXEC;
	if (ph->p_offset + ph->p_filesz < ph->p_offset ||
	    ph->p_offset + ph->p_filesz > elf_size)
		return -ENOEXEC;
	if (vend < vstart || vend > USER_STACK_TOP)
		return -ENOEXEC;

	if (ph->p_flags & PF_W)
		flags |= VM_WRITE;
	if (ph->p_flags & PF_X)
		flags |= VM_EXEC;

	for (uva = vstart; uva < vend; uva += PAGE_SIZE) {
		struct page *page;
		void *kvaddr;
		uintptr_t seg_file_start;
		uintptr_t seg_file_end;
		uintptr_t page_start;
		uintptr_t page_end;
		uintptr_t copy_start;
		uintptr_t copy_end;
		size_t dst_off;
		size_t src_off;
		size_t len;

		page = buddy_alloc_pages(PAGE_SIZE);
		if (!page) {
			unload_segment_pages(mm, vstart, vend);
			return -ENOMEM;
		}

		kvaddr = page_to_virt(page);
		memset(kvaddr, 0, PAGE_SIZE);

		page_start = uva;
		page_end = uva + PAGE_SIZE;
		seg_file_start = ph->p_vaddr;
		seg_file_end = ph->p_vaddr + ph->p_filesz;

		copy_start = page_start > seg_file_start ? page_start : seg_file_start;
		copy_end = page_end < seg_file_end ? page_end : seg_file_end;

		if (copy_end > copy_start) {
			dst_off = copy_start - page_start;
			src_off = copy_start - seg_file_start + ph->p_offset;
			len = copy_end - copy_start;
			memcpy((char *)kvaddr + dst_off, elf_buf + src_off, len);
		}

		if (map_user_page(mm, uva, page, flags) != 0) {
			buddy_free_pages(page);
			unload_segment_pages(mm, vstart, vend);
			return -ENOMEM;
		}
	}

	return 0;
}

static int alloc_user_stack(struct task_struct *task)
{
	struct mm_struct *mm = task->mm;
	struct page *page;
	void *kvaddr;

	page = buddy_alloc_pages(PAGE_SIZE);
	if (!page)
		return -ENOMEM;

	kvaddr = page_to_virt(page);
	memset(kvaddr, 0, PAGE_SIZE);

	if (map_user_page(mm, USER_STACK_TOP - PAGE_SIZE, page,
			  VM_READ | VM_WRITE) != 0) {
		buddy_free_pages(page);
		return -ENOMEM;
	}

	mm->ustack_top = USER_STACK_TOP;
	return 0;
}

static int read_whole_file(struct file *file, char **buf_out, size_t *size_out)
{
	struct stat st;
	char *buf;
	size_t total = 0;

	if (vfs_fstat(file, &st) < 0)
		return -EIO;
	if (st.st_size == 0 || st.st_size > MAX_ELF_SIZE)
		return -ENOEXEC;

	buf = (char *)kmalloc(st.st_size);
	if (!buf)
		return -ENOMEM;

	while (total < st.st_size) {
		ssize_t n = vfs_read(file, buf + total, st.st_size - total, NULL);
		if (n <= 0)
			break;
		total += n;
	}

	if (total != st.st_size) {
		kfree(buf);
		return -EIO;
	}

	*buf_out = buf;
	*size_out = st.st_size;
	return 0;
}

static int verify_elf(struct elf64_ehdr *ehdr, size_t elf_size)
{
	if (memcmp((const char *)ehdr->e_ident, ELFMAG, SELFMAG) != 0) {
		printk("loader: bad ELF magic\n");
		return -ENOEXEC;
	}

	if (ehdr->e_ident[EI_CLASS] != ELFCLASS64 ||
	    ehdr->e_ident[EI_DATA] != ELFDATA2LSB ||
	    ehdr->e_ident[EI_VERSION] != EV_CURRENT) {
		printk("loader: unsupported ELF ident\n");
		return -ENOEXEC;
	}

	if (ehdr->e_type != ET_EXEC || ehdr->e_machine != EM_AARCH64) {
		printk("loader: not AArch64 executable\n");
		return -ENOEXEC;
	}

	if (ehdr->e_phentsize != sizeof(struct elf64_phdr)) {
		printk("loader: bad phdr size\n");
		return -ENOEXEC;
	}

	if (ehdr->e_phnum == 0 || ehdr->e_phnum > 128) {
		printk("loader: bad phdr count\n");
		return -ENOEXEC;
	}

	if (ehdr->e_phoff < sizeof(*ehdr) ||
	    ehdr->e_phoff + (uint64_t)ehdr->e_phnum * sizeof(struct elf64_phdr) <
	    ehdr->e_phoff ||
	    ehdr->e_phoff + (uint64_t)ehdr->e_phnum * sizeof(struct elf64_phdr) >
	    elf_size) {
		printk("loader: bad phdr offset\n");
		return -ENOEXEC;
	}

	return 0;
}

void run_user_init(const char *path)
{
	struct task_struct *task;
	struct file *file;
	struct mm_struct *mm;
	char *elf_buf = NULL;
	size_t elf_size = 0;
	struct elf64_ehdr *ehdr;
	struct elf64_phdr *phdrs;
	struct pt_regs *regs;
	void *kstack;
	int i;
	int ret;

	task = task_alloc("init");
	if (!task) {
		printk("loader: failed to allocate init task\n");
		return;
	}

	mm = task->mm;

	mm->pgd = (uint64_t *)kzalloc_pages(PAGE_SIZE);
	if (!mm->pgd)
		goto fail_task;

	kstack = kzalloc_pages(PAGE_SIZE);
	if (!kstack)
		goto fail_task;
	mm->kstack_top = (uint64_t)kstack + PAGE_SIZE;

	file = vfs_open(path, O_RDONLY, 0);
	if (!file) {
		printk("loader: cannot open %s\n", path);
		goto fail_task;
	}

	ret = read_whole_file(file, &elf_buf, &elf_size);
	vfs_close(file);
	if (ret < 0) {
		printk("loader: failed to read %s\n", path);
		goto fail_task;
	}

	ehdr = (struct elf64_ehdr *)elf_buf;
	ret = verify_elf(ehdr, elf_size);
	if (ret < 0)
		goto fail_buf;

	phdrs = (struct elf64_phdr *)(elf_buf + ehdr->e_phoff);
	for (i = 0; i < ehdr->e_phnum; i++) {
		if (phdrs[i].p_type == PT_LOAD) {
			ret = load_elf_segment(task, &phdrs[i], elf_buf, elf_size);
			if (ret < 0) {
				printk("loader: failed to load segment %d\n", i);
				goto fail_buf;
			}
		}
	}

	if (alloc_user_stack(task) < 0) {
		printk("loader: failed to allocate stack\n");
		goto fail_buf;
	}

	kfree(elf_buf);

	regs = (struct pt_regs *)(mm->kstack_top - sizeof(*regs));
	memset(regs, 0, sizeof(*regs));
	regs->elr = ehdr->e_entry;
	regs->spsr = 0; /* EL0t, IRQs unmasked */
	regs->sp_el0 = USER_STACK_TOP;

	task->thread.sp = (uint64_t)regs;
	task->thread.lr = (uint64_t)ret_to_user;

	sched_enqueue(task);
	schedule();
	return;

fail_buf:
	kfree(elf_buf);
fail_task:
	task_free(task);
}
