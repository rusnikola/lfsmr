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

#ifndef __LF_PPC_H
#define __LF_PPC_H 1

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

#if defined(__powerpc64__)
# define __LFPPC_LL "ldarx"
# define __LFPPC_LD "ldx"
# define __LFPPC_SC "stdcx"
#elif defined(__powerpc__)
# define __LFPPC_LL "lwarx"
# define __LFPPC_LD "lwzx"
# define __LFPPC_SC "stwcx"
#endif

#define __LFPPC_CR_EQ	0x20000000U

#ifdef __LITTLE_ENDIAN__
# define __LFPPC_MPTR(x)	(x)
# define __LFPPC_MREF(x)	((__typeof(x))((char *)(x) + sizeof(lfatomic_t)))
#else
# define __LFPPC_MPTR(x)	((__typeof(x))((char *)(x) + sizeof(lfatomic_t)))
# define __LFPPC_MREF(x)	(x)
#endif

#define __LFPPC_OP(enter,exit,op,v,a)									\
	__asm__ __volatile__ (												\
	enter "\n"															\
	".E%=:	" __LFPPC_LL " %[ref],0,%[mref]\n"							\
	"	subf %[ptr],%[ref],%[mptr]\n"									\
	"	" __LFPPC_LD " %[ptr],%[ptr],%[ref]\n"							\
	"	" op " %[val],%[ref],%[arg]\n"									\
	"	" __LFPPC_SC ". %[val],0,%[mref]\n"								\
	"	bne- .E%=\n	"													\
	exit																\
	: [ptr] "=&b" (ptr), [ref] "=&r" (ref),								\
	  [val] "=&r" (val), "+m" (*obj)									\
	: [mptr] "r" (__LFPPC_MPTR(obj)), [arg] v (a),						\
	  [mref] "r" (__LFPPC_MREF(obj))									\
	: "cr0"																\
)

#define __LFPPC_CMPXCHG(loop,enter,exit,extra,tmp,out,mc,...)			\
	__asm__ __volatile__ (												\
	enter "\n"															\
	".E%=:	" __LFPPC_LL " %[first],0,%[mfirst]\n"						\
	"	subf %[second],%[first],%[msecond]\n"							\
	"	" __LFPPC_LD " %[second],%[second],%[first]\n"					\
	"	xor " out ",%[first],%[arg_first]\n"							\
	extra																\
	"	or. " out "," out "," tmp "\n"									\
	"	bne- .X%=\n"													\
	"	" __LFPPC_SC ". %[desired_first],0,%[mfirst]\n"					\
	loop																\
	exit																\
	: [first] "=&r" (first), [second] "=&b" (second),					\
	  "+m" (*obj), ##__VA_ARGS__										\
	: [mfirst] "r" (mfirst), [msecond] "r" (msecond),					\
	  [arg_first] "r" (arg_first), [arg_second] mc (arg_second),		\
	  [desired_first] "r" (desired_first)								\
	: "cr0"																\
)

#define __LFPPC_STRONG	"	bne- .E%=\n.X%=:	"
#define __LFPPC_WEAK	".X%=:	mfcr %[first]\n	"

#define __LFPPC_CMPXCHG_STRONG_0(enter,exit)							\
		__LFPPC_CMPXCHG(__LFPPC_STRONG, enter, exit, "",				\
			"%[second]", "%[result]", "I",								\
			[result] "=&r" (result))
#define __LFPPC_CMPXCHG_STRONG_R(enter,exit)							\
		__LFPPC_CMPXCHG(__LFPPC_STRONG, enter, exit,					\
			"	xor %[tmp],%[second],%[arg_second]\n",					\
			"%[tmp]", "%[result]", "r",									\
			[result] "=&r" (result), [tmp] "=&r" (tmp))

#define __LFPPC_CMPXCHG_WEAK_0(enter,exit)								\
		__LFPPC_CMPXCHG(__LFPPC_WEAK, enter, exit, "",					\
			"%[second]", "%[first]", "I")
#define __LFPPC_CMPXCHG_WEAK_R(enter,exit)								\
		__LFPPC_CMPXCHG(__LFPPC_WEAK, enter, exit,						\
			"	xor %[second],%[second],%[arg_second]\n",				\
			"%[second]", "%[first]", "r")

static inline bool __lfbig_cmpxchgptr_strong(_Atomic(lfatomic_big_t) * obj,
		lfatomic_big_t * expected, lfatomic_big_t desired,
		memory_order succ, memory_order fail)
{
	lfatomic_t first, second;
	lfatomic_t arg_first = (lfatomic_t) *expected;
	lfatomic_t arg_second = (lfatomic_t) (*expected >> sizeof(lfatomic_t) * 8);
	lfatomic_t desired_first = (lfatomic_t) desired;
	void * mfirst = __LFPPC_MPTR(obj);
	void * msecond = __LFPPC_MREF(obj);
	lfatomic_t result, tmp;

	/* A special case when the reference counter is 0. */
	if (__builtin_constant_p(arg_second) && arg_second == 0)
		__LFPPC_CMPXCHG_STRONG_0("lwsync", "isync");
	else
		__LFPPC_CMPXCHG_STRONG_R("lwsync", "isync");

	*expected = ((lfatomic_big_t) second << sizeof(lfatomic_t) * 8) | first;
	return !result;
}

static inline bool __lfbig_cmpxchgptr_weak(_Atomic(lfatomic_big_t) * obj,
		lfatomic_big_t * expected, lfatomic_big_t desired,
		memory_order succ, memory_order fail)
{
	lfatomic_t first, second;
	lfatomic_t arg_first = (lfatomic_t) *expected;
	lfatomic_t arg_second = (lfatomic_t) (*expected >> sizeof(lfatomic_t) * 8);
	lfatomic_t desired_first = (lfatomic_t) desired;
	void * mfirst = __LFPPC_MPTR(obj);
	void * msecond = __LFPPC_MREF(obj);

	/* A special case when the reference counter is 0. */
	if (__builtin_constant_p(arg_second) && arg_second == 0)
		__LFPPC_CMPXCHG_WEAK_0("lwsync", "isync");
	else
		__LFPPC_CMPXCHG_WEAK_R("lwsync", "isync");

	/* Reload if failed. */
	if (!(first & __LFPPC_CR_EQ))
		*expected = __lfbig_load(obj, fail);
	return !!(first & __LFPPC_CR_EQ);
}

static inline bool __lfbig_cmpxchgref_strong(_Atomic(lfatomic_big_t) * obj,
		lfatomic_big_t * expected, lfatomic_big_t desired,
		memory_order succ, memory_order fail)
{
	lfatomic_t first, second;
	lfatomic_t arg_first = (lfatomic_t) (*expected >> sizeof(lfatomic_t) * 8);
	lfatomic_t arg_second = (lfatomic_t) *expected;
	lfatomic_t desired_first = (lfatomic_t) (desired >> sizeof(lfatomic_t) * 8);
	void * mfirst = __LFPPC_MREF(obj);
	void * msecond = __LFPPC_MPTR(obj);
	lfatomic_t result, tmp;

	__LFPPC_CMPXCHG_STRONG_R("lwsync", "isync");

	*expected = ((lfatomic_big_t) first << sizeof(lfatomic_t) * 8) | second;
	return !result;
}

static inline bool __lfbig_cmpxchgref_weak(_Atomic(lfatomic_big_t) * obj,
		lfatomic_big_t * expected, lfatomic_big_t desired,
		memory_order succ, memory_order fail)
{
	lfatomic_t first, second;
	lfatomic_t arg_first = (lfatomic_t) (*expected >> sizeof(lfatomic_t) * 8);
	lfatomic_t arg_second = (lfatomic_t) *expected;
	lfatomic_t desired_first = (lfatomic_t) (desired >> sizeof(lfatomic_t) * 8);
	void * mfirst = __LFPPC_MREF(obj);
	void * msecond = __LFPPC_MPTR(obj);

	__LFPPC_CMPXCHG_WEAK_R("lwsync", "isync");

	/* Reload if failed. */
	if (!(first & __LFPPC_CR_EQ))
		*expected = __lfbig_load(obj, fail);
	return !!(first & __LFPPC_CR_EQ);
}

static inline lfatomic_big_t __lfbig_fetch_add(_Atomic(lfatomic_big_t) * obj,
		lfatomic_big_t arg, memory_order order)
{
	lfatomic_t ptr, ref, val;
	lfatomic_t arg_ref = (lfatomic_t) (arg >> (sizeof(lfatomic_t) * 8));

	if (__builtin_constant_p(arg_ref) && arg_ref < 32768U)
		__LFPPC_OP("lwsync", "isync", "addi", "I", arg_ref);
	else
		__LFPPC_OP("lwsync", "isync", "add", "r", arg_ref);

	__LF_ASSUME(ref + arg_ref == val);
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

#endif /* !__LF_PPC_H */

/* vi: set tabstop=4: */
