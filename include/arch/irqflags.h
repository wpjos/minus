#ifndef __IRQFLAGS_H__
#define __IRQFLAGS_H__

#include "types.h"

/*
 * Local IRQ save/restore for single-core EL1 code.
 * These read and write the full DAIF register so all mask bits are preserved.
 * #2 selects the IRQ mask (PSTATE.I) in the DAIFSet/DAIFClr immediate.
 */

#define local_irq_save(flags)					\
	__asm__ __volatile__(					\
		"mrs\t%0, daif\n"				\
		"msr\tdaifset, #2"				\
		: "=r" (flags)					\
		:						\
		: "memory")

#define local_irq_restore(flags)				\
	__asm__ __volatile__(					\
		"msr\tdaif, %0"					\
		:						\
		: "r" (flags)					\
		: "memory")

#define local_irq_disable()					\
	__asm__ __volatile__("msr\tdaifset, #2" ::: "memory")

#define local_irq_enable()					\
	__asm__ __volatile__("msr\tdaifclr, #2" ::: "memory")

#endif /* __IRQFLAGS_H__ */
