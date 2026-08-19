#ifndef __SUBSYS_H__
#define __SUBSYS_H__

/*
 * Generic subsystem descriptor.
 *
 * Each major subsystem (mm, irq, sched, proc, fs) registers a static
 * descriptor in the .subsys.init linker section.  start_kernel() calls
 * subsys_init_all() once, which walks the table in ascending level order.
 */

struct subsys_ops {
	const char *name;
	int level;
	int (*init)(void);
};

struct subsys {
	const struct subsys_ops *ops;
};

/* Relative init ordering.  Large gaps make future insertion easy. */
#define SUBSYS_LEVEL_MM		10
#define SUBSYS_LEVEL_IRQ	20
#define SUBSYS_LEVEL_SCHED	30
#define SUBSYS_LEVEL_PROC	40
#define SUBSYS_LEVEL_FS		50

#define subsys_register(name, _ops)						\
	static struct subsys __subsys_##name					\
		__attribute__((__used__, __section__(".subsys.init"))) = {	\
			.ops = (_ops),						\
		}

/* Initialize all registered subsystems in level order. */
int subsys_init_all(void);

/* Lookup a registered subsystem by name (NULL if not found). */
struct subsys *subsys_find(const char *name);

#endif /* __SUBSYS_H__ */
