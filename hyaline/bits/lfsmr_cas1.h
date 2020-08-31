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

#if !defined(__LFSMR_H) && !defined(__LFSMRO_H)
# error "Do not include bits/lfsmr_cas1.h, use lfsmr.h instead."
#endif

#include "lfsmr_common.h"

#define __LFSMR_IMPL1(w, type_t)											\
																			\
struct lfsmr##w##_vector {													\
	_Alignas(LF_CACHE_BYTES) LFATOMIC(type_t) head;							\
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
		atomic_init(&hdr->vector[i].head, 0);								\
	} while (++i != count);													\
}																			\
																			\
static inline type_t __lfsmr##w##_link(struct lfsmr##w * hdr, size_t vec)	\
{																			\
	return (atomic_load_explicit(&hdr->vector[vec].head,					\
				memory_order_acquire) & ~(type_t) 0x1);						\
}																			\
																			\
static inline bool lfsmr##w##_enter(struct lfsmr##w * hdr, size_t vec,		\
		lfsmr##w##_handle_t * smr, const void * base, lf_check_t check)		\
{																			\
	*smr = 0;																\
	atomic_store_explicit(&hdr->vector[vec].head, 0x1,						\
					memory_order_seq_cst);									\
	return true;															\
}																			\
																			\
static inline bool __lfsmr##w##_leave(struct lfsmr##w * hdr, size_t vec,	\
		size_t order, lfsmr##w##_handle_t smr, type_t * list,				\
		const void * base, lf_check_t check)								\
{																			\
	type_t curr;															\
																			\
	curr = atomic_exchange_explicit(&hdr->vector[vec].head, 0,				\
					memory_order_seq_cst) & ~(type_t) 0x1;					\
	if (curr != 0) {														\
		size_t threshold = 0;												\
		return __lfsmr##w##_traverse(hdr, vec, order, NULL, list, base,		\
					check, &threshold, curr, (uintptr_t) smr);				\
	}																		\
	return true;															\
}																			\
																			\
static inline bool __lfsmr##w##_retire(struct lfsmr##w * hdr,				\
		size_t order, type_t first, lfsmr##w##_free_t smr_free,				\
		const void * base)													\
{																			\
	size_t count = (size_t) 1U << order, i = 0;								\
	struct lfsmr##w##_node * node;											\
	type_t head, last;														\
	type_t curr = first, adjs = 0, list = 0;								\
																			\
	/* Add to the retirement lists. */										\
	do {																	\
		node = lfsmr##w##_addr(curr, base);									\
		last = atomic_load_explicit(&hdr->vector[i].head,					\
						memory_order_acquire);								\
		do {																\
			if (!(last & 0x1))												\
				goto next;													\
			node->next = (type_t) (last & ~(type_t) 0x1);					\
			head = curr | 0x1;												\
		} while (!atomic_compare_exchange_weak_explicit(					\
				&hdr->vector[i].head, &last, head,							\
				memory_order_acq_rel, memory_order_acquire));				\
		curr = node->batch_next;											\
		adjs++;																\
		next: ;																\
	} while (++i != count);													\
																			\
	if (!__lfsmr##w##_adjust_refs(hdr, &list, first, adjs, base))			\
		return false;														\
																			\
	__lfsmr##w##_free(hdr, list, smr_free, base);							\
	return true;															\
}

/* vi: set tabstop=4: */
