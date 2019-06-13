/*
 * Copyright (c) 2018 Ruslan Nikolaev.  All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef __BITS_LF_H
#define __BITS_LF_H	1

#include <inttypes.h>
#include <sys/types.h>
#include <stdbool.h>
#include <stddef.h>

#if UINTPTR_MAX == UINT32_C(0xFFFFFFFF)
# define __LFPTR_WIDTH	32
typedef uint64_t lfref_t;
#elif UINTPTR_MAX == UINT64_C(0xFFFFFFFFFFFFFFFF)
# define __LFPTR_WIDTH	64
typedef __uint128_t lfref_t;
#else
# error "Unsupported word length."
#endif

#include "config.h"

#ifdef __GNUC__
# define __LF_ASSUME(c) do { if (!(c)) __builtin_unreachable(); } while (0)
#else
# define __LF_ASSUME(c)
#endif

/* GCC does not have a sane implementation of wide atomics for x86-64
   even when specifying -mcx16 in recent versions, so use inline assembly
   workarounds whenever possible. No AArch64 support in GCC for right now.
   Conversely, LLVM works fine but requires a small workaround for x86
   (32-bit) CPUs when atomically loading 64-bit values. */
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__) &&	\
	!defined(__llvm__) && defined(__GCC_ASM_FLAG_OUTPUTS__)
# include "gcc_x86.h"
#elif (defined(__i386__) || defined(__x86_64__)) && defined(__llvm__)
# include "llvm_x86.h"
#elif defined(__powerpc__) && defined(__GNUC__)
# include "ppc.h"
#elif defined(__mips__)
# include "mips.h"
#else
# include "c11.h"
#endif

/* Reference counting. */
#define __LFREF_IMPL(w, dtype_t)											\
static const size_t __lfref_shift##w = sizeof(dtype_t) * 4;					\
static const size_t __lfrptr_shift##w = 0;									\
static const dtype_t __lfref_mask##w =										\
				~(dtype_t) 0U << (sizeof(dtype_t) * 4);						\
static const dtype_t __lfref_step##w =										\
				(dtype_t) 1U << (sizeof(dtype_t) * 4);

/* Pointer index for double-width types. */
#ifdef __LITTLE_ENDIAN__
# define __LFREF_LINK	0
#else
# define __LFREF_LINK	1
#endif

/* ABA tagging with split (word-atomic) load/cmpxchg operation. */
#if __LFLOAD_SPLIT(LFATOMIC_BIG_WIDTH) == 1 ||								\
		__LFCMPXCHG_SPLIT(LFATOMIC_BIG_WIDTH) == 1
# define __LFABA_IMPL(w, type_t)											\
static const size_t __lfaba_shift##w = sizeof(lfatomic_big_t) * 4;			\
static const size_t __lfaptr_shift##w = 0;									\
static const lfatomic_big_t __lfaba_mask##w =								\
				~(lfatomic_big_t) 0U << (sizeof(lfatomic_big_t) * 4);		\
static const lfatomic_big_t __lfaba_step##w =								\
				(lfatomic_big_t) 1U << (sizeof(lfatomic_big_t) * 4);
#endif

/* ABA tagging when load/cmpxchg is not split. Note that unlike previous
   case, __lfaptr_shift is required to be 0. */
#if __LFLOAD_SPLIT(LFATOMIC_BIG_WIDTH) == 0 &&								\
		__LFCMPXCHG_SPLIT(LFATOMIC_BIG_WIDTH) == 0
# define __LFABA_IMPL(w, type_t)											\
static const size_t __lfaba_shift##w = sizeof(type_t) * 8;					\
static const size_t __lfaptr_shift##w = 0;									\
static const lfatomic_big_t __lfaba_mask##w =								\
				~(lfatomic_big_t) 0U << (sizeof(type_t) * 8);				\
static const lfatomic_big_t __lfaba_step##w =								\
				(lfatomic_big_t) 1U << (sizeof(type_t) * 8);
#endif

typedef bool (*lf_check_t) (void * data, void * addr, size_t size);

static inline size_t lf_pow2(size_t order)
{
	return (size_t) 1U << order;
}

static inline bool LF_DONTCHECK(void * head, void * addr, size_t size)
{
	return true;
}

#define lf_container_of(addr, type, field)									\
	(type *) ((char *) (addr) - offsetof(type, field))

#ifdef __cplusplus
# define LF_ERROR	UINTPTR_MAX
#else
# define LF_ERROR	((void *) UINTPTR_MAX)
#endif

/* Available on all 64-bit and CAS2 32-bit architectures. */
#if LFATOMIC_BIG_WIDTH >= 64
__LFABA_IMPL(32, uint32_t)
__LFREF_IMPL(32, uint64_t)
__LFREF_ATOMICS_IMPL(32, uint32_t, uint64_t)
#endif

/* Available on CAS2 64-bit architectures. */
#if LFATOMIC_BIG_WIDTH >= 128
__LFABA_IMPL(64, uint64_t)
__LFREF_IMPL(64, __uint128_t)
__LFREF_ATOMICS_IMPL(64, uint64_t, __uint128_t)
#endif

/* Available on CAS2 32/64-bit architectures. */
#if LFATOMIC_BIG_WIDTH >= 2 * __LFPTR_WIDTH
__LFABA_IMPL(, uintptr_t)
__LFREF_IMPL(, lfref_t)
__LFREF_ATOMICS_IMPL(, uintptr_t, lfref_t)
#endif

#endif	/* !__BITS_LF_H */

/* vi: set tabstop=4: */
