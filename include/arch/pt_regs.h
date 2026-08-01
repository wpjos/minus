#ifndef __PT_REGS_H__
#define __PT_REGS_H__

#include "types.h"

/*
 * Saved exception frame layout.
 * Must match the offsets defined in kernel/arch/vectors.S.
 */
struct pt_regs {
	uint64_t x[31];
	uint64_t elr;
	uint64_t spsr;
	uint64_t esr;
	uint64_t sp_el0;
};

#endif /* __PT_REGS_H__ */
