#include "ipc.h"
#include "errno.h"

/*
 * Local IPC dispatcher for the monolithic build.
 *
 * In the monolithic kernel cross-subsystem calls go through the service ops
 * tables directly (via the *_call() macros), so this function is not currently
 * used.  The stub is provided so that future micro-kernel client code can call
 * ipc_call() and get a clear "not implemented" error until the real IPC layer
 * is wired in.
 */
long ipc_call(struct ipc_msg *msg)
{
	(void)msg;
	return -ENOSYS;
}
