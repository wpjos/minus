#ifndef __UACCESS_H__
#define __UACCESS_H__

#include "types.h"

/*
 * Copy @n bytes from userspace address @from to kernel address @to.
 * Returns 0 on success, or the number of bytes left uncopied on fault.
 */
long copy_from_user(void *to, const void *from, size_t n);

/*
 * Copy @n bytes from kernel address @from to userspace address @to.
 * Returns 0 on success, or the number of bytes left uncopied on fault.
 */
long copy_to_user(void *to, const void *from, size_t n);

/*
 * Copy a null-terminated string of at most @n bytes (including terminator)
 * from userspace @src to kernel @dst. Returns the number of bytes copied
 * (including the terminator) on success, or the number of uncopied bytes
 * remaining on fault. A terminating NUL is always stored if @n > 0.
 */
long strncpy_from_user(char *dst, const char *src, size_t n);

#endif /* __UACCESS_H__ */
