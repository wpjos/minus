#ifndef __CAP_H__
#define __CAP_H__

#include "types.h"

/*
 * Capability handle.
 *
 * A capability is an opaque 64-bit token that grants a subset of rights on a
 * kernel object.  In the monolithic build it is implemented as an index into a
 * global capability table; the table stores the raw object pointer and a rights
 * mask.  In a micro-kernel build the same token is used by the IPC layer to
 * identify an object in a remote server.
 *
 * Layout (monolithic):
 *   bits [31:0]   - index into the global cap table
 *   bits [63:32]  - generation number, incremented on free/reuse
 *
 * Ownership convention: a cap obtained from a service call is consumed by
 * the matching close/destroy operation of that same service (which performs
 * the cap_free).  Callers must NOT cap_free such caps themselves.  The only
 * caps a caller frees with cap_free() are caps it allocated itself via
 * cap_alloc().
 */
typedef uint64_t cap_t;

#define CAP_NULL 0ULL

/* Capability rights. */
struct cap_rights {
	uint64_t rights;
};

#define CAP_R_READ  (1ULL << 0)
#define CAP_R_WRITE (1ULL << 1)
#define CAP_R_EXEC  (1ULL << 2)
#define CAP_R_MAP   (1ULL << 3)
#define CAP_R_CTL   (1ULL << 4)

/* Convenience constructors. */
static inline struct cap_rights cap_rights(uint64_t r)
{
	struct cap_rights cr = { r };
	return cr;
}

static inline struct cap_rights cap_rights_all(void)
{
	return cap_rights(~0ULL);
}

static inline struct cap_rights cap_rights_none(void)
{
	return cap_rights(0);
}

/*
 * Allocate a capability for @obj with the given rights.
 * Returns CAP_NULL on failure.
 */
cap_t cap_alloc(void *obj, struct cap_rights rights);

/*
 * Resolve @cap to the underlying object if the required rights are granted.
 * Returns NULL if the capability is invalid or rights are insufficient.
 */
void *cap_resolve(cap_t cap, struct cap_rights required);

/*
 * Free a capability.  After this call the token becomes invalid; future
 * cap_resolve() on the same value will return NULL (unless the slot is reused
 * with a new generation).
 */
void cap_free(cap_t cap);

#endif /* __CAP_H__ */
