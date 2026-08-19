#include "subsys.h"
#include "errno.h"
#include "string.h"
#include "printk.h"

extern struct subsys __subsys_start[];
extern struct subsys __subsys_end[];

#define SUBSYS_MAX 16

/*
 * Simple bubble sort by level.  The number of subsystems is tiny, so
 * determinism is more important than complexity.  We copy the ops pointers
 * into a writable array because the linker section is mapped read-only.
 */
static int subsys_sort(const struct subsys_ops **ops, int count)
{
	int i, j;

	for (i = 0; i < count; i++) {
		for (j = i + 1; j < count; j++) {
			if (ops[j]->level < ops[i]->level) {
				const struct subsys_ops *tmp = ops[i];
				ops[i] = ops[j];
				ops[j] = tmp;
			}
		}
	}

	return 0;
}

int subsys_init_all(void)
{
	const struct subsys_ops *ops[SUBSYS_MAX];
	struct subsys *s;
	int count = 0;
	int prev_level = -1;
	int i, ret;

	for (s = __subsys_start; s < __subsys_end; s++) {
		if (!s->ops || !s->ops->init)
			continue;
		if (count >= SUBSYS_MAX) {
			printk("subsys: too many subsystems\n");
			return -EINVAL;
		}
		ops[count++] = s->ops;
	}

	subsys_sort(ops, count);

	for (i = 0; i < count; i++) {
		const struct subsys_ops *op = ops[i];

		if (op->level < prev_level) {
			printk("subsys: ordering bug for '%s' level=%d\n",
			       op->name, op->level);
			return -EINVAL;
		}
		prev_level = op->level;

		ret = op->init();
		if (ret < 0) {
			printk("subsys: '%s' init failed: %d\n", op->name, ret);
			return ret;
		}

		printk("%s initialized\n", op->name);
	}

	return 0;
}

struct subsys *subsys_find(const char *name)
{
	struct subsys *s;

	if (!name)
		return NULL;

	for (s = __subsys_start; s < __subsys_end; s++) {
		if (s->ops && s->ops->name &&
		    strcmp(s->ops->name, name) == 0)
			return s;
	}

	return NULL;
}
