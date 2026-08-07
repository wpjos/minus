#include "types.h"
#include "stat.h"
#include "fcntl.h"
#include "dirent.h"

/* AArch64 syscall numbers */
#define SYS_GETDENTS64	61
#define SYS_MKDIRAT	34
#define SYS_OPENAT	56
#define SYS_CLOSE	57
#define SYS_READ	63
#define SYS_WRITE	64
#define SYS_NEWFSTATAT	76
#define SYS_EXIT	93

#define PATH_MAX	256
#define LINE_MAX	128

static inline long syscall0(long n)
{
	register long x8 __asm__("x8") = n;
	register long x0 __asm__("x0");
	__asm__ volatile("svc #0" : "=r"(x0) : "r"(x8)
			 : "memory", "x1", "x2", "x3", "x4", "x5",
			   "x6", "x7", "x9", "x10", "x11", "x12",
			   "x13", "x14", "x15", "x16", "x17", "x18");
	return x0;
}

static inline long syscall1(long n, long a)
{
	register long x8 __asm__("x8") = n;
	register long x0 __asm__("x0") = a;
	__asm__ volatile("svc #0" : "=r"(x0) : "r"(x0), "r"(x8)
			 : "memory", "x1", "x2", "x3", "x4", "x5",
			   "x6", "x7", "x9", "x10", "x11", "x12",
			   "x13", "x14", "x15", "x16", "x17", "x18");
	return x0;
}

static inline long syscall2(long n, long a, long b)
{
	register long x8 __asm__("x8") = n;
	register long x0 __asm__("x0") = a;
	register long x1 __asm__("x1") = b;
	__asm__ volatile("svc #0" : "=r"(x0) : "r"(x0), "r"(x1), "r"(x8)
			 : "memory", "x2", "x3", "x4", "x5",
			   "x6", "x7", "x9", "x10", "x11", "x12",
			   "x13", "x14", "x15", "x16", "x17", "x18");
	return x0;
}

static inline long syscall3(long n, long a, long b, long c)
{
	register long x8 __asm__("x8") = n;
	register long x0 __asm__("x0") = a;
	register long x1 __asm__("x1") = b;
	register long x2 __asm__("x2") = c;
	__asm__ volatile("svc #0" : "=r"(x0) : "r"(x0), "r"(x1), "r"(x2), "r"(x8)
			 : "memory", "x3", "x4", "x5",
			   "x6", "x7", "x9", "x10", "x11", "x12",
			   "x13", "x14", "x15", "x16", "x17", "x18");
	return x0;
}

static inline long syscall4(long n, long a, long b, long c, long d)
{
	register long x8 __asm__("x8") = n;
	register long x0 __asm__("x0") = a;
	register long x1 __asm__("x1") = b;
	register long x2 __asm__("x2") = c;
	register long x3 __asm__("x3") = d;
	__asm__ volatile("svc #0" : "=r"(x0) : "r"(x0), "r"(x1), "r"(x2), "r"(x3), "r"(x8)
			 : "memory", "x4", "x5",
			   "x6", "x7", "x9", "x10", "x11", "x12",
			   "x13", "x14", "x15", "x16", "x17", "x18");
	return x0;
}

static void sys_exit(int code)
{
	syscall1(SYS_EXIT, code);
	while (1)
		;
}

static long sys_write(int fd, const void *buf, size_t count)
{
	return syscall3(SYS_WRITE, fd, (long)buf, count);
}

static long sys_read(int fd, void *buf, size_t count)
{
	return syscall3(SYS_READ, fd, (long)buf, count);
}

static long sys_openat(int dirfd, const char *path, int flags, unsigned int mode)
{
	return syscall4(SYS_OPENAT, dirfd, (long)path, flags, mode);
}

static long sys_close(int fd)
{
	return syscall1(SYS_CLOSE, fd);
}

static long sys_mkdirat(int dirfd, const char *path, unsigned int mode)
{
	return syscall3(SYS_MKDIRAT, dirfd, (long)path, mode);
}

static long sys_getdents64(int fd, void *buf, unsigned int count)
{
	return syscall3(SYS_GETDENTS64, fd, (long)buf, count);
}

static long sys_newfstatat(int dirfd, const char *path, struct stat *st, int flags)
{
	return syscall4(SYS_NEWFSTATAT, dirfd, (long)path, (long)st, flags);
}

static size_t strlen(const char *s)
{
	size_t n = 0;
	while (s[n])
		n++;
	return n;
}

static void *memcpy(void *dst, const void *src, size_t n)
{
	char *d = dst;
	const char *s = src;
	while (n--)
		*d++ = *s++;
	return dst;
}

static char *strncpy(char *dst, const char *src, size_t n)
{
	size_t i;

	for (i = 0; i < n && src[i]; i++)
		dst[i] = src[i];
	for (; i < n; i++)
		dst[i] = '\0';
	return dst;
}

static char *strncat(char *dst, const char *src, size_t n)
{
	size_t dst_len = strlen(dst);
	size_t i;

	for (i = 0; i < n && src[i]; i++)
		dst[dst_len + i] = src[i];
	dst[dst_len + i] = '\0';
	return dst;
}

static void print(const char *s)
{
	sys_write(1, s, strlen(s));
}

static int strcmp(const char *a, const char *b)
{
	while (*a && *a == *b) {
		a++;
		b++;
	}
	return (unsigned char)*a - (unsigned char)*b;
}

static int isspace(int c)
{
	return c == ' ' || c == '\t';
}

static char cwd[PATH_MAX] = "/";

static void make_path(char *out, size_t outsz, const char *path)
{
	size_t cwd_len;

	if (path[0] == '/') {
		strncpy(out, path, outsz - 1);
		out[outsz - 1] = '\0';
		return;
	}

	cwd_len = strlen(cwd);
	if (cwd_len >= outsz)
		cwd_len = outsz - 1;
	memcpy(out, cwd, cwd_len);
	out[cwd_len] = '\0';

	if (cwd_len > 0 && out[cwd_len - 1] != '/' && cwd_len < outsz - 1) {
		out[cwd_len++] = '/';
		out[cwd_len] = '\0';
	}

	if (cwd_len < outsz - 1)
		strncat(out, path, outsz - cwd_len - 1);
}

static void cmd_ls(const char *arg)
{
	char path[PATH_MAX];
	char buf[512];
	struct dirent64_s *de;
	int fd;
	long n;
	long pos;

	if (arg[0])
		make_path(path, sizeof(path), arg);
	else {
		strncpy(path, cwd, sizeof(path) - 1);
		path[sizeof(path) - 1] = '\0';
	}

	fd = sys_openat(-1, path, O_DIRECTORY | O_RDONLY, 0);
	if (fd < 0) {
		print("ls: cannot open directory\n");
		return;
	}

	while (1) {
		n = sys_getdents64(fd, buf, sizeof(buf));
		if (n <= 0)
			break;
		pos = 0;
		while (pos < n) {
			de = (struct dirent64_s *)(buf + pos);
			if (strcmp(de->d_name, ".") != 0 &&
			    strcmp(de->d_name, "..") != 0 &&
			    strcmp(de->d_name, "lost+found") != 0) {
				print(de->d_name);
				print("\n");
			}
			pos += de->d_reclen;
		}
	}

	sys_close(fd);
}

static void cmd_cd(const char *arg)
{
	char path[PATH_MAX];
	struct stat st;

	if (!arg[0]) {
		strncpy(cwd, "/", sizeof(cwd) - 1);
		cwd[sizeof(cwd) - 1] = '\0';
		return;
	}

	make_path(path, sizeof(path), arg);

	if (sys_newfstatat(-1, path, &st, 0) < 0) {
		print("cd: no such file or directory\n");
		return;
	}

	if (!S_ISDIR(st.st_mode)) {
		print("cd: not a directory\n");
		return;
	}

	strncpy(cwd, path, sizeof(cwd) - 1);
	cwd[sizeof(cwd) - 1] = '\0';
}

static void cmd_mkdir(const char *arg)
{
	char path[PATH_MAX];

	if (!arg[0]) {
		print("mkdir: missing operand\n");
		return;
	}

	make_path(path, sizeof(path), arg);
	if (sys_mkdirat(-1, path, 0755) < 0)
		print("mkdir: failed\n");
}

static void cmd_cat(const char *arg)
{
	char path[PATH_MAX];
	char buf[256];
	int fd;
	long n;

	if (!arg[0]) {
		print("cat: missing operand\n");
		return;
	}

	make_path(path, sizeof(path), arg);
	fd = sys_openat(-1, path, O_RDONLY, 0);
	if (fd < 0) {
		print("cat: cannot open file\n");
		return;
	}

	while ((n = sys_read(fd, buf, sizeof(buf))) > 0)
		sys_write(1, buf, n);

	sys_close(fd);
}

static void tokenize(char *line, char *cmd, char *arg)
{
	char *p = line;
	char *d;
	size_t n;

	while (isspace(*p))
		p++;

	d = cmd;
	n = 0;
	while (*p && !isspace(*p) && n < 31) {
		*d++ = *p++;
		n++;
	}
	*d = '\0';

	while (isspace(*p))
		p++;

	d = arg;
	n = 0;
	while (*p && *p != '\n' && *p != '\r' && n < LINE_MAX - 1) {
		*d++ = *p++;
		n++;
	}
	*d = '\0';
}

static void read_line(char *line, size_t max)
{
	size_t i = 0;
	char c;

	while (i < max - 1) {
		if (sys_read(0, &c, 1) != 1)
			break;

		if (c == '\r' || c == '\n') {
			print("\n");
			break;
		}

		if (c == '\b' || c == 0x7f) {
			if (i > 0) {
				i--;
				print("\b \b");
			}
			continue;
		}

		if (c >= ' ' && c <= '~') {
			line[i++] = c;
			sys_write(1, &c, 1);
		}
	}

	line[i] = '\0';
}

void _start(void)
{
	char line[LINE_MAX];
	char cmd[32];
	char arg[LINE_MAX];

	while (1) {
		print("# ");
		read_line(line, sizeof(line));
		tokenize(line, cmd, arg);

		if (cmd[0] == '\0')
			continue;

		if (strcmp(cmd, "exit") == 0)
			sys_exit(0);
		else if (strcmp(cmd, "ls") == 0)
			cmd_ls(arg);
		else if (strcmp(cmd, "cd") == 0)
			cmd_cd(arg);
		else if (strcmp(cmd, "mkdir") == 0)
			cmd_mkdir(arg);
		else if (strcmp(cmd, "cat") == 0)
			cmd_cat(arg);
		else {
			print("unknown command: ");
			print(cmd);
			print("\n");
		}
	}
}
