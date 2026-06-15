#ifndef __OF_H__
#define __OF_H__

#include "types.h"

/* Forward declaration: struct resource is defined in platform.h */
struct resource;

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

/*
 * Get the index-th memory resource from a reg property.
 * Uses parent node for #address-cells and #size-cells.
 * Returns 0 on success, -1 on error.
 */
int of_get_address_resource(const void *fdt, int nodeoffset, int parent,
			    int index, struct resource *res);

/* Count the number of address entries in a reg property */
int of_get_address_count(const void *fdt, int nodeoffset, int parent);

/*
 * Get the index-th IRQ resource from an interrupts property.
 * Reads #interrupt-cells from the node's interrupt-parent.
 * Returns 0 on success, -1 on error.
 */
int of_get_irq_resource(const void *fdt, int nodeoffset, int index,
			struct resource *res);

/* Count the number of interrupts in an interrupts property */
int of_get_irq_count(const void *fdt, int nodeoffset);

/* Scan device tree and register platform devices */
int of_platform_populate(const void *fdt);

#endif /* __OF_H__ */
