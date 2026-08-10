#include "fb.h"

/* AArch64 syscall numbers */
#define SYS_OPENAT	56
#define SYS_CLOSE	57
#define SYS_IOCTL	29
#define SYS_MMAP	222

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

static inline long syscall6(long n, long a, long b, long c, long d, long e,
			    long f)
{
	register long x8 __asm__("x8") = n;
	register long x0 __asm__("x0") = a;
	register long x1 __asm__("x1") = b;
	register long x2 __asm__("x2") = c;
	register long x3 __asm__("x3") = d;
	register long x4 __asm__("x4") = e;
	register long x5 __asm__("x5") = f;
	__asm__ volatile("svc #0" : "=r"(x0) : "r"(x0), "r"(x1), "r"(x2), "r"(x3),
			 "r"(x4), "r"(x5), "r"(x8)
			 : "memory", "x6", "x7", "x9", "x10", "x11",
			   "x12", "x13", "x14", "x15", "x16", "x17", "x18");
	return x0;
}

int fb_open(void)
{
	return (int)syscall4(SYS_OPENAT, -1, (long)"/dev/fb0", 2 /* O_RDWR */, 0);
}

int fb_info(int fd, struct fb_info_req *info)
{
	return (int)syscall3(SYS_IOCTL, fd, FBIOGET_INFO, (long)info);
}

void *fb_mmap(int fd, size_t *size)
{
	struct fb_info_req info;
	long addr;

	if (fb_info(fd, &info) < 0)
		return (void *)-1;

	addr = syscall6(SYS_MMAP, 0, (long)info.size, PROT_READ | PROT_WRITE,
			MAP_SHARED, fd, 0);
	if (addr < 0)
		return (void *)addr;

	if (size)
		*size = info.size;
	return (void *)addr;
}

int fb_flush(int fd)
{
	return (int)syscall3(SYS_IOCTL, fd, FBIO_FLUSH, 0);
}

int fb_close(int fd)
{
	return (int)syscall1(SYS_CLOSE, fd);
}
