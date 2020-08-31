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
# error "Do not include bits/lfsmr_common.h, use lfsmr.h instead."
#endif

#include "lf.h"

#define __LFSMR_COMMON_IMPL(w, type_t)										\
typedef uintptr_t lfsmr##w##_handle_t;										\
typedef struct lfsmr##w##_batch lfsmr##w##_batch_t;							\
struct lfsmr##w;															\
																			\
struct lfsmr##w##_node {													\
	_Alignas(sizeof(type_t)) type_t next;									\
	type_t batch_link;														\
	union {																	\
		LFATOMIC(type_t) refs;												\
		type_t batch_next;													\
	};																		\
};																			\
																			\
struct lfsmr##w##_batch {													\
	type_t first;															\
	type_t last;															\
	type_t counter;															\
};																			\
																			\
typedef void (*lfsmr##w##_free_t) (struct lfsmr##w *,						\
		struct lfsmr##w##_node *);											\
																			\
static inline type_t __lfsmr##w##_link(struct lfsmr##w * hdr, size_t vec);	\
static inline bool __lfsmr##w##_retire(struct lfsmr##w * hdr, size_t order,	\
		type_t first, lfsmr##w##_free_t smr_free, const void * base);		\
static inline bool lfsmr##w##_enter(struct lfsmr##w * hdr, size_t vec,		\
		lfsmr##w##_handle_t * smr, const void * base, lf_check_t check);	\
static inline bool __lfsmr##w##_leave(struct lfsmr##w * hdr, size_t vec,	\
		size_t order, lfsmr##w##_handle_t smr, type_t * list,				\
		const void * base, lf_check_t check);								\
																			\
static inline struct lfsmr##w##_node * lfsmr##w##_addr(						\
	uintptr_t offset, const void * base)									\
{																			\
	return (struct lfsmr##w##_node *) ((uintptr_t) base + offset);			\
}																			\
																			\
static inline bool __lfsmr##w##_adjust_refs(struct lfsmr##w * hdr,			\
		type_t * list, type_t prev, type_t refs, const void * base)			\
{																			\
	struct lfsmr##w##_node * node;											\
	node = lfsmr##w##_addr(prev, base);										\
	prev = node->batch_link;												\
	node = lfsmr##w##_addr(prev, base);										\
	if (atomic_fetch_add_explicit(&node->refs, refs,						\
							memory_order_acq_rel) == -refs) {				\
		node->next = *list;													\
		*list = prev;														\
	}																		\
	return true;															\
}																			\
																			\
static inline bool __lfsmr##w##_traverse(struct lfsmr##w * hdr, size_t vec,	\
	size_t order, lfsmr##w##_handle_t * smr, type_t * list,					\
	const void * base, lf_check_t check, size_t * threshold,				\
	uintptr_t next, uintptr_t end)											\
{																			\
	struct lfsmr##w##_node * node;											\
	size_t length = 0;														\
	type_t link;															\
	uintptr_t curr;															\
																			\
	do {																	\
		curr = next;														\
		if (!curr)															\
			break;															\
		node = lfsmr##w##_addr(curr, base);									\
		if (!check(hdr, node, sizeof(*node)))								\
			return false;													\
		next = node->next;													\
		link = node->batch_link;											\
		node = lfsmr##w##_addr(link, base);									\
		if (!check(hdr, node, sizeof(*node)))								\
			return false;													\
		/* If the last reference, put into the local list. */				\
		if (atomic_fetch_sub_explicit(&node->refs, 1, memory_order_acq_rel)	\
						== 1) {												\
			node->next = *list;												\
			*list = link;													\
			if (*threshold && ++length >= *threshold) {						\
				if (!__lfsmr##w##_leave(hdr, vec, order, *smr, list, base,	\
										check))								\
					return false;											\
				*threshold = 0;												\
			}																\
		}																	\
	} while (curr != end);													\
																			\
	return true;															\
}																			\
																			\
static inline void __lfsmr##w##_free(struct lfsmr##w * hdr, type_t list,	\
	lfsmr##w##_free_t smr_free, const void * base)							\
{																			\
	struct lfsmr##w##_node * node;											\
	type_t start;															\
																			\
	while (list != 0) {														\
		node = lfsmr##w##_addr(list, base);									\
		start = node->batch_link;											\
		list = node->next;													\
		do {																\
			node = lfsmr##w##_addr(start, base);							\
			start = node->batch_next;										\
			smr_free(hdr, node);											\
		} while (start != 0);												\
	}																		\
}																			\
																			\
static inline bool lfsmr##w##_leave(struct lfsmr##w * hdr, size_t vec,		\
	size_t order, lfsmr##w##_handle_t smr, lfsmr##w##_free_t smr_free,		\
	const void * base, lf_check_t check)									\
{																			\
	type_t list = 0;														\
	if (!__lfsmr##w##_leave(hdr, vec, order, smr, &list, base, check))		\
		return false;														\
	__lfsmr##w##_free(hdr, list, smr_free, base);							\
	return true;															\
}																			\
																			\
static inline bool lfsmr##w##_trim(struct lfsmr##w * hdr, size_t vec,		\
	size_t order, lfsmr##w##_handle_t * smr, lfsmr##w##_free_t smr_free,	\
	const void * base, lf_check_t check, size_t threshold)					\
{																			\
	struct lfsmr##w##_node * node;											\
	lfsmr##w##_handle_t end = *smr;											\
	size_t new_threshold = threshold;										\
	type_t link = __lfsmr##w##_link(hdr, vec);								\
	type_t list = 0;														\
																			\
	if (link != end) {														\
		*smr = link;														\
		node = lfsmr##w##_addr(link, base);									\
		if (!check(hdr, node, sizeof(*node)))								\
			return false;													\
		link = node->next;													\
		if (!__lfsmr##w##_traverse(hdr, vec, order, smr, &list, base,		\
				check, &new_threshold, link, (uintptr_t) end))				\
			return false;													\
		__lfsmr##w##_free(hdr, list, smr_free, base);						\
		/* Leave was called, i.e. new_threshold = 0. */						\
		if (threshold != new_threshold)										\
			return lfsmr##w##_enter(hdr, vec, smr, base, check);			\
	}																		\
	return true;															\
}																			\
																			\
static inline void lfsmr##w##_batch_init(struct lfsmr##w##_batch * batch)	\
{																			\
	batch->first = 0;														\
	batch->counter = 0;														\
}																			\
																			\
static inline bool lfsmr##w##_batch_empty(struct lfsmr##w##_batch * batch)	\
{																			\
	return batch->counter == 0;												\
}																			\
																			\
static inline bool lfsmr##w##_retire_one(struct lfsmr##w * hdr,				\
		struct lfsmr##w##_node * node, lfsmr##w##_free_t smr_free,			\
		const void * base)													\
{																			\
	type_t first;															\
	first = (type_t) ((uintptr_t) node) - (type_t) ((uintptr_t) base);		\
	return __lfsmr##w##_retire(hdr, 0, first, smr_free, base);				\
}																			\
																			\
static inline bool lfsmr##w##_retire(struct lfsmr##w * hdr,					\
		size_t order, struct lfsmr##w##_node * node,						\
		lfsmr##w##_free_t smr_free,	const void * base,						\
		struct lfsmr##w##_batch * batch, size_t threshold)					\
{																			\
	type_t curr;															\
	curr = (type_t) ((uintptr_t) node) - (type_t) ((uintptr_t) base);		\
	if (!batch->first) {													\
		batch->last = curr;													\
	} else {																\
		node->batch_link = batch->last;										\
	}																		\
	/* Implicitly initializes refs to 0 for the last node. */				\
	node->batch_next = batch->first;										\
	batch->first = curr;													\
	if (++batch->counter >= threshold) {									\
		node = lfsmr##w##_addr(batch->last, base);							\
		node->batch_link = batch->first;									\
		if (!__lfsmr##w##_retire(hdr, order, batch->first, smr_free, base))	\
			return false;													\
		lfsmr##w##_batch_init(batch);										\
	}																		\
	return true;															\
}

/* vi: set tabstop=4: */
