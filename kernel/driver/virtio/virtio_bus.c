#include "virtio.h"
#include "module.h"

static int virtio_bus_match(struct device *dev, struct driver *drv)
{
	struct virtio_device *vdev = virtio_device_of(dev);
	struct virtio_driver *vdrv = virtio_driver_of(drv);
	const struct virtio_device_id *id;

	if (!vdrv->id_table)
		return 0;

	for (id = vdrv->id_table; id->device_id != 0; id++) {
		if (vdev->device_id == id->device_id)
			return 1;
	}

	return 0;
}

static int virtio_bus_probe(struct device *dev)
{
	struct virtio_device *vdev = virtio_device_of(dev);
	struct virtio_driver *vdrv = virtio_driver_of(dev->driver);

	if (vdrv->probe)
		return vdrv->probe(vdev);

	return 0;
}

static int virtio_bus_remove(struct device *dev)
{
	struct virtio_device *vdev = virtio_device_of(dev);
	struct virtio_driver *vdrv = virtio_driver_of(dev->driver);

	if (vdrv && vdrv->remove)
		return vdrv->remove(vdev);

	return 0;
}

struct bus_type virtio_bus_type = {
	.name   = "virtio",
	.match  = virtio_bus_match,
	.probe  = virtio_bus_probe,
	.remove = virtio_bus_remove,
};

int virtio_driver_register(struct virtio_driver *drv)
{
	drv->drv.bus = &virtio_bus_type;
	return driver_register(&drv->drv);
}

int virtio_device_register(struct virtio_device *vdev)
{
	vdev->dev.bus = &virtio_bus_type;
	return device_register(&vdev->dev);
}

static void virtio_bus_init(void)
{
	bus_register(&virtio_bus_type);
}

module_register(virtio_bus, MODULE_LEVEL_CORE, virtio_bus_init);
