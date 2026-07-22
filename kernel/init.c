#include "module.h"
#include "printk.h"
#include "mm.h"
#include "irq.h"

const char logo[] = "hello minus!!!\n";

int start_kernel(void)
{
	mm_init();
	/* Install exception vectors before any driver may trigger a fault */
	irq_init();

	/* Register all drivers (triggers match & probe) */
	module_init();

	printk("%s\n", &logo[0]);

	/* Enable interrupts once the interrupt controller is ready */
	irq_unmask();

	while (1)
		;
	return 0;
}
