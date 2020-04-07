/*
Implementation by Ronny Van Keer, hereby denoted as "the implementer".

For more information, feedback or questions, please refer to our website:
https://keccak.team/

To the extent possible under law, the implementer has waived all copyright
and related or neighboring rights to the source code in this file.
http://creativecommons.org/publicdomain/zero/1.0/
*/

#include <assert.h>
#include <string.h>
#include "align.h"
#include "KangarooTwelve.h"
#include "KeccakP-1600-SnP.h"

void KangarooTwelve_SetProcessorCapabilities(void);
int K12_SSSE3_requested_disabled = 0;
int K12_AVX2_requested_disabled = 0;
int K12_AVX512_requested_disabled = 0;
int K12_enableSSSE3 = 0;
int K12_enableAVX2 = 0;
int K12_enableAVX512 = 0;

/* ---------------------------------------------------------------- */

ALIGN(KeccakP1600_stateAlignment) typedef struct Opaque_KangarooTwelve_FStruct {
    uint8_t state[KeccakP1600_stateSizeInBytes];
    uint8_t byteIOIndex;
    uint8_t squeezing;
} Opaque_KangarooTwelve_F;

#define K12_security        128
#define K12_capacity        (2*K12_security)
#define K12_capacityInBytes (K12_capacity/8)
#define K12_rate            (1600-K12_capacity)
#define K12_rateInBytes     (K12_rate/8)
#define K12_rateInLanes     (K12_rate/64)

static void KangarooTwelve_F_Initialize(Opaque_KangarooTwelve_F *instance)
{
    KeccakP1600_Initialize(instance->state);
    instance->byteIOIndex = 0;
    instance->squeezing = 0;
}

static void KangarooTwelve_F_Absorb(Opaque_KangarooTwelve_F *instance, const unsigned char *data, size_t dataByteLen)
{
    size_t i, j;
    uint8_t partialBlock;
    const unsigned char *curData;
    const uint8_t rateInBytes = K12_rateInBytes;

    assert(instance->squeezing == 0);

    i = 0;
    curData = data;
    while(i < dataByteLen) {
        if ((instance->byteIOIndex == 0) && (dataByteLen >= (i + rateInBytes))) {
#ifdef KeccakP1600_12rounds_FastLoop_supported
            /* processing full blocks first */
            j = KeccakP1600_12rounds_FastLoop_Absorb(instance->state, K12_rateInLanes, curData, dataByteLen - i);
            i += j;
            curData += j;
#endif
            for(j=dataByteLen-i; j>=rateInBytes; j-=rateInBytes) {
                KeccakP1600_AddBytes(instance->state, curData, 0, rateInBytes);
                KeccakP1600_Permute_12rounds(instance->state);
                curData+=rateInBytes;
            }
            i = dataByteLen - j;
        }
        else {
            /* normal lane: using the message queue */
            if ((dataByteLen - i) + instance->byteIOIndex > (size_t)rateInBytes)
                partialBlock = rateInBytes-instance->byteIOIndex;
            else
                partialBlock = (uint8_t)(dataByteLen - i);
            i += partialBlock;

            KeccakP1600_AddBytes(instance->state, curData, instance->byteIOIndex, partialBlock);
            curData += partialBlock;
            instance->byteIOIndex += partialBlock;
            if (instance->byteIOIndex == rateInBytes) {
                KeccakP1600_Permute_12rounds(instance->state);
                instance->byteIOIndex = 0;
            }
        }
    }
}

static void KangarooTwelve_F_AbsorbLastFewBits(Opaque_KangarooTwelve_F *instance, unsigned char delimitedData)
{
    const unsigned int rateInBytes = K12_rateInBytes;

    assert(delimitedData != 0);
    assert(instance->squeezing == 0);

    /* Last few bits, whose delimiter coincides with first bit of padding */
    KeccakP1600_AddByte(instance->state, delimitedData, instance->byteIOIndex);
    /* If the first bit of padding is at position rate-1, we need a whole new block for the second bit of padding */
    if ((delimitedData >= 0x80) && (instance->byteIOIndex == (rateInBytes-1)))
        KeccakP1600_Permute_12rounds(instance->state);
    /* Second bit of padding */
    KeccakP1600_AddByte(instance->state, 0x80, rateInBytes-1);
    KeccakP1600_Permute_12rounds(instance->state);
    instance->byteIOIndex = 0;
    instance->squeezing = 1;
}

static void KangarooTwelve_F_Squeeze(Opaque_KangarooTwelve_F *instance, unsigned char *data, size_t dataByteLen)
{
    size_t i, j;
    unsigned int partialBlock;
    const unsigned int rateInBytes = K12_rateInBytes;
    unsigned char *curData;

    if (!instance->squeezing)
        KangarooTwelve_F_AbsorbLastFewBits(instance, 0x01);

    i = 0;
    curData = data;
    while(i < dataByteLen) {
        if ((instance->byteIOIndex == rateInBytes) && (dataByteLen >= (i + rateInBytes))) {
            for(j=dataByteLen-i; j>=rateInBytes; j-=rateInBytes) {
                KeccakP1600_Permute_12rounds(instance->state);
                KeccakP1600_ExtractBytes(instance->state, curData, 0, rateInBytes);
                curData+=rateInBytes;
            }
            i = dataByteLen - j;
        }
        else {
            /* normal lane: using the message queue */
            if (instance->byteIOIndex == rateInBytes) {
                KeccakP1600_Permute_12rounds(instance->state);
                instance->byteIOIndex = 0;
            }
            partialBlock = (unsigned int)(dataByteLen - i);
            if (partialBlock+instance->byteIOIndex > rateInBytes)
                partialBlock = rateInBytes-instance->byteIOIndex;
            i += partialBlock;

            KeccakP1600_ExtractBytes(instance->state, curData, instance->byteIOIndex, partialBlock);
            curData += partialBlock;
            instance->byteIOIndex += partialBlock;
        }
    }
}

/* ---------------------------------------------------------------- */

typedef enum {
    NOT_INITIALIZED,
    ABSORBING,
    FINAL,
    SQUEEZING
} KCP_Phases;
typedef KCP_Phases KangarooTwelve_Phases;

typedef struct {
    Opaque_KangarooTwelve_F queueNode;
    Opaque_KangarooTwelve_F finalNode;
    size_t fixedOutputLength;
    size_t blockNumber;
    size_t queueAbsorbedLen;
    KangarooTwelve_Phases phase;
} Opaque_KangarooTwelve_Instance;

static void importF(Opaque_KangarooTwelve_F *internal_Instance, const KangarooTwelve_F *external_Instance)
{
    KeccakP1600_Initialize(internal_Instance->state);
    KeccakP1600_AddBytes(internal_Instance->state, external_Instance->state, 0, 200);
    internal_Instance->byteIOIndex = external_Instance->byteIOIndex;
    internal_Instance->squeezing = external_Instance->squeezing;
}

static void exportF(KangarooTwelve_F *external_Instance, const Opaque_KangarooTwelve_F *internal_Instance)
{
    KeccakP1600_ExtractBytes(internal_Instance->state, external_Instance->state, 0, 200);
    external_Instance->byteIOIndex = (uint8_t)internal_Instance->byteIOIndex;
    external_Instance->squeezing = (uint8_t)internal_Instance->squeezing;
}

static void import(Opaque_KangarooTwelve_Instance *internal_Instance, const KangarooTwelve_Instance *external_Instance)
{
    importF(&internal_Instance->queueNode, &external_Instance->queueNode);
    importF(&internal_Instance->finalNode, &external_Instance->finalNode);
    internal_Instance->fixedOutputLength = external_Instance->fixedOutputLength;
    internal_Instance->blockNumber = external_Instance->blockNumber;
    internal_Instance->queueAbsorbedLen = external_Instance->queueAbsorbedLen;
    internal_Instance->phase = (KangarooTwelve_Phases)external_Instance->phase;
}

static void export(KangarooTwelve_Instance *external_Instance, const Opaque_KangarooTwelve_Instance *internal_Instance)
{
    exportF(&external_Instance->queueNode, &internal_Instance->queueNode);
    exportF(&external_Instance->finalNode, &internal_Instance->finalNode);
    external_Instance->fixedOutputLength = internal_Instance->fixedOutputLength;
    external_Instance->blockNumber = internal_Instance->blockNumber;
    external_Instance->queueAbsorbedLen = internal_Instance->queueAbsorbedLen;
    external_Instance->phase = (int)internal_Instance->phase;
}

#define K12_chunkSize       8192
#define K12_suffixLeaf      0x0B /* '110': message hop, simple padding, inner node */

#ifndef KeccakP1600_disableParallelism

int KeccakP1600times2_IsAvailable()
{
    int result = 0;
    result |= K12_enableAVX512;
    result |= K12_enableSSSE3;
    return result;
}

const char * KeccakP1600times2_GetImplementation()
{
    if (K12_enableAVX512)
        return "AVX-512 implementation";
    else
    if (K12_enableSSSE3)
        return "SSSE3 implementation";
    else
        return "";
}

void KangarooTwelve_SSSE3_Process2Leaves(const unsigned char *input, unsigned char *output);
void KangarooTwelve_AVX512_Process2Leaves(const unsigned char *input, unsigned char *output);

void KangarooTwelve_Process2Leaves(const unsigned char *input, unsigned char *output)
{
    if (K12_enableAVX512)
        KangarooTwelve_AVX512_Process2Leaves(input, output);
    else
    if (K12_enableSSSE3)
        KangarooTwelve_SSSE3_Process2Leaves(input, output);
}

int KeccakP1600times4_IsAvailable()
{
    int result = 0;
    result |= K12_enableAVX512;
    result |= K12_enableAVX2;
    return result;
}

const char * KeccakP1600times4_GetImplementation()
{
    if (K12_enableAVX512)
        return "AVX-512 implementation";
    else
    if (K12_enableAVX2)
        return "AVX2 implementation";
    else
        return "";
}

void KangarooTwelve_AVX2_Process4Leaves(const unsigned char *input, unsigned char *output);
void KangarooTwelve_AVX512_Process4Leaves(const unsigned char *input, unsigned char *output);

void KangarooTwelve_Process4Leaves(const unsigned char *input, unsigned char *output)
{
    if (K12_enableAVX512)
        KangarooTwelve_AVX512_Process4Leaves(input, output);
    else
    if (K12_enableAVX2)
        KangarooTwelve_AVX2_Process4Leaves(input, output);
}

int KeccakP1600times8_IsAvailable()
{
    int result = 0;
    result |= K12_enableAVX512;
    return result;
}

const char * KeccakP1600times8_GetImplementation()
{
    if (K12_enableAVX512)
        return "AVX-512 implementation";
    else
        return "";
}

void KangarooTwelve_AVX512_Process8Leaves(const unsigned char *input, unsigned char *output);

void KangarooTwelve_Process8Leaves(const unsigned char *input, unsigned char *output)
{
    if (K12_enableAVX512)
        KangarooTwelve_AVX512_Process8Leaves(input, output);
}

#define ProcessLeaves( Parallellism ) \
    while ( inLen >= Parallellism * K12_chunkSize ) { \
        unsigned char intermediate[Parallellism*K12_capacityInBytes]; \
        \
        KangarooTwelve_Process##Parallellism##Leaves(input, intermediate); \
        input += Parallellism * K12_chunkSize; \
        inLen -= Parallellism * K12_chunkSize; \
        ktInstance->blockNumber += Parallellism; \
        KangarooTwelve_F_Absorb(&ktInstance->finalNode, intermediate, Parallellism * K12_capacityInBytes); \
    }

#endif

static unsigned int right_encode( unsigned char * encbuf, size_t value )
{
    unsigned int n, i;
    size_t v;

    for ( v = value, n = 0; v && (n < sizeof(size_t)); ++n, v >>= 8 )
        ; /* empty */
    for ( i = 1; i <= n; ++i )
        encbuf[i-1] = (unsigned char)(value >> (8 * (n-i)));
    encbuf[n] = (unsigned char)n;
    return n + 1;
}

static void K12_internal_Initialize(Opaque_KangarooTwelve_Instance *ktInstance, size_t outputLen)
{
    KangarooTwelve_SetProcessorCapabilities();
    ktInstance->fixedOutputLength = outputLen;
    ktInstance->queueAbsorbedLen = 0;
    ktInstance->blockNumber = 0;
    ktInstance->phase = ABSORBING;
    KangarooTwelve_F_Initialize(&ktInstance->finalNode);
}

int KangarooTwelve_Initialize(KangarooTwelve_Instance *ktInstance, size_t outputByteLen)
{
    Opaque_KangarooTwelve_Instance internal;

    K12_internal_Initialize(&internal, outputByteLen);
    export(ktInstance, &internal);
    return 0;
}

static int K12_internal_Update(Opaque_KangarooTwelve_Instance *ktInstance, const unsigned char *input, size_t inLen)
{
    if (ktInstance->phase != ABSORBING)
        return 1;

    if ( ktInstance->blockNumber == 0 ) {
        /* First block, absorb in final node */
        unsigned int len = (inLen < (K12_chunkSize - ktInstance->queueAbsorbedLen)) ? inLen : (K12_chunkSize - ktInstance->queueAbsorbedLen);
        KangarooTwelve_F_Absorb(&ktInstance->finalNode, input, len);
        input += len;
        inLen -= len;
        ktInstance->queueAbsorbedLen += len;
        if ( (ktInstance->queueAbsorbedLen == K12_chunkSize) && (inLen != 0) ) {
            /* First block complete and more input data available, finalize it */
            const unsigned char padding = 0x03; /* '110^6': message hop, simple padding */
            ktInstance->queueAbsorbedLen = 0;
            ktInstance->blockNumber = 1;
            KangarooTwelve_F_Absorb(&ktInstance->finalNode, &padding, 1);
            ktInstance->finalNode.byteIOIndex = (ktInstance->finalNode.byteIOIndex + 7) & ~7; /* Zero padding up to 64 bits */
        }
    }
    else if ( ktInstance->queueAbsorbedLen != 0 ) {
        /* There is data in the queue, absorb further in queue until block complete */
        unsigned int len = (inLen < (K12_chunkSize - ktInstance->queueAbsorbedLen)) ? inLen : (K12_chunkSize - ktInstance->queueAbsorbedLen);
        KangarooTwelve_F_Absorb(&ktInstance->queueNode, input, len);
        input += len;
        inLen -= len;
        ktInstance->queueAbsorbedLen += len;
        if ( ktInstance->queueAbsorbedLen == K12_chunkSize ) {
            unsigned char intermediate[K12_capacityInBytes];
            ktInstance->queueAbsorbedLen = 0;
            ++ktInstance->blockNumber;
            KangarooTwelve_F_AbsorbLastFewBits(&ktInstance->queueNode, K12_suffixLeaf);
            KangarooTwelve_F_Squeeze(&ktInstance->queueNode, intermediate, K12_capacityInBytes);
            KangarooTwelve_F_Absorb(&ktInstance->finalNode, intermediate, K12_capacityInBytes);
        }
    }

#ifndef KeccakP1600_disableParallelism
    if (KeccakP1600times8_IsAvailable()) {
        ProcessLeaves(8);
    }

    if (KeccakP1600times4_IsAvailable()) {
        ProcessLeaves(4);
    }

    if (KeccakP1600times2_IsAvailable()) {
        ProcessLeaves(2);
    }
#endif

    while ( inLen > 0 ) {
        unsigned int len = (inLen < K12_chunkSize) ? inLen : K12_chunkSize;
        KangarooTwelve_F_Initialize(&ktInstance->queueNode);
        KangarooTwelve_F_Absorb(&ktInstance->queueNode, input, len);
        input += len;
        inLen -= len;
        if ( len == K12_chunkSize ) {
            unsigned char intermediate[K12_capacityInBytes];
            ++ktInstance->blockNumber;
            KangarooTwelve_F_AbsorbLastFewBits(&ktInstance->queueNode, K12_suffixLeaf);
            KangarooTwelve_F_Squeeze(&ktInstance->queueNode, intermediate, K12_capacityInBytes);
            KangarooTwelve_F_Absorb(&ktInstance->finalNode, intermediate, K12_capacityInBytes);
        }
        else
            ktInstance->queueAbsorbedLen = len;
    }

    return 0;
}

int KangarooTwelve_Update(KangarooTwelve_Instance *ktInstance, const unsigned char *input, size_t inputByteLen)
{
    Opaque_KangarooTwelve_Instance internal;
    int result;

    import(&internal, ktInstance);
    result = K12_internal_Update(&internal, input, inputByteLen);
    export(ktInstance, &internal);
    return result;
}

static int K12_internal_Final(Opaque_KangarooTwelve_Instance *ktInstance, unsigned char * output, const unsigned char * customization, size_t customLen)
{
    unsigned char encbuf[sizeof(size_t)+1+2];
    unsigned char padding;

    if (ktInstance->phase != ABSORBING)
        return 1;

    /* Absorb customization | right_encode(customLen) */
    if ((customLen != 0) && (K12_internal_Update(ktInstance, customization, customLen) != 0))
        return 1;
    if (K12_internal_Update(ktInstance, encbuf, right_encode(encbuf, customLen)) != 0)
        return 1;

    if ( ktInstance->blockNumber == 0 ) {
        /* Non complete first block in final node, pad it */
        padding = 0x07; /*  '11': message hop, final node */
    }
    else {
        unsigned int n;

        if ( ktInstance->queueAbsorbedLen != 0 ) {
            /* There is data in the queue node */
            unsigned char intermediate[K12_capacityInBytes];
            ++ktInstance->blockNumber;
            KangarooTwelve_F_AbsorbLastFewBits(&ktInstance->queueNode, K12_suffixLeaf);
            KangarooTwelve_F_Squeeze(&ktInstance->queueNode, intermediate, K12_capacityInBytes);
            KangarooTwelve_F_Absorb(&ktInstance->finalNode, intermediate, K12_capacityInBytes);
        }
        --ktInstance->blockNumber; /* Absorb right_encode(number of Chaining Values) || 0xFF || 0xFF */
        n = right_encode(encbuf, ktInstance->blockNumber);
        encbuf[n++] = 0xFF;
        encbuf[n++] = 0xFF;
        KangarooTwelve_F_Absorb(&ktInstance->finalNode, encbuf, n);
        padding = 0x06; /* '01': chaining hop, final node */
    }
    KangarooTwelve_F_AbsorbLastFewBits(&ktInstance->finalNode, padding);
    if ( ktInstance->fixedOutputLength != 0 ) {
        ktInstance->phase = FINAL;
        KangarooTwelve_F_Squeeze(&ktInstance->finalNode, output, ktInstance->fixedOutputLength);
        return 0;
    }
    ktInstance->phase = SQUEEZING;
    return 0;
}

int KangarooTwelve_Final(KangarooTwelve_Instance *ktInstance, unsigned char *output, const unsigned char *customization, size_t customByteLen)
{
    Opaque_KangarooTwelve_Instance internal;
    int result;

    import(&internal, ktInstance);
    result = K12_internal_Final(&internal, output, customization, customByteLen);
    export(ktInstance, &internal);
    return result;
}

static int K12_internal_Squeeze(Opaque_KangarooTwelve_Instance *ktInstance, unsigned char * output, size_t outputLen)
{
    if (ktInstance->phase != SQUEEZING)
        return 1;
    KangarooTwelve_F_Squeeze(&ktInstance->finalNode, output, outputLen);
    return 0;
}

int KangarooTwelve_Squeeze(KangarooTwelve_Instance *ktInstance, unsigned char *output, size_t outputByteLen)
{
    Opaque_KangarooTwelve_Instance internal;
    int result;

    import(&internal, ktInstance);
    result = K12_internal_Squeeze(&internal, output, outputByteLen);
    export(ktInstance, &internal);
    return result;
}

int KangarooTwelve( const unsigned char * input, size_t inLen, unsigned char * output, size_t outLen, const unsigned char * customization, size_t customLen )
{
    Opaque_KangarooTwelve_Instance ktInstance;

    if (outLen == 0)
        return 1;
    K12_internal_Initialize(&ktInstance, outLen);
    if (K12_internal_Update(&ktInstance, input, inLen) != 0)
        return 1;
    return K12_internal_Final(&ktInstance, output, customization, customLen);
}

/* ---------------------------------------------------------------- */

/* Processor capability detection code by Samuel Neves and Jack O'Connor, see
 * https://github.com/BLAKE3-team/BLAKE3/blob/master/c/blake3_dispatch.c
 */

#if defined(__x86_64__) || defined(_M_X64)
#define IS_X86
#define IS_X86_64
#endif

#if defined(__i386__) || defined(_M_IX86)
#define IS_X86
#define IS_X86_32
#endif

#if defined(IS_X86)
static uint64_t xgetbv() {
#if defined(_MSC_VER)
  return _xgetbv(0);
#else
  uint32_t eax = 0, edx = 0;
  __asm__ __volatile__("xgetbv\n" : "=a"(eax), "=d"(edx) : "c"(0));
  return ((uint64_t)edx << 32) | eax;
#endif
}

static void cpuid(uint32_t out[4], uint32_t id) {
#if defined(_MSC_VER)
  __cpuid((int *)out, id);
#elif defined(__i386__) || defined(_M_IX86)
  __asm__ __volatile__("movl %%ebx, %1\n"
                       "cpuid\n"
                       "xchgl %1, %%ebx\n"
                       : "=a"(out[0]), "=r"(out[1]), "=c"(out[2]), "=d"(out[3])
                       : "a"(id));
#else
  __asm__ __volatile__("cpuid\n"
                       : "=a"(out[0]), "=b"(out[1]), "=c"(out[2]), "=d"(out[3])
                       : "a"(id));
#endif
}

static void cpuidex(uint32_t out[4], uint32_t id, uint32_t sid) {
#if defined(_MSC_VER)
  __cpuidex((int *)out, id, sid);
#elif defined(__i386__) || defined(_M_IX86)
  __asm__ __volatile__("movl %%ebx, %1\n"
                       "cpuid\n"
                       "xchgl %1, %%ebx\n"
                       : "=a"(out[0]), "=r"(out[1]), "=c"(out[2]), "=d"(out[3])
                       : "a"(id), "c"(sid));
#else
  __asm__ __volatile__("cpuid\n"
                       : "=a"(out[0]), "=b"(out[1]), "=c"(out[2]), "=d"(out[3])
                       : "a"(id), "c"(sid));
#endif
}

#endif

enum cpu_feature {
  SSE2 = 1 << 0,
  SSSE3 = 1 << 1,
  SSE41 = 1 << 2,
  AVX = 1 << 3,
  AVX2 = 1 << 4,
  AVX512F = 1 << 5,
  AVX512VL = 1 << 6,
  /* ... */
  UNDEFINED = 1 << 30
};

static enum cpu_feature g_cpu_features = UNDEFINED;

static enum cpu_feature
    get_cpu_features(void) {

  if (g_cpu_features != UNDEFINED) {
    return g_cpu_features;
  } else {
#if defined(IS_X86)
    uint32_t regs[4] = {0};
    uint32_t *eax = &regs[0], *ebx = &regs[1], *ecx = &regs[2], *edx = &regs[3];
    (void)edx;
    enum cpu_feature features = 0;
    cpuid(regs, 0);
    const int max_id = *eax;
    cpuid(regs, 1);
#if defined(__amd64__) || defined(_M_X64)
    features |= SSE2;
#else
    if (*edx & (1UL << 26))
      features |= SSE2;
#endif
    if (*ecx & (1UL << 0))
      features |= SSSE3;
    if (*ecx & (1UL << 19))
      features |= SSE41;

    if (*ecx & (1UL << 27)) { // OSXSAVE
      const uint64_t mask = xgetbv();
      if ((mask & 6) == 6) { // SSE and AVX states
        if (*ecx & (1UL << 28))
          features |= AVX;
        if (max_id >= 7) {
          cpuidex(regs, 7, 0);
          if (*ebx & (1UL << 5))
            features |= AVX2;
          if ((mask & 224) == 224) { // Opmask, ZMM_Hi256, Hi16_Zmm
            if (*ebx & (1UL << 31))
              features |= AVX512VL;
            if (*ebx & (1UL << 16))
              features |= AVX512F;
          }
        }
      }
    }
    g_cpu_features = features;
    return features;
#else
    /* How to detect NEON? */
    return 0;
#endif
  }
}

void KangarooTwelve_SetProcessorCapabilities(void)
{
    enum cpu_feature features = get_cpu_features();
    K12_enableSSSE3 = (features & SSSE3) && !K12_SSSE3_requested_disabled;
    K12_enableAVX2 = (features & AVX2) && !K12_AVX2_requested_disabled;
    K12_enableAVX512 = (features & AVX512F) && (features & AVX512VL) && !K12_AVX512_requested_disabled;
}

#ifndef KeccakP1600_disableParallelism
int KangarooTwelve_DisableSSSE3(void) {
    KangarooTwelve_SetProcessorCapabilities();
    K12_SSSE3_requested_disabled = 1;
    if (K12_enableSSSE3) {
        KangarooTwelve_SetProcessorCapabilities();
        return 1;  // SSSE3 was disabled on this call.
    } else {
        return 0;  // Nothing changed.
    }
}

int KangarooTwelve_DisableAVX2(void) {
    KangarooTwelve_SetProcessorCapabilities();
    K12_AVX2_requested_disabled = 1;
    if (K12_enableAVX2) {
        KangarooTwelve_SetProcessorCapabilities();
        return 1;  // AVX2 was disabled on this call.
    } else {
        return 0;  // Nothing changed.
    }
}

int KangarooTwelve_DisableAVX512(void) {
    KangarooTwelve_SetProcessorCapabilities();
    K12_AVX512_requested_disabled = 1;
    if (K12_enableAVX512) {
        KangarooTwelve_SetProcessorCapabilities();
        return 1;  // AVX512 was disabled on this call.
    } else {
        return 0;  // Nothing changed.
    }
}

void KangarooTwelve_EnableAllCpuFeatures(void) {
    K12_SSSE3_requested_disabled = 0;
    K12_AVX2_requested_disabled = 0;
    K12_AVX512_requested_disabled = 0;
    KangarooTwelve_SetProcessorCapabilities();
}
#endif  // KeccakP1600_disableParallelism
