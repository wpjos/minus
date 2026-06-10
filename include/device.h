#ifndef __DEVICE_H__
#define __DEVICE_H__

#include "types.h"
#include "dlist.h"

/* Forward declarations for circular references */
struct device;
struct driver;

/*
 * OF (Open Firmware) device identification
 * Simplified from Linux's of_device_id
 */
struct of_device_id {
	const char *compatible;
	const void *data;
};

/*
 * Bus type abstraction
 * Defines how devices and drivers are matched on a bus
 */
struct bus_type {
	const char *name;
	struct dlist_node devices;	/* list of devices on this bus */
	struct dlist_node drivers;	/* list of drivers for this bus */

	int  (*match)(struct device *dev, struct driver *drv);
	int  (*probe)(struct device *dev);
	int  (*remove)(struct device *dev);
};

/*
 * Base device structure
 * Every device in the system inherits from this
 */
struct device {
	const char *name;
	struct bus_type *bus;
	struct driver *driver;		/* bound driver, NULL if unbound */
	struct dlist_node node;		/* node in bus->devices list */
	void *driver_data;		/* driver private data */
	void *platform_data;		/* platform-specific data */
	int of_node;			/* FDT node offset, -1 if none */
};

/*
 * Base driver structure
 * Every driver inherits from this
 */
struct driver {
	const char *name;
	struct bus_type *bus;
	struct dlist_node node;		/* node in bus->drivers list */
	const struct of_device_id *of_match_table;
	int  (*probe)(struct device *dev);
	int  (*remove)(struct device *dev);
};

/* Core API */
int bus_register(struct bus_type *bus);
int device_register(struct device *dev);
int driver_register(struct driver *drv);

#endif /* __DEVICE_H__ */
