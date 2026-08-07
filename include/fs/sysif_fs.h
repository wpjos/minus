#ifndef __SYSIF_FS__
#define __SYSIF_FS__

#include "types.h"
#include "stat.h"

long sys_openat(int dirfd, const char *pathname, int flags, uint16_t mode);
long sys_close(unsigned int fd);
long sys_read(unsigned int fd, char *buf, size_t count);
long sys_write(unsigned int fd, const char *buf, size_t count);
long sys_lseek(unsigned int fd, long offset, int whence);
long sys_newfstatat(int dirfd, const char *pathname, struct stat *statbuf,
		    int flags);
long sys_fstat(unsigned int fd, struct stat *statbuf);
long sys_unlinkat(int dirfd, const char *pathname, int flags);
long sys_mkdirat(int dirfd, const char *pathname, uint16_t mode);
long sys_getdents64(unsigned int fd, char *buf, unsigned int count);

#endif /* __FS_CALL_H__ */
