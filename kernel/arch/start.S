.global _start
.section ".text.head", "ax", %progbits
_start:
	adrp x0, _stack			// 加载 _stack 所在页的基地址到 x0
	add  sp, x0, #:lo12:_stack	// 加上低12位偏移，得到完整地址并赋值给SP
	msr  spsel, #1			// 使用 SP_EL1，使异常走 SPx 向量组
	bl start_kernel

.section ".bss.stack", "aw", %nobits
.align 4
.space 16384
_stack:
