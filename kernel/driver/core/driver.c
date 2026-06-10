#include "device.h"
#include "dlist.h"

int driver_register(struct driver *drv)
{
	struct device *dev;

	if (!drv || !drv->bus)
		return -1;

	dlist_add(&drv->bus->drivers, &drv->node);

	/* Try to match with existing devices */
	dlist_for_each_entry(dev, &drv->bus->devices, node) {
		if (!dev->driver && drv->bus->match &&
		    drv->bus->match(dev, drv)) {
			dev->driver = drv;
			if (drv->bus->probe)
				drv->bus->probe(dev);
		}
	}

	return 0;
}
