#include "module.h"
#include "printk.h"
#include "mm.h"

const char logo[] = "hello minus!!!\n";

int start_kernel(void)
{
	/* Register all drivers (triggers match & probe) */
	module_init();

	printk("%s\n", &logo[0]);
	early_mm_init();

	while(1);
	return 0;
}
