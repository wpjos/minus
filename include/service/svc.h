#ifndef __SVC_H__
#define __SVC_H__

#include "types.h"

/*
 * Simple service registry for subsystem capability tables.
 *
 * Each major subsystem (mm, fs, proc, sched) registers a const ops table
 * during its subsys_init().  Other subsystems obtain the table via
 * svc_lookup() and call only through those function pointers.
 *
 * This keeps subsystem boundaries explicit and makes it possible to swap
 * a local implementation for an IPC stub when experimenting with micro-kernel
 * style decomposition.
 */

#define SVC_MAX 16

typedef uint32_t svc_id_t;
#define SVC_ID_INVALID 0

struct svc_entry {
	svc_id_t	id;
	const char	*name;
	const void	*ops;
};

/* Register a service ops table.  Must be called after mm_init(). */
void svc_register(const char *name, const void *ops);

/* Lookup a registered service by name.  Returns NULL if not found. */
const void *svc_lookup(const char *name);

/* Return the integer service id for a registered name, or SVC_ID_INVALID. */
svc_id_t svc_id(const char *name);

#endif /* __SVC_H__ */
