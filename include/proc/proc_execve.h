#ifndef __PROC_EXECVE_H__
#define __PROC_EXECVE_H__

#include "types.h"

int proc_execve(const char *filename, char *const argv[], char *const envp[]);
int proc_spawn(const char *filename, char *const argv[], char *const envp[]);

#endif /* __PROC_EXECVE_H__ */
