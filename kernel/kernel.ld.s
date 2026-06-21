#include "memory.h"
OUTPUT_ARCH(aarch64)
ENTRY(_start)
SECTIONS
{
	. = VIRT_LOAD_OFFSET;
	__image_start = .;
	.text : AT(.text) {
		*(.text.head)
		*(.text)
	}
	.init : {
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
	}
	.dtb : {
		__dtb_start = .;
		*(.dtb);
		__dtb_end = .;
	}
	.init.pgtable : {
		. = ALIGN(4096);
		__init_pgd = .;
		. += 3 * PAGE_SIZE;
		__idmap_pgd = .;
		. += 3 * PAGE_SIZE;
	}

	.data : {
		. = ALIGN(4096);
		*(.init.stack)
		. = . + 4096;
		__init_stack = .;
		*(.data)
	}
	.bss : { *(.bss) }

	. = ALIGN(4096);
	__image_end = .;
}
