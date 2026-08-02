#include "cmdline.h"
#include "fdt.h"
#include "of.h"
#include "string.h"
#include "types.h"

int parse_bootargs(struct bootargs *args)
{
	const void *fdt = fdt_base();
	const char *prop;
	int chosen;
	int len;
	char buf[256];
	const char *p;
	char tok[64];

	if (!args)
		return -1;

	args->root_device[0] = '\0';
	args->root_fstype[0] = '\0';
	args->root_ro = 0;

	chosen = fdt_path_offset(fdt, "/chosen");
	if (chosen < 0)
		return -1;

	prop = of_get_property(fdt, chosen, "bootargs", &len);
	if (!prop || len <= 0)
		return -1;

	if ((size_t)len >= sizeof(buf))
		len = sizeof(buf) - 1;
	memcpy(buf, prop, len);
	buf[len] = '\0';

	p = buf;
	while (*p) {
		const char *start;
		size_t toklen;

		while (*p == ' ')
			p++;
		if (!*p)
			break;

		start = p;
		while (*p && *p != ' ')
			p++;
		toklen = (size_t)(p - start);

		if (toklen >= sizeof(tok))
			toklen = sizeof(tok) - 1;
		memcpy(tok, start, toklen);
		tok[toklen] = '\0';

		if (strncmp(tok, "root=", 5) == 0) {
			const char *val = tok + 5;
			size_t n = strlen(val);
			if (n >= sizeof(args->root_device))
				n = sizeof(args->root_device) - 1;
			memcpy(args->root_device, val, n);
			args->root_device[n] = '\0';
		} else if (strncmp(tok, "rootfstype=", 11) == 0) {
			const char *val = tok + 11;
			size_t n = strlen(val);
			if (n >= sizeof(args->root_fstype))
				n = sizeof(args->root_fstype) - 1;
			memcpy(args->root_fstype, val, n);
			args->root_fstype[n] = '\0';
		} else if (strcmp(tok, "ro") == 0) {
			args->root_ro = 1;
		} else if (strcmp(tok, "rw") == 0) {
			args->root_ro = 0;
		}
	}

	return 0;
}
