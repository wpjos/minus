#include "svc.h"
#include "string.h"
#include "printk.h"

static struct svc_entry g_svc_table[SVC_MAX];
static int g_svc_count;
static svc_id_t g_next_svc_id = 1;

void svc_register(const char *name, const void *ops)
{
	if (!name || !ops)
		return;

	if (g_svc_count >= SVC_MAX) {
		printk("svc: too many services\n");
		return;
	}

	if (svc_lookup(name)) {
		printk("svc: '%s' already registered\n", name);
		return;
	}

	g_svc_table[g_svc_count].id = g_next_svc_id++;
	g_svc_table[g_svc_count].name = name;
	g_svc_table[g_svc_count].ops = ops;
	g_svc_count++;
}

const void *svc_lookup(const char *name)
{
	int i;

	if (!name)
		return NULL;

	for (i = 0; i < g_svc_count; i++) {
		if (g_svc_table[i].name &&
		    strcmp(g_svc_table[i].name, name) == 0)
			return g_svc_table[i].ops;
	}

	return NULL;
}

svc_id_t svc_id(const char *name)
{
	int i;

	if (!name)
		return SVC_ID_INVALID;

	for (i = 0; i < g_svc_count; i++) {
		if (g_svc_table[i].name &&
		    strcmp(g_svc_table[i].name, name) == 0)
			return g_svc_table[i].id;
	}

	return SVC_ID_INVALID;
}
