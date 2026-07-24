#include "gic.h"

static const struct gic_ops *g_active_ops;

void gic_register_ops(const struct gic_ops *ops)
{
	g_active_ops = ops;
}

int gic_irq_enable(unsigned int irq)
{
	if (g_active_ops && g_active_ops->irq_enable)
		return g_active_ops->irq_enable(irq);
	return -1;
}

int gic_irq_disable(unsigned int irq)
{
	if (g_active_ops && g_active_ops->irq_disable)
		return g_active_ops->irq_disable(irq);
	return -1;
}

int gic_irq_set_priority(unsigned int irq, uint8_t prio)
{
	if (g_active_ops && g_active_ops->irq_set_priority)
		return g_active_ops->irq_set_priority(irq, prio);
	return -1;
}

int gic_irq_set_type(unsigned int irq, unsigned int type)
{
	if (g_active_ops && g_active_ops->irq_set_type)
		return g_active_ops->irq_set_type(irq, type);
	return -1;
}

int gic_irq_set_group(unsigned int irq, unsigned int group)
{
	if (g_active_ops && g_active_ops->irq_set_group)
		return g_active_ops->irq_set_group(irq, group);
	return -1;
}

int gic_request_irq(unsigned int irq, irq_handler_t handler, void *dev_id)
{
	if (g_active_ops && g_active_ops->request_irq)
		return g_active_ops->request_irq(irq, handler, dev_id);
	return -1;
}

void gic_handle_irq(void)
{
	if (g_active_ops && g_active_ops->handle_irq)
		g_active_ops->handle_irq();
}
