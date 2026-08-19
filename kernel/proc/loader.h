#ifndef __LOADER_H__
#define __LOADER_H__

#include "types.h"
#include "cap.h"

#define USER_LOAD_BASE	0x400000UL
#define USER_STACK_TOP	0x80000000UL

/*
 * Load an ELF image into @vspace_cap through the mm and fs service
 * tables - the loader knows nothing of pages, page tables or block
 * devices.  On success stores the entry PC and the user stack top.
 * On failure partial mappings are left behind: destroy the vspace.
 */
int proc_load_elf(const char *path, cap_t vspace_cap,
		  uintptr_t *entry, uintptr_t *stack_top);

#endif /* __LOADER_H__ */
