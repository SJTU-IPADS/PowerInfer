/*
 * C11 <stdatomic.h> emulation header
 *
 * PLEASE LICENSE, (C) 2022, Michael Clark <michaeljclark@mac.com>
 *
 * All rights to this work are granted for all purposes, with exception of
 * author's implied right of copyright to defend the free use of this work.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifdef _WIN32

/*
 * C11 <stdatomic.h> emulation
 *
 * This header can be included from C and uses C11 _Generic selection.
 * This header requires MSVC flags: "/O2 /TC /std:c11 /volatile:iso".
 *
 * Note: some primitives may be missing and some primitives may haver
 * stronger ordering than is required thus not produce optimal code,
 * and some primitives may be buggy.
 */
#include <windows.h>
#include "winnt.h"

#define __concat2(x,y) x ## y
#define __concat3(x,y,z) x ## y ## z

#if UINTPTR_MAX == 0xFFFFFFFFFFFFFFFFull
#define __intptr __int64
#define __ptr i64
#elif UINTPTR_MAX == 0xFFFFFFFFu
#define __intptr __int32
#define __ptr i32
#else
#error unable to determine pointer width
#endif

#define _Atomic volatile

#define ATOMIC_BOOL_LOCK_FREE 1
#define ATOMIC_CHAR_LOCK_FREE 1
#define ATOMIC_SHORT_LOCK_FREE 1
#define ATOMIC_INT_LOCK_FREE 1
#define ATOMIC_LONG_LOCK_FREE 1
#define ATOMIC_LLONG_LOCK_FREE 1
#define ATOMIC_POINTER_LOCK_FREE 1

#define ATOMIC_FLAG_INIT { 0 }

#define __ATOMIC_RELAXED 0
#define __ATOMIC_CONSUME 1
#define __ATOMIC_ACQUIRE 2
#define __ATOMIC_RELEASE 3
#define __ATOMIC_ACQ_REL 4
#define __ATOMIC_SEQ_CST 5

typedef enum memory_order {
    memory_order_relaxed = __ATOMIC_RELAXED,
    memory_order_consume = __ATOMIC_CONSUME,
    memory_order_acquire = __ATOMIC_ACQUIRE,
    memory_order_release = __ATOMIC_RELEASE,
    memory_order_acq_rel = __ATOMIC_ACQ_REL,
    memory_order_seq_cst = __ATOMIC_SEQ_CST
} memory_order;

typedef long long llong;
typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef unsigned long long ullong;

typedef _Atomic _Bool atomic_bool;
typedef _Atomic char atomic_char;
typedef _Atomic unsigned char atomic_uchar;
typedef _Atomic short atomic_short;
typedef _Atomic unsigned short atomic_ushort;
typedef _Atomic int atomic_int;
typedef _Atomic unsigned int atomic_uint;
typedef _Atomic long atomic_long;
typedef _Atomic unsigned long atomic_ulong;
typedef _Atomic long long atomic_llong;
typedef _Atomic unsigned long long atomic_ullong;
typedef _Atomic intptr_t atomic_intptr_t;
typedef _Atomic uintptr_t atomic_uintptr_t;
typedef _Atomic size_t atomic_size_t;
typedef _Atomic ptrdiff_t atomic_ptrdiff_t;
typedef _Atomic intmax_t atomic_intmax_t;
typedef _Atomic uintmax_t atomic_uintmax_t;
typedef void* _Atomic atomic_ptr;

typedef struct atomic_flag { atomic_bool _Value; } atomic_flag;

static inline __int8  __msvc_xchg_i8(__int8 volatile* addr, __int8 val)
{
    return _InterlockedExchange8(addr, val);
}
static inline __int16 __msvc_xchg_i16(__int16 volatile* addr, __int16 val)
{
    return _InterlockedExchange16(addr, val);
}
static inline __int32 __msvc_xchg_i32(__int32 volatile* addr, __int32 val)
{
    return _InterlockedExchange(addr, val);
}
static inline __int64 __msvc_xchg_i64(__int64 volatile* addr, __int64 val)
{
    return _InterlockedExchange64(addr, val);
}

#define __msvc_xchg_ptr(ptr) __concat2(__msvc_xchg_,ptr)

static inline char __c11_atomic_exchange__atomic_char(atomic_char* obj, char desired)
{
    return (char)__msvc_xchg_i8((__int8 volatile*)obj, (__int8)desired);
}
static inline short __c11_atomic_exchange__atomic_short(atomic_short* obj, short desired)
{
    return (short)__msvc_xchg_i16((__int16 volatile*)obj, (__int16)desired);
}
static inline int __c11_atomic_exchange__atomic_int(atomic_int* obj, int desired)
{
    return (int)__msvc_xchg_i32((__int32 volatile*)obj, (__int32)desired);
}
static inline long __c11_atomic_exchange__atomic_long(atomic_long* obj, long desired)
{
    return (int)__msvc_xchg_i32((__int32 volatile*)obj, (__int32)desired);
}
static inline llong __c11_atomic_exchange__atomic_llong(atomic_llong* obj, llong desired)
{
    return (llong)__msvc_xchg_i64((__int64 volatile*)obj, (__int64)desired);
}
static inline uchar __c11_atomic_exchange__atomic_uchar(atomic_uchar* obj, uchar desired)
{
    return (char)__msvc_xchg_i8((__int8 volatile*)obj, (__int8)desired);
}
static inline ushort __c11_atomic_exchange__atomic_ushort(atomic_ushort* obj, ushort desired)
{
    return (short)__msvc_xchg_i16((__int16 volatile*)obj, (__int16)desired);
}
static inline uint __c11_atomic_exchange__atomic_uint(atomic_uint* obj, uint desired)
{
    return (int)__msvc_xchg_i32((__int32 volatile*)obj, (__int32)desired);
}
static inline ulong __c11_atomic_exchange__atomic_ulong(atomic_ulong* obj, ulong desired)
{
    return (int)__msvc_xchg_i32((__int32 volatile*)obj, (__int32)desired);
}
static inline ullong __c11_atomic_exchange__atomic_ullong(atomic_ullong* obj, ullong desired)
{
    return (llong)__msvc_xchg_i64((__int64 volatile*)obj, (__int64)desired);
}
static inline void* __c11_atomic_exchange__atomic_ptr(atomic_ptr* obj, void* desired)
{
    return (void*)__msvc_xchg_ptr(__ptr)((__intptr volatile*)obj, (ptrdiff_t)desired);
}

#define __c11_atomic_exchange(obj,desired)                \
_Generic((obj),                                           \
atomic_char*: __c11_atomic_exchange__atomic_char,         \
atomic_uchar*: __c11_atomic_exchange__atomic_uchar,       \
atomic_short*: __c11_atomic_exchange__atomic_short,       \
atomic_ushort*: __c11_atomic_exchange__atomic_ushort,     \
atomic_int*: __c11_atomic_exchange__atomic_int,           \
atomic_uint*: __c11_atomic_exchange__atomic_uint,         \
atomic_long*: __c11_atomic_exchange__atomic_long,         \
atomic_ulong*: __c11_atomic_exchange__atomic_ulong,       \
atomic_llong*: __c11_atomic_exchange__atomic_llong,       \
atomic_ullong*: __c11_atomic_exchange__atomic_ullong      \
)(obj,desired)

#define atomic_exchange(obj,desired) __c11_atomic_exchange(obj,desired)
#define atomic_store(obj,desired) __c11_atomic_exchange(obj,desired)
#define atomic_exchange_explicit(obj,desired,mo) __c11_atomic_exchange(obj,desired)
#define atomic_store_explicit(obj,desired,mo) __c11_atomic_exchange(obj,desired)

static inline __int8  __msvc_cmpxchg_i8(__int8 volatile* addr, __int8 oldval, __int8 newval)
{
    return _InterlockedCompareExchange8((__int8 volatile*)addr, newval, oldval);
}
static inline __int16 __msvc_cmpxchg_i16(__int16 volatile* addr, __int16 oldval, __int16 newval)
{
    return _InterlockedCompareExchange16((__int16 volatile*)addr, newval, oldval);
}
static inline __int32 __msvc_cmpxchg_i32(__int32 volatile* addr, __int32 oldval, __int32 newval)
{
    return _InterlockedCompareExchange((__int32 volatile*)addr, newval, oldval);
}
static inline __int64 __msvc_cmpxchg_i64(__int64 volatile* addr, __int64 oldval, __int64 newval)
{
    return _InterlockedCompareExchange64((__int64 volatile*)addr, newval, oldval);
}

#define __msvc_cmpxchg_ptr(ptr) __concat2(__msvc_cmpxchg_,ptr)

static inline _Bool __c11_atomic_compare_exchange_strong__atomic_char(atomic_char* obj, char* expected, char desired)
{
    char cmp = *expected, val = __msvc_cmpxchg_i8((__int8 volatile*)obj, (__int8)cmp, (__int8)desired); return val == cmp;
}
static inline _Bool __c11_atomic_compare_exchange_strong__atomic_short(atomic_short* obj, short* expected, short desired)
{
    short cmp = *expected, val = __msvc_cmpxchg_i16((__int16 volatile*)obj, (__int16)cmp, (__int16)desired); return val == cmp;
}
static inline _Bool __c11_atomic_compare_exchange_strong__atomic_int(atomic_int* obj, int* expected, int desired)
{
    int cmp = *expected, val = __msvc_cmpxchg_i32((__int32 volatile*)obj, (__int32)cmp, (__int32)desired); return val == cmp;
}
static inline _Bool __c11_atomic_compare_exchange_strong__atomic_long(atomic_long* obj, long* expected, long desired)
{
    long cmp = *expected, val = __msvc_cmpxchg_i32((__int32 volatile*)obj, (__int32)cmp, (__int32)desired); return val == cmp;
}
static inline _Bool __c11_atomic_compare_exchange_strong__atomic_llong(atomic_llong* obj, llong* expected, llong desired)
{
    llong cmp = *expected, val = __msvc_cmpxchg_i64((__int64 volatile*)obj, (__int64)cmp, (__int64)desired); return val == cmp;
}
static inline _Bool __c11_atomic_compare_exchange_strong__atomic_uchar(atomic_uchar* obj, uchar* expected, uchar desired)
{
    uchar cmp = *expected, val = __msvc_cmpxchg_i8((__int8 volatile*)obj, (__int8)cmp, (__int8)desired); return val == cmp;
}
static inline _Bool __c11_atomic_compare_exchange_strong__atomic_ushort(atomic_ushort* obj, ushort* expected, ushort desired)
{
    ushort cmp = *expected, val = __msvc_cmpxchg_i16((__int16 volatile*)obj, (__int16)cmp, (__int16)desired); return val == cmp;
}
static inline _Bool __c11_atomic_compare_exchange_strong__atomic_uint(atomic_uint* obj, uint* expected, uint desired)
{
    uint cmp = *expected, val = __msvc_cmpxchg_i32((__int32 volatile*)obj, (__int32)cmp, (__int32)desired); return val == cmp;
}
static inline _Bool __c11_atomic_compare_exchange_strong__atomic_ulong(atomic_ulong* obj, ulong* expected, ulong desired)
{
    ulong cmp = *expected, val = __msvc_cmpxchg_i32((__int32 volatile*)obj, (__int32)cmp, (__int32)desired); return val == cmp;
}
static inline _Bool __c11_atomic_compare_exchange_strong__atomic_ullong(atomic_ullong* obj, ullong* expected, ullong desired)
{
    ullong cmp = *expected, val = __msvc_cmpxchg_i64((__int64 volatile*)obj, (__int64)cmp, (__int64)desired); return val == cmp;
}
static inline _Bool __c11_atomic_compare_exchange_strong__atomic_ptr(atomic_ptr* obj, void** expected, void* desired)
{
    ptrdiff_t cmp = *(ptrdiff_t*)expected, val = __msvc_cmpxchg_ptr(__ptr)((__intptr volatile*)obj, (ptrdiff_t)cmp, (ptrdiff_t)desired); return (ptrdiff_t)val == cmp;
}


#define __c11_atomic_compare_exchange_strong(obj,expected,desired)       \
_Generic((obj),                                                          \
atomic_char*: __c11_atomic_compare_exchange_strong__atomic_char,         \
atomic_uchar*: __c11_atomic_compare_exchange_strong__atomic_uchar,       \
atomic_short*: __c11_atomic_compare_exchange_strong__atomic_short,       \
atomic_ushort*: __c11_atomic_compare_exchange_strong__atomic_ushort,     \
atomic_int*: __c11_atomic_compare_exchange_strong__atomic_int,           \
atomic_uint*: __c11_atomic_compare_exchange_strong__atomic_uint,         \
atomic_long*: __c11_atomic_compare_exchange_strong__atomic_long,         \
atomic_ulong*: __c11_atomic_compare_exchange_strong__atomic_ulong,       \
atomic_llong*: __c11_atomic_compare_exchange_strong__atomic_llong,       \
atomic_ullong*: __c11_atomic_compare_exchange_strong__atomic_ullong,     \
atomic_ptr*: __c11_atomic_compare_exchange_strong__atomic_ptr            \
)(obj,expected,desired)

#define atomic_compare_exchange_weak(obj,expected,desired) __c11_atomic_compare_exchange_strong(obj,expected,desired)
#define atomic_compare_exchange_strong(obj,expected,desired) __c11_atomic_compare_exchange_strong(obj,expected,desired)
#define atomic_compare_exchange_weak_explicit(obj,expected,desired,smo,fmo) __c11_atomic_compare_exchange_strong(obj,expected,desired)
#define atomic_compare_exchange_strong_explicit(obj,expected,desired,smo,fmo) __c11_atomic_compare_exchange_strong(obj,expected,desired)

#if !(defined __STDC_VERSION__ && __STDC_VERSION__ > 201710L)
#define ATOMIC_VAR_INIT(VALUE)	(VALUE)
#endif

/*
 * atomic_fetch_add
 */

static inline __int8  __msvc_xadd_i8(__int8  volatile* addr, __int8  val)
{
    return _InterlockedExchangeAdd8(addr, val);
}
static inline __int16 __msvc_xadd_i16(__int16 volatile* addr, __int16 val)
{
    return _InterlockedExchangeAdd16(addr, val);
}
static inline __int32 __msvc_xadd_i32(__int32 volatile* addr, __int32 val)
{
    return _InterlockedExchangeAdd(addr, val);
}
static inline __int64 __msvc_xadd_i64(__int64 volatile* addr, __int64 val)
{
    return _InterlockedExchangeAdd64(addr, val);
}

#define __msvc_xadd_ptr(ptr) __concat2(__msvc_xadd_,ptr)

static inline char __c11_atomic_fetch_add__atomic_char(atomic_char* obj, char arg)
{
    return (char)__msvc_xadd_i8((__int8 volatile*)obj, (__int8)arg);
}
static inline short __c11_atomic_fetch_add__atomic_short(atomic_short* obj, short arg)
{
    return (short)__msvc_xadd_i16((__int16 volatile*)obj, (__int16)arg);
}
static inline int __c11_atomic_fetch_add__atomic_int(atomic_int* obj, int arg)
{
    return (int)__msvc_xadd_i32((__int32 volatile*)obj, (__int32)arg);
}
static inline long __c11_atomic_fetch_add__atomic_long(atomic_long* obj, long arg)
{
    return (long)__msvc_xadd_i32((__int32 volatile*)obj, (__int32)arg);
}
static inline llong __c11_atomic_fetch_add__atomic_llong(atomic_llong* obj, llong arg)
{
    return (llong)__msvc_xadd_i64((__int64 volatile*)obj, (__int64)arg);
}
static inline uchar __c11_atomic_fetch_add__atomic_uchar(atomic_uchar* obj, uchar arg)
{
    return (uchar)__msvc_xadd_i8((__int8 volatile*)obj, (__int8)arg);
}
static inline ushort __c11_atomic_fetch_add__atomic_ushort(atomic_ushort* obj, ushort arg)
{
    return (ushort)__msvc_xadd_i16((__int16 volatile*)obj, (__int16)arg);
}
static inline uint __c11_atomic_fetch_add__atomic_uint(atomic_uint* obj, uint arg)
{
    return (uint)__msvc_xadd_i32((__int32 volatile*)obj, (__int32)arg);
}
static inline ulong __c11_atomic_fetch_add__atomic_ulong(atomic_ulong* obj, ulong arg)
{
    return (ulong)__msvc_xadd_i32((__int32 volatile*)obj, (__int32)arg);
}
static inline ullong __c11_atomic_fetch_add__atomic_ullong(atomic_ullong* obj, ullong arg)
{
    return (ullong)__msvc_xadd_i64((__int64 volatile*)obj, (__int64)arg);
}
static inline void* __c11_atomic_fetch_add__atomic_ptr(atomic_ptr* obj, void* arg)
{
    return (void*)__msvc_xadd_ptr(__ptr)((__intptr volatile*)obj, (__intptr)arg);
}

#define __c11_atomic_fetch_add(obj,arg)                    \
_Generic((obj),                                            \
atomic_char*: __c11_atomic_fetch_add__atomic_char,         \
atomic_uchar*: __c11_atomic_fetch_add__atomic_uchar,       \
atomic_short*: __c11_atomic_fetch_add__atomic_short,       \
atomic_ushort*: __c11_atomic_fetch_add__atomic_ushort,     \
atomic_int*: __c11_atomic_fetch_add__atomic_int,           \
atomic_uint*: __c11_atomic_fetch_add__atomic_uint,         \
atomic_long*: __c11_atomic_fetch_add__atomic_long,         \
atomic_ulong*: __c11_atomic_fetch_add__atomic_ulong,       \
atomic_llong*: __c11_atomic_fetch_add__atomic_llong,       \
atomic_ullong*: __c11_atomic_fetch_add__atomic_ullong,     \
atomic_ptr*: __c11_atomic_fetch_add__atomic_ptr            \
)(obj,arg)

#define atomic_fetch_add(obj,arg) __c11_atomic_fetch_add(obj,arg)
#define atomic_fetch_sub(obj,arg) __c11_atomic_fetch_add(obj,-(arg))
#define atomic_fetch_add_explicit(obj,arg,mo) __c11_atomic_fetch_add(obj,arg)
#define atomic_fetch_sub_explicit(obj,arg,mo) __c11_atomic_fetch_add(obj,-(arg))

/*
 * atomic_load
 */

static inline char __c11_atomic_load__atomic_char(atomic_char* obj)
{
    char val; _ReadBarrier(); val = *obj; _ReadWriteBarrier(); return val;
}
static inline short __c11_atomic_load__atomic_short(atomic_short* obj)
{
    short val; _ReadBarrier(); val = *obj; _ReadWriteBarrier(); return val;
}
static inline int __c11_atomic_load__atomic_int(atomic_int* obj)
{
    int val; _ReadBarrier(); val = *obj; _ReadWriteBarrier(); return val;
}
static inline long __c11_atomic_load__atomic_long(atomic_long* obj)
{
    long val; _ReadBarrier(); val = *obj; _ReadWriteBarrier(); return val;
}
static inline llong __c11_atomic_load__atomic_llong(atomic_llong* obj)
{
    llong val; _ReadBarrier(); val = *obj; _ReadWriteBarrier(); return val;
}
static inline uchar __c11_atomic_load__atomic_uchar(atomic_uchar* obj)
{
    uchar val; _ReadBarrier(); val = *obj; _ReadWriteBarrier(); return val;
}
static inline ushort __c11_atomic_load__atomic_ushort(atomic_ushort* obj)
{
    ushort val; _ReadBarrier(); val = *obj; _ReadWriteBarrier(); return val;
}
static inline uint __c11_atomic_load__atomic_uint(atomic_uint* obj)
{
    uint val; _ReadBarrier(); val = *obj; _ReadWriteBarrier(); return val;
}
static inline ulong __c11_atomic_load__atomic_ulong(atomic_ulong* obj)
{
    ulong val; _ReadBarrier(); val = *obj; _ReadWriteBarrier(); return val;
}
static inline ullong __c11_atomic_load__atomic_ullong(atomic_ullong* obj)
{
    ullong val; _ReadBarrier(); val = *obj; _ReadWriteBarrier(); return val;
}
static inline void* __c11_atomic_load__atomic_ptr(atomic_ptr* obj)
{
    void* val; _ReadBarrier(); val = *obj; _ReadWriteBarrier(); return val;
}

#define __c11_atomic_load(obj)                        \
_Generic((obj),                                       \
atomic_char*: __c11_atomic_load__atomic_char,         \
atomic_uchar*: __c11_atomic_load__atomic_uchar,       \
atomic_short*: __c11_atomic_load__atomic_short,       \
atomic_ushort*: __c11_atomic_load__atomic_ushort,     \
atomic_int*: __c11_atomic_load__atomic_int,           \
atomic_uint*: __c11_atomic_load__atomic_uint,         \
atomic_long*: __c11_atomic_load__atomic_long,         \
atomic_ulong*: __c11_atomic_load__atomic_ulong,       \
atomic_llong*: __c11_atomic_load__atomic_llong,       \
atomic_ullong*: __c11_atomic_load__atomic_ullong,     \
atomic_ptr*: __c11_atomic_load__atomic_ptr            \
)(obj)

#define atomic_load(obj) __c11_atomic_load(obj)
#define atomic_load_explicit(obj,mo) __c11_atomic_load(obj)

/*
 * atomic_fetch_{op} template for {and,or,xor} using atomic_compare_exchange
 */

#define __C11_ATOMIC_FETCH_OP_TEMPLATE(prefix,type,op) static inline type           \
    __concat3(prefix,atomic_,type)(__concat2(atomic_,type) *obj, type arg) {        \
    type oldval, newval;                                                            \
    do { oldval = atomic_load(obj); newval = oldval op arg; }                       \
    while (!atomic_compare_exchange_strong(obj, &oldval, newval));                  \
    return oldval;                                                                  \
}

#define __C11_ATOMIC_FETCH_OP_POINTER_TEMPLATE(prefix,op) static inline void*       \
    __concat2(prefix,atomic_ptr)(atomic_ptr *obj, void* arg) {                      \
    ptrdiff_t oldval, newval;                                                       \
    do { oldval = (ptrdiff_t)atomic_load(obj); newval = oldval op (ptrdiff_t)arg; } \
    while (!atomic_compare_exchange_strong(obj, (void**)&oldval, (void*)newval));   \
    return (void*)oldval;						            \
}

 /*
  * atomic_fetch_and
  */

__C11_ATOMIC_FETCH_OP_TEMPLATE(__c11_atomic_fetch_and__, char, &)
__C11_ATOMIC_FETCH_OP_TEMPLATE(__c11_atomic_fetch_and__, short, &)
__C11_ATOMIC_FETCH_OP_TEMPLATE(__c11_atomic_fetch_and__, int, &)
__C11_ATOMIC_FETCH_OP_TEMPLATE(__c11_atomic_fetch_and__, long, &)
__C11_ATOMIC_FETCH_OP_TEMPLATE(__c11_atomic_fetch_and__, llong, &)
__C11_ATOMIC_FETCH_OP_TEMPLATE(__c11_atomic_fetch_and__, uchar, &)
__C11_ATOMIC_FETCH_OP_TEMPLATE(__c11_atomic_fetch_and__, ushort, &)
__C11_ATOMIC_FETCH_OP_TEMPLATE(__c11_atomic_fetch_and__, uint, &)
__C11_ATOMIC_FETCH_OP_TEMPLATE(__c11_atomic_fetch_and__, ulong, &)
__C11_ATOMIC_FETCH_OP_TEMPLATE(__c11_atomic_fetch_and__, ullong, &)
__C11_ATOMIC_FETCH_OP_POINTER_TEMPLATE(__c11_atomic_fetch_and__, &)

#define __c11_atomic_fetch_and(obj,arg)                    \
_Generic((obj),                                            \
atomic_char*: __c11_atomic_fetch_and__atomic_char,         \
atomic_uchar*: __c11_atomic_fetch_and__atomic_uchar,       \
atomic_short*: __c11_atomic_fetch_and__atomic_short,       \
atomic_ushort*: __c11_atomic_fetch_and__atomic_ushort,     \
atomic_int*: __c11_atomic_fetch_and__atomic_int,           \
atomic_uint*: __c11_atomic_fetch_and__atomic_uint,         \
atomic_long*: __c11_atomic_fetch_and__atomic_long,         \
atomic_ulong*: __c11_atomic_fetch_and__atomic_ulong,       \
atomic_llong*: __c11_atomic_fetch_and__atomic_llong,       \
atomic_ullong*: __c11_atomic_fetch_and__atomic_ullong,	   \
atomic_ptr*: __c11_atomic_fetch_and__atomic_ptr            \
)(obj,arg)

#define atomic_fetch_and(obj,arg) __c11_atomic_fetch_and(obj,arg)
#define atomic_fetch_and_explicit(obj,arg,mo) __c11_atomic_fetch_and(obj,arg)

/*
 * atomic_fetch_or
 */

    __C11_ATOMIC_FETCH_OP_TEMPLATE(__c11_atomic_fetch_or__, char, | )
    __C11_ATOMIC_FETCH_OP_TEMPLATE(__c11_atomic_fetch_or__, short, | )
    __C11_ATOMIC_FETCH_OP_TEMPLATE(__c11_atomic_fetch_or__, int, | )
    __C11_ATOMIC_FETCH_OP_TEMPLATE(__c11_atomic_fetch_or__, long, | )
    __C11_ATOMIC_FETCH_OP_TEMPLATE(__c11_atomic_fetch_or__, llong, | )
    __C11_ATOMIC_FETCH_OP_TEMPLATE(__c11_atomic_fetch_or__, uchar, | )
    __C11_ATOMIC_FETCH_OP_TEMPLATE(__c11_atomic_fetch_or__, ushort, | )
    __C11_ATOMIC_FETCH_OP_TEMPLATE(__c11_atomic_fetch_or__, uint, | )
    __C11_ATOMIC_FETCH_OP_TEMPLATE(__c11_atomic_fetch_or__, ulong, | )
    __C11_ATOMIC_FETCH_OP_TEMPLATE(__c11_atomic_fetch_or__, ullong, | )
    __C11_ATOMIC_FETCH_OP_POINTER_TEMPLATE(__c11_atomic_fetch_or__, | )

#define __c11_atomic_fetch_or(obj,arg)			   \
_Generic((obj),                                            \
atomic_char*: __c11_atomic_fetch_or__atomic_char,          \
atomic_uchar*: __c11_atomic_fetch_or__atomic_uchar,        \
atomic_short*: __c11_atomic_fetch_or__atomic_short,        \
atomic_ushort*: __c11_atomic_fetch_or__atomic_ushort,      \
atomic_int*: __c11_atomic_fetch_or__atomic_int,            \
atomic_uint*: __c11_atomic_fetch_or__atomic_uint,          \
atomic_long*: __c11_atomic_fetch_or__atomic_long,          \
atomic_ulong*: __c11_atomic_fetch_or__atomic_ulong,	   \
atomic_llong*: __c11_atomic_fetch_or__atomic_llong,        \
atomic_ullong*: __c11_atomic_fetch_or__atomic_ullong,	   \
atomic_ptr*: __c11_atomic_fetch_or__atomic_ptr             \
)(obj,arg)

#define atomic_fetch_or(obj,arg) __c11_atomic_fetch_or(obj,arg)
#define atomic_fetch_or_explicit(obj,arg,mo) __c11_atomic_fetch_or(obj,arg)

    /*
     * atomic_fetch_xor
     */

    __C11_ATOMIC_FETCH_OP_TEMPLATE(__c11_atomic_fetch_xor__, char, ^)
    __C11_ATOMIC_FETCH_OP_TEMPLATE(__c11_atomic_fetch_xor__, short, ^)
    __C11_ATOMIC_FETCH_OP_TEMPLATE(__c11_atomic_fetch_xor__, int, ^)
    __C11_ATOMIC_FETCH_OP_TEMPLATE(__c11_atomic_fetch_xor__, long, ^)
    __C11_ATOMIC_FETCH_OP_TEMPLATE(__c11_atomic_fetch_xor__, llong, ^)
    __C11_ATOMIC_FETCH_OP_TEMPLATE(__c11_atomic_fetch_xor__, uchar, ^)
    __C11_ATOMIC_FETCH_OP_TEMPLATE(__c11_atomic_fetch_xor__, ushort, ^)
    __C11_ATOMIC_FETCH_OP_TEMPLATE(__c11_atomic_fetch_xor__, uint, ^)
    __C11_ATOMIC_FETCH_OP_TEMPLATE(__c11_atomic_fetch_xor__, ulong, ^)
    __C11_ATOMIC_FETCH_OP_TEMPLATE(__c11_atomic_fetch_xor__, ullong, ^)
    __C11_ATOMIC_FETCH_OP_POINTER_TEMPLATE(__c11_atomic_fetch_xor__, ^)

#define __c11_atomic_fetch_xor(obj,arg)                    \
_Generic((obj),                                            \
atomic_char*: __c11_atomic_fetch_xor__atomic_char,         \
atomic_uchar*: __c11_atomic_fetch_xor__atomic_uchar,       \
atomic_short*: __c11_atomic_fetch_xor__atomic_short,       \
atomic_ushort*: __c11_atomic_fetch_xor__atomic_ushort,     \
atomic_int*: __c11_atomic_fetch_xor__atomic_int,           \
atomic_uint*: __c11_atomic_fetch_xor__atomic_uint,         \
atomic_long*: __c11_atomic_fetch_xor__atomic_long,         \
atomic_ulong*: __c11_atomic_fetch_xor__atomic_ulong,	   \
atomic_llong*: __c11_atomic_fetch_xor__atomic_llong,       \
atomic_ullong*: __c11_atomic_fetch_xor__atomic_ullong,	   \
atomic_ptr*: __c11_atomic_fetch_xor__atomic_ptr            \
)(obj,arg)

#define atomic_fetch_xor(obj,arg) __c11_atomic_fetch_xor(obj,arg)
#define atomic_fetch_xor_explicit(obj,arg,mo) __c11_atomic_fetch_xor(obj,arg)

    /*
     * atomic_flag_test_and_set, atomic_flag_clear
     */

    static inline _Bool atomic_flag_test_and_set(volatile atomic_flag* obj)
{
    char o = 0;
    return atomic_compare_exchange_strong((atomic_char*)&obj->_Value, &o, 1) ? 0 : 1;
}

static inline void atomic_flag_clear(volatile atomic_flag* obj)
{
    atomic_store_explicit((atomic_char*)&obj->_Value, 0, memory_order_release);
}

#define atomic_flag_test_and_set_explicit(obj,mo) atomic_flag_test_and_set(obj)
#define atomic_flag_clear_explicit(obj,mo) atomic_flag_clear(obj)

#endif