#include "irq.h"
#include "gic.h"

/* Defined in vectors.S */
extern char vector_table[];

void irq_init(void)
{
	/* Install EL1 vector table as early as possible */
	__asm__ volatile("msr vbar_el1, %0" : : "r"(vector_table));
}

void irq_unmask(void)
{
	/* Unmask IRQ (clear I bit in DAIF) */
	__asm__ volatile("msr daifclr, #2");
}

int request_irq(unsigned int irq, irq_handler_t handler, void *dev_id)
{
	return gic_request_irq(irq, handler, dev_id);
}

int enable_irq(unsigned int irq)
{
	return gic_irq_enable(irq);
}

int disable_irq(unsigned int irq)
{
	return gic_irq_disable(irq);
}
