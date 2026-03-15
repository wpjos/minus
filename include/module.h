#ifndef __MODULE_H__
#define __MODULE_H__

typedef void (*init_call_t)(void);

#define module_register(name, fn)						\
	static init_call_t __init_call_##name 					\
		__attribute__((__used__, __section__(".module.init"))) = fn	\

void module_init(void);

#endif
