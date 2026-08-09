#ifndef __SYSIF_PROC_H__
#define __SYSIF_PROC_H__

long sys_execve(const char *filename, char *const argv[],
		char *const envp[]);
long sys_getpid(void);
long sys_exit(int code);

#endif
