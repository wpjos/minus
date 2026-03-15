#ifndef __STRING_H__
#define __STRING_H__

#include "types.h"

#define MAX_BUF_LEN 128

int strlen(const char *str);
size_t strnlen(const char *s, size_t maxlen);
char *strcpy(char *dst, const char *src);
int vsprintf(char *str, const char *format, va_list args);
void *memset(void *str, int c, size_t n);
void *memcpy(void *dst, const void *src, size_t n);
int strcmp(const char *s, const char *t);
int strncmp(const char *s, const char *t, size_t n);
char *strstr(const char *s, const char *t);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const char *s, const char *t, size_t n);
void *memchr(const void *s, int c, size_t n);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
unsigned long strtoul(const char *nptr, char **endptr, int base);

#endif
