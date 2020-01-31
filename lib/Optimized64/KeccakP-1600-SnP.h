/*
Implementation by Gilles Van Assche and Ronny Van Keer, hereby denoted as "the implementer".

For more information, feedback or questions, please refer to our website:
https://keccak.team/

To the extent possible under law, the implementer has waived all copyright
and related or neighboring rights to the source code in this file.
http://creativecommons.org/publicdomain/zero/1.0/

---

Please refer to the XKCP for more details.
*/

#ifndef _KeccakP_1600_SnP_h_
#define _KeccakP_1600_SnP_h_

#include "brg_endian.h"

/* Keccak-p[1600] */

#if defined(__AVX512F__)

#define KeccakP1600_implementation      "AVX-512 optimized implementation"
#define KeccakP1600_stateSizeInBytes    200
#define KeccakP1600_stateAlignment      64
#define KeccakP1600_12rounds_FastLoop_supported

#define KeccakP1600_Initialize KeccakP1600_AVX512_Initialize
#define KeccakP1600_AddBytes KeccakP1600_AVX512_AddBytes
#define KeccakP1600_Permute_12rounds KeccakP1600_AVX512_Permute_12rounds
#define KeccakP1600_ExtractBytes KeccakP1600_AVX512_ExtractBytes
#define KeccakP1600_12rounds_FastLoop_Absorb KeccakP1600_AVX512_12rounds_FastLoop_Absorb

#define KeccakP1600_StaticInitialize()
void KeccakP1600_Initialize(void *state);
#define KeccakP1600_AddByte(state, byte, offset) ((unsigned char*)(state))[(offset)] ^= (byte)
void KeccakP1600_AddBytes(void *state, const unsigned char *data, unsigned int offset, unsigned int length);
void KeccakP1600_Permute_12rounds(void *state);
void KeccakP1600_ExtractBytes(const void *state, unsigned char *data, unsigned int offset, unsigned int length);
size_t KeccakP1600_12rounds_FastLoop_Absorb(void *state, unsigned int laneCount, const unsigned char *data, size_t dataByteLen);

#else
#if defined(__AVX2__)

#define KeccakP1600_implementation      "AVX2 optimized implementation"
#define KeccakP1600_stateSizeInBytes    200
#define KeccakP1600_stateAlignment      32
#define KeccakP1600_12rounds_FastLoop_supported

#define KeccakP1600_Initialize KeccakP1600_AVX2_Initialize
#define KeccakP1600_AddByte KeccakP1600_AVX2_AddByte
#define KeccakP1600_AddBytes KeccakP1600_AVX2_AddBytes
#define KeccakP1600_Permute_12rounds KeccakP1600_AVX2_Permute_12rounds
#define KeccakP1600_ExtractBytes KeccakP1600_AVX2_ExtractBytes
#define KeccakP1600_12rounds_FastLoop_Absorb KeccakP1600_AVX2_12rounds_FastLoop_Absorb

#define KeccakP1600_StaticInitialize()
void KeccakP1600_Initialize(void *state);
void KeccakP1600_AddByte(void *state, unsigned char data, unsigned int offset);
void KeccakP1600_AddBytes(void *state, const unsigned char *data, unsigned int offset, unsigned int length);
void KeccakP1600_Permute_12rounds(void *state);
void KeccakP1600_ExtractBytes(const void *state, unsigned char *data, unsigned int offset, unsigned int length);
size_t KeccakP1600_12rounds_FastLoop_Absorb(void *state, unsigned int laneCount, const unsigned char *data, size_t dataByteLen);

#else

#define KeccakP1600_implementation_config "all rounds unrolled"
#define KeccakP1600_fullUnrolling
/* Or */
/*
#define KeccakP1600_implementation_config "6 rounds unrolled"
#define KeccakP1600_unrolling 6
*/
/* Or */
/*
#define KeccakP1600_implementation_config "lane complementing, 6 rounds unrolled"
#define KeccakP1600_unrolling 6
#define KeccakP1600_useLaneComplementing
*/
/* Or */
/*
#define KeccakP1600_implementation_config "lane complementing, all rounds unrolled"
#define KeccakP1600_fullUnrolling
#define KeccakP1600_useLaneComplementing
*/
/* Or */
/*
#define KeccakP1600_implementation_config "lane complementing, all rounds unrolled, using SHLD for rotations"
#define KeccakP1600_fullUnrolling
#define KeccakP1600_useLaneComplementing
#define KeccakP1600_useSHLD
*/

#define KeccakP1600_implementation      "generic 64-bit optimized implementation (" KeccakP1600_implementation_config ")"
#define KeccakP1600_stateSizeInBytes    200
#define KeccakP1600_stateAlignment      8
#define KeccakF1600_FastLoop_supported
#define KeccakP1600_12rounds_FastLoop_supported

#include <stddef.h>

#define KeccakP1600_StaticInitialize()
void KeccakP1600_Initialize(void *state);
#if (PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN)
#define KeccakP1600_AddByte(state, byte, offset) \
    ((unsigned char*)(state))[(offset)] ^= (byte)
#else
void KeccakP1600_AddByte(void *state, unsigned char data, unsigned int offset);
#endif
void KeccakP1600_AddBytes(void *state, const unsigned char *data, unsigned int offset, unsigned int length);
void KeccakP1600_Permute_12rounds(void *state);
void KeccakP1600_ExtractBytes(const void *state, unsigned char *data, unsigned int offset, unsigned int length);
size_t KeccakP1600_12rounds_FastLoop_Absorb(void *state, unsigned int laneCount, const unsigned char *data, size_t dataByteLen);

#endif
#endif

/* Keccak-p[1600]×2 */

#if defined(__AVX512F__)

#define KeccakP1600times2_implementation_config "AVX512, 12 rounds unrolled"
#define KeccakP1600times2_fullUnrolling
#define KeccakP1600times2_useAVX512

#define KeccakP1600times2_implementation        "512-bit SIMD implementation (" KeccakP1600times2_implementation_config ")"
#define KeccakP1600times2_statesSizeInBytes     400
#define KeccakP1600times2_statesAlignment       64
#define KeccakP1600times2_12rounds_FastLoop_supported

#define KeccakP1600times2_StaticInitialize()
void KeccakP1600times2_InitializeAll(void *states);
#define KeccakP1600times2_AddByte(states, instanceIndex, byte, offset) \
    ((unsigned char*)(states))[(instanceIndex)*8 + ((offset)/8)*2*8 + (offset)%8] ^= (byte)
void KeccakP1600times2_AddBytes(void *states, unsigned int instanceIndex, const unsigned char *data, unsigned int offset, unsigned int length);
void KeccakP1600times2_AddLanesAll(void *states, const unsigned char *data, unsigned int laneCount, unsigned int laneOffset);
void KeccakP1600times2_PermuteAll_12rounds(void *states);
void KeccakP1600times2_ExtractBytes(const void *states, unsigned int instanceIndex, unsigned char *data, unsigned int offset, unsigned int length);
void KeccakP1600times2_ExtractLanesAll(const void *states, unsigned char *data, unsigned int laneCount, unsigned int laneOffset);
size_t KeccakP1600times2_12rounds_FastLoop_Absorb(void *states, unsigned int laneCount, unsigned int laneOffsetParallel, unsigned int laneOffsetSerial, const unsigned char *data, size_t dataByteLen);

#else
#if defined(__SSE2__)

#define KeccakP1600times2_implementation_config "SSE2, 2 rounds unrolled"
#define KeccakP1600times2_unrolling 2
#define KeccakP1600times2_useSSE
#define KeccakP1600times2_useSSE2

#define KeccakP1600times2_implementation        "128-bit SIMD implementation (" KeccakP1600times2_implementation_config ")"
#define KeccakP1600times2_statesSizeInBytes     400
#define KeccakP1600times2_statesAlignment       16

#define KeccakP1600times2_StaticInitialize()
void KeccakP1600times2_InitializeAll(void *states);
#define KeccakP1600times2_AddByte(states, instanceIndex, byte, offset) \
    ((unsigned char*)(states))[(instanceIndex)*8 + ((offset)/8)*2*8 + (offset)%8] ^= (byte)
void KeccakP1600times2_AddBytes(void *states, unsigned int instanceIndex, const unsigned char *data, unsigned int offset, unsigned int length);
void KeccakP1600times2_AddLanesAll(void *states, const unsigned char *data, unsigned int laneCount, unsigned int laneOffset);
void KeccakP1600times2_PermuteAll_12rounds(void *states);
void KeccakP1600times2_ExtractBytes(const void *states, unsigned int instanceIndex, unsigned char *data, unsigned int offset, unsigned int length);
void KeccakP1600times2_ExtractLanesAll(const void *states, unsigned char *data, unsigned int laneCount, unsigned int laneOffset);

#endif
#endif

/* Keccak-p[1600]×4 */

#if defined(__AVX512F__)

#define KeccakP1600times4_implementation_config "AVX512, 12 rounds unrolled"
#define KeccakP1600times4_fullUnrolling
#define KeccakP1600times4_useAVX512

#define KeccakP1600times4_implementation        "512-bit SIMD implementation (" KeccakP1600times4_implementation_config ")"
#define KeccakP1600times4_statesSizeInBytes     800
#define KeccakP1600times4_statesAlignment       64
#define KeccakP1600times4_12rounds_FastLoop_supported

#define KeccakP1600times4_StaticInitialize()
void KeccakP1600times4_InitializeAll(void *states);
#define KeccakP1600times4_AddByte(states, instanceIndex, byte, offset) \
    ((unsigned char*)(states))[(instanceIndex)*8 + ((offset)/8)*4*8 + (offset)%8] ^= (byte)
void KeccakP1600times4_AddBytes(void *states, unsigned int instanceIndex, const unsigned char *data, unsigned int offset, unsigned int length);
void KeccakP1600times4_AddLanesAll(void *states, const unsigned char *data, unsigned int laneCount, unsigned int laneOffset);
void KeccakP1600times4_PermuteAll_12rounds(void *states);
void KeccakP1600times4_ExtractBytes(const void *states, unsigned int instanceIndex, unsigned char *data, unsigned int offset, unsigned int length);
void KeccakP1600times4_ExtractLanesAll(const void *states, unsigned char *data, unsigned int laneCount, unsigned int laneOffset);
size_t KeccakP1600times4_12rounds_FastLoop_Absorb(void *states, unsigned int laneCount, unsigned int laneOffsetParallel, unsigned int laneOffsetSerial, const unsigned char *data, size_t dataByteLen);

#else
#if defined(__AVX2__)

#define KeccakP1600times4_implementation_config "AVX2, 12 rounds unrolled"
#define KeccakP1600times4_unrolling 12
#define KeccakP1600times4_useAVX2

#define KeccakP1600times4_implementation        "256-bit SIMD implementation (" KeccakP1600times4_implementation_config ")"
#define KeccakP1600times4_statesSizeInBytes     800
#define KeccakP1600times4_statesAlignment       32
#define KeccakP1600times4_12rounds_FastLoop_supported

#define KeccakP1600times4_StaticInitialize()
void KeccakP1600times4_InitializeAll(void *states);
#define KeccakP1600times4_AddByte(states, instanceIndex, byte, offset) \
    ((unsigned char*)(states))[(instanceIndex)*8 + ((offset)/8)*4*8 + (offset)%8] ^= (byte)
void KeccakP1600times4_AddBytes(void *states, unsigned int instanceIndex, const unsigned char *data, unsigned int offset, unsigned int length);
void KeccakP1600times4_AddLanesAll(void *states, const unsigned char *data, unsigned int laneCount, unsigned int laneOffset);
void KeccakP1600times4_PermuteAll_12rounds(void *states);
void KeccakP1600times4_ExtractBytes(const void *states, unsigned int instanceIndex, unsigned char *data, unsigned int offset, unsigned int length);
void KeccakP1600times4_ExtractLanesAll(const void *states, unsigned char *data, unsigned int laneCount, unsigned int laneOffset);
size_t KeccakP1600times4_12rounds_FastLoop_Absorb(void *states, unsigned int laneCount, unsigned int laneOffsetParallel, unsigned int laneOffsetSerial, const unsigned char *data, size_t dataByteLen);

#endif
#endif

/* Keccak-p[1600]×8 */

#if defined(__AVX512F__)

#define KeccakP1600times8_implementation_config "AVX512, 12 rounds unrolled"
#define KeccakP1600times8_fullUnrolling
#define KeccakP1600times8_useAVX512

#define KeccakP1600times8_implementation        "512-bit SIMD implementation (" KeccakP1600times8_implementation_config ")"
#define KeccakP1600times8_statesSizeInBytes     1600
#define KeccakP1600times8_statesAlignment       64
#define KeccakP1600times8_12rounds_FastLoop_supported

#define KeccakP1600times8_StaticInitialize()
void KeccakP1600times8_InitializeAll(void *states);
#define KeccakP1600times8_AddByte(states, instanceIndex, byte, offset) \
    ((unsigned char*)(states))[(instanceIndex)*8 + ((offset)/8)*8*8 + (offset)%8] ^= (byte)
void KeccakP1600times8_AddBytes(void *states, unsigned int instanceIndex, const unsigned char *data, unsigned int offset, unsigned int length);
void KeccakP1600times8_AddLanesAll(void *states, const unsigned char *data, unsigned int laneCount, unsigned int laneOffset);
void KeccakP1600times8_PermuteAll_12rounds(void *states);
void KeccakP1600times8_ExtractBytes(const void *states, unsigned int instanceIndex, unsigned char *data, unsigned int offset, unsigned int length);
void KeccakP1600times8_ExtractLanesAll(const void *states, unsigned char *data, unsigned int laneCount, unsigned int laneOffset);
size_t KeccakP1600times8_12rounds_FastLoop_Absorb(void *states, unsigned int laneCount, unsigned int laneOffsetParallel, unsigned int laneOffsetSerial, const unsigned char *data, size_t dataByteLen);

#endif

#endif
