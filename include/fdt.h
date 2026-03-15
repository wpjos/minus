#ifndef __FDT_H__
#define __FDT_H__

#include "libfdt.h"

extern char __dtb_start[];

static inline void *fdt_base(void)
{
	return &__dtb_start[0];
}

#define fdt_for_each_node(node, fdt)	\
	int __depth = 0;		\
	for (node = -1; (node = fdt_next_node(fdt, node, &__depth)) >= 0 && __depth >= 0;)

#endif
