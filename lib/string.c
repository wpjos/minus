#include "string.h"

int strcmp(const char *s, const char *t)
{
	while (*s && *t && (*s == *t)) {
		s++;
		t++;
	}
	return (*s != *t);
}

int strncmp(const char *s, const char *t, size_t n)
{
	if (n == 0) {
		return 0;
	}
	for (size_t i = 0; i < n; i++) {
		if (s[i] != t[i]) {
			return 1;
		}
	}
	return 0;
}


int strlen(const char *str)
{
	int len = 0;
	while (str && *str++ != '\0') {
		len++;
	}
	return len;
}

size_t strnlen(const char *s, size_t maxlen)
{
    size_t len = 0;

    while (len < maxlen && s[len] != '\0')
        len++;

    return len;
}

char *strcpy(char *dst, const char *src)
{
	char *res = dst;
	while (src && *src != '\0') {
		*dst++ = *src++;
	}
	return res;
}

void *memcpy(void *dst, const void *src, size_t n)
{
	if (dst == NULL) {
		return NULL;
	}
	for (size_t i = 0; i < n; i++) {
		*((char*)dst + i) = *((char *)src + i);
	}
	return dst;
}

void *memset(void *str, int c, size_t n)
{
	if (str == NULL) {
		return NULL;
	}
	for (size_t i = 0; i < n; i++) {
		*((char*)str + i) = (char)c;
	}
	return str;
}

char *strstr(const char *haystack, const char *needle) {
    if (needle == NULL || *needle == '\0') {
        return (char *)haystack;
    }

    // 计算 needle 长度
    unsigned int needle_len = 0;
    while (needle[needle_len]) {
        needle_len++;
    }

    // 计算 haystack 剩余长度，如果小于 needle_len 就不用比了
    while (*haystack) {
        const char *h = haystack;
        const char *n = needle;
        unsigned int i;

        // 比较 needle_len 个字符
        for (i = 0; i < needle_len; i++) {
            if (h[i] != n[i]) {
                break;
            }
            if (h[i] == '\0') {  // haystack 提前结束
                return NULL;
            }
        }

        if (i == needle_len) {
            return (char *)haystack;
        }

        haystack++;
    }

    return NULL;
}

static void swap(char *buf, size_t n)
{
	for (size_t i = 0; i < n / 2; i++) {
		char c = buf[i];
		buf[i] = buf[n - 1 - i];
		buf[n - 1 - i] = c;
	}
}

static char *to_hex(char *buf, uint64_t n)
{
	size_t cnt = 0;
	int c;

	do {
		c = n & 0x0f;
		if (c >= 10) {
			buf[cnt++] = c - 10 + 'A';
		} else {
			buf[cnt++] = c + '0';
		}
		n >>= 4;
	} while (n);

	buf[cnt++] = 'x';
	buf[cnt++] = '0';

	swap(buf, cnt);
	buf[cnt] = '\0';
	return buf;
}

static char *to_decimal(char *buf, uint64_t n)
{
	size_t cnt = 0;

	do {
		buf[cnt++] = n % 10 + '0';
		n /= 10;
	} while (n);

	swap(buf, cnt);
	buf[cnt] = '\0';
	return buf;
}

static char *tostr(char *buf, uint64_t n, size_t base)
{
	if (base == 16) {
		return to_hex(buf, n);
	}
	return to_decimal(buf, n);
}

int vsprintf(char *str, const char *format, va_list args)
{
	char buf[MAX_BUF_LEN];
	const char *p = format;
	char *tmp;

	while (p && *p != '\0') {
		if (*p != '%') {
			*str++ = *p++;
			continue;
		}
		p++;
		if (!p) {
			break;
		}
		if (*p == 's') {
			tmp = va_arg(args, char *);
		} else if (*p == 'p') {
			tmp = tostr(&buf[0], va_arg(args, long), 16);
		} else {
			tmp = tostr(&buf[0], va_arg(args, int), 10);
		}
		while (tmp && *tmp != '\0') {
			*str++ = *tmp++;
		}
		p++;
	}
	*str = '\0';
	return 0;
}

void *memmove(void *dest, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;

    // 如果没有数据要拷贝，直接返回
    if (n == 0 || d == s)
        return dest;

    // 检查是否重叠以及重叠的方向
    if (d < s) {
        // 情况1: dest 在 src 之前
        // 从前向后拷贝（和 memcpy 一样）
        while (n--) {
            *d++ = *s++;
        }
    } else {
        // 情况2: dest 在 src 之后（有重叠）
        // 从后向前拷贝，避免覆盖
        d += n;
        s += n;
        while (n--) {
            *--d = *--s;
        }
    }

    return dest;
}

int memcmp(const char *s, const char *t, size_t n)
{
	return strncmp(s, t, n);
}

void *memchr(const void *s, int c, size_t n)
{
    const unsigned char *p = (const unsigned char *)s;
    unsigned char ch = (unsigned char)c;

    while (n--) {
        if (*p == ch)
            return (void *)p;
        p++;
    }

    return NULL;
}

char *strchr(const char *s, int c)
{
    char ch = (char)c;

    while (*s != '\0') {
        if (*s == ch)
            return (char *)s;
        s++;
    }

    // 如果查找的是 '\0'，返回指向字符串结尾的指针
    if (ch == '\0')
        return (char *)s;

    return NULL;
}

char *strrchr(const char *s, int c)
{
    const char *last = NULL;
    char ch = (char)c;

    // 遍历整个字符串
    while (*s) {
        if (*s == ch)
            last = s;
        s++;
    }

    // 如果查找的是 '\0'，返回字符串结尾
    if (ch == '\0')
        return (char *)s;

    return (char *)last;
}

unsigned long strtoul(const char *nptr, char **endptr, int base)
{
    const char *s = nptr;
    unsigned long result = 0;
    int sign = 1;

    // 忽略 base 参数，只支持十进制
    (void)base;

    // 跳过空白
    while (*s == ' ' || *s == '\t')
        s++;

    // 处理符号
    if (*s == '-') {
        sign = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }

    // 转换数字
    while (*s >= '0' && *s <= '9') {
        result = result * 10 + (*s - '0');
        s++;
    }

    // 设置结束指针
    if (endptr)
        *endptr = (char *)s;

    return sign < 0 ? -result : result;
}
