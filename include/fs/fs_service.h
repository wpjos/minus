#ifndef __FS_SERVICE_H__
#define __FS_SERVICE_H__

#include "types.h"
#include "cap.h"

/*
 * FS subsystem service interface.
 *
 * Cross-module boundary for the filesystem.  This table holds exactly the
 * operations other modules genuinely need, addressed by capability:
 *
 *   - proc's task lifecycle (files_create/files_destroy/setup_std_fds)
 *     targets a task that is NOT running yet, so the per-CPU fdtable
 *     mirror cannot express it - hence explicit cap addressing.
 *   - init's rootfs mount and proc's ELF loading are context-free
 *     operations (no fdtable involved at all).
 *
 * What is deliberately NOT here: current-task file operations
 * (open/read/write/mmap/stat/mkdir/...).  A syscall always acts on behalf
 * of the calling task, so those live in fdtable.c (fd_*) / sysif/, using
 * the per-CPU mirror - passing a cap would address a context the syscall
 * layer can never legally name.
 *
 * All pointers are KERNEL pointers.  The syscall layer (sysif/) bounces
 * user buffers/paths into kernel memory before calling down, so this
 * table never needs to know about user address spaces.
 *
 * Capability ownership: files_create returns a cap owned by the caller;
 * files_destroy consumes it (including cap_free).
 */
struct fs_service {
	/*
	 * Per-task file descriptor table lifecycle.  The fdtable object is
	 * owned by FS; proc creates/destroys it through these hooks.
	 */
	cap_t (*files_create)(void);
	void  (*files_destroy)(cap_t files_cap);

	/*
	 * Install stdin/stdout/stderr for a new task.  Opens /dev/console and
	 * binds it to fds 0-2 of @files_cap in one shot.
	 */
	long (*setup_std_fds)(cap_t files_cap);

	/*
	 * Mount the root filesystem and complete the initial namespace
	 * (currently /dev + devfs).  Namespace setup is FS's own business;
	 * init only decides which device/fstype to try.
	 */
	long (*mount_root)(const char *source, const char *fstype);

	/*
	 * Read a whole file into a freshly kmalloc'ed buffer (caller frees
	 * with kfree).  Context-free: no fdtable, no fd - used by proc's
	 * ELF loader, which may run in a task with no fdtable at all.
	 */
	long (*load_file)(const char *path, void **buf, size_t *size);
};

const struct fs_service *fs_service(void);

/* Convenience wrapper: fs_call(fn, args...) -> fs_service()->fn(args...). */
#define fs_call(fn, ...) fs_service()->fn(__VA_ARGS__)

#endif /* __FS_SERVICE_H__ */
