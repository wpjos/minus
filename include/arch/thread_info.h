#ifndef __THREAD_INFO_H__
#define __THREAD_INFO_H__

#include "types.h"

/*
 * Architecture thread context, owned by core.
 *
 * This is the execution context the architecture switcher (core/arch/
 * switch.S) saves and restores.  It lives in core because it is below
 * the task abstraction: core switches thread contexts, and a kthread
 * embeds one at offset 0.  switch.S hard-codes offsets into this
 * layout; keep them in sync.
 */
struct fpsimd_state {
	__uint128_t v[32];
	uint32_t fpsr;
	uint32_t fpcr;
};

struct thread_info {
	uint64_t x19;
	uint64_t x20;
	uint64_t x21;
	uint64_t x22;
	uint64_t x23;
	uint64_t x24;
	uint64_t x25;
	uint64_t x26;
	uint64_t x27;
	uint64_t x28;
	uint64_t x29;
	uint64_t lr;		/* x30 */
	uint64_t sp;		/* SP_EL1 at switch point */
	struct fpsimd_state fpstate;	/* FP/SIMD context */
};

#endif /* __THREAD_INFO_H__ */
