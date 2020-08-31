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

#if !defined(__LFBSMR_H) && !defined(__LFBSMRO_H)
# error "Do not include bits/lfbsmr_cas1.h, use lfbsmr.h instead."
#endif

#include "lfbsmr_common.h"

#define __LFBSMR_IMPL1(w, type_t)											\
																			\
struct lfbsmr##w##_vector {													\
	_Alignas(LF_CACHE_BYTES) LFATOMIC(type_t) head;							\
	_Alignas(LF_CACHE_BYTES) LFATOMIC(lfepoch_t) access;					\
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
		atomic_init(&hdr->vector[i].head, 0);								\
		__lfepoch_init(&hdr->vector[i].access, 0);							\
	} while (++i != count);													\
	__lfepoch_init(&hdr->global, 0);										\
}																			\
																			\
static inline type_t __lfbsmr##w##_link(struct lfbsmr##w * hdr, size_t vec)	\
{																			\
	return (atomic_load_explicit(&hdr->vector[vec].head,					\
				memory_order_acquire) & ~(type_t) 0x1);						\
}																			\
																			\
static inline lfepoch_t __lfbsmr##w##_access(struct lfbsmr##w * hdr,		\
		size_t vec, lfepoch_t access, lfepoch_t epoch)						\
{																			\
	atomic_store_explicit(&hdr->vector[vec].access, epoch,					\
			memory_order_seq_cst);											\
	return epoch;															\
}																			\
																			\
static inline bool lfbsmr##w##_enter(struct lfbsmr##w * hdr, size_t * vec,	\
		size_t order, lfbsmr##w##_handle_t * smr,							\
		const void * base, lf_check_t check)								\
{																			\
	*smr = 0;																\
	atomic_store_explicit(&hdr->vector[*vec].head, 0x1,						\
					memory_order_seq_cst);									\
	return true;															\
}																			\
																			\
static inline bool __lfbsmr##w##_leave(struct lfbsmr##w * hdr, size_t vec,	\
		size_t order, lfbsmr##w##_handle_t smr, type_t * list,				\
		const void * base, lf_check_t check)								\
{																			\
	type_t curr;															\
																			\
	curr = atomic_exchange_explicit(&hdr->vector[vec].head, 0,				\
					memory_order_seq_cst) & ~(type_t) 0x1;					\
	if (curr != 0) {														\
		size_t threshold = 0;												\
		return __lfbsmr##w##_traverse(hdr, vec, order, smr, list, base,		\
					check, &threshold, curr, smr);							\
	}																		\
	return true;															\
}																			\
																			\
static inline void __lfbsmr##w##_ack(struct lfbsmr##w * hdr,				\
		size_t vec, type_t counter)											\
{																			\
}																			\
																			\
static inline type_t lfbsmr##w##_deref(struct lfbsmr##w * hdr, size_t vec,	\
		type_t * ptr)														\
{																			\
	lfepoch_t access, global;												\
	type_t value;															\
																			\
	access = __lfepoch_load(&hdr->vector[vec].access,						\
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
	size_t count = (size_t) 1U << order, i = 0;								\
	struct lfbsmr##w##_node * node;											\
	type_t head, last;														\
	type_t curr = first, adjs = 0, list = 0;								\
	lfepoch_t access;														\
																			\
	/* Add to the retirement lists. */										\
	do {																	\
		node = lfbsmr##w##_addr(curr, base);								\
		access = __lfepoch_load(&hdr->vector[i].access,						\
						memory_order_acquire);								\
		if (__LFBSMR_EPOCH_CMP(access, >=, epoch)) {						\
			last = atomic_load_explicit(&hdr->vector[i].head,				\
							memory_order_acquire);							\
			do {															\
				if (!(last & 0x1))											\
					goto next;												\
				node->next = (type_t) (last & ~(type_t) 0x1);				\
				head = curr | 0x1;											\
			} while (!atomic_compare_exchange_weak_explicit(				\
				&hdr->vector[i].head, &last, head,							\
				memory_order_acq_rel, memory_order_acquire));				\
			curr = node->batch_next;										\
			adjs++;															\
		}																	\
		next: ;																\
	} while (++i != count);													\
																			\
	if (!__lfbsmr##w##_adjust_refs(hdr, &list, first, adjs, base))			\
		return false;														\
																			\
	__lfbsmr##w##_free(hdr, list, smr_free, base);							\
	return true;															\
}

/* vi: set tabstop=4: */
