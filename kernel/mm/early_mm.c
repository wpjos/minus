#include "mm.h"
#include "fdt.h"
#include "printk.h"

#define EARLY_MM_ALIGN       8UL
#define EARLY_MM_MAX_BLOCKS  256

#define ALIGN_UP(x, align)	(((x) + ((align) - 1)) & ~((align) - 1))
#define ALIGN_DOWN(x, align)	((x) & ~((align) - 1))

struct early_mm_block {
	uint64_t addr;
	uint64_t size;
	int used;
};

static struct early_mm_block g_blocks[EARLY_MM_MAX_BLOCKS];
static int g_block_cnt;

static void early_mm_add_block(uint64_t base, uint64_t size)
{
	if (g_block_cnt >= EARLY_MM_MAX_BLOCKS || size < EARLY_MM_ALIGN)
		return;

	uint64_t aligned_base = ALIGN_UP(base, EARLY_MM_ALIGN);
	uint64_t aligned_end = ALIGN_DOWN(base + size, EARLY_MM_ALIGN);
	uint64_t aligned_size = aligned_end - aligned_base;

	if (aligned_size < EARLY_MM_ALIGN)
		return;

	g_blocks[g_block_cnt].addr = aligned_base;
	g_blocks[g_block_cnt].size = aligned_size;
	g_blocks[g_block_cnt].used = 0;
	g_block_cnt++;
}

static void early_mm_fdt_register_one(const fdt32_t *reg, uint64_t len,
				int address_cells, int size_cells)
{
	uint64_t i = 0, j;
	while (i * sizeof(fdt32_t) < len) {
		uint64_t addr = 0, size = 0;
		for (j = i + address_cells; i < j; i++)
			addr = (addr << 32) + fdt32_to_cpu(reg[i]);
		for (j = i + size_cells; i < j; i++)
			size = (size << 32) + fdt32_to_cpu(reg[i]);
		early_mm_add_block(addr, size);
	}
}

static void early_mm_fdt_register_all(void *fdt, int root)
{
	int address_cells = fdt_address_cells(fdt, root);
	int size_cells = fdt_size_cells(fdt, root);
	int node, prop_len;

	fdt_for_each_node(node, fdt) {
		const char *type = fdt_getprop(fdt, node, "device_type", NULL);
		if (!type || type[0] != 'm')
			continue;
		const fdt32_t *reg = fdt_getprop(fdt, node, "reg", &prop_len);
		if (!reg)
			continue;
		early_mm_fdt_register_one(reg, prop_len, address_cells, size_cells);
	}
}

void early_mm_init(void)
{
	void *fdt = fdt_base();
	if (!fdt)
		return;
	int root = fdt_path_offset(fdt, "/");
	if (root < 0)
		return;
	early_mm_fdt_register_all(fdt, root);
	printk("early_mm: %d blocks\n", g_block_cnt);
}

void *early_mm_alloc(uint64_t size)
{
	if (size == 0)
		return NULL;

	uint64_t aligned_size = ALIGN_UP(size, EARLY_MM_ALIGN);

	for (int i = 0; i < g_block_cnt; i++) {
		if (g_blocks[i].used || g_blocks[i].size < aligned_size)
			continue;

		g_blocks[i].used = 1;

		if (g_blocks[i].size > aligned_size + EARLY_MM_ALIGN &&
		    g_block_cnt < EARLY_MM_MAX_BLOCKS) {
			g_blocks[g_block_cnt].addr = g_blocks[i].addr + aligned_size;
			g_blocks[g_block_cnt].size = g_blocks[i].size - aligned_size;
			g_blocks[g_block_cnt].used = 0;
			g_block_cnt++;
			g_blocks[i].size = aligned_size;
		}

		return (void *)(uintptr_t)g_blocks[i].addr;
	}

	return NULL;
}

void early_mm_free(void *ptr)
{
	if (!ptr)
		return;

	uint64_t addr = (uint64_t)(uintptr_t)ptr;
	int idx = -1;

	for (int i = 0; i < g_block_cnt; i++) {
		if (g_blocks[i].used && g_blocks[i].addr == addr) {
			idx = i;
			break;
		}
	}

	if (idx < 0)
		return;

	g_blocks[idx].used = 0;

	int merged;
	do {
		merged = 0;
		for (int i = 0; i < g_block_cnt; i++) {
			if (i == idx || g_blocks[i].used || g_blocks[i].size == 0)
				continue;
			if (g_blocks[i].addr + g_blocks[i].size == g_blocks[idx].addr) {
				g_blocks[i].size += g_blocks[idx].size;
				g_blocks[idx].size = 0;
				idx = i;
				merged = 1;
				break;
			} else if (g_blocks[idx].addr + g_blocks[idx].size == g_blocks[i].addr) {
				g_blocks[idx].size += g_blocks[i].size;
				g_blocks[i].size = 0;
				merged = 1;
				break;
			}
		}
	} while (merged);

	for (int i = g_block_cnt - 1; i >= 0; i--) {
		if (g_blocks[i].size == 0) {
			g_blocks[i] = g_blocks[g_block_cnt - 1];
			g_block_cnt--;
		}
	}
}
