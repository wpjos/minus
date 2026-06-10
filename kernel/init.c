#include "module.h"
#include "printk.h"
#include "mm.h"
#include "fdt.h"
#include "platform.h"
#include "of.h"

const char logo[] = "hello minus!!!\n";

int start_kernel(void)
{
	/* Initialize driver core: platform bus + FDT device population */
	platform_bus_init();
	of_platform_populate(fdt_base());

	/* Register all drivers (triggers match & probe) */
	module_init();

	printk("%s\n", &logo[0]);
	early_mm_init();

	while(1);
	return 0;
}
