#include "device.h"
#include "dlist.h"

int device_register(struct device *dev)
{
	struct driver *drv;

	if (!dev || !dev->bus)
		return -1;

	dlist_add(&dev->bus->devices, &dev->node);

	/* Try to match with existing drivers */
	dlist_for_each_entry(drv, &dev->bus->drivers, node) {
		if (dev->bus->match && dev->bus->match(dev, drv)) {
			dev->driver = drv;
			if (dev->bus->probe)
				dev->bus->probe(dev);
			break;
		}
	}

	return 0;
}
