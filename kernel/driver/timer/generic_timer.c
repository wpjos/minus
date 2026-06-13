#include "platform.h"
#include "module.h"
#include "types.h"
#include "printk.h"
#include "generic_timer.h"

static const struct of_device_id generic_timer_of_match[] = {
	{ .compatible = "arm,armv8-timer" },
	{ /* sentinel */ }
};

static int generic_timer_probe(struct platform_device *pdev)
{
	generic_timer_set_cntkctl(EL0PCTEN | EL0VCTEN);
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
