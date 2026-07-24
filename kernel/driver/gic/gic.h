#ifndef __GIC_H__
#define __GIC_H__

#include "types.h"
#include "irq.h"

/*
 * Common GIC public API.  The actual implementation (GICv2 or GICv3) is
 * selected at runtime by whichever driver successfully probes the interrupt
 * controller.
 */

struct gic_ops {
	int (*irq_enable)(unsigned int irq);
	int (*irq_disable)(unsigned int irq);
	int (*irq_set_priority)(unsigned int irq, uint8_t prio);
	int (*irq_set_type)(unsigned int irq, unsigned int type);
	int (*irq_set_group)(unsigned int irq, unsigned int group);
	int (*request_irq)(unsigned int irq, irq_handler_t handler, void *dev_id);
	void (*handle_irq)(void);
};

void gic_register_ops(const struct gic_ops *ops);

int gic_irq_enable(unsigned int irq);
int gic_irq_disable(unsigned int irq);
int gic_irq_set_priority(unsigned int irq, uint8_t prio);
int gic_irq_set_type(unsigned int irq, unsigned int type);
int gic_irq_set_group(unsigned int irq, unsigned int group);
int gic_request_irq(unsigned int irq, irq_handler_t handler, void *dev_id);
void gic_handle_irq(void);

#endif /* __GIC_H__ */
