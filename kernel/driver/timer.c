#include "platform.h"
#include "module.h"
#include "types.h"
#include "printk.h"

static void timer_init(void)
{
	printk("timer_init");
}
module_register(timer, MODULE_LEVEL_MID, timer_init);
