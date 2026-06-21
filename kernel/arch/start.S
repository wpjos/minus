.global _start
.section ".text.head", "ax", %progbits
_start:
	adr	x19, 1f
	ldr	x20, =1f
	sub	x19, x19, x20

	/* Use SP_EL1 for exceptions */
	msr	spsel, #1

	/* SP = physical address of _stack */
	ldr	x20, =_stack
	add	sp, x20, x19

	bl	early_mmu_init

	/*
	 * early_mmu_init returns with the MMU enabled, but the CPU is still
	 * fetching from the physical PC.  Branch to the virtual address of the
	 * next instruction so execution continues in the high virtual mapping.
	 */
	ldr	x0, =1f
	br	x0
1:
	bl	start_kernel

.section ".bss.stack", "aw", %nobits
.align 4
.space 16384
_stack:
