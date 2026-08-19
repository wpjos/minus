#include "subsys.h"
#include "errno.h"
#include "string.h"
#include "printk.h"

extern struct subsys_ops __subsys_start[];
extern struct subsys_ops __subsys_end[];

#define SUBSYS_MAX 16

/*
 * Simple insertion sort by level.  The number of subsystems is tiny, so
 * determinism is more important than complexity.  We copy the ops pointers
 * into a writable array because the linker section is mapped read-only.
 */
static int subsys_sort(const struct subsys_ops **ops, int count)
{
	int i, j;

	for (i = 1; i < count; i++) {
		const struct subsys_ops *key = ops[i];

		for (j = i - 1; j >= 0 && ops[j]->level > key->level; j--)
			ops[j + 1] = ops[j];
		ops[j + 1] = key;
	}

	return 0;
}

int subsys_init_all(void)
{
	const struct subsys_ops *ops[SUBSYS_MAX];
	struct subsys_ops *op;
	int count = 0;
	int prev_level = -1;
	int i, ret;

	for (op = __subsys_start; op < __subsys_end; op++) {
		if (!op->name || !op->init)
			continue;
		if (count >= SUBSYS_MAX) {
			printk("subsys: too many subsystems\n");
			return -EINVAL;
		}
		ops[count++] = op;
	}

	subsys_sort(ops, count);

	for (i = 0; i < count; i++) {
		const struct subsys_ops *p = ops[i];

		if (p->level < prev_level) {
			printk("subsys: ordering bug for '%s' level=%d\n",
			       p->name, p->level);
			return -EINVAL;
		}
		prev_level = p->level;

		ret = p->init();
		if (ret < 0) {
			printk("subsys: '%s' init failed: %d\n", p->name, ret);
			return ret;
		}

		printk("%s initialized\n", p->name);
	}

	return 0;
}
