#ifndef __IRQ_H__
#define __IRQ_H__

#include "types.h"

/*
 * IRQ handler prototype.
 * @irq:    interrupt number
 * @dev_id: driver private data passed at registration time
 */
typedef int (*irq_handler_t)(unsigned int irq, void *dev_id);

/*
 * Register a handler for a given IRQ.
 * Returns 0 on success, -1 on error.
 */
int request_irq(unsigned int irq, irq_handler_t handler, void *dev_id);

/*
 * Enable/disable a single IRQ at the interrupt controller.
 */
int enable_irq(unsigned int irq);
int disable_irq(unsigned int irq);

/*
 * Top-level GICv3 interrupt dispatch function.
 * Called by the architecture-specific IRQ exception handler.
 */
void gic_handle_irq(void);

/*
 * Architecture-specific exception vector initialization.
 * Defined in kernel/arch/irq.c
 */
void irq_init(void);
void irq_unmask(void);

#endif /* __IRQ_H__ */
