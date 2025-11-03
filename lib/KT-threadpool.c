/*
K12 based on the eXtended Keccak Code Package (XKCP)
https://github.com/XKCP/XKCP

Thread pool abstraction layer - common functions.

To the extent possible under law, the implementer has waived all copyright
and related or neighboring rights to the source code in this file.
http://creativecommons.org/publicdomain/zero/1.0/
*/

#include "KT-threadpool.h"

/* Detect pthread availability */
#if defined(_POSIX_THREADS) || defined(__unix__) || defined(__unix) || \
    (defined(__APPLE__) && defined(__MACH__)) || defined(__linux__) || \
    defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#define HAVE_PTHREADS 1
#endif

const KT_ThreadPool_API* KT_ThreadPool_GetDefault(void)
{
#ifdef HAVE_PTHREADS
    return &KT_ThreadPool_Pthread;
#else
    return &KT_ThreadPool_Sequential;
#endif
}
