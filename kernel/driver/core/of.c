#include "of.h"
#include "platform.h"
#include "fdt.h"
#include "string.h"
#include "types.h"

#define MAX_PLATFORM_DEVICES	64

static struct platform_device of_devices[MAX_PLATFORM_DEVICES];
static int of_device_count;

const void *of_get_property(const void *fdt, int nodeoffset,
			    const char *name, int *lenp)
{
	return fdt_getprop(fdt, nodeoffset, name, lenp);
}

/*
 * Get the compatible string list for a node
 * Returns the number of compatible strings found
 */
int of_get_compatible(const void *fdt, int nodeoffset,
		      const char **out, int max_count)
{
	const char *prop;
	int len;
	int count = 0;
	const char *str, *end;

	prop = fdt_getprop(fdt, nodeoffset, "compatible", &len);
	if (!prop)
		return 0;

	str = prop;
	end = str + len;

	while (str < end && count < max_count) {
		out[count++] = str;
		while (str < end && *str != '\0')
			str++;
		str++;
	}

	return count;
}

/*
 * Check if a node matches a compatible string
 */
int of_device_is_compatible(const void *fdt, int nodeoffset,
			    const char *compat)
{
	const char *compatibles[16];
	int count, i;

	count = of_get_compatible(fdt, nodeoffset, compatibles, 16);
	for (i = 0; i < count; i++) {
		if (strcmp(compatibles[i], compat) == 0)
			return 1;
	}

	return 0;
}

/*
 * Get address and size from reg property
 * Uses parent node for #address-cells and #size-cells
 */
int of_get_address(const void *fdt, int nodeoffset, int parent,
		   uint64_t *addr, uint64_t *size)
{
	const fdt32_t *prop;
	int len;
	int ac, sc;

	ac = fdt_address_cells(fdt, parent);
	sc = fdt_size_cells(fdt, parent);
	if (ac < 0 || sc < 0)
		return -1;

	prop = fdt_getprop(fdt, nodeoffset, "reg", &len);
	if (!prop)
		return -1;

	/* Check we have enough data for at least one entry */
	int entry_cells = ac + sc;
	if (len < entry_cells * (int)sizeof(fdt32_t))
		return -1;

	*addr = 0;
	if (ac >= 2)
		*addr = ((uint64_t)fdt32_to_cpu(prop[0]) << 32) |
			fdt32_to_cpu(prop[1]);
	else
		*addr = fdt32_to_cpu(prop[0]);

	*size = 0;
	if (sc >= 2)
		*size = ((uint64_t)fdt32_to_cpu(prop[ac]) << 32) |
			fdt32_to_cpu(prop[ac + 1]);
	else
		*size = fdt32_to_cpu(prop[ac]);

	return 0;
}

/*
 * Get interrupt number from interrupts property
 * Simplified for QEMU virt (3 cells per interrupt: type, irq, flags)
 */
int of_get_irq(const void *fdt, int nodeoffset, int index)
{
	const fdt32_t *prop;
	int len;

	prop = fdt_getprop(fdt, nodeoffset, "interrupts", &len);
	if (!prop)
		return 0;

	/* QEMU virt uses 3 cells per interrupt (GIC: type, irq, flags) */
	if (len < (index + 1) * 3 * (int)sizeof(fdt32_t))
		return 0;

	prop += index * 3;
	return fdt32_to_cpu(prop[1]);
}

/*
 * Check if a node should be skipped during platform population
 */
static int of_skip_node(const char *name)
{
	if (strcmp(name, "cpus") == 0 ||
	    strcmp(name, "memory") == 0 ||
	    strcmp(name, "chosen") == 0 ||
	    strcmp(name, "aliases") == 0)
		return 1;
	return 0;
}

/*
 * Populate platform devices from device tree
 * Scans for nodes with "compatible" property and creates platform devices
 */
int of_platform_populate(const void *fdt)
{
	int node;

	fdt_for_each_node(node, fdt) {
		const char *name;
		const char *compat;
		int len;
		int parent;
		struct platform_device *pdev;

		name = fdt_get_name(fdt, node, NULL);
		if (!name)
			continue;

		/* Skip special nodes */
		if (of_skip_node(name))
			continue;

		/* Skip nodes without compatible property */
		compat = fdt_getprop(fdt, node, "compatible", &len);
		if (!compat)
			continue;

		if (of_device_count >= MAX_PLATFORM_DEVICES)
			break;

		pdev = &of_devices[of_device_count];
		memset(pdev, 0, sizeof(*pdev));

		pdev->dev.name = name;
		pdev->dev.of_node = node;
		pdev->compatible = compat;

		/* Parse reg property using parent's #address-cells/#size-cells */
		parent = fdt_parent_offset(fdt, node);
		if (parent >= 0) {
			of_get_address(fdt, node, parent,
				       &pdev->resource_start,
				       &pdev->resource_size);
		}

		/* Parse interrupts */
		pdev->irq = of_get_irq(fdt, node, 0);

		platform_device_register(pdev);
		of_device_count++;
	}

	return 0;
}
