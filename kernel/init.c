#include "module.h"
#include "printk.h"
#include "libfdt.h"

const char logo[] = "hello minus!!!\n";
extern char __dtb_start[];
extern char __dtb_end[];

int start_kernel(void)
{
	module_init();
//	mm_init();
	printk("%s\n", &logo[0]);

	void *fdt = &__dtb_start[0];
	// 2. 获取根节点
	int nodeoff = fdt_path_offset(fdt, "/");
	// 3. 获取子节点
	int subnode;
	fdt_for_each_subnode(subnode, fdt, nodeoff) {
		const char *name = fdt_get_name(fdt, subnode, NULL);
		printk("Subnode: %s\n", name);
	}
	while(1);
	return 0;
}
