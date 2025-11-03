/*
K12 based on the eXtended Keccak Code Package (XKCP)
https://github.com/XKCP/XKCP

KangarooTwelve, designed by Guido Bertoni, Joan Daemen, Michaël Peeters, Gilles Van Assche, Ronny Van Keer and Benoît Viguier.

Threading support implementation using portable thread pool abstraction.

PLATFORM COMPATIBILITY:
The library uses a portable thread pool abstraction that supports multiple backends:
- Built-in pthread backend (Linux, macOS, BSD, Unix-like systems)
- Built-in sequential backend (all platforms, no actual parallelism)
- Custom application-provided backends (see KT-threadpool.h)

By default, the pthread backend is used on systems with pthread support,
and the sequential backend is used elsewhere.

To the extent possible under law, the implementer has waived all copyright
and related or neighboring rights to the source code in this file.
http://creativecommons.org/publicdomain/zero/1.0/
*/

#ifndef _KangarooTwelve_threading_h_
#define _KangarooTwelve_threading_h_

#include <stddef.h>
#include "KT-threadpool.h"

/**
 * Internal function to process multiple chunks in parallel using threads.
 *
 * @param threadpool_api    Thread pool API implementation
 * @param threadpool_handle Thread pool handle
 * @param thread_count      Number of threads in the pool
 * @param input             Pointer to input data (multiple chunks)
 * @param chunkCount        Number of chunks to process
 * @param output            Pointer to output buffer for chaining values
 * @param securityLevel     128 for KT128 or 256 for KT256
 * @return 0 if successful, 1 otherwise
 */
int KT_ProcessChunksThreaded(const KT_ThreadPool_API* threadpool_api,
                             void* threadpool_handle,
                             int thread_count,
                             const unsigned char *input,
                             size_t chunkCount,
                             unsigned char *output,
                             int securityLevel);

#endif /* _KangarooTwelve_threading_h_ */
