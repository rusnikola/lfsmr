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

#ifndef __LFBSMR_H
# error "Do not include bits/lfbsmr_cas2.h, use lfbsmr.h instead."
#endif

#include "lfbsmr_common.h"

#define __LFBSMR_STALL_THRESHOLD	32768

/* Generic implementation that uses CAS2 available on
   i586+, x86-64 and ARMv6+. It is also adopted for
   single-width LL/SC on PowerPC and MIPS. */

/********************************************
 *           head reference format
 * +---------------------------------------+
 * |  reference count  |      pointer      |
 * +---------------------------------------+
 *       32/64 bits          32/64 bits
 *
 ********************************************/

#define __LFBSMR_IMPL2(w, stype_t, type_t, dtype_t)							\
																			\
struct lfbsmr##w##_vector {													\
	_Alignas(LF_CACHE_BYTES) LFATOMIC(dtype_t) head;						\
	_Alignas(LF_CACHE_BYTES) LFATOMIC(lfepoch_t) access;					\
	_Alignas(LF_CACHE_BYTES) LFATOMIC(type_t) counter;						\
	_Alignas(LF_CACHE_BYTES) char _pad[0];									\
};																			\
																			\
struct lfbsmr##w {															\
	_Alignas(LF_CACHE_BYTES) LFATOMIC(lfepoch_t) global;					\
	struct lfbsmr##w##_vector vector[0];									\
};																			\
																			\
__LFBSMR_COMMON_IMPL(w, type_t)												\
																			\
static inline void lfbsmr##w##_init(struct lfbsmr##w * hdr, size_t order)	\
{																			\
	size_t count = (size_t) 1U << order, i = 0;								\
	do {																	\
		__lfref_init##w(&hdr->vector[i].head, 0);							\
		__lfepoch_init(&hdr->vector[i].access, 0);							\
		atomic_init(&hdr->vector[i].counter, 0);							\
	} while (++i != count);													\
	__lfepoch_init(&hdr->global, 0);										\
}																			\
																			\
static inline type_t __lfbsmr##w##_link(struct lfbsmr##w * hdr, size_t vec)	\
{																			\
	return __lfref_link##w(&hdr->vector[vec].head, memory_order_acquire);	\
}																			\
																			\
static inline lfepoch_t __lfbsmr##w##_access(struct lfbsmr##w * hdr,		\
		size_t vec, lfepoch_t access, lfepoch_t epoch)						\
{																			\
	while (!__lfepoch_cmpxchg_weak(&hdr->vector[vec].access, &access,		\
				epoch, memory_order_acq_rel, memory_order_acquire)) {		\
		if (__LFBSMR_EPOCH_CMP(access, >=, epoch))							\
			return access;													\
	}																		\
	return epoch;															\
}																			\
																			\
static inline bool lfbsmr##w##_enter(struct lfbsmr##w * hdr, size_t * vec,	\
		size_t order, lfbsmr##w##_handle_t * smr, const void * base,		\
		lf_check_t check)													\
{																			\
	size_t mask = (size_t) 1U << order, i = *vec;							\
	dtype_t head;															\
	while (1) {																\
		size_t j = (i + 1) & (mask - 1);									\
		if (j == *vec ||													\
				(stype_t) atomic_load_explicit(&hdr->vector[i].counter,		\
						memory_order_relaxed) < __LFBSMR_STALL_THRESHOLD) {	\
			head = __lfref_fetch_add##w(&hdr->vector[i].head,				\
						__lfref_step##w, memory_order_acq_rel);				\
			*smr = (lfbsmr##w##_handle_t)									\
					((head & ~__lfref_mask##w) >> __lfrptr_shift##w);		\
			*vec = i;														\
			return true;													\
		}																	\
		i = j;																\
	}																		\
}																			\
																			\
static inline void __lfbsmr##w##_ack(struct lfbsmr##w * hdr,				\
		size_t vec, type_t counter)											\
{																			\
	atomic_fetch_sub_explicit(&hdr->vector[vec].counter, counter,			\
						memory_order_relaxed);								\
}																			\
																			\
static inline bool __lfbsmr##w##_leave(struct lfbsmr##w * hdr, size_t vec,	\
		size_t order, lfbsmr##w##_handle_t smr, type_t * list,				\
		const void * base, lf_check_t check)								\
{																			\
	const type_t addend = ((~(type_t) 0U) >> order) + 1U;					\
	struct lfbsmr##w##_node * node;											\
	dtype_t head, last;														\
	type_t next = next; /* Silence 'uninitialized' warnings. */				\
	uintptr_t start, curr;													\
																			\
	last = __lfref_load##w(&hdr->vector[vec].head, memory_order_acquire);	\
	do {																	\
		curr = (uintptr_t) ((last & ~__lfref_mask##w) >>					\
						__lfrptr_shift##w);									\
		if (curr != smr) {													\
			node = lfbsmr##w##_addr(curr, base);							\
			if (!check(hdr, node, sizeof(*node)))							\
				return false;												\
			next = node->next;												\
		}																	\
		head = (last - __lfref_step##w) & __lfref_mask##w;					\
		if (!__LFREF_CMPXCHG_FULL(dtype_t) || head)							\
			head |= last & ~__lfref_mask##w;								\
	} while (!__lfref_cmpxchgref_weak##w(&hdr->vector[vec].head, &last,		\
				head, memory_order_acq_rel, memory_order_acquire));			\
	start = (uintptr_t) ((last & ~__lfref_mask##w) >> __lfrptr_shift##w);	\
	if (!__LFREF_CMPXCHG_FULL(dtype_t) && start &&							\
					!((head & __lfref_mask##w) >> __lfref_shift##w)) {		\
		/* For an incomplete cmpxchg, perform an extra step. */				\
		if (__lfref_cmpxchgptr_strong##w(&hdr->vector[vec].head, &head, 0,	\
				memory_order_acq_rel, memory_order_acquire)) {				\
			if (!__lfbsmr##w##_adjust_refs(hdr, list, start, addend, base))	\
				return false;												\
		}																	\
	} else if (!head && start != 0) {										\
		if (!__lfbsmr##w##_adjust_refs(hdr, list, start, addend, base))		\
			return false;													\
	}																		\
	if (curr != smr) {														\
		size_t threshold = 0;												\
		return __lfbsmr##w##_traverse(hdr, vec, order, smr,					\
					list, base, check, &threshold, next, smr);				\
	}																		\
	return true;															\
}																			\
																			\
static inline type_t lfbsmr##w##_deref(struct lfbsmr##w * hdr, size_t vec,	\
		type_t * ptr)														\
{																			\
	type_t value;															\
	lfepoch_t global, access = __lfepoch_load(&hdr->vector[vec].access,		\
					memory_order_acquire);									\
	while (1) {																\
		value = __atomic_load_n(ptr, __ATOMIC_ACQUIRE);						\
		global = __lfepoch_load(&hdr->global, memory_order_acquire);		\
		if (access == global)												\
			break;															\
		access = __lfbsmr##w##_access(hdr, vec, access, global);			\
	}																		\
	return value;															\
}																			\
																			\
static inline bool __lfbsmr##w##_retire(struct lfbsmr##w * hdr,				\
		size_t order, type_t first, lfbsmr##w##_free_t smr_free,			\
		const void * base, lfepoch_t epoch)									\
{																			\
	const type_t addend = ((~(type_t) 0U) >> order) + 1U;					\
	size_t count = (size_t) 1U << order, i = 0;								\
	struct lfbsmr##w##_node * node;											\
	dtype_t head, last;														\
	type_t curr = first, prev, refs, adjs = 0, list = 0;					\
	lfepoch_t access;														\
	bool do_adjs = false;													\
																			\
	/* Add to the retirement lists. */										\
	do {																	\
		node = lfbsmr##w##_addr(curr, base);								\
		access = __lfepoch_load(&hdr->vector[i].access,						\
						memory_order_acquire);								\
		if (__LFBSMR_EPOCH_CMP(access, >=, epoch)) {						\
			last = __lfref_load##w(&hdr->vector[i].head,					\
							memory_order_acquire);							\
			do {															\
				refs = (type_t) ((last & __lfref_mask##w) >>				\
								__lfref_shift##w);							\
				if (!refs)													\
					goto adjust;											\
				prev = (type_t) ((last & ~__lfref_mask##w) >>				\
								__lfrptr_shift##w);							\
				node->next = prev;											\
				head = (last & __lfref_mask##w) |							\
					(((dtype_t) curr << __lfrptr_shift##w)					\
					 & ~__lfref_mask##w);									\
			} while (!__lfref_cmpxchgptr_weak##w(&hdr->vector[i].head,		\
				&last, head, memory_order_acq_rel, memory_order_acquire));	\
			/* Adjust the reference counter. */								\
			if (prev && !__lfbsmr##w##_adjust_refs(hdr, &list, prev,		\
					addend + refs, base))									\
				return false;												\
			atomic_fetch_add_explicit(&hdr->vector[i].counter, refs,		\
							memory_order_relaxed);							\
			curr = node->batch_next;										\
			continue;														\
		}																	\
adjust:																		\
		do_adjs = true;														\
		adjs += addend;														\
	} while (++i != count);													\
																			\
	if (do_adjs) {															\
		if (!__lfbsmr##w##_adjust_refs(hdr, &list, first, adjs, base))		\
			return false;													\
	}																		\
																			\
	__lfbsmr##w##_free(hdr, list, smr_free, base);							\
	return true;															\
}

/* vi: set tabstop=4: */
