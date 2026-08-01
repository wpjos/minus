.section ".text", "ax"

#include "entry-common.h"

/*
 * Offsets into struct thread_info (must match include/kernel/task.h).
 * General registers occupy the first 96 bytes, then fpstate.
 */
#define TI_X19		0
#define TI_X29		80
#define TI_LR		88
#define TI_SP		96
#define TI_FPSTATE	112
#define TI_FPSR		(TI_FPSTATE + 32 * 16)
#define TI_FPCR		(TI_FPSR + 4)

/*
 * struct task_struct *__switch_to(struct task_struct *prev,
 *                                 struct task_struct *next)
 * x0 = prev, x1 = next
 * struct thread_info is at offset 0 of task_struct.
 */
.global __switch_to
__switch_to:
	/* Save prev callee-saved general registers. */
	stp	x19, x20, [x0, #TI_X19]
	stp	x21, x22, [x0, #(TI_X19 + 16)]
	stp	x23, x24, [x0, #(TI_X19 + 32)]
	stp	x25, x26, [x0, #(TI_X19 + 48)]
	stp	x27, x28, [x0, #(TI_X19 + 64)]
	stp	x29, x30, [x0, #TI_X29]
	mov	x2, sp
	str	x2, [x0, #TI_SP]

	/* Save prev FP/SIMD context. */
	stp	q0,  q1,  [x0, #(TI_FPSTATE + 0 * 16)]
	stp	q2,  q3,  [x0, #(TI_FPSTATE + 2 * 16)]
	stp	q4,  q5,  [x0, #(TI_FPSTATE + 4 * 16)]
	stp	q6,  q7,  [x0, #(TI_FPSTATE + 6 * 16)]
	stp	q8,  q9,  [x0, #(TI_FPSTATE + 8 * 16)]
	stp	q10, q11, [x0, #(TI_FPSTATE + 10 * 16)]
	stp	q12, q13, [x0, #(TI_FPSTATE + 12 * 16)]
	stp	q14, q15, [x0, #(TI_FPSTATE + 14 * 16)]
	stp	q16, q17, [x0, #(TI_FPSTATE + 16 * 16)]
	stp	q18, q19, [x0, #(TI_FPSTATE + 18 * 16)]
	stp	q20, q21, [x0, #(TI_FPSTATE + 20 * 16)]
	stp	q22, q23, [x0, #(TI_FPSTATE + 22 * 16)]
	stp	q24, q25, [x0, #(TI_FPSTATE + 24 * 16)]
	stp	q26, q27, [x0, #(TI_FPSTATE + 26 * 16)]
	stp	q28, q29, [x0, #(TI_FPSTATE + 28 * 16)]
	stp	q30, q31, [x0, #(TI_FPSTATE + 30 * 16)]
	mrs	x2, fpsr
	str	w2, [x0, #TI_FPSR]
	mrs	x2, fpcr
	str	w2, [x0, #TI_FPCR]

	/*
	 * Keep prev in a caller-saved register across next's restore.
	 * switch_to() returns prev in x0 for finish_task_switch().
	 */
	mov	x16, x0

	/* Switch to next's kernel stack first. */
	ldr	x2, [x1, #TI_SP]
	mov	sp, x2

	/* Restore next FP/SIMD context (does not touch x1). */
	ldp	q0,  q1,  [x1, #(TI_FPSTATE + 0 * 16)]
	ldp	q2,  q3,  [x1, #(TI_FPSTATE + 2 * 16)]
	ldp	q4,  q5,  [x1, #(TI_FPSTATE + 4 * 16)]
	ldp	q6,  q7,  [x1, #(TI_FPSTATE + 6 * 16)]
	ldp	q8,  q9,  [x1, #(TI_FPSTATE + 8 * 16)]
	ldp	q10, q11, [x1, #(TI_FPSTATE + 10 * 16)]
	ldp	q12, q13, [x1, #(TI_FPSTATE + 12 * 16)]
	ldp	q14, q15, [x1, #(TI_FPSTATE + 14 * 16)]
	ldp	q16, q17, [x1, #(TI_FPSTATE + 16 * 16)]
	ldp	q18, q19, [x1, #(TI_FPSTATE + 18 * 16)]
	ldp	q20, q21, [x1, #(TI_FPSTATE + 20 * 16)]
	ldp	q22, q23, [x1, #(TI_FPSTATE + 22 * 16)]
	ldp	q24, q25, [x1, #(TI_FPSTATE + 24 * 16)]
	ldp	q26, q27, [x1, #(TI_FPSTATE + 26 * 16)]
	ldp	q28, q29, [x1, #(TI_FPSTATE + 28 * 16)]
	ldp	q30, q31, [x1, #(TI_FPSTATE + 30 * 16)]
	ldr	w2, [x1, #TI_FPSR]
	msr	fpsr, x2
	ldr	w2, [x1, #TI_FPCR]
	msr	fpcr, x2

	/*
	 * Publish current = next while x1 still holds next.  After restoring
	 * next's general registers x1 will be clobbered with next's saved x1.
	 */
	ldr	x2, =current
	str	x1, [x2]

	/* Restore next callee-saved general registers. */
	ldp	x19, x20, [x1, #TI_X19]
	ldp	x21, x22, [x1, #(TI_X19 + 16)]
	ldp	x23, x24, [x1, #(TI_X19 + 32)]
	ldp	x25, x26, [x1, #(TI_X19 + 48)]
	ldp	x27, x28, [x1, #(TI_X19 + 64)]
	ldp	x29, x30, [x1, #TI_X29]

	/* Return prev in x0. */
	mov	x0, x16
	ret
