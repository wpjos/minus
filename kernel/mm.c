#include "mm.h"
#include "fdt.h"
#include "printk.h"

struct mm_bank_t {
	uint64_t addr;
	uint64_t size;
};

static void mm_bank_add(struct mm_bank_t bank)
{

}

void mm_init(void)
{
	int node, len, i, j;
	int address_cells, size_cells;
	uint64_t addr = 0, size = 0;
	void *fdt = fdt_base();
	fdt_for_each_node(node, fdt) {
		const char *type = fdt_getprop(fdt, node, "device_type", NULL);
		if (type == NULL || strcmp(type, "memory") != 0) {
			continue;
		}
		const fdt32_t *reg = fdt_getprop(fdt, node, "reg", &len);
		if (reg == NULL) {
			continue;
		}
		int root = fdt_path_offset(fdt, "/");
		if (root < 0) {
			break;
		}
		address_cells = fdt_address_cells(fdt, root);
		size_cells = fdt_size_cells(fdt, root);
		i = 0, j = 0, len /= sizeof(fdt32_t);
		while (i < len) {
			addr = 0, size = 0;
			for (j = i + address_cells; i < j; i++) {
				addr = (addr << 32) + fdt32_to_cpu(reg[i]);
			}
			for (j = i + size_cells; i < j; i++) {
				size = (size << 32) + fdt32_to_cpu(reg[i]);
			}
			mm_bank_add((struct mm_bank_t){.addr = addr, .size = size});
		}
	}
}
