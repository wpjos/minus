#include "memory.h"
OUTPUT_ARCH(aarch64)
ENTRY(_start)
SECTIONS
{
	. = VIRT_LOAD_OFFSET;
	__image_start = .;
	.text : AT(.text) {
		__text_start = .;
		*(.text.head)
		*(.text)
		. = ALIGN(4096);
		__text_end = .;
	}
	.init : {
		__init_start = .;
		__module_start = .;
		*(.module_l0.init)
		*(.module_l1.init)
		*(.module_l2.init)
		*(.module_l3.init)
		*(.module_l4.init)
		*(.module_l5.init)
		*(.module_l6.init)
		*(.module_l7.init)
		__module_end = .;

		__vector_start = .;
		*(.vector.init)
		__vector_end = .;
		. = ALIGN(4096);
		__init_end = .;
	}
	.dtb : {
		__dtb_start = .;
		*(.dtb);
		__dtb_end = .;
		. = ALIGN(4096);
	}
	.init.pgtable : {
		. = ALIGN(4096);
		__pgtable_start = .;
		__init_pgd = .;
		. += PAGE_SIZE;
		__early_pgtable_start = .;
		__early_init_pgd = .;
		. += PAGE_SIZE;
		__early_idmap_pgd = .;
		. += PAGE_SIZE;
		/*
		 * Reserve a small physical-memory region immediately after the kernel
		 * image for early page-table pages.  This region is used while the MMU
		 * is being enabled and during paging_init(), before the buddy allocator
		 * is ready.  It is kept reserved in memblock so it is never handed to
		 * the buddy allocator.
		 */
		__early_mmu_pool_start = .;
		. += EARLY_MMU_POOL_SIZE;
		__early_mmu_pool_end = .;
		__early_pgtable_end = .;
		__pgtable_end = .;
	}
	.rodata : {
		. = ALIGN(4096);
		__rodata_start = .;
		*(.rodata)
		. = ALIGN(4096);
		__rodata_end = .;
	}
	.data : {
		. = ALIGN(4096);
		__data_start = .;
		*(.init.stack)
		. = . + 4096;
		__init_stack = .;
		*(.data)
		. = ALIGN(4096);
		__data_end = .;
	}
	.bss : {
		__bss_start = .;
		*(.bss)
		. = ALIGN(4096);
		__bss_end = .;
	}

	. = ALIGN(4096);
	__image_end = .;
}
