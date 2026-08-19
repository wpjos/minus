#ifndef __MODULE_H__
#define __MODULE_H__

/*
 * Driver / bus / controller init framework.
 *
 * This is intentionally separate from subsys_init: module_register() must only
 * be used for hardware drivers and controller bring-up.  Core kernel
 * subsystems (mm, irq, sched, proc, fs) and filesystem types are initialized
 * through subsys_register() and explicit fs_subsys_init() calls, not here.
 */

#define MODULE_LEVEL_CORE	1
#define MODULE_LEVEL_HIGH	2
#define MODULE_LEVEL_MID	3
#define MODULE_LEVEL_LOW	4

typedef void (*init_call_t)(void);

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#define module_register(name, level, fn)						\
	static init_call_t __init_call_##name 					\
		__attribute__((__used__, __section__(".module_l" STR(level) ".init"))) = fn	\

void module_init(void);

#endif
