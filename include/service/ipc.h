#ifndef __IPC_H__
#define __IPC_H__

#include "types.h"
#include "cap.h"

/*
 * Generic IPC message.
 *
 * This structure is the universal currency for cross-subsystem calls.  In the
 * monolithic build ipc_call() is a direct function dispatch; in a micro-kernel
 * build it becomes a trap to the kernel IPC layer.
 *
 * The argument layout matches the AArch64 syscall calling convention: up to six
 * integer/pointer arguments and up to four capabilities.
 */
#define IPC_ARGS_MAX 6
#define IPC_CAPS_MAX 4

struct ipc_msg {
	uint32_t dst;		/* destination service id */
	uint32_t opcode;	/* method opcode within service */
	uint64_t args[IPC_ARGS_MAX];
	cap_t    cap_in[IPC_CAPS_MAX];
	cap_t    cap_out[IPC_CAPS_MAX];
	long     result;
};

/*
 * Synchronous IPC call.  Fills msg->result and optionally msg->cap_out.
 * Returns 0 on success, negative errno on failure.
 */
long ipc_call(struct ipc_msg *msg);

#endif /* __IPC_H__ */
