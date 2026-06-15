.section ".vector.init", "ax"
.align 11
.global vector_table
vector_table:

.macro ventry label
	b \label
.endm

/* Current EL with SP0 */
	.org 0x000
	ventry	bad_sync
	.org 0x080
	ventry	bad_irq
	.org 0x100
	ventry	bad_fiq
	.org 0x180
	ventry	bad_error

/* Current EL with SPx */
	.org 0x200
	ventry	bad_sync
	.org 0x280
	ventry	irq_dispatch
	.org 0x300
	ventry	fiq_dispatch
	.org 0x380
	ventry	bad_error

/* Lower EL using AArch64 */
	.org 0x400
	ventry	bad_sync
	.org 0x480
	ventry	bad_irq
	.org 0x500
	ventry	bad_fiq
	.org 0x580
	ventry	bad_error

/* Lower EL using AArch32 */
	.org 0x600
	ventry	bad_sync
	.org 0x680
	ventry	bad_irq
	.org 0x700
	ventry	bad_fiq
	.org 0x780
	ventry	bad_error

/* Size of the saved exception frame (34 x 8 bytes) */
#define S_FRAME_SIZE	272

/* Offsets within the frame */
#define S_X0		0
#define S_X1		8
#define S_X2		16
#define S_X3		24
#define S_X4		32
#define S_X5		40
#define S_X6		48
#define S_X7		56
#define S_X8		64
#define S_X9		72
#define S_X10		80
#define S_X11		88
#define S_X12		96
#define S_X13		104
#define S_X14		112
#define S_X15		120
#define S_X16		128
#define S_X17		136
#define S_X18		144
#define S_X19		152
#define S_X20		160
#define S_X21		168
#define S_X22		176
#define S_X23		184
#define S_X24		192
#define S_X25		200
#define S_X26		208
#define S_X27		216
#define S_X28		224
#define S_X29		232
#define S_LR		240
#define S_ELR		248
#define S_SPSR		256
#define S_ESR		264

.macro save_all
	sub	sp, sp, #S_FRAME_SIZE
	stp	x0, x1, [sp, #S_X0]
	stp	x2, x3, [sp, #S_X2]
	stp	x4, x5, [sp, #S_X4]
	stp	x6, x7, [sp, #S_X6]
	stp	x8, x9, [sp, #S_X8]
	stp	x10, x11, [sp, #S_X10]
	stp	x12, x13, [sp, #S_X12]
	stp	x14, x15, [sp, #S_X14]
	stp	x16, x17, [sp, #S_X16]
	stp	x18, x19, [sp, #S_X18]
	stp	x20, x21, [sp, #S_X20]
	stp	x22, x23, [sp, #S_X22]
	stp	x24, x25, [sp, #S_X24]
	stp	x26, x27, [sp, #S_X26]
	stp	x28, x29, [sp, #S_X28]
	str	x30, [sp, #S_LR]
	mrs	x0, elr_el1
	mrs	x1, spsr_el1
	mrs	x2, esr_el1
	stp	x0, x1, [sp, #S_ELR]
	str	x2, [sp, #S_ESR]
.endm

.macro restore_all
	ldp	x0, x1, [sp, #S_ELR]
	msr	elr_el1, x0
	msr	spsr_el1, x1
	ldp	x0, x1, [sp, #S_X0]
	ldp	x2, x3, [sp, #S_X2]
	ldp	x4, x5, [sp, #S_X4]
	ldp	x6, x7, [sp, #S_X6]
	ldp	x8, x9, [sp, #S_X8]
	ldp	x10, x11, [sp, #S_X10]
	ldp	x12, x13, [sp, #S_X12]
	ldp	x14, x15, [sp, #S_X14]
	ldp	x16, x17, [sp, #S_X16]
	ldp	x18, x19, [sp, #S_X18]
	ldp	x20, x21, [sp, #S_X20]
	ldp	x22, x23, [sp, #S_X22]
	ldp	x24, x25, [sp, #S_X24]
	ldp	x26, x27, [sp, #S_X26]
	ldp	x28, x29, [sp, #S_X28]
	ldr	x30, [sp, #S_LR]
	add	sp, sp, #S_FRAME_SIZE
	eret
.endm

irq_dispatch:
	save_all
	mov	x0, sp
	bl	handle_irq_exception
	restore_all

fiq_dispatch:
	save_all
	mov	x0, sp
	bl	handle_fiq_exception
	restore_all

bad_sync:
bad_irq:
bad_fiq:
bad_error:
	save_all
	mov	x0, sp
	bl	handle_bad_exception
	b	.
