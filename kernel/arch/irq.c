#include "irq.h"
#include "gic_v3.h"
#include "printk.h"
#include "types.h"

/* Defined in vectors.S */
extern char vector_table[];

/*
 * Saved exception frame layout, must match vectors.S
 */
struct pt_regs {
	uint64_t x[31];
	uint64_t elr;
	uint64_t spsr;
	uint64_t esr;
};

void handle_irq_exception(struct pt_regs *regs)
{
	(void)regs;
	gic_handle_irq();
}

void handle_fiq_exception(struct pt_regs *regs)
{
	(void)regs;
	printk("fiq: unexpected FIQ\n");
}

void handle_bad_exception(struct pt_regs *regs)
{
	printk("fatal exception\n");
	printk("  elr=%p  spsr=%p  esr=%p\n",
	       (void *)regs->elr, (void *)regs->spsr, (void *)regs->esr);
	while (1)
		;
}

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
