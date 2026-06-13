#ifndef __PLATFORM_H__
#define __PLATFORM_H__

#include "device.h"

/*
 * Platform device
 * Represents devices on the "platform" bus (simple-bus compatible)
 * These are typically memory-mapped peripherals defined in device tree
 */
struct platform_device {
	struct device dev;
	uint64_t resource_start;	/* physical base address */
	uint64_t resource_size;	/* register region size */
	uint32_t irq;			/* interrupt number, 0 if none */
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

/* Global platform bus instance */
extern struct bus_type platform_bus_type;

#endif /* __PLATFORM_H__ */
