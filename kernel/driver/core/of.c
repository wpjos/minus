#include "of.h"
#include "platform.h"
#include "fdt.h"
#include "string.h"
#include "types.h"
#include "mm.h"
#include "slab.h"
#include "memory.h"

#define MAX_PLATFORM_DEVICES	64

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
 * Read an address/size made up of @cells 32-bit cells.
 */
static uint64_t of_read_cells(const fdt32_t *prop, int cells)
{
	uint64_t val = 0;
	int i;

	for (i = 0; i < cells; i++)
		val = (val << 32) | fdt32_to_cpu(prop[i]);
	return val;
}

/*
 * Translate a device-tree bus address up to the CPU physical address space
 * by walking the parent chain and applying each bus node's "ranges".
 * Returns 0 on success, -1 on failure.
 */
static int of_translate_address(const void *fdt, int nodeoffset,
				uint64_t addr, uint64_t *out)
{
	while (1) {
		int parent = fdt_parent_offset(fdt, nodeoffset);
		int grandparent;

		if (parent < 0)
			return -1;

		grandparent = fdt_parent_offset(fdt, parent);
		if (grandparent < 0) {
			/* parent is the root: address is already CPU physical */
			*out = addr;
			return 0;
		}

		{
			int ac = fdt_address_cells(fdt, parent);
			int sc = fdt_size_cells(fdt, parent);
			int pac = fdt_address_cells(fdt, grandparent);
			const fdt32_t *ranges;
			int len;
			int entry_cells;
			int n, i;
			int matched = 0;

			if (ac < 0 || sc < 0 || pac < 0)
				return -1;

			ranges = fdt_getprop(fdt, parent, "ranges", &len);
			if (!ranges)
				return -1;

			if (len == 0) {
				/* empty "ranges;" means 1:1, continue upward */
				nodeoffset = parent;
				continue;
			}

			entry_cells = ac + pac + sc;
			if (len < entry_cells * (int)sizeof(fdt32_t))
				return -1;

			n = len / (entry_cells * sizeof(fdt32_t));
			for (i = 0; i < n; i++) {
				const fdt32_t *e = ranges + i * entry_cells;
				uint64_t child_base = of_read_cells(e, ac);
				uint64_t parent_base = of_read_cells(e + ac, pac);
				uint64_t size = of_read_cells(e + ac + pac, sc);

				if (addr >= child_base &&
				    addr < child_base + size) {
					addr = parent_base + (addr - child_base);
					matched = 1;
					break;
				}
			}

			if (!matched)
				return -1;
		}

		nodeoffset = parent;
	}
}

/*
 * Get the index-th memory resource from a reg property.
 * Uses parent node for #address-cells and #size-cells and translates
 * the address through any parent "ranges" properties.
 */
int of_get_address_resource(const void *fdt, int nodeoffset, int parent,
			    int index, struct resource *res)
{
	const fdt32_t *prop;
	int len;
	int ac, sc, entry_cells, offset;
	uint64_t addr, size;

	ac = fdt_address_cells(fdt, parent);
	sc = fdt_size_cells(fdt, parent);
	if (ac < 0 || sc < 0)
		return -1;

	prop = fdt_getprop(fdt, nodeoffset, "reg", &len);
	if (!prop)
		return -1;

	entry_cells = ac + sc;
	if (len < (index + 1) * entry_cells * (int)sizeof(fdt32_t))
		return -1;

	offset = index * entry_cells;

	addr = of_read_cells(prop + offset, ac);
	size = of_read_cells(prop + offset + ac, sc);

	if (of_translate_address(fdt, nodeoffset, addr, &addr) < 0)
		return -1;

	res->start = addr;
	res->end = addr + size - 1;
	res->flags = IORESOURCE_MEM;
	res->name = NULL;

	return 0;
}

/*
 * Count the number of address entries in a reg property.
 */
int of_get_address_count(const void *fdt, int nodeoffset, int parent)
{
	const fdt32_t *prop;
	int len;
	int ac, sc, entry_cells;

	ac = fdt_address_cells(fdt, parent);
	sc = fdt_size_cells(fdt, parent);
	if (ac < 0 || sc < 0)
		return 0;

	prop = fdt_getprop(fdt, nodeoffset, "reg", &len);
	if (!prop)
		return 0;

	entry_cells = ac + sc;
	if (len < entry_cells * (int)sizeof(fdt32_t))
		return 0;

	return len / (entry_cells * sizeof(fdt32_t));
}

/*
 * Find the interrupt-parent of a node.
 * Returns the node offset, or a negative error code.
 */
static int of_find_interrupt_parent(const void *fdt, int nodeoffset)
{
	const fdt32_t *prop;
	int len;
	int parent;
	uint32_t phandle;

	prop = fdt_getprop(fdt, nodeoffset, "interrupt-parent", &len);
	if (prop && len >= (int)sizeof(fdt32_t)) {
		phandle = fdt32_to_cpu(prop[0]);
		return fdt_node_offset_by_phandle(fdt, phandle);
	}

	/* No explicit interrupt-parent: inherit from the device parent */
	parent = fdt_parent_offset(fdt, nodeoffset);
	if (parent < 0)
		return parent;

	/* If the parent is an interrupt-controller, use it */
	if (fdt_getprop(fdt, parent, "interrupt-controller", NULL))
		return parent;

	/* Otherwise keep walking up */
	return of_find_interrupt_parent(fdt, parent);
}

/*
 * Read #interrupt-cells from an interrupt controller node.
 * Returns the cell count, or a negative error code.
 */
static int of_get_interrupt_cells(const void *fdt, int intc)
{
	const fdt32_t *prop;
	int len;

	if (intc < 0)
		return -1;

	prop = fdt_getprop(fdt, intc, "#interrupt-cells", &len);
	if (!prop || len < (int)sizeof(fdt32_t))
		return -1;

	return (int)fdt32_to_cpu(prop[0]);
}

/*
 * Convert raw interrupt cells to an IRQ number.
 * ARM GIC: cell[0] = type (0=SPI, 1=PPI, 2=SGI),
 *           cell[1] = index within that type,
 *           cell[2] = flags.
 */
static int of_irq_from_cells(const fdt32_t *cells, int nr_cells)
{
	uint32_t type, irq;

	if (nr_cells <= 0)
		return -1;

	if (nr_cells == 1)
		return (int)fdt32_to_cpu(cells[0]);

	type = fdt32_to_cpu(cells[0]);
	irq = fdt32_to_cpu(cells[1]);

	switch (type) {
	case 0: /* SPI */
		return (int)(irq + 32);
	case 1: /* PPI */
		return (int)(irq + 16);
	case 2: /* SGI */
		return (int)irq;
	default:
		return -1;
	}
}

/*
 * Get the index-th IRQ resource from an interrupts property.
 * Reads #interrupt-cells from the node's interrupt-parent.
 */
int of_get_irq_resource(const void *fdt, int nodeoffset, int index,
			struct resource *res)
{
	const fdt32_t *prop;
	int len;
	int intc, icells;
	int irq;

	intc = of_find_interrupt_parent(fdt, nodeoffset);
	if (intc < 0)
		return -1;

	icells = of_get_interrupt_cells(fdt, intc);
	if (icells < 0)
		return -1;

	prop = fdt_getprop(fdt, nodeoffset, "interrupts", &len);
	if (!prop)
		return -1;

	if (len < (index + 1) * icells * (int)sizeof(fdt32_t))
		return -1;

	irq = of_irq_from_cells(prop + index * icells, icells);
	if (irq < 0)
		return -1;

	res->start = (uint64_t)irq;
	res->end = (uint64_t)irq;
	res->flags = IORESOURCE_IRQ;
	res->name = NULL;

	return 0;
}

/*
 * Count the number of interrupts in an interrupts property.
 */
int of_get_irq_count(const void *fdt, int nodeoffset)
{
	const fdt32_t *prop;
	int len;
	int intc, icells;

	prop = fdt_getprop(fdt, nodeoffset, "interrupts", &len);
	if (!prop)
		return 0;

	intc = of_find_interrupt_parent(fdt, nodeoffset);
	if (intc < 0)
		return 0;

	icells = of_get_interrupt_cells(fdt, intc);
	if (icells <= 0)
		return 0;

	if (len < icells * (int)sizeof(fdt32_t))
		return 0;

	return len / (icells * sizeof(fdt32_t));
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
		int i, count;

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

		pdev = (struct platform_device *)kmalloc(sizeof(struct platform_device));
		if (!pdev)
			continue;
		memset(pdev, 0, sizeof(*pdev));

		pdev->dev.name = name;
		pdev->dev.of_node = node;
		pdev->compatible = compat;

		/* Parse all reg entries using parent's #address-cells/#size-cells */
		parent = fdt_parent_offset(fdt, node);
		if (parent >= 0) {
			count = of_get_address_count(fdt, node, parent);
			if (count > PLATFORM_MAX_RESOURCES)
				count = PLATFORM_MAX_RESOURCES;
			for (i = 0; i < count; i++) {
				of_get_address_resource(fdt, node, parent, i,
							&pdev->resource[i]);
			}
			pdev->num_resources = count;
		}

		/* Parse all interrupts */
		count = of_get_irq_count(fdt, node);
		if (count > PLATFORM_MAX_RESOURCES - pdev->num_resources)
			count = PLATFORM_MAX_RESOURCES - pdev->num_resources;
		for (i = 0; i < count; i++) {
			of_get_irq_resource(fdt, node, i,
					    &pdev->resource[pdev->num_resources + i]);
		}
		pdev->num_resources += count;

		platform_device_register(pdev);
	}

	return 0;
}
