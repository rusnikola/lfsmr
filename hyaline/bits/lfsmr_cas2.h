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

#ifndef __LFSMR_H
# error "Do not include bits/lfsmr_cas2.h, use lfsmr.h instead."
#endif

#include "lfsmr_common.h"

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

#define __LFSMR_IMPL2(w, type_t, dtype_t)									\
																			\
struct lfsmr##w##_vector {													\
	_Alignas(LF_CACHE_BYTES) LFATOMIC(dtype_t) head;						\
	_Alignas(LF_CACHE_BYTES) char _pad[0];									\
};																			\
																			\
struct lfsmr##w {															\
	struct lfsmr##w##_vector vector[0];										\
};																			\
																			\
__LFSMR_COMMON_IMPL(w, type_t)												\
																			\
static inline void lfsmr##w##_init(struct lfsmr##w * hdr, size_t order)		\
{																			\
	size_t count = (size_t) 1U << order, i = 0;								\
	do {																	\
		__lfref_init##w(&hdr->vector[i].head, 0);							\
	} while (++i != count);													\
}																			\
																			\
static inline type_t __lfsmr##w##_link(struct lfsmr##w * hdr, size_t vec)	\
{																			\
	return __lfref_link##w(&hdr->vector[vec].head, memory_order_acquire);	\
}																			\
																			\
static inline bool lfsmr##w##_enter(struct lfsmr##w * hdr, size_t vec,		\
		lfsmr##w##_handle_t * smr, const void * base, lf_check_t check)		\
{																			\
	dtype_t head = __lfref_fetch_add##w(&hdr->vector[vec].head,				\
						__lfref_step##w, memory_order_acq_rel);				\
	*smr = (lfsmr##w##_handle_t)											\
		((head & ~__lfref_mask##w) >> __lfrptr_shift##w);					\
	return true;															\
}																			\
																			\
static inline bool __lfsmr##w##_leave(struct lfsmr##w * hdr, size_t vec,	\
		size_t order, lfsmr##w##_handle_t smr, type_t * list,				\
		const void * base, lf_check_t check)								\
{																			\
	const type_t addend = ((~(type_t) 0U) >> order) + 1U;					\
	struct lfsmr##w##_node * node;											\
	dtype_t head, last;														\
	type_t next = next; /* Silence 'uninitialized' warnings. */				\
	uintptr_t start, curr;													\
																			\
	last = __lfref_load##w(&hdr->vector[vec].head, memory_order_acquire);	\
	do {																	\
		curr = (uintptr_t) ((last & ~__lfref_mask##w) >>					\
						__lfrptr_shift##w);									\
		if (curr != smr) {													\
			node = lfsmr##w##_addr(curr, base);								\
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
			if (!__lfsmr##w##_adjust_refs(hdr, list, start, addend, base))	\
				return false;												\
		}																	\
	} else if (!head && start != 0) {										\
		if (!__lfsmr##w##_adjust_refs(hdr, list, start, addend, base))		\
			return false;													\
	}																		\
	if (curr != smr) {														\
		size_t threshold = 0;												\
		return __lfsmr##w##_traverse(hdr, vec, order, NULL, list, base,		\
					check, &threshold, next, (uintptr_t) smr);				\
	}																		\
	return true;															\
}																			\
																			\
static inline bool __lfsmr##w##_retire(struct lfsmr##w * hdr,				\
		size_t order, type_t first, lfsmr##w##_free_t smr_free,				\
		const void * base)													\
{																			\
	const type_t addend = ((~(type_t) 0U) >> order) + 1U;					\
	size_t count = (size_t) 1U << order, i = 0;								\
	struct lfsmr##w##_node * node;											\
	dtype_t head, last;														\
	type_t curr = first, prev, refs, adjs = 0, list = 0;					\
	bool do_adjs = false;													\
																			\
	/* Add to the retirement lists. */										\
	do {																	\
		node = lfsmr##w##_addr(curr, base);									\
		last = __lfref_load##w(&hdr->vector[i].head, memory_order_acquire);	\
		do {																\
			refs = (type_t) ((last & __lfref_mask##w) >> __lfref_shift##w);	\
			if (!refs) {													\
				do_adjs = true;												\
				adjs += addend;												\
				goto next;													\
			}																\
			prev = (type_t) ((last & ~__lfref_mask##w) >>					\
							__lfrptr_shift##w);								\
			node->next = prev;												\
			head = (last & __lfref_mask##w) |								\
				(((dtype_t) curr << __lfrptr_shift##w) & ~__lfref_mask##w);	\
		} while (!__lfref_cmpxchgptr_weak##w(&hdr->vector[i].head, &last,	\
					head, memory_order_acq_rel, memory_order_acquire));		\
		/* Adjust the reference counter. */									\
		if (prev && !__lfsmr##w##_adjust_refs(hdr, &list, prev,				\
				addend + refs, base))										\
			return false;													\
		curr = node->batch_next;											\
		next: ;																\
	} while (++i != count);													\
																			\
	if (do_adjs) {															\
		if (!__lfsmr##w##_adjust_refs(hdr, &list, first, adjs, base))		\
			return false;													\
	}																		\
																			\
	__lfsmr##w##_free(hdr, list, smr_free, base);							\
	return true;															\
}

/* vi: set tabstop=4: */
