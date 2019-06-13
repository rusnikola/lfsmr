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

#ifndef __LF_GCC_X86_H
#define __LF_GCC_X86_H 1

#include <stdatomic.h>

#define LFATOMIC(x)				_Atomic(x)
#define LFATOMIC_VAR_INIT(x)	ATOMIC_VAR_INIT(x)

static inline void __lfbig_init(_Atomic(lfatomic_big_t) * obj,
		lfatomic_big_t val)
{
	*((volatile lfatomic_big_t *) obj) = val;
}

static inline lfatomic_big_t __lfbig_load(_Atomic(lfatomic_big_t) * obj,
		memory_order order)
{
	return *((volatile lfatomic_big_t *) obj);
}

static inline bool __lfbig_cmpxchg_strong(_Atomic(lfatomic_big_t) * obj,
		lfatomic_big_t * expected, lfatomic_big_t desired,
		memory_order succ, memory_order fail)
{
	lfatomic_t low = (lfatomic_t) desired;
	lfatomic_t high = (lfatomic_t) (desired >> (sizeof(lfatomic_t) * 8));
	bool result;

#if defined(__x86_64__)
# define __LFX86_CMPXCHG "cmpxchg16b"
#elif defined(__i386__)
# define __LFX86_CMPXCHG "cmpxchg8b"
#endif
	__asm__ __volatile__ ("lock " __LFX86_CMPXCHG " %0"
						  : "+m" (*obj), "=@ccz" (result), "+A" (*expected)
						  : "b" (low), "c" (high)
	);
#undef __LFX86_CMPXCHG

	return result;
}

static inline bool __lfbig_cmpxchg_weak(_Atomic(lfatomic_big_t) * obj,
		lfatomic_big_t * expected, lfatomic_big_t desired,
		memory_order succ, memory_order fail)
{
	return __lfbig_cmpxchg_strong(obj, expected, desired, succ, fail);
}

static inline lfatomic_big_t __lfbig_fetch_add(_Atomic(lfatomic_big_t) * obj,
		lfatomic_big_t arg, memory_order order)
{
	lfatomic_big_t new_val, old_val = __lfbig_load(obj, order);
	do {
		new_val = old_val + arg;
	} while (!__lfbig_cmpxchg_weak(obj, &old_val, new_val, order, order));
	__LF_ASSUME(new_val == old_val + arg);
	return old_val;
}

#define __lfaba_init			__lfbig_init
#define __lfaba_load			__lfbig_load
#define __lfaba_cmpxchg_weak	__lfbig_cmpxchg_weak
#define __lfaba_cmpxchg_strong	__lfbig_cmpxchg_strong

static inline void __lfepoch_init(_Atomic(lfepoch_t) * obj, lfepoch_t val)
{
	atomic_init(obj, val);
}

static inline lfepoch_t __lfepoch_load(_Atomic(lfepoch_t) * obj,
		memory_order order)
{
	/* Already uses atomic 64-bit FPU loads for i386. */
	return atomic_load_explicit(obj, order);
}

static inline bool __lfepoch_cmpxchg_weak(_Atomic(lfepoch_t) * obj,
		lfepoch_t * expected, lfepoch_t desired,
		memory_order succ, memory_order fail)
{
#ifdef __i386__
	return __lfbig_cmpxchg_weak((_Atomic(lfatomic_big_t) *) obj,
				(lfatomic_big_t *) expected, desired, succ, fail);
#else
	return atomic_compare_exchange_weak_explicit(obj, expected, desired,
				succ, fail);
#endif
}

static inline lfepoch_t __lfepoch_fetch_add(_Atomic(lfepoch_t) * obj,
		lfepoch_t arg, memory_order order)
{
#ifdef __i386__
	return __lfbig_fetch_add((_Atomic(lfatomic_big_t) *) obj, arg, order);
#else
	return atomic_fetch_add_explicit(obj, arg, order);
#endif
}

#define __LFREF_CMPXCHG_FULL(dtype_t)	(1)

#define __LFREF_ATOMICS_IMPL(w, type_t, dtype_t)							\
static inline void __lfref_init##w(_Atomic(dtype_t) * obj, dtype_t val)		\
{																			\
	if (sizeof(dtype_t) != sizeof(lfatomic_big_t)) {						\
		atomic_init(obj, val);												\
	} else {																\
		__lfbig_init((_Atomic(lfatomic_big_t) *) obj, val);					\
	}																		\
}																			\
																			\
static inline dtype_t __lfref_load##w(_Atomic(dtype_t) * obj,				\
		memory_order order)													\
{																			\
	if (sizeof(dtype_t) != sizeof(lfatomic_big_t)) {						\
		return atomic_load_explicit(obj, order);							\
	} else {																\
		return __lfbig_load((_Atomic(lfatomic_big_t) *) obj, order);		\
	}																		\
}																			\
																			\
static inline type_t __lfref_link##w(_Atomic(dtype_t) * obj,				\
		memory_order order)													\
{																			\
	if (sizeof(dtype_t) != sizeof(lfatomic_big_t)) {						\
		return (atomic_load_explicit(obj, order) & ~__lfref_mask##w) >>		\
					__lfrptr_shift##w;										\
	} else {																\
		return *((volatile type_t *) obj + __LFREF_LINK);					\
	}																		\
}																			\
																			\
static inline bool __lfref_cmpxchgptr_weak##w(_Atomic(dtype_t) * obj,		\
		dtype_t * expected, dtype_t desired,								\
		memory_order succ, memory_order fail)								\
{																			\
	if (sizeof(dtype_t) != sizeof(lfatomic_big_t)) {						\
		return atomic_compare_exchange_weak_explicit(obj,					\
			expected, desired, succ, fail);									\
	} else {																\
		return __lfbig_cmpxchg_weak((_Atomic(lfatomic_big_t) *) obj,		\
				(lfatomic_big_t *) expected, desired, succ, fail);			\
	}																		\
}																			\
																			\
static inline bool __lfref_cmpxchgptr_strong##w(_Atomic(dtype_t) * obj,		\
		dtype_t * expected, dtype_t desired,								\
		memory_order succ, memory_order fail)								\
{																			\
	if (sizeof(dtype_t) != sizeof(lfatomic_big_t)) {						\
		return atomic_compare_exchange_strong_explicit(obj,					\
			expected, desired, succ, fail);									\
	} else {																\
		return __lfbig_cmpxchg_strong((_Atomic(lfatomic_big_t) *) obj,		\
				(lfatomic_big_t *) expected, desired, succ, fail);			\
	}																		\
}																			\
																			\
static inline bool __lfref_cmpxchgref_weak##w(_Atomic(dtype_t) * obj,		\
		dtype_t * expected, dtype_t desired,								\
		memory_order succ, memory_order fail)								\
{																			\
	if (sizeof(dtype_t) != sizeof(lfatomic_big_t)) {						\
		return atomic_compare_exchange_weak_explicit(obj,					\
			expected, desired, succ, fail);									\
	} else {																\
		return __lfbig_cmpxchg_weak((_Atomic(lfatomic_big_t) *) obj,		\
				(lfatomic_big_t *) expected, desired, succ, fail);			\
	}																		\
}																			\
																			\
static inline bool __lfref_cmpxchgref_strong##w(_Atomic(dtype_t) * obj,		\
		dtype_t * expected, dtype_t desired,								\
		memory_order succ, memory_order fail)								\
{																			\
	if (sizeof(dtype_t) != sizeof(lfatomic_big_t)) {						\
		return atomic_compare_exchange_strong_explicit(obj,					\
			expected, desired, succ, fail);									\
	} else {																\
		return __lfbig_cmpxchg_strong((_Atomic(lfatomic_big_t) *) obj,		\
				(lfatomic_big_t *) expected, desired, succ, fail);			\
	}																		\
}																			\
																			\
static inline dtype_t __lfref_fetch_add##w(_Atomic(dtype_t) * obj,			\
		dtype_t arg, memory_order order)									\
{																			\
	if (sizeof(dtype_t) != sizeof(lfatomic_big_t)) {						\
		return atomic_fetch_add_explicit(obj, arg, order);					\
	} else {																\
		return __lfbig_fetch_add((_Atomic(lfatomic_big_t) *) obj,			\
				arg, order);												\
	}																		\
}

#endif /* !__LF_GGC_X86_H */

/* vi: set tabstop=4: */
