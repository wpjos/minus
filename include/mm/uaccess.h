#ifndef __UACCESS_H__
#define __UACCESS_H__

#include "types.h"
#include "mm_types.h"
#include "cap.h"

/*
 * Copy @n bytes from userspace address @from to kernel address @to.
 * Returns 0 on success, or the number of bytes left uncopied on fault.
 */
long copy_from_user(void *to, user_addr_t from, size_t n);

/*
 * Copy @n bytes from kernel address @from to userspace address @to.
 * Returns 0 on success, or the number of bytes left uncopied on fault.
 */
long copy_to_user(user_addr_t to, const void *from, size_t n);

/*
 * Copy a null-terminated string of at most @n bytes (including terminator)
 * from userspace @src to kernel @dst. Returns the number of bytes copied
 * (including the terminator) on success, or the number of uncopied bytes
 * remaining on fault. A terminating NUL is always stored if @n > 0.
 */
long strncpy_from_user(char *dst, user_addr_t src, size_t n);

/*
 * Current task's address space, mirrored per-CPU through core's switch
 * notification bus (uaccess_init() subscribes at mm bring-up).  The
 * pointer form is the once-resolved vspace used by the copy paths; the
 * cap form is for mm_call()-style consumers that must stay on the
 * capability boundary (e.g. fb0's mmap).  Both are NULL / CAP_NULL in
 * early-boot context.
 */
struct vspace;
struct vspace *mm_current_vspace(void);
cap_t mm_current_vspace_cap(void);
int uaccess_init(void);

#endif /* __UACCESS_H__ */
