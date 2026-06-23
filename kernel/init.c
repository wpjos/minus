#include "module.h"
#include "printk.h"
#include "mm.h"
#include "buddy.h"
#include "slab.h"
#include "irq.h"

const char logo[] = "hello minus!!!\n";

int start_kernel(void)
{
	early_mm_init();
	buddy_init();
	paging_init();
	slab_init();
	/* Install exception vectors before any driver may trigger a fault */
	irq_init();

	/* Register all drivers (triggers match & probe) */
	module_init();

	printk("%s\n", &logo[0]);


	/* Enable interrupts once the interrupt controller is ready */
	irq_unmask();

	while(1);
	return 0;
}
