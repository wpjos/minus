#include "module.h"
#include "printk.h"
#include "mm.h"
#include "fdt.h"

const char logo[] = "hello minus!!!\n";

int start_kernel(void)
{
	module_init();
	printk("%s\n", &logo[0]);
	mm_init();

	while(1);
	return 0;
}
