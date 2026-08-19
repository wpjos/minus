#ifndef __SUBSYS_H__
#define __SUBSYS_H__

/*
 * Generic subsystem descriptor.
 *
 * Each major subsystem (mm, irq, sched, proc, fs) registers a static
 * descriptor in the .subsys.init linker section.  start_kernel() calls
 * subsys_init_all() once, which walks the table in ascending level order.
 *
 * Boundaries:
 *   - subsys: core kernel subsystems that must be ready before drivers.
 *   - module: drivers / buses / controllers only (see driver/module.h).
 */

struct subsys_ops {
	const char *name;
	int level;
	int (*init)(void);
};

/* Relative init ordering.  Large gaps make future insertion easy. */
#define SUBSYS_LEVEL_MM		10
#define SUBSYS_LEVEL_IRQ	20
#define SUBSYS_LEVEL_SCHED	30
#define SUBSYS_LEVEL_PROC	40
#define SUBSYS_LEVEL_SYSCALL	45
#define SUBSYS_LEVEL_FS		50
#define SUBSYS_LEVEL_BLOCK	55
#define SUBSYS_LEVEL_FS_BACKEND	60

#define subsys_register(sname, _level, _init_fn)				\
	static const struct subsys_ops __subsys_ops_##sname			\
		__attribute__((__used__, __section__(".subsys.init"))) = {	\
			.name = #sname,						\
			.level = (_level),						\
			.init = (_init_fn),						\
		}

/* Initialize all registered subsystems in level order. */
int subsys_init_all(void);

#endif /* __SUBSYS_H__ */
