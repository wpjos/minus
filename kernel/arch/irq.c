#include "irq.h"
#include "gic.h"
#include "subsys.h"
#include "printk.h"

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

static int irq_subsys_init(void)
{
	irq_init();
	return 0;
}

static const struct subsys_ops irq_subsys_ops = {
	.name = "irq",
	.level = SUBSYS_LEVEL_IRQ,
	.init = irq_subsys_init,
};
subsys_register(irq, &irq_subsys_ops);
