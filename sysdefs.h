
#ifndef __SYSDEFS_H_INCLUDED
#define __SYSDEFS_H_INCLUDED

#define inline __inline
#define forceinline __forceinline
#define fastcall __fastcall             // parameters in registers

typedef unsigned long long uint64_t;

typedef uint64_t QWORD;

typedef unsigned long long u64;
typedef long long i64;
typedef unsigned long u32;
typedef unsigned short u16;
typedef signed  short i16;
typedef unsigned char u8;

#ifdef _MSC_VER
#define ATTR_ALIGN(x) __declspec(align(x))
#define strtoull _strtoui64
#endif

#ifdef __ICL
#pragma warning(disable : 2259)
#endif

#if __ICL >= 1000 || defined(__GNUC__)
static inline u8 rol8(u8 val, u8 shift)
{
    __asm__ volatile ("rolb %1,%0" : "=r"(val) : "cI"(shift), "0"(val));
    return val;
}

static inline u8 ror8(u8 val, u8 shift)
{
    __asm__ volatile ("rorb %1,%0" : "=r"(val) : "cI"(shift), "0"(val));
    return val;
}
static inline void asm_pause() { __asm__("pause"); }
#else
extern "C" unsigned char __cdecl _rotr8(unsigned char value, unsigned char shift);
extern "C" unsigned char __cdecl _rotl8(unsigned char value, unsigned char shift);
#pragma intrinsic(_rotr8)
#pragma intrinsic(_rotl8)
static inline u8 rol8(u8 val, u8 shift) { return _rotl8(val, shift); }
static inline u8 ror8(u8 val, u8 shift) { return _rotr8(val, shift); }
extern "C" void __cdecl _mm_pause();
#pragma intrinsic(_mm_pause)
static inline void asm_pause() { _mm_pause();/* __asm {rep nop}*/ }
#endif

#if defined(_MSC_VER) && _MSC_VER < 1300
static inline u16 _byteswap_ushort(u16 i){return (i>>8)|(i<<8);}
static inline u32 _byteswap_ulong(u32 i){return _byteswap_ushort((u16)(i>>16))|(_byteswap_ushort((u16)i)<<16);};
#endif

#ifdef __GNUC__
#include <stdint.h>
#define HANDLE_PRAGMA_PACK_PUSH_POP

#define ATTR_ALIGN(x) __attribute__((aligned(x)))

#ifndef __clang__
    #ifndef __forceinline
    #define __forceinline __attribute__((always_inline))
    #endif // __forceinline
    #undef forceinline
    #define forceinline __forceinline
    #define _byteswap_ulong(x) _bswap(x) 
#endif // __clang__

static __inline__ void __debugbreak__(void)
{
  __asm__ __volatile__ ("int $3");
}

#define __debugbreak __debugbreak__
#ifndef _countof
#define _countof(x) (sizeof(x)/sizeof((x)[0]))
#endif

#define __assume(x)

#endif // __GNUC__

#endif // __SYSDEFS_H_INCLUDED
