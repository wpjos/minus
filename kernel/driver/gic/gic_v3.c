#include "gic_v3.h"

static int gic_probe(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id gic_of_match[] = {
	{ .compatible = "arm,gic-v3" },
	{ /* sentinel */ }
};

static struct platform_driver gic_driver = {
	.drv = { .name = "gic_v3" },
	.probe = gic_probe,
	.remove = NULL,
	.of_match_table = gic_of_match,
};

static void gic_init(void)
{
	platform_driver_register(&gic_driver);
}
module_register(gic, MODULE_LEVEL_HIGH, gic_init);
