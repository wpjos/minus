#ifndef __TYPES_H__
#define __TYPES_H__

#include "stdarg.h"
#include "stdbool.h"

typedef unsigned long uintptr_t;
typedef unsigned int size_t;
typedef unsigned char uchar;

typedef unsigned long long __u64;
typedef unsigned int __u32;
typedef unsigned short __u16;
typedef unsigned char __u8;

typedef long long __s64;
typedef int __s32;
typedef short __s16;
typedef char __s8;

typedef __u16  __le16;
typedef __u32  __le32;
typedef __u64  __le64;

typedef __u64  uint64_t;
typedef __u32  uint32_t;
typedef __u16  uint16_t;
typedef __u8   uint8_t;

typedef __s64  int64_t;
typedef __s32  int32_t;
typedef __s16  int16_t;
typedef __s8   int8_t;

#define NULL ((void *)0)
#define null ((void *)0)

#define BITS_PER_BYTE		(8UL)

#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

#define container_of(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})


#endif
