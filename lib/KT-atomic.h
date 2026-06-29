/*
K12 based on the eXtended Keccak Code Package (XKCP)
https://github.com/XKCP/XKCP

To the extent possible under law, the implementer has waived all copyright
and related or neighboring rights to the source code in this file.
http://creativecommons.org/publicdomain/zero/1.0/
*/

#ifndef KT_ATOMIC_H
#define KT_ATOMIC_H

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L) && !defined(__STDC_NO_ATOMICS__)
#include <stdatomic.h>
#define K12_ATOMIC(type) _Atomic(type)
#else
#define K12_ATOMIC(type) type
#endif

#endif  /* KT_ATOMIC_H */
