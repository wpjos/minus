.global _start
.section ".text.head", "ax", %progbits

.global _start
_start:
	// QEMU raspi4b 启动 4 个核心；只允许 CPU 0 继续，其他核心挂起等待
	// x0 由 bootloader 传入，保存着 DTB 的物理地址；
	// x0-x3 在启动汇编阶段保留，不做临时寄存器使用。
	mrs	x4, mpidr_el1
	and	x4, x4, #0xff
	cbz	x4, 2f
0:
	wfe
	b	0b
2:
	// 1. 读取当前异常级别，判断是否在 EL2
	mrs	x4, CurrentEL

	// CurrentEL 的 bits [3:2] 编码：0b01=EL1, 0b10=EL2, 0b11=EL3
	// 判断当前异常级别：0b01=EL1, 0b10=EL2, 0b11=EL3
	lsr	x4, x4, #2          // 将 bits [3:2] 移到最低两位
	and	x4, x4, #0b11       // 只保留最低两位
	cmp	x4, #3              // 是否等于 3 (EL3) ?
	b.eq	start_in_el3        // 如果是 EL3，先降到 EL1
	cmp	x4, #2              // 是否等于 2 (EL2) ?
	b.eq	start_in_el2        // 如果是 EL2，降到 EL1
	b	start_in_el1          // 已经是 EL1 或更低，直接进入启动代码

start_in_el3:
	// --- 以下是在 EL3 下执行的配置 ---
	// 设置 SCR_EL3：NS=1，RW=1，保证目标为 AArch64 非安全状态
	mov	x4, #((1 << 10) | 1) // SCR_EL3.RW = 1, NS = 1
	msr	scr_el3, x4
	isb

	// 设置 SPSR_EL3，用于 ERET 返回后的处理器状态
	// 目标：EL2h（使用 SP_EL2），AArch64，屏蔽 DAIF
	mov	x4, #0x3c8
	msr	spsr_el3, x4

	// 设置 ELR_EL3 为进入 EL2 后的执行地址（继续降到 EL1）
	adr	x4, start_in_el2
	msr	elr_el3, x4

	eret

start_in_el2:
	// --- 以下是在 EL2 下执行的配置 ---
	// 允许 EL1 访问 physical counter/timer，否则 EL1 访问 CNTP_*_EL0 会 trap 到 EL2
	mrs	x4, cnthctl_el2
	orr	x4, x4, #((1 << 0) | (1 << 1)) // EL1PCTEN | EL1PCEN
	msr	cnthctl_el2, x4
	isb

	// 先读出原始 HCR_EL2 传给 early_mmu_init 诊断，然后强制清零只留 RW=1
	mrs	x5, hcr_el2
	mov	x4, #(1 << 31)      // HCR_EL2.RW = 1, 其余清零（包括 VM=0）
	msr	hcr_el2, x4
	isb

	// 3. 设置 SPSR_EL2，用于 ERET 返回后的处理器状态
	//    我们目标：EL1h（使用 SP_EL1），且 CPU 运行在 AArch64
	//    SPSR_EL2 的 M[4:0] 应设置为 0b0100 (EL1h) 或 0b0101 (EL1t)
	//    这里选择 EL1h：0b0100 = 0x4，同时屏蔽 DAIF 避免向量表未装好前触发异常
	mov	x4, #0x3c4
	msr	spsr_el2, x4

	// 4. 设置 ELR_EL2 为跳转目标地址（即进入 EL1 后的执行地址）
	//    这里假设 start_in_el1 标签就是我们要去的 EL1 地址
	adr	x4, start_in_el1
	msr	elr_el2, x4

	eret

start_in_el1:
	adr	x19, 1f
	ldr	x20, =1f
	sub	x19, x19, x20
	/* Save runtime LOAD_OFFSET (phys - virt) for C code. */
	ldr	x20, =g_load_offset
	add	x20, x20, x19
	str	x19, [x20]

	/* Save the DTB physical address passed by the bootloader in x0. */
	ldr	x20, =g_dtb_base
	add	x20, x20, x19
	str	x0, [x20]
1:
	/* Use SP_EL1 for exceptions */
	msr	spsel, #1

	/* SP = physical address of __init_stack */
	ldr	x20, =__init_stack
	add	sp, x20, x19

	bl	early_mmu_init

	ldr	x20, =__init_stack
	mov	sp, x20

	ldr	x20, =start_kernel
	br	x20
