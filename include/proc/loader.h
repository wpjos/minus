#ifndef __LOADER_H__
#define __LOADER_H__

#include "types.h"

struct task_struct;

#define USER_LOAD_BASE	0x400000UL
#define USER_STACK_TOP	0x80000000UL

int proc_load_elf(const char *path, struct task_struct *task,
		  uintptr_t *entry, uintptr_t *stack_top);

#endif /* __LOADER_H__ */
