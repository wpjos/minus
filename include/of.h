#ifndef __OF_H__
#define __OF_H__

#include "types.h"

/*
 * Open Firmware (Device Tree) helper functions
 */

/* Get a property by name */
const void *of_get_property(const void *fdt, int nodeoffset,
			    const char *name, int *lenp);

/* Get the compatible string list for a node, returns count */
int of_get_compatible(const void *fdt, int nodeoffset,
		      const char **out, int max_count);

/* Check if a node matches a compatible string */
int of_device_is_compatible(const void *fdt, int nodeoffset,
			    const char *compat);

/* Get address and size from reg property (uses parent for #cells) */
int of_get_address(const void *fdt, int nodeoffset, int parent,
		   uint64_t *addr, uint64_t *size);

/* Get interrupt number from interrupts property */
int of_get_irq(const void *fdt, int nodeoffset, int index);

/* Scan device tree and register platform devices */
int of_platform_populate(const void *fdt);

#endif /* __OF_H__ */
