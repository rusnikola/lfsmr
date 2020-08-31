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
# error "Do not include bits/lfbsmr_common.h, use lfbsmr.h instead."
#endif

#include "lf.h"

#define __LFBSMR_EPOCH_CMP(x, op, y)	\
	((lfepoch_signed_t) ((x) - (y)) op 0)

#define __LFBSMR_COMMON_IMPL(w, type_t)										\
typedef uintptr_t lfbsmr##w##_handle_t;										\
typedef struct lfbsmr##w##_batch lfbsmr##w##_batch_t;						\
struct lfbsmr##w;															\
																			\
struct lfbsmr##w##_node {													\
	union {																	\
		struct {															\
			_Alignas(sizeof(type_t)) type_t next;							\
			type_t batch_link;												\
			union {															\
				LFATOMIC(type_t) refs;										\
				type_t batch_next;											\
			};																\
		};																	\
		_Alignas(sizeof(lfepoch_t)) lfepoch_t birth_epoch;					\
	};																		\
};																			\
																			\
struct lfbsmr##w##_batch {													\
	lfepoch_t min_epoch;													\
	type_t first;															\
	type_t last;															\
	type_t counter;															\
};																			\
																			\
typedef void (*lfbsmr##w##_free_t) (struct lfbsmr##w *,						\
		struct lfbsmr##w##_node *);											\
																			\
static inline type_t __lfbsmr##w##_link(struct lfbsmr##w *, size_t vec);	\
static inline void __lfbsmr##w##_ack(struct lfbsmr##w * hdr,				\
		size_t vec, type_t counter);										\
static inline bool __lfbsmr##w##_retire(struct lfbsmr##w *, size_t order,	\
		type_t first, lfbsmr##w##_free_t smr_free, const void * base,		\
		lfepoch_t epoch);													\
static inline bool lfbsmr##w##_enter(struct lfbsmr##w * hdr, size_t * vec,	\
		size_t order, lfbsmr##w##_handle_t * smr, const void * base,		\
		lf_check_t check);													\
static inline bool __lfbsmr##w##_leave(struct lfbsmr##w * hdr, size_t vec,	\
		size_t order, lfbsmr##w##_handle_t smr, type_t * list,				\
		const void * base, lf_check_t check);								\
																			\
static inline struct lfbsmr##w##_node * lfbsmr##w##_addr(					\
	uintptr_t offset, const void * base)									\
{																			\
	return (struct lfbsmr##w##_node *) ((uintptr_t) base + offset);			\
}																			\
																			\
static inline bool __lfbsmr##w##_adjust_refs(struct lfbsmr##w * hdr,		\
		type_t * list, type_t prev, type_t refs, const void * base)			\
{																			\
	struct lfbsmr##w##_node * node;											\
	node = lfbsmr##w##_addr(prev, base);									\
	prev = node->batch_link;												\
	node = lfbsmr##w##_addr(prev, base);									\
	if (atomic_fetch_add_explicit(&node->refs, refs,						\
							memory_order_acq_rel) == -refs) {				\
		node->next = *list;													\
		*list = prev;														\
	}																		\
	return true;															\
}																			\
																			\
static inline bool __lfbsmr##w##_traverse(struct lfbsmr##w * hdr,			\
	size_t vec, size_t order, lfbsmr##w##_handle_t smr,						\
	type_t * list, const void * base, lf_check_t check,						\
	size_t * threshold, uintptr_t next, uintptr_t end)						\
{																			\
	struct lfbsmr##w##_node * node;											\
	size_t length = 0;														\
	type_t counter = 0, link;												\
	uintptr_t curr;															\
																			\
	do {																	\
		counter++;															\
		curr = next;														\
		if (!curr)															\
			break;															\
		node = lfbsmr##w##_addr(curr, base);								\
		if (!check(hdr, node, sizeof(*node)))								\
			return false;													\
		next = node->next;													\
		link = node->batch_link;											\
		node = lfbsmr##w##_addr(link, base);								\
		if (!check(hdr, node, sizeof(*node)))								\
			return false;													\
		/* If the last reference, put into the local list. */				\
		if (atomic_fetch_sub_explicit(&node->refs, 1, memory_order_acq_rel)	\
						== 1) {												\
			node->next = *list;												\
			*list = link;													\
			if (*threshold && ++length >= *threshold) {						\
				if (!__lfbsmr##w##_leave(hdr, vec, order, smr, list,		\
						base, check))										\
					return false;											\
				*threshold = 0;												\
			}																\
		}																	\
	} while (curr != end);													\
	__lfbsmr##w##_ack(hdr, vec, counter);									\
																			\
	return true;															\
}																			\
																			\
static inline void __lfbsmr##w##_free(struct lfbsmr##w * hdr, type_t list,	\
	lfbsmr##w##_free_t smr_free, const void * base)							\
{																			\
	struct lfbsmr##w##_node * node;											\
	type_t start;															\
																			\
	while (list != 0) {														\
		node = lfbsmr##w##_addr(list, base);								\
		start = node->batch_link;											\
		list = node->next;													\
		do {																\
			node = lfbsmr##w##_addr(start, base);							\
			start = node->batch_next;										\
			smr_free(hdr, node);											\
		} while (start != 0);												\
	}																		\
}																			\
																			\
static inline bool lfbsmr##w##_leave(struct lfbsmr##w * hdr, size_t vec,	\
	size_t order, lfbsmr##w##_handle_t smr, lfbsmr##w##_free_t smr_free,	\
	const void * base, lf_check_t check)									\
{																			\
	type_t list = 0;														\
	if (!__lfbsmr##w##_leave(hdr, vec, order, smr, &list, base, check))		\
		return false;														\
	__lfbsmr##w##_free(hdr, list, smr_free, base);							\
	return true;															\
}																			\
																			\
static inline bool lfbsmr##w##_trim(struct lfbsmr##w * hdr, size_t * vec,	\
	size_t order, lfbsmr##w##_handle_t * smr, lfbsmr##w##_free_t smr_free,	\
	const void * base, lf_check_t check, size_t threshold)					\
{																			\
	struct lfbsmr##w##_node * node;											\
	uintptr_t end = *smr;													\
	size_t new_threshold = threshold;										\
	type_t link = __lfbsmr##w##_link(hdr, *vec);							\
	type_t list = 0;														\
																			\
	if (link != end) {														\
		*smr = link;														\
		node = lfbsmr##w##_addr(link, base);								\
		if (!check(hdr, node, sizeof(*node)))								\
			return false;													\
		link = node->next;													\
		if (!__lfbsmr##w##_traverse(hdr, *vec, order, *smr, &list,			\
				base, check, &new_threshold, link, end))					\
			return false;													\
		__lfbsmr##w##_free(hdr, list, smr_free, base);						\
		/* Leave was called, i.e. new_threshold = 0. */						\
		if (threshold != new_threshold)										\
			return lfbsmr##w##_enter(hdr, vec, order, smr, base, check);	\
	}																		\
	return true;															\
}																			\
																			\
static inline void lfbsmr##w##_batch_init(struct lfbsmr##w##_batch * batch)	\
{																			\
	batch->first = 0;														\
	batch->counter = 0;														\
}																			\
																			\
static inline bool lfbsmr##w##_batch_empty(struct lfbsmr##w##_batch *		\
				batch)														\
{																			\
	return batch->counter == 0;												\
}																			\
																			\
static inline bool lfbsmr##w##_retire_one(struct lfbsmr##w * hdr,			\
		struct lfbsmr##w##_node * node, lfbsmr##w##_free_t smr_free,		\
		const void * base, lfepoch_t epoch)									\
{																			\
	type_t first;															\
	first = (type_t) ((uintptr_t) node) - (type_t) ((uintptr_t) base);		\
	return __lfbsmr##w##_retire(hdr, 0, first, smr_free, base,				\
					node->birth_epoch);										\
}																			\
																			\
static inline bool lfbsmr##w##_retire(struct lfbsmr##w * hdr,				\
		size_t order, struct lfbsmr##w##_node * node,						\
		lfbsmr##w##_free_t smr_free, const void * base,						\
		struct lfbsmr##w##_batch * batch, size_t threshold)					\
{																			\
	type_t curr;															\
	curr = (type_t) ((uintptr_t) node) - (type_t) ((uintptr_t) base);		\
	if (!batch->first) {													\
		batch->min_epoch = node->birth_epoch;								\
		batch->last = curr;													\
	} else {																\
		if (__LFBSMR_EPOCH_CMP(batch->min_epoch, >, node->birth_epoch))		\
			batch->min_epoch = node->birth_epoch;							\
		node->batch_link = batch->last;										\
	}																		\
	/* Implicitly initializes refs to 0 for the last node. */				\
	node->batch_next = batch->first;										\
	batch->first = curr;													\
	if (++batch->counter >= threshold) {									\
		node = lfbsmr##w##_addr(batch->last, base);							\
		node->batch_link = batch->first;									\
		if (!__lfbsmr##w##_retire(hdr, order, batch->first, smr_free,		\
								base, batch->min_epoch))					\
			return false;													\
		lfbsmr##w##_batch_init(batch);										\
	}																		\
	return true;															\
}																			\
																			\
static inline void lfbsmr##w##_init_node(struct lfbsmr##w *hdr,				\
	struct lfbsmr##w##_node *node, size_t *counter, size_t freq)			\
{																			\
	node->birth_epoch = ++(*counter) % freq != 0 ?							\
		__lfepoch_load(&hdr->global, memory_order_acquire) :				\
		__lfepoch_fetch_add(&hdr->global, 1, memory_order_acq_rel) + 1;		\
}

/* vi: set tabstop=4: */
