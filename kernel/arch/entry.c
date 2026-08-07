#include "pt_regs.h"
#include "irq.h"
#include "printk.h"
#include "syscall.h"

#define ESR_EL1_EC_SHIFT	26
#define ESR_EL1_EC_MASK		0x3f
#define ESR_EL1_EC_SVC64	0x15

static inline uint64_t esr_ec(uint64_t esr)
{
	return (esr >> ESR_EL1_EC_SHIFT) & ESR_EL1_EC_MASK;
}

void bad_mode(struct pt_regs *regs)
{
	printk("bad_mode: unexpected exception elr=%p spsr=%p esr=%p\n",
	       regs->elr, regs->spsr, regs->esr);
	while (1)
		;
}

void el0_sync(struct pt_regs *regs)
{
	uint64_t ec = esr_ec(regs->esr);

	if (ec == ESR_EL1_EC_SVC64) {
		do_syscall(regs);
		return;
	}

	printk("el0_sync: unhandled exception esr=%p elr=%p\n",
	       (void *)regs->esr, (void *)regs->elr);
	while (1)
		;
}

void el0_irq(struct pt_regs *regs)
{
	(void)regs;
	gic_handle_irq();
}

void el0_fiq(struct pt_regs *regs)
{
	(void)regs;
	printk("el0_fiq: unexpected\n");
	while (1)
		;
}

void el0_error(struct pt_regs *regs)
{
	(void)regs;
	printk("el0_error: unexpected\n");
	while (1)
		;
}

void el1_sync(struct pt_regs *regs)
{
	printk("el1_sync: esr=%p elr=%p\n", regs->esr, regs->elr);
	while (1)
		;
}

void el1_irq(struct pt_regs *regs)
{
	(void)regs;
	gic_handle_irq();
}

void el1_fiq(struct pt_regs *regs)
{
	(void)regs;
	printk("el1_fiq: unexpected\n");
	while (1)
		;
}

void el1_error(struct pt_regs *regs)
{
	(void)regs;
	printk("el1_error: unexpected\n");
	while (1)
		;
}
