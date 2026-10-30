/*!
 * \file	sccp_atomic.h
 * \brief	SCCP Atomic Header
 * \author	Diederik de Groot <ddegroot[at] users.sf.net>
 * \note	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 * Lock free atomic primitives built on the gcc/clang __atomic builtins (available in every compiler that can build
 * Asterisk >= 20). The former mutex based fallback, which gave up on the lock after a number of failed trylock() calls
 * and modified the counters unlocked, is gone.
 *
 * The third/fourth parameter (a pointer to a mutex in the old API) is kept for source compatibility and ignored.
 */
#pragma once

/* returns the previous value */
#define ATOMIC_INCR(_a, _b, _c) __atomic_fetch_add((_a), (_b), __ATOMIC_SEQ_CST)
/* returns the previous value */
#define ATOMIC_DECR(_a, _b, _c) __atomic_fetch_sub((_a), (_b), __ATOMIC_SEQ_CST)
#define ATOMIC_FETCH(_a, _c)    __atomic_load_n((_a), __ATOMIC_SEQ_CST)

/* compare and swap, returns the value found in *_a before the operation (equals _b when the swap succeeded) */
#define CAS32(_a, _b, _c, _d)                                                                                      \
	({                                                                                                         \
		CAS32_TYPE __cas_expected = (_b);                                                                  \
		__atomic_compare_exchange_n((_a), &__cas_expected, (_c), 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); \
		__cas_expected;                                                                                    \
	})

/* compare and swap of a pointer, returns TRUE when the swap succeeded */
#define CAS_PTR(_a, _b, _c, _d)                                                                                                           \
	({                                                                                                                                \
		__typeof__(*(_a)) __casptr_expected = (_b);                                                                               \
		(boolean_t)__atomic_compare_exchange_n((_a), &__casptr_expected, (_c), 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);           \
	})
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
