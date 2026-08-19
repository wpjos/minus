#include "loader.h"
#include "elf.h"
#include "mm.h"
#include "mm_service.h"
#include "fs_service.h"
#include "mm_types.h"
#include "memory.h"
#include "string.h"
#include "errno.h"
#include "printk.h"

/*
 * ELF loader.  A pure service client: the image bytes come from the fs
 * service, every user mapping is created through mm's vspace_map_page
 * (which hands back a kernel alias to copy into).  Nothing here touches
 * mm or fs internals; replacing either server does not move this file.
 */

static int load_segment(cap_t vspace_cap, struct elf64_phdr *ph,
			const char *elf_buf, size_t elf_size)
{
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
		void *kva;
		uintptr_t seg_file_start;
		uintptr_t seg_file_end;
		uintptr_t copy_start;
		uintptr_t copy_end;
		size_t dst_off;
		size_t src_off;
		size_t len;

		if (mm_call(vspace_map_page, vspace_cap, uva, flags, &kva))
			return -ENOMEM;

		seg_file_start = ph->p_vaddr;
		seg_file_end = ph->p_vaddr + ph->p_filesz;
		copy_start = uva > seg_file_start ? uva : seg_file_start;
		copy_end = uva + PAGE_SIZE < seg_file_end ? uva + PAGE_SIZE
							  : seg_file_end;

		if (copy_end > copy_start) {
			dst_off = copy_start - uva;
			src_off = copy_start - seg_file_start + ph->p_offset;
			len = copy_end - copy_start;
			memcpy((char *)kva + dst_off, elf_buf + src_off, len);
		}
	}

	return 0;
}

static int alloc_user_stack(cap_t vspace_cap)
{
	void *kva;

	return (int)mm_call(vspace_map_page, vspace_cap,
			    USER_STACK_TOP - PAGE_SIZE,
			    VM_READ | VM_WRITE, &kva);
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

int proc_load_elf(const char *path, cap_t vspace_cap,
		  uintptr_t *entry, uintptr_t *stack_top)
{
	char *elf_buf = NULL;
	size_t elf_size = 0;
	struct elf64_ehdr *ehdr;
	struct elf64_phdr *phdrs;
	int i;
	int ret;

	if (!path || vspace_cap == CAP_NULL || !entry || !stack_top)
		return -EINVAL;

	/*
	 * Read the whole file through the FS service: context-free, no
	 * fdtable - this may run in the idle task during bring-up.
	 */
	ret = (int)fs_call(load_file, path, (void **)&elf_buf, &elf_size);
	if (ret < 0)
		return ret;

	ehdr = (struct elf64_ehdr *)elf_buf;
	ret = verify_elf(ehdr, elf_size);
	if (ret < 0)
		goto out_buf;

	phdrs = (struct elf64_phdr *)(elf_buf + ehdr->e_phoff);
	for (i = 0; i < ehdr->e_phnum; i++) {
		if (phdrs[i].p_type == PT_LOAD) {
			ret = load_segment(vspace_cap, &phdrs[i], elf_buf,
					   elf_size);
			if (ret < 0) {
				printk("loader: failed to load segment %d\n", i);
				goto out_buf;
			}
		}
	}

	if (alloc_user_stack(vspace_cap) != 0) {
		ret = -ENOMEM;
		goto out_buf;
	}

	*entry = ehdr->e_entry;
	*stack_top = USER_STACK_TOP;

	kfree(elf_buf);
	return 0;

out_buf:
	kfree(elf_buf);
	return ret;
}
