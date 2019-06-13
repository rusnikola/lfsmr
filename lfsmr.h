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
#define __LFSMR_H	1

#include <stdbool.h>

#include "bits/lfsmr_cas2.h"
#include "bits/lfsmr_cas1.h"

/* Available on all architectures. */
#define LFSMR_ALIGN	(_Alignof(struct lfsmr))
#define LFSMR_SIZE(x)		\
	((x) * sizeof(struct lfsmr_vector) + offsetof(struct lfsmr, vector))
#if LFATOMIC_BIG_WIDTH >= 2 * __LFPTR_WIDTH &&	\
		!__LFCMPXCHG_SPLIT(2 * __LFPTR_WIDTH)
__LFSMR_IMPL2(, uintptr_t, lfref_t)
#else
__LFSMR_IMPL1(, uintptr_t)
#endif

#define LFSMR32_ALIGN	(_Alignof(struct lfsmr32))
#define LFSMR32_SIZE(x)		\
	((x) * sizeof(struct lfsmr32_vector) + offsetof(struct lfsmr32, vector))
#if LFATOMIC_BIG_WIDTH >= 64 && !__LFCMPXCHG_SPLIT(64)
__LFSMR_IMPL2(32, uint32_t, uint64_t)
#else
__LFSMR_IMPL1(32, uint32_t)
#endif

/* Available on 64-bit architectures. */
#if LFATOMIC_WIDTH >= 64
# define LFSMR64_ALIGN	(_Alignof(struct lfsmr64))
# define LFSMR64_SIZE(x)		\
	((x) * sizeof(struct lfsmr64_vector) + offsetof(struct lfsmr64, vector))
# if LFATOMIC_BIG_WIDTH >= 128 && !__LFCMPXCHG_SPLIT(128)
__LFSMR_IMPL2(64, uint64_t, __uint128_t)
# else
__LFSMR_IMPL1(64, uint64_t)
# endif
#endif

#endif	/* !__LFSMR_H */

/* vi: set tabstop=4: */
