#include "entry-common.h"

.section ".text", "ax"
.global ret_to_user
ret_to_user:
	restore_all

.section ".vector.init", "ax"
.align 11
.global vector_table
vector_table:

.macro ventry label
	b \label
.endm

/* Current EL with SP0 - unexpected, kernel uses SP_EL1 */
	.org 0x000
	ventry bad_entry
	.org 0x080
	ventry bad_entry
	.org 0x100
	ventry bad_entry
	.org 0x180
	ventry bad_entry

/* Current EL with SPx */
	.org 0x200
	ventry el1_sync_entry
	.org 0x280
	ventry el1_irq_entry
	.org 0x300
	ventry el1_fiq_entry
	.org 0x380
	ventry el1_error_entry

/* Lower EL using AArch64 */
	.org 0x400
	ventry el0_sync_entry
	.org 0x480
	ventry el0_irq_entry
	.org 0x500
	ventry el0_fiq_entry
	.org 0x580
	ventry el0_error_entry

/* Lower EL using AArch32 - not supported */
	.org 0x600
	ventry bad_entry
	.org 0x680
	ventry bad_entry
	.org 0x700
	ventry bad_entry
	.org 0x780
	ventry bad_entry

/*
 * Generate an exception vector entry stub:
 *   save_all, call the C handler with pt_regs, restore_all.
 */
.macro vector_entry name, handler
.global \name
\name:
	save_all
	mov	x0, sp
	bl	\handler
	restore_all
.endm

/*
 * Bad / unexpected exception entry. Should never happen for categories
 * 1 (Current EL, SP0) and 4 (Lower EL, AArch32) in this configuration.
 */
.global bad_entry
bad_entry:
	save_all
	mov	x0, sp
	bl	bad_mode
	b	.

	vector_entry el1_sync_entry, el1_sync
	vector_entry el1_irq_entry, el1_irq
	vector_entry el1_fiq_entry, el1_fiq
	vector_entry el1_error_entry, el1_error

	vector_entry el0_sync_entry, el0_sync
	vector_entry el0_irq_entry, el0_irq
	vector_entry el0_fiq_entry, el0_fiq
	vector_entry el0_error_entry, el0_error
