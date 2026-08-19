#ifndef __CMDLINE_H__
#define __CMDLINE_H__

/* Parsed kernel boot arguments */
struct bootargs {
	char root_device[32];
	char root_fstype[16];
	int root_ro;
};

/*
 * Parse /chosen/bootargs from the device tree.
 * Fills @args with recognized values; unrecognized tokens are ignored.
 * Returns 0 on success, -ENOENT if /chosen/bootargs is missing, or another
 * negative errno on invalid arguments / parse failure.
 */
int parse_bootargs(struct bootargs *args);

#endif /* __CMDLINE_H__ */
