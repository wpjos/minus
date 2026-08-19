#ifndef __MM_TYPES_H__
#define __MM_TYPES_H__

#include "types.h"

/*
 * Public MM types.
 *
 * These are the only MM-related types visible outside the MM subsystem.
 * All MM objects (vspace, etc.) are referenced by cap_t handles.
 */

typedef uint64_t user_addr_t;

/* VM region flags used by map_user_pages and mmap. */
#define VM_READ  (1U << 0)
#define VM_WRITE (1U << 1)
#define VM_EXEC  (1U << 2)
#define VM_DEVICE (1U << 3)

#endif /* __MM_TYPES_H__ */
