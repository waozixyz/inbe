#ifndef LOTUS_TYPES_H
#define LOTUS_TYPES_H

#if !defined(PLAN9) && !defined(_PLAN9)

#include <stdint.h>
#include <stddef.h>

typedef uint8_t  lotus_u8;
typedef int8_t   lotus_i8;
typedef uint16_t lotus_u16;
typedef int16_t  lotus_i16;
typedef uint32_t lotus_u32;
typedef int32_t  lotus_i32;
typedef uint64_t lotus_u64;
typedef int64_t  lotus_i64;

typedef size_t   lotus_usize;

#else

#include <u.h>
#include <libc.h>

typedef uchar  lotus_u8;
typedef schar  lotus_i8;
typedef u16int lotus_u16;
typedef s16int lotus_i16;
typedef u32int lotus_u32;
typedef s32int lotus_i32;
typedef uvlong lotus_u64;
typedef vlong  lotus_i64;

typedef ulong  lotus_usize;

#endif

#endif