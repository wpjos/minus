#ifndef __GENERIC_TIMER_H
#define __GENERIC_TIMER_H

#include "sysreg.h"

//cntkctl
#define EL0PCTEN	(1ULL << 0)
#define EL0VCTEN	(1ULL << 1)

//cntpctl
#define ENABLE		(1ULL << 0)
#define IMASK		(1ULL << 1)

DEFINE_SYS_REG_RW_OPS(generic_timer, cntkctl, CNTKCTL_EL1);
DEFINE_SYS_REG_RW_OPS(generic_timer, cntpctl, CNTP_CTL_EL0);
DEFINE_SYS_REG_RW_OPS(generic_timer, cntpcval, CNTP_CVAL_EL0);
DEFINE_SYS_REG_RO_OPS(generic_timer, cntpct, CNTPCT_EL0);

#endif
