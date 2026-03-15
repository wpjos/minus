OUTPUT_ARCH(aarch64)
ENTRY(_start)
SECTIONS
{
	. = 0x0;
	.text : {
		*(.text.head)
		*(.text)
	}
	.init : {
		__module_start = .;
		*(.module.init)
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
	.data : {
		. = ALIGN(4096);
		*(.init.stack)
		. = . + 4096;
		__init_stack = .;
		*(.data)
	}
	.bss : { *(.bss) }

	. = ALIGN(4096);
	_end = .;
}
