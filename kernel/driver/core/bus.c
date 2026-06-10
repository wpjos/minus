#include "device.h"
#include "dlist.h"

int bus_register(struct bus_type *bus)
{
	dlist_init(&bus->devices);
	dlist_init(&bus->drivers);
	return 0;
}
