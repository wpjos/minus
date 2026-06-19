#ifndef __PLATFORM_H__
#define __PLATFORM_H__

#include "device.h"

/*
 * Resource flags.
 * A device resource can describe a memory region, an IRQ, DMA channel, etc.
 */
#define IORESOURCE_MEM		(1U << 9)
#define IORESOURCE_IRQ		(1U << 10)

/*
 * Maximum resources per platform device.
 * Must cover all reg entries plus all interrupts for a single device.
 */
#define PLATFORM_MAX_RESOURCES	8

/*
 * Device resource descriptor.
 * Describes one address range or one interrupt from the device tree.
 */
struct resource {
	uint64_t start;			/* start address or IRQ number */
	uint64_t end;			/* inclusive end address or IRQ number */
	uint32_t flags;			/* IORESOURCE_* */
	const char *name;		/* optional name */
};

/*
 * Platform device
 * Represents devices on the "platform" bus (simple-bus compatible)
 * These are typically memory-mapped peripherals defined in device tree.
 * A device may have multiple resources, including memory regions and IRQs.
 */
struct platform_device {
	struct device dev;
	struct resource resource[PLATFORM_MAX_RESOURCES];
	int num_resources;
	const char *compatible;		/* DT compatible string (points into FDT blob) */
};

/*
 * Platform driver
 * Driver for platform devices
 */
struct platform_driver {
	struct driver drv;
	const struct of_device_id *of_match_table;
	int (*probe)(struct platform_device *pdev);
	int (*remove)(struct platform_device *pdev);
};

/* Helper macros - use __d to avoid parameter/member name clash */
#define platform_device_of(__d) \
	container_of(__d, struct platform_device, dev)
#define platform_driver_of(__d) \
	container_of(__d, struct platform_driver, drv)

/* Platform bus API */
int platform_device_register(struct platform_device *pdev);
int platform_driver_register(struct platform_driver *pdrv);

/*
 * Helpers to fetch a device's Nth resource of a given type.
 * Returns NULL if the index-th resource of that type is not found.
 */
static inline struct resource *platform_get_resource(struct platform_device *pdev,
					     uint32_t type,
					     unsigned int index)
{
	int i;

	if (!pdev)
		return NULL;

	for (i = 0; i < pdev->num_resources; i++) {
		if ((pdev->resource[i].flags & type) == type) {
			if (index == 0)
				return &pdev->resource[i];
			index--;
		}
	}
	return NULL;
}

static inline int platform_get_irq(struct platform_device *pdev,
				   unsigned int index)
{
	struct resource *res = platform_get_resource(pdev, IORESOURCE_IRQ, index);

	if (!res)
		return 0;
	return (int)res->start;
}

static inline uint64_t resource_size(const struct resource *res)
{
	if (!res)
		return 0;
	return res->end - res->start + 1;
}

/* Global platform bus instance */
extern struct bus_type platform_bus_type;

#endif /* __PLATFORM_H__ */
