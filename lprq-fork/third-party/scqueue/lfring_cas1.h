/* ----------------------------------------------------------------------------
 *
 * Dual 2-BSD/MIT license. Either or both licenses can be used.
 *
 * ----------------------------------------------------------------------------
 *
 * Copyright (c) 2019 Ruslan Nikolaev.  All Rights Reserved.
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
 *
 * ----------------------------------------------------------------------------
 *
 * Copyright (c) 2019 Ruslan Nikolaev
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * ----------------------------------------------------------------------------
 */

#ifndef __LFRING_H
#define __LFRING_H	1

#include <stdbool.h>
#include <inttypes.h>
#include <sys/types.h>
#include <stdatomic.h>
#include <stdio.h>

#include "lfring.h"
#include "lf/lf.h"

#define __lfring_cmp(x, op, y)	((lfsatomic_t) ((x) - (y)) op 0)

#if LFRING_MIN_ORDER != 0
static inline size_t __lfring_raw_map(lfatomic_t idx, size_t order, size_t n)
{
	return (size_t) (((idx & (n - 1)) >> (order - LFRING_MIN_ORDER)) |
			((idx << LFRING_MIN_ORDER) & (n - 1)));
}
#else
static inline size_t __lfring_raw_map(lfatomic_t idx, size_t order, size_t n)
{
	return (size_t) (idx & (n - 1));
}
#endif

#define __LFRING_CLOSED (((lfatomic_t) 1) << (LFATOMIC_WIDTH - 1))

static inline size_t __lfring_map(lfatomic_t idx, size_t order, size_t n)
{
	return __lfring_raw_map(idx, order + 1, n);
}

#define __lfring_threshold3(half, n) ((long) ((half) + (n) - 1))

static inline size_t lfring_pow2(size_t order)
{
	return (size_t) 1U << order;
}

struct __lfring {
	__attribute__ ((aligned(LF_CACHE_BYTES))) _Atomic(lfatomic_t) head;
	__attribute__ ((aligned(LF_CACHE_BYTES))) _Atomic(lfsatomic_t) threshold;
	__attribute__ ((aligned(LF_CACHE_BYTES))) _Atomic(lfatomic_t) tail;
	__attribute__ ((aligned(LF_CACHE_BYTES))) _Atomic(lfatomic_t) array[1];
};

struct lfring;

static inline void __lfring_catchup(struct lfring * ring,
	lfatomic_t tail, lfatomic_t head)
{
	struct __lfring * q = (struct __lfring *) ring;

	while (!atomic_compare_exchange_weak_explicit(&q->tail, &tail, head,
			memory_order_acq_rel, memory_order_acquire)) {
		head = atomic_load_explicit(&q->head, memory_order_acquire);
		tail = atomic_load_explicit(&q->tail, memory_order_acquire);
		if (__lfring_cmp(tail, >=, head))
			break;
	}
}

#endif	/* !__LFRING_H */

/* vi: set tabstop=4: */
