#include "platform.h"
#include "module.h"
#include "types.h"
#include "printk.h"
#include "irq.h"
#include "generic_timer.h"

/* Tick interval in milliseconds */
#define TICK_MS		10

static unsigned int g_timer_irq;
static uint64_t g_tick_count;

static int generic_timer_tick_handler(unsigned int irq, void *dev_id)
{
	(void)irq;
	(void)dev_id;

	/* Reload timer to keep ticking */
	generic_timer_set_cntptval((uint64_t)(generic_timer_get_cntfrq() / 1000 * TICK_MS));

	g_tick_count++;
	if ((g_tick_count % 100) == 0)
		printk("tick: %d\n", (int)g_tick_count);

	return 0;
}

static const struct of_device_id generic_timer_of_match[] = {
	{ .compatible = "arm,armv8-timer" },
	{ /* sentinel */ }
};

static int generic_timer_probe(struct platform_device *pdev)
{
	uint64_t freq;
	uint64_t interval;

	/* Enable EL0 access to the physical and virtual counters */
	generic_timer_set_cntkctl(EL0PCTEN | EL0VCTEN);

	freq = generic_timer_get_cntfrq();
	if (freq == 0) {
		printk("generic_timer: CNTFRQ is zero\n");
		return -1;
	}

	/* Use the non-secure EL1 physical timer PPI (second interrupt cell) */
	g_timer_irq = (unsigned int)platform_get_irq(pdev, 1);
	if (g_timer_irq == 0) {
		printk("generic_timer: no IRQ resource\n");
		return -1;
	}

	if (request_irq(g_timer_irq, generic_timer_tick_handler, NULL) != 0) {
		printk("generic_timer: failed to register IRQ %u\n", g_timer_irq);
		return -1;
	}

	/* Calculate tick count for TICK_MS */
	interval = freq / 1000 * TICK_MS;

	/* Set initial timer value and enable the physical timer, unmasked */
	generic_timer_set_cntptval(interval);
	generic_timer_set_cntpctl(ENABLE);

	if (enable_irq(g_timer_irq) != 0) {
		printk("generic_timer: failed to enable IRQ %u\n", g_timer_irq);
		return -1;
	}

	printk("generic_timer: freq=%d Hz, irq=%u, interval=%d\n",
	       (int)freq, g_timer_irq, (int)interval);
	return 0;
}

static struct platform_driver generic_timer_driver = {
	.drv = { .name = "armv8-timer" },
	.probe = generic_timer_probe,
	.remove = NULL,
	.of_match_table = generic_timer_of_match,
};

static void generic_timer_init(void)
{
	platform_driver_register(&generic_timer_driver);
}
module_register(generic_timer, MODULE_LEVEL_MID, generic_timer_init);
