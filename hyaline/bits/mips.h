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

#ifndef __LF_MIPS_H
#define __LF_MIPS_H 1

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
	lfatomic_big_t res;
	_Atomic(lfatomic_t) * hobj = (_Atomic(lfatomic_t) *) obj;
	lfatomic_t * hres = (lfatomic_t *) &res;

	hres[0] = atomic_load_explicit(hobj, order);
	hres[1] = atomic_load_explicit(hobj + 1, order);
	return res;
}

#if _MIPS_SIM == _MIPS_SIM_ABI64
# define __LFMIPS_LL "lld"
# define __LFMIPS_LD "ld"
# define __LFMIPS_SC "scd"
#elif _MIPS_SIM == _MIPS_SIM_ABI32 || _MIPS_SIM == _MIPS_SIM_NABI32
# define __LFMIPS_LL "ll"
# define __LFMIPS_LD "lw"
# define __LFMIPS_SC "sc"
#endif

#if defined(__MIPSEL__)
# define __LFMIPS_OPTR		(0)
# define __LFMIPS_OREF		(sizeof(lfatomic_t))
#elif defined(__MIPSEB__)
# define __LFMIPS_OPTR		(sizeof(lfatomic_t))
# define __LFMIPS_OREF		(0)
#endif

#define __LFMIPS_OP(enter,exit,op,v,a)									\
	__asm__ __volatile__ (												\
	enter "\n"															\
	"	.set	noreorder\n"											\
	".E%=:	" __LFMIPS_LL "	%[ref],%[oref](%[obj])\n"					\
	"	subu	%[obj],%[obj],%[ref]\n"									\
	"	addu	%[obj],%[ref],%[obj]\n"									\
	"	" __LFMIPS_LD "	%[ptr],%[optr](%[obj])\n"						\
	"	" op "	%[val],%[ref],%[arg]\n"									\
	"	" __LFMIPS_SC "	%[val],%[oref](%[obj])\n"						\
	"	beq	%[val],$0,.E%=\n"											\
	"	nop\n"															\
	"	.set	reorder\n"												\
	"	" exit															\
	: [ptr] "=&r" (ptr), [ref] "=&r" (ref),								\
	  [val] "=&r" (val), "+m" (*obj)									\
	: [obj] "r" (obj), [oref] "I" (__LFMIPS_OREF),						\
	  [optr] "I" (__LFMIPS_OPTR), [arg] v (a)							\
)

#define __LFMIPS_CMPXCHG(loop,enter,exit,second_reg,...)				\
	__asm__ __volatile__ (												\
	enter "\n"															\
	"	.set	noreorder\n"											\
	".E%=:	" __LFMIPS_LL "	%[first],%[ofirst](%[obj])\n"				\
	"	subu	%[obj],%[obj],%[first]\n"								\
	"	addu	%[obj],%[first],%[obj]\n"								\
	"	bne	%[first],%[arg_first],.X%=\n"								\
	"	" __LFMIPS_LD "	%[second],%[osecond](%[obj])\n"					\
	"	bne	%[second]," second_reg ",.X%=\n"							\
	"	move	%[result],%[desired_first]\n"							\
	"	" __LFMIPS_SC "	%[result],%[ofirst](%[obj])\n"					\
	loop																\
	"	.set	reorder\n"												\
	"	" exit "\n"														\
	".X%=:"																\
	: [first] "=&r" (first), [second] "=&r" (second),					\
	  [result] "=&r" (result), "+m" (*obj)								\
	: [obj] "r" (obj), [ofirst] "I" (ofirst),							\
	  [osecond] "I" (osecond), [arg_first] "r" (arg_first),				\
	  [desired_first] "r" (desired_first), ##__VA_ARGS__				\
)

#define __LFMIPS_STRONG	"	beq	%[result],$0,.E%=\n"					\
						"	nop\n"
#define __LFMIPS_WEAK	""

#define __LFMIPS_CMPXCHG_STRONG_0(enter,exit)							\
		__LFMIPS_CMPXCHG(__LFMIPS_STRONG, enter, exit, "$0")
#define __LFMIPS_CMPXCHG_STRONG_R(enter,exit)							\
		__LFMIPS_CMPXCHG(__LFMIPS_STRONG, enter, exit, "%[arg_second]",	\
						 [arg_second] "r" (arg_second))

#define __LFMIPS_CMPXCHG_WEAK_0(enter,exit)								\
		__LFMIPS_CMPXCHG(__LFMIPS_WEAK, enter, exit, "$0",				\
						 "[result]" (0))
#define __LFMIPS_CMPXCHG_WEAK_R(enter,exit)								\
		__LFMIPS_CMPXCHG(__LFMIPS_WEAK, enter, exit, "%[arg_second]",	\
						 [arg_second] "r" (arg_second),					\
						 "[result]" (0))

static inline bool __lfbig_cmpxchgptr_strong(_Atomic(lfatomic_big_t) * obj,
		lfatomic_big_t * expected, lfatomic_big_t desired,
		memory_order succ, memory_order fail)
{
	lfatomic_t first, second;
	lfatomic_t arg_first = (lfatomic_t) *expected;
	lfatomic_t arg_second = (lfatomic_t) (*expected >> sizeof(lfatomic_t) * 8);
	lfatomic_t desired_first = (lfatomic_t) desired;
	size_t ofirst = __LFMIPS_OPTR;
	size_t osecond = __LFMIPS_OREF;
	lfatomic_t result;

	/* A special case when the reference counter is 0. */
	if (__builtin_constant_p(arg_second) && arg_second == 0)
		__LFMIPS_CMPXCHG_STRONG_0("sync", "sync");
	else
		__LFMIPS_CMPXCHG_STRONG_R("sync", "sync");

	*expected = ((lfatomic_big_t) second << sizeof(lfatomic_t) * 8) | first;
	return first == arg_first && second == arg_second;
}

static inline bool __lfbig_cmpxchgptr_weak(_Atomic(lfatomic_big_t) * obj,
		lfatomic_big_t * expected, lfatomic_big_t desired,
		memory_order succ, memory_order fail)
{
	lfatomic_t first, second;
	lfatomic_t arg_first = (lfatomic_t) *expected;
	lfatomic_t arg_second = (lfatomic_t) (*expected >> sizeof(lfatomic_t) * 8);
	lfatomic_t desired_first = (lfatomic_t) desired;
	size_t ofirst = __LFMIPS_OPTR;
	size_t osecond = __LFMIPS_OREF;
	lfatomic_t result;
	bool success;

	/* A special case when the reference counter is 0. */
	if (__builtin_constant_p(arg_second) && arg_second == 0)
		__LFMIPS_CMPXCHG_WEAK_0("sync", "sync");
	else
		__LFMIPS_CMPXCHG_WEAK_R("sync", "sync");

	/* Reload if failed. We need to check arg_second because the 'move'
	   instruction happens in the delay slot for the branch instruction. */
	success = (result && second == arg_second);
	*expected = success ?
			((lfatomic_big_t) second << sizeof(lfatomic_t) * 8) | first
				: __lfbig_load(obj, fail);
	return success;
}

static inline bool __lfbig_cmpxchgref_strong(_Atomic(lfatomic_big_t) * obj,
		lfatomic_big_t * expected, lfatomic_big_t desired,
		memory_order succ, memory_order fail)
{
	lfatomic_t first, second;
	lfatomic_t arg_first = (lfatomic_t) (*expected >> sizeof(lfatomic_t) * 8);
	lfatomic_t arg_second = (lfatomic_t) *expected;
	lfatomic_t desired_first = (lfatomic_t) (desired >> sizeof(lfatomic_t) * 8);
	size_t ofirst = __LFMIPS_OREF;
	size_t osecond = __LFMIPS_OPTR;
	lfatomic_t result;

	__LFMIPS_CMPXCHG_STRONG_R("sync", "sync");

	*expected = ((lfatomic_big_t) first << sizeof(lfatomic_t) * 8) | second;
	return first == arg_first && second == arg_second;
}

static inline bool __lfbig_cmpxchgref_weak(_Atomic(lfatomic_big_t) * obj,
		lfatomic_big_t * expected, lfatomic_big_t desired,
		memory_order succ, memory_order fail)
{
	lfatomic_t first, second;
	lfatomic_t arg_first = (lfatomic_t) (*expected >> sizeof(lfatomic_t) * 8);
	lfatomic_t arg_second = (lfatomic_t) *expected;
	lfatomic_t desired_first = (lfatomic_t) (desired >> sizeof(lfatomic_t) * 8);
	size_t ofirst = __LFMIPS_OREF;
	size_t osecond = __LFMIPS_OPTR;
	lfatomic_t result;
	bool success;

	__LFMIPS_CMPXCHG_WEAK_R("sync", "sync");

	/* Reload if failed. We need to check arg_second because the 'move'
	   instruction happens in the delay slot for the branch instruction. */
	success = (result && second == arg_second);
	*expected = success ?
			((lfatomic_big_t) first << sizeof(lfatomic_t) * 8) | second
				: __lfbig_load(obj, fail);
	return success;
}

static inline lfatomic_big_t __lfbig_fetch_add(_Atomic(lfatomic_big_t) * obj,
		lfatomic_big_t arg, memory_order order)
{
	lfatomic_t ptr, ref, val;
	lfatomic_t arg_ref = (lfatomic_t) (arg >> (sizeof(lfatomic_t) * 8));

	if (__builtin_constant_p(arg_ref) && arg_ref < 32768U)
		__LFMIPS_OP("sync", "sync", "addiu", "I", arg_ref);
	else
		__LFMIPS_OP("sync", "sync", "addu", "r", arg_ref);

	return ((lfatomic_big_t) ref << (sizeof(lfatomic_t) * 8)) | ptr;
}

#define __lfepoch_init			atomic_init
#define __lfepoch_load			atomic_load_explicit
#define __lfepoch_cmpxchg_weak	atomic_compare_exchange_weak_explicit
#define __lfepoch_fetch_add		atomic_fetch_add_explicit

#define __LFREF_CMPXCHG_FULL(dtype_t)	(sizeof(dtype_t) <= sizeof(lfatomic_t))

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
		_Atomic(type_t) * hobj = (_Atomic(type_t) *) obj;					\
		return atomic_load_explicit(&hobj[__LFREF_LINK], order);			\
	}																		\
}																			\
																			\
static inline dtype_t __lfref_cmpxchgptr_weak##w(_Atomic(dtype_t) * obj,	\
		dtype_t * expected, dtype_t desired,								\
		memory_order succ, memory_order fail)								\
{																			\
	if (sizeof(dtype_t) != sizeof(lfatomic_big_t)) {						\
		return atomic_compare_exchange_weak_explicit(obj,					\
			expected, desired, succ, fail);									\
	} else {																\
		return __lfbig_cmpxchgptr_weak((_Atomic(lfatomic_big_t) *) obj,		\
				(lfatomic_big_t *) expected, desired, succ, fail);			\
	}																		\
}																			\
																			\
static inline dtype_t __lfref_cmpxchgptr_strong##w(_Atomic(dtype_t) * obj,	\
		dtype_t * expected, dtype_t desired,								\
		memory_order succ, memory_order fail)								\
{																			\
	if (sizeof(dtype_t) != sizeof(lfatomic_big_t)) {						\
		return atomic_compare_exchange_strong_explicit(obj,					\
			expected, desired, succ, fail);									\
	} else {																\
		return __lfbig_cmpxchgptr_strong((_Atomic(lfatomic_big_t) *) obj,	\
				(lfatomic_big_t *) expected, desired, succ, fail);			\
	}																		\
}																			\
																			\
static inline dtype_t __lfref_cmpxchgref_weak##w(_Atomic(dtype_t) * obj,	\
		dtype_t * expected, dtype_t desired,								\
		memory_order succ, memory_order fail)								\
{																			\
	if (sizeof(dtype_t) != sizeof(lfatomic_big_t)) {						\
		return atomic_compare_exchange_weak_explicit(obj,					\
			expected, desired, succ, fail);									\
	} else {																\
		return __lfbig_cmpxchgref_weak((_Atomic(lfatomic_big_t) *) obj,		\
				(lfatomic_big_t *) expected, desired, succ, fail);			\
	}																		\
}																			\
																			\
static inline dtype_t __lfref_cmpxchgref_strong##w(_Atomic(dtype_t) * obj,	\
		dtype_t * expected, dtype_t desired,								\
		memory_order succ, memory_order fail)								\
{																			\
	if (sizeof(dtype_t) != sizeof(lfatomic_big_t)) {						\
		return atomic_compare_exchange_strong_explicit(obj,					\
			expected, desired, succ, fail);									\
	} else {																\
		return __lfbig_cmpxchgref_strong((_Atomic(lfatomic_big_t) *) obj,	\
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

#endif /* !__LF_MIPS_H */

/* vi: set tabstop=4: */
