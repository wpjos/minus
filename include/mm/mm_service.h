#ifndef __MM_SERVICE_H__
#define __MM_SERVICE_H__

#include "types.h"
#include "cap.h"
#include "mm_types.h"

/*
 * MM subsystem service interface.
 *
 * Other subsystems obtain this table via mm_service() and call only through
 * these pointers.  In a micro-kernel style build the pointers can be replaced
 * by stubs that IPC to a memory manager server.
 *
 * Only cross-address-space contracts are exposed here: user buffer access and
 * vspace management (cap in, scalars out).  The kernel heap allocator
 * (kmalloc/kzalloc/kfree in mm.h) is NOT a service: every subsystem, including
 * future micro-kernel servers, links it locally.
 */
struct mm_service {
	long (*copy_from_user)(void *to, user_addr_t from, size_t n);
	long (*copy_to_user)(user_addr_t to, const void *from, size_t n);
	long (*strncpy_from_user)(char *dst, user_addr_t src, size_t n);

	int (*map_user_pages)(cap_t vspace_cap, uint64_t phys, size_t size,
			      uint32_t flags, uint64_t *uva);

	/*
	 * Back one user page at @uva (page aligned) with a freshly
	 * allocated zeroed page and hand the caller its kernel alias to
	 * copy into.  The ELF loader builds images through this - it never
	 * touches mm internals (pages, page tables) directly.  On failure
	 * the caller destroys the vspace to reclaim partial mappings.
	 */
	long (*vspace_map_page)(cap_t vspace_cap, uint64_t uva,
				uint32_t flags, void **kva);

	cap_t (*vspace_create)(void);
	void (*vspace_destroy)(cap_t vspace_cap);
};

/* Returns the registered MM service table (never NULL after mm_init). */
const struct mm_service *mm_service(void);

/* Convenience wrapper: mm_call(fn, args...) -> mm_service()->fn(args...). */
#define mm_call(fn, ...) mm_service()->fn(__VA_ARGS__)

#endif /* __MM_SERVICE_H__ */
