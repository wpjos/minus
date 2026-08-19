#ifndef __PROC_EXECVE_H__
#define __PROC_EXECVE_H__

/*
 * proc_execve.c internals, shared with nothing: spawn/execve build an
 * image (vspace, fdtable, loaded ELF, std fds) and then activate it as
 * a new thread (spawn) or as a replacement for the current one (exec).
 * The public entry points live in process.h.
 */

#endif /* __PROC_EXECVE_H__ */
