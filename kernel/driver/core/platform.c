#include "platform.h"
#include "dlist.h"
#include "string.h"

/* Forward declarations for bus callbacks */
static int platform_match(struct device *dev, struct driver *drv);
static int platform_probe(struct device *dev);
static int platform_remove(struct device *dev);

/*
 * Global platform bus instance
 */
struct bus_type platform_bus_type = {
	.name   = "platform",
	.match  = platform_match,
	.probe  = platform_probe,
	.remove = platform_remove,
};

/*
 * Platform bus match: compare device compatible string
 * against driver's of_match_table
 */
static int platform_match(struct device *dev, struct driver *drv)
{
	struct platform_device *pdev = platform_device_of(dev);
	struct platform_driver *pdrv = platform_driver_of(drv);
	const struct of_device_id *id;

	if (!pdrv->of_match_table || !pdev->compatible)
		return 0;

	for (id = pdrv->of_match_table; id->compatible; id++) {
		if (strcmp(pdev->compatible, id->compatible) == 0)
			return 1;
	}

	return 0;
}

/*
 * Platform bus probe: call driver's probe with platform_device
 */
static int platform_probe(struct device *dev)
{
	struct platform_device *pdev = platform_device_of(dev);
	struct platform_driver *pdrv = platform_driver_of(dev->driver);

	if (pdrv->probe)
		return pdrv->probe(pdev);

	return 0;
}

/*
 * Platform bus remove: call driver's remove with platform_device
 */
static int platform_remove(struct device *dev)
{
	struct platform_device *pdev = platform_device_of(dev);
	struct platform_driver *pdrv = platform_driver_of(dev->driver);

	if (pdrv && pdrv->remove)
		return pdrv->remove(pdev);

	return 0;
}

/*
 * Initialize the platform bus
 */
int platform_bus_init(void)
{
	return bus_register(&platform_bus_type);
}

/*
 * Register a platform device
 */
int platform_device_register(struct platform_device *pdev)
{
	pdev->dev.bus = &platform_bus_type;
	return device_register(&pdev->dev);
}

/*
 * Register a platform driver
 */
int platform_driver_register(struct platform_driver *pdrv)
{
	pdrv->drv.bus = &platform_bus_type;
	pdrv->drv.of_match_table = pdrv->of_match_table;
	return driver_register(&pdrv->drv);
}
