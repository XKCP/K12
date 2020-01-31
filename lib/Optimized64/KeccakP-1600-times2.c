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

#if defined(__SSE2__) || defined(__AVX512F__)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <smmintrin.h>
#include <wmmintrin.h>
#include <immintrin.h>
#include <emmintrin.h>
#include "KeccakP-1600-SnP.h"

#ifdef ALIGN
#undef ALIGN
#endif

#if defined(__GNUC__)
#define ALIGN(x) __attribute__ ((aligned(x)))
#elif defined(_MSC_VER)
#define ALIGN(x) __declspec(align(x))
#elif defined(__ARMCC_VERSION)
#define ALIGN(x) __align(x)
#else
#define ALIGN(x)
#endif

typedef uint8_t     UINT8;
typedef uint32_t    UINT32;
typedef uint64_t    UINT64;

typedef __m128i     V128;
typedef __m256i     V256;
typedef __m512i     V512;

#define laneIndex(instanceIndex, lanePosition)  ((lanePosition)*2 + instanceIndex)
#define SnP_laneLengthInBytes                   8

#if defined(KeccakP1600times2_useAVX512)

#define XOR(a,b)                    _mm_xor_si128(a,b)
#define XOR3(a,b,c)                 _mm_ternarylogic_epi64(a,b,c,0x96)
#define XOR5(a,b,c,d,e)             XOR3(XOR3(a,b,c),d,e)
#define XOR512(a,b)                 _mm512_xor_si512(a,b)
#define ROL(a,offset)               _mm_rol_epi64(a,offset)
#define Chi(a,b,c)                  _mm_ternarylogic_epi64(a,b,c,0xD2)

#define CONST128_64(a)              _mm_set1_epi64((__m64)(a))
#define LOAD4_32(a,b,c,d)           _mm_set_epi32((UINT64)(a), (UINT32)(b), (UINT32)(c), (UINT32)(d))
#define LOAD8_32(a,b,c,d,e,f,g,h)   _mm256_set_epi32((UINT64)(a), (UINT32)(b), (UINT32)(c), (UINT32)(d), (UINT32)(e), (UINT32)(f), (UINT32)(g), (UINT32)(h))
#define LOAD_GATHER2_64(idx,p)      _mm_i32gather_epi64( (const void*)(p), idx, 8)
#define LOAD_GATHER8_64(idx,p)      _mm512_i32gather_epi64( idx, (const void*)(p), 8)
#define STORE_SCATTER2_64(p,idx, v) _mm_i32scatter_epi64( (void*)(p), idx, v, 8)
#define STORE_SCATTER8_64(p,idx, v) _mm512_i32scatter_epi64( (void*)(p), idx, v, 8)

#endif

#if defined(KeccakP1600times2_useSSE)
    #define ANDnu128(a, b)      _mm_andnot_si128(a, b)
    #define CONST128(a)         _mm_load_si128((const V128 *)&(a))
    #define LOAD128(a)          _mm_load_si128((const V128 *)&(a))
    #define LOAD128u(a)         _mm_loadu_si128((const V128 *)&(a))
    #define LOAD6464(a, b)      _mm_set_epi64((__m64)(a), (__m64)(b))
    #define CONST128_64(a)      _mm_set1_epi64((__m64)(a))
    #if defined(KeccakP1600times2_useXOP)
        #define ROL64in128(a, o)    _mm_roti_epi64(a, o)
        #define ROL64in128_8(a)     ROL64in128(a, 8)
        #define ROL64in128_56(a)    ROL64in128(a, 56)
    #else
        #define ROL64in128(a, o)    _mm_or_si128(_mm_slli_epi64(a, o), _mm_srli_epi64(a, 64-(o)))
        #define ROL64in128_8(a)     _mm_shuffle_epi8(a, CONST128(rho8))
        #define ROL64in128_56(a)    _mm_shuffle_epi8(a, CONST128(rho56))
static const UINT64 rho8[2] = {0x0605040302010007, 0x0E0D0C0B0A09080F};
static const UINT64 rho56[2] = {0x0007060504030201, 0x080F0E0D0C0B0A09};
    #endif
    #define STORE128(a, b)      _mm_store_si128((V128 *)&(a), b)
    #define STORE128u(a, b)     _mm_storeu_si128((V128 *)&(a), b)
    #define STORE64L(a, b)      _mm_storel_pi((__m64 *)&(a), (__m128)b)
    #define STORE64H(a, b)      _mm_storeh_pi((__m64 *)&(a), (__m128)b)
    #define XOR128(a, b)        _mm_xor_si128(a, b)
    #define XOReq128(a, b)      a = _mm_xor_si128(a, b)
    #define ZERO128()           _mm_setzero_si128()
#if defined(KeccakP1600times2_useSSE2)
    #define UNPACKL( a, b )     _mm_unpacklo_epi64((a), (b))
    #define UNPACKH( a, b )     _mm_unpackhi_epi64((a), (b))
#endif
#endif

void KeccakP1600times2_InitializeAll(void *states)
{
    memset(states, 0, KeccakP1600times2_statesSizeInBytes);
}

void KeccakP1600times2_AddBytes(void *states, unsigned int instanceIndex, const unsigned char *data, unsigned int offset, unsigned int length)
{
    unsigned int sizeLeft = length;
    unsigned int lanePosition = offset/SnP_laneLengthInBytes;
    unsigned int offsetInLane = offset%SnP_laneLengthInBytes;
    const unsigned char *curData = data;
    UINT64 *statesAsLanes = (UINT64 *)states;

    if ((sizeLeft > 0) && (offsetInLane != 0)) {
        unsigned int bytesInLane = SnP_laneLengthInBytes - offsetInLane;
        UINT64 lane = 0;
        if (bytesInLane > sizeLeft)
            bytesInLane = sizeLeft;
        memcpy((unsigned char*)&lane + offsetInLane, curData, bytesInLane);
        statesAsLanes[laneIndex(instanceIndex, lanePosition)] ^= lane;
        sizeLeft -= bytesInLane;
        lanePosition++;
        curData += bytesInLane;
    }

    while(sizeLeft >= SnP_laneLengthInBytes) {
        UINT64 lane = *((const UINT64*)curData);
        statesAsLanes[laneIndex(instanceIndex, lanePosition)] ^= lane;
        sizeLeft -= SnP_laneLengthInBytes;
        lanePosition++;
        curData += SnP_laneLengthInBytes;
    }

    if (sizeLeft > 0) {
        UINT64 lane = 0;
        memcpy(&lane, curData, sizeLeft);
        statesAsLanes[laneIndex(instanceIndex, lanePosition)] ^= lane;
    }
}

#if defined(KeccakP1600times2_useAVX512)
void KeccakP1600times2_AddLanesAll(void *states, const unsigned char *data, unsigned int laneCount, unsigned int laneOffset)
{
    V128 *stateAsLanes128 = states;
    V512 *stateAsLanes512 = states;
    const UINT64 *dataAsLanes = (const UINT64 *)data;
    unsigned int i;
    V128 index128 = LOAD4_32(0, 0, 1*laneOffset, 0*laneOffset);
    V256 index512 = LOAD8_32(1*laneOffset+3, 0*laneOffset+3, 1*laneOffset+2, 0*laneOffset+2, 1*laneOffset+1, 0*laneOffset+1, 1*laneOffset, 0*laneOffset);

    #define Add_In1( argIndex )  stateAsLanes128[argIndex] = XOR(stateAsLanes128[argIndex], LOAD_GATHER2_64(index128, dataAsLanes+argIndex))
    #define Add_In4( argIndex )  stateAsLanes512[argIndex/4] = XOR512(stateAsLanes512[argIndex/4], LOAD_GATHER8_64(index512, dataAsLanes+argIndex))
    if ( laneCount >= 16 )  {
        Add_In4( 0 );
        Add_In4( 4 );
        Add_In4( 8 );
        Add_In4( 12 );
        if ( laneCount >= 20 )  {
            Add_In4( 16 );
            for(i=20; i<laneCount; i++)
                Add_In1( i );
        }
        else {
            for(i=16; i<laneCount; i++)
                Add_In1( i );
        }
    }
    else {
        for(i=0; i<laneCount; i++)
            Add_In1( i );
    }
    #undef  Add_In1
    #undef  Add_In4
}
#endif

#if defined(KeccakP1600times2_useSSE2)
void KeccakP1600times2_AddLanesAll(void *states, const unsigned char *data, unsigned int laneCount, unsigned int laneOffset)
{
    V128 *stateAsLanes = (V128 *)states;
    unsigned int i;
    const UINT64 *curData0 = (const UINT64 *)data;
    const UINT64 *curData1 = (const UINT64 *)(data+laneOffset*SnP_laneLengthInBytes);
    #define XOR_In( argIndex )  XOReq128( stateAsLanes[argIndex], LOAD6464(curData1[argIndex], curData0[argIndex]))
    if ( laneCount >= 17 )  {
        XOR_In( 0 );
        XOR_In( 1 );
        XOR_In( 2 );
        XOR_In( 3 );
        XOR_In( 4 );
        XOR_In( 5 );
        XOR_In( 6 );
        XOR_In( 7 );
        XOR_In( 8 );
        XOR_In( 9 );
        XOR_In( 10 );
        XOR_In( 11 );
        XOR_In( 12 );
        XOR_In( 13 );
        XOR_In( 14 );
        XOR_In( 15 );
        XOR_In( 16 );
        if ( laneCount >= 21 )  {
            XOR_In( 17 );
            XOR_In( 18 );
            XOR_In( 19 );
            XOR_In( 20 );
            for(i=21; i<laneCount; i++)
                XOR_In( i );
        }
        else {
            for(i=17; i<laneCount; i++)
                XOR_In( i );
        }
    }
    else {
        for(i=0; i<laneCount; i++)
            XOR_In( i );
    }
    #undef  XOR_In
}
#endif

void KeccakP1600times2_ExtractBytes(const void *states, unsigned int instanceIndex, unsigned char *data, unsigned int offset, unsigned int length)
{
    unsigned int sizeLeft = length;
    unsigned int lanePosition = offset/SnP_laneLengthInBytes;
    unsigned int offsetInLane = offset%SnP_laneLengthInBytes;
    unsigned char *curData = data;
    const UINT64 *statesAsLanes = (const UINT64 *)states;

    if ((sizeLeft > 0) && (offsetInLane != 0)) {
        unsigned int bytesInLane = SnP_laneLengthInBytes - offsetInLane;
        if (bytesInLane > sizeLeft)
            bytesInLane = sizeLeft;
        memcpy( curData, ((unsigned char *)&statesAsLanes[laneIndex(instanceIndex, lanePosition)]) + offsetInLane, bytesInLane);
        sizeLeft -= bytesInLane;
        lanePosition++;
        curData += bytesInLane;
    }

    while(sizeLeft >= SnP_laneLengthInBytes) {
        *(UINT64*)curData = statesAsLanes[laneIndex(instanceIndex, lanePosition)];
        sizeLeft -= SnP_laneLengthInBytes;
        lanePosition++;
        curData += SnP_laneLengthInBytes;
    }

    if (sizeLeft > 0) {
        memcpy( curData, &statesAsLanes[laneIndex(instanceIndex, lanePosition)], sizeLeft);
    }
}

#if defined(KeccakP1600times2_useAVX512)
void KeccakP1600times2_ExtractLanesAll(const void *states, unsigned char *data, unsigned int laneCount, unsigned int laneOffset)
{
    const V128 *stateAsLanes128 = states;
    const V512 *stateAsLanes512 = states;
    UINT64 *dataAsLanes = (UINT64 *)data;
    unsigned int i;
    V128 index128 = LOAD4_32(0, 0, 1*laneOffset, 0*laneOffset);
    V256 index512 = LOAD8_32(1*laneOffset+3, 0*laneOffset+3, 1*laneOffset+2, 0*laneOffset+2, 1*laneOffset+1, 0*laneOffset+1, 1*laneOffset, 0*laneOffset);

    #define Extr1( argIndex )  STORE_SCATTER2_64(dataAsLanes+argIndex, index128, stateAsLanes128[argIndex])
    #define Extr4( argIndex )  STORE_SCATTER8_64(dataAsLanes+argIndex, index512, stateAsLanes512[argIndex/4])
    if ( laneCount >= 16 )  {
        Extr4( 0 );
        Extr4( 4 );
        Extr4( 8 );
        Extr4( 12 );
        if ( laneCount >= 20 )  {
            Extr4( 16 );
            for(i=20; i<laneCount; i++)
                Extr1( i );
        }
        else {
            for(i=16; i<laneCount; i++)
                Extr1( i );
        }
    }
    else {
        for(i=0; i<laneCount; i++)
            Extr1( i );
    }
    #undef  Extr1
    #undef  Extr4
}
#endif

#if defined(KeccakP1600times2_useSSE2)
void KeccakP1600times2_ExtractLanesAll(const void *states, unsigned char *data, unsigned int laneCount, unsigned int laneOffset)
{
    const V128 *stateAsLanes = (const V128 *)states;
    V128 lanes;
    unsigned int i;
    UINT64 *curData0 = (UINT64 *)data;
    UINT64 *curData1 = (UINT64 *)(data+laneOffset*SnP_laneLengthInBytes);

    #define Extr( argIndex )    lanes = LOAD128( stateAsLanes[argIndex] ),          \
                                STORE64L( curData0[argIndex], lanes ),              \
                                STORE64H( curData1[argIndex], lanes )

    #define Extr2( argIndex )   lanes0 = LOAD128( stateAsLanes[argIndex] ),         \
                                lanes1 = LOAD128( stateAsLanes[(argIndex)+1] ),     \
                                lanes =  UNPACKL( lanes0, lanes1 ),                 \
                                lanes0 = UNPACKH( lanes0, lanes1 ),                 \
                                STORE128u( *(V128*)&curData0[argIndex], lanes ),    \
                                STORE128u( *(V128*)&curData1[argIndex], lanes0 )
    if ( laneCount >= 16 )  {
        V128 lanes0, lanes1;
        Extr2( 0 );
        Extr2( 2 );
        Extr2( 4 );
        Extr2( 6 );
        Extr2( 8 );
        Extr2( 10 );
        Extr2( 12 );
        Extr2( 14 );
        if ( laneCount >= 20 )  {
            Extr2( 16 );
            Extr2( 18 );
            for(i=20; i<laneCount; i++)
                Extr( i );
        }
        else {
            for(i=16; i<laneCount; i++)
                Extr( i );
        }
    }
    else {
        for(i=0; i<laneCount; i++)
            Extr( i );
    }
    #undef  Extr
    #undef  Extr2
}
#endif

static ALIGN(KeccakP1600times2_statesAlignment) const UINT64 KeccakP1600RoundConstants[24] = {
    0x0000000000000001ULL,
    0x0000000000008082ULL,
    0x800000000000808aULL,
    0x8000000080008000ULL,
    0x000000000000808bULL,
    0x0000000080000001ULL,
    0x8000000080008081ULL,
    0x8000000000008009ULL,
    0x000000000000008aULL,
    0x0000000000000088ULL,
    0x0000000080008009ULL,
    0x000000008000000aULL,
    0x000000008000808bULL,
    0x800000000000008bULL,
    0x8000000000008089ULL,
    0x8000000000008003ULL,
    0x8000000000008002ULL,
    0x8000000000000080ULL,
    0x000000000000800aULL,
    0x800000008000000aULL,
    0x8000000080008081ULL,
    0x8000000000008080ULL,
    0x0000000080000001ULL,
    0x8000000080008008ULL};

#if defined(KeccakP1600times2_useAVX512)

#define KeccakP_DeclareVars \
    V128    _Ba, _Be, _Bi, _Bo, _Bu; \
    V128    _Da, _De, _Di, _Do, _Du; \
    V128    _ba, _be, _bi, _bo, _bu; \
    V128    _ga, _ge, _gi, _go, _gu; \
    V128    _ka, _ke, _ki, _ko, _ku; \
    V128    _ma, _me, _mi, _mo, _mu; \
    V128    _sa, _se, _si, _so, _su

#define KeccakP_ThetaRhoPiChi( _L1, _L2, _L3, _L4, _L5, _Bb1, _Bb2, _Bb3, _Bb4, _Bb5, _Rr1, _Rr2, _Rr3, _Rr4, _Rr5 ) \
    _Bb1 = XOR(_L1, _Da); \
    _Bb2 = XOR(_L2, _De); \
    _Bb3 = XOR(_L3, _Di); \
    _Bb4 = XOR(_L4, _Do); \
    _Bb5 = XOR(_L5, _Du); \
    if (_Rr1 != 0) _Bb1 = ROL(_Bb1, _Rr1); \
    _Bb2 = ROL(_Bb2, _Rr2); \
    _Bb3 = ROL(_Bb3, _Rr3); \
    _Bb4 = ROL(_Bb4, _Rr4); \
    _Bb5 = ROL(_Bb5, _Rr5); \
    _L1 = Chi( _Ba, _Be, _Bi); \
    _L2 = Chi( _Be, _Bi, _Bo); \
    _L3 = Chi( _Bi, _Bo, _Bu); \
    _L4 = Chi( _Bo, _Bu, _Ba); \
    _L5 = Chi( _Bu, _Ba, _Be);

#define KeccakP_ThetaRhoPiChiIota0( _L1, _L2, _L3, _L4, _L5, _rc ) \
    _Ba = XOR5( _ba, _ga, _ka, _ma, _sa ); /* Theta effect */ \
    _Be = XOR5( _be, _ge, _ke, _me, _se ); \
    _Bi = XOR5( _bi, _gi, _ki, _mi, _si ); \
    _Bo = XOR5( _bo, _go, _ko, _mo, _so ); \
    _Bu = XOR5( _bu, _gu, _ku, _mu, _su ); \
    _Da = ROL( _Be, 1 ); \
    _De = ROL( _Bi, 1 ); \
    _Di = ROL( _Bo, 1 ); \
    _Do = ROL( _Bu, 1 ); \
    _Du = ROL( _Ba, 1 ); \
    _Da = XOR( _Da, _Bu ); \
    _De = XOR( _De, _Ba ); \
    _Di = XOR( _Di, _Be ); \
    _Do = XOR( _Do, _Bi ); \
    _Du = XOR( _Du, _Bo ); \
    KeccakP_ThetaRhoPiChi( _L1, _L2, _L3, _L4, _L5, _Ba, _Be, _Bi, _Bo, _Bu,  0, 44, 43, 21, 14 ); \
    _L1 = XOR(_L1, _rc) /* Iota */

#define KeccakP_ThetaRhoPiChi1( _L1, _L2, _L3, _L4, _L5 ) \
    KeccakP_ThetaRhoPiChi( _L1, _L2, _L3, _L4, _L5, _Bi, _Bo, _Bu, _Ba, _Be,  3, 45, 61, 28, 20 )

#define KeccakP_ThetaRhoPiChi2( _L1, _L2, _L3, _L4, _L5 ) \
    KeccakP_ThetaRhoPiChi( _L1, _L2, _L3, _L4, _L5, _Bu, _Ba, _Be, _Bi, _Bo, 18,  1,  6, 25,  8 )

#define KeccakP_ThetaRhoPiChi3( _L1, _L2, _L3, _L4, _L5 ) \
    KeccakP_ThetaRhoPiChi( _L1, _L2, _L3, _L4, _L5, _Be, _Bi, _Bo, _Bu, _Ba, 36, 10, 15, 56, 27 )

#define KeccakP_ThetaRhoPiChi4( _L1, _L2, _L3, _L4, _L5 ) \
    KeccakP_ThetaRhoPiChi( _L1, _L2, _L3, _L4, _L5, _Bo, _Bu, _Ba, _Be, _Bi, 41,  2, 62, 55, 39 )

#define KeccakP_4rounds( i ) \
    KeccakP_ThetaRhoPiChiIota0(_ba, _ge, _ki, _mo, _su, CONST128_64(KeccakP1600RoundConstants[i]) ); \
    KeccakP_ThetaRhoPiChi1(    _ka, _me, _si, _bo, _gu ); \
    KeccakP_ThetaRhoPiChi2(    _sa, _be, _gi, _ko, _mu ); \
    KeccakP_ThetaRhoPiChi3(    _ga, _ke, _mi, _so, _bu ); \
    KeccakP_ThetaRhoPiChi4(    _ma, _se, _bi, _go, _ku ); \
\
    KeccakP_ThetaRhoPiChiIota0(_ba, _me, _gi, _so, _ku, CONST128_64(KeccakP1600RoundConstants[i+1]) ); \
    KeccakP_ThetaRhoPiChi1(    _sa, _ke, _bi, _mo, _gu ); \
    KeccakP_ThetaRhoPiChi2(    _ma, _ge, _si, _ko, _bu ); \
    KeccakP_ThetaRhoPiChi3(    _ka, _be, _mi, _go, _su ); \
    KeccakP_ThetaRhoPiChi4(    _ga, _se, _ki, _bo, _mu ); \
\
    KeccakP_ThetaRhoPiChiIota0(_ba, _ke, _si, _go, _mu, CONST128_64(KeccakP1600RoundConstants[i+2]) ); \
    KeccakP_ThetaRhoPiChi1(    _ma, _be, _ki, _so, _gu ); \
    KeccakP_ThetaRhoPiChi2(    _ga, _me, _bi, _ko, _su ); \
    KeccakP_ThetaRhoPiChi3(    _sa, _ge, _mi, _bo, _ku ); \
    KeccakP_ThetaRhoPiChi4(    _ka, _se, _gi, _mo, _bu ); \
\
    KeccakP_ThetaRhoPiChiIota0(_ba, _be, _bi, _bo, _bu, CONST128_64(KeccakP1600RoundConstants[i+3]) ); \
    KeccakP_ThetaRhoPiChi1(    _ga, _ge, _gi, _go, _gu ); \
    KeccakP_ThetaRhoPiChi2(    _ka, _ke, _ki, _ko, _ku ); \
    KeccakP_ThetaRhoPiChi3(    _ma, _me, _mi, _mo, _mu ); \
    KeccakP_ThetaRhoPiChi4(    _sa, _se, _si, _so, _su )

#ifdef KeccakP1600times2_fullUnrolling

#define rounds12 \
    KeccakP_4rounds( 12 ); \
    KeccakP_4rounds( 16 ); \
    KeccakP_4rounds( 20 )

#elif (KeccakP1600times2_unrolling == 4)

#define rounds12 \
    i = 12; \
    do { \
        KeccakP_4rounds( i ); \
    } while( (i += 4) < 24 )

#elif (KeccakP1600times2_unrolling == 12)

#define rounds12 \
    KeccakP_4rounds( 12 ); \
    KeccakP_4rounds( 16 ); \
    KeccakP_4rounds( 20 )

#else
#error "Unrolling is not correctly specified!"
#endif

#define copyFromState(pState) \
    _ba = pState[ 0]; \
    _be = pState[ 1]; \
    _bi = pState[ 2]; \
    _bo = pState[ 3]; \
    _bu = pState[ 4]; \
    _ga = pState[ 5]; \
    _ge = pState[ 6]; \
    _gi = pState[ 7]; \
    _go = pState[ 8]; \
    _gu = pState[ 9]; \
    _ka = pState[10]; \
    _ke = pState[11]; \
    _ki = pState[12]; \
    _ko = pState[13]; \
    _ku = pState[14]; \
    _ma = pState[15]; \
    _me = pState[16]; \
    _mi = pState[17]; \
    _mo = pState[18]; \
    _mu = pState[19]; \
    _sa = pState[20]; \
    _se = pState[21]; \
    _si = pState[22]; \
    _so = pState[23]; \
    _su = pState[24]

#define copyToState(pState) \
    pState[ 0] = _ba; \
    pState[ 1] = _be; \
    pState[ 2] = _bi; \
    pState[ 3] = _bo; \
    pState[ 4] = _bu; \
    pState[ 5] = _ga; \
    pState[ 6] = _ge; \
    pState[ 7] = _gi; \
    pState[ 8] = _go; \
    pState[ 9] = _gu; \
    pState[10] = _ka; \
    pState[11] = _ke; \
    pState[12] = _ki; \
    pState[13] = _ko; \
    pState[14] = _ku; \
    pState[15] = _ma; \
    pState[16] = _me; \
    pState[17] = _mi; \
    pState[18] = _mo; \
    pState[19] = _mu; \
    pState[20] = _sa; \
    pState[21] = _se; \
    pState[22] = _si; \
    pState[23] = _so; \
    pState[24] = _su

void KeccakP1600times2_PermuteAll_12rounds(void *states)
{
    V128 *statesAsLanes = states;
    KeccakP_DeclareVars;
    #if (KeccakP1600times2_unrolling < 12)
    unsigned int i;
    #endif

    copyFromState(statesAsLanes);
    rounds12;
    copyToState(statesAsLanes);
}

size_t KeccakP1600times2_12rounds_FastLoop_Absorb(void *states, unsigned int laneCount, unsigned int laneOffsetParallel, unsigned int laneOffsetSerial, const unsigned char *data, size_t dataByteLen)
{
    size_t dataMinimumSize = (laneOffsetParallel*1 + laneCount)*8;

    if (laneCount == 21) {
        #if (KeccakP1600times2_unrolling < 12)
        unsigned int i;
        #endif
        const unsigned char *dataStart = data;
        V128 *statesAsLanes = states;
        const UINT64 *dataAsLanes = (const UINT64 *)data;
        KeccakP_DeclareVars;
	    V128 index = LOAD4_32(0, 0, 1*laneOffsetParallel, 0*laneOffsetParallel);

        copyFromState(statesAsLanes);
        while(dataByteLen >= dataMinimumSize) {
            #define Add_In( argLane, argIndex )  argLane = XOR(argLane, LOAD_GATHER2_64(index, dataAsLanes+argIndex))
            Add_In( _ba, 0 );
            Add_In( _be, 1 );
            Add_In( _bi, 2 );
            Add_In( _bo, 3 );
            Add_In( _bu, 4 );
            Add_In( _ga, 5 );
            Add_In( _ge, 6 );
            Add_In( _gi, 7 );
            Add_In( _go, 8 );
            Add_In( _gu, 9 );
            Add_In( _ka, 10 );
            Add_In( _ke, 11 );
            Add_In( _ki, 12 );
            Add_In( _ko, 13 );
            Add_In( _ku, 14 );
            Add_In( _ma, 15 );
            Add_In( _me, 16 );
            Add_In( _mi, 17 );
            Add_In( _mo, 18 );
            Add_In( _mu, 19 );
            Add_In( _sa, 20 );
            #undef  Add_In
            rounds12;
            dataAsLanes += laneOffsetSerial;
            dataByteLen -= laneOffsetSerial*8;
        }
        copyToState(statesAsLanes);
        return (const unsigned char *)dataAsLanes - dataStart;
    }
    else {
        const unsigned char *dataStart = data;

        while(dataByteLen >= dataMinimumSize) {
            KeccakP1600times2_AddLanesAll(states, data, laneCount, laneOffsetParallel);
            KeccakP1600times2_PermuteAll_12rounds(states);
            data += laneOffsetSerial*8;
            dataByteLen -= laneOffsetSerial*8;
        }
        return data - dataStart;
    }
}
#endif

#if defined(KeccakP1600times2_useSSE2)
#define declareABCDE \
    V128 Aba, Abe, Abi, Abo, Abu; \
    V128 Aga, Age, Agi, Ago, Agu; \
    V128 Aka, Ake, Aki, Ako, Aku; \
    V128 Ama, Ame, Ami, Amo, Amu; \
    V128 Asa, Ase, Asi, Aso, Asu; \
    V128 Bba, Bbe, Bbi, Bbo, Bbu; \
    V128 Bga, Bge, Bgi, Bgo, Bgu; \
    V128 Bka, Bke, Bki, Bko, Bku; \
    V128 Bma, Bme, Bmi, Bmo, Bmu; \
    V128 Bsa, Bse, Bsi, Bso, Bsu; \
    V128 Ca, Ce, Ci, Co, Cu; \
    V128 Da, De, Di, Do, Du; \
    V128 Eba, Ebe, Ebi, Ebo, Ebu; \
    V128 Ega, Ege, Egi, Ego, Egu; \
    V128 Eka, Eke, Eki, Eko, Eku; \
    V128 Ema, Eme, Emi, Emo, Emu; \
    V128 Esa, Ese, Esi, Eso, Esu; \

#define prepareTheta \
    Ca = XOR128(Aba, XOR128(Aga, XOR128(Aka, XOR128(Ama, Asa)))); \
    Ce = XOR128(Abe, XOR128(Age, XOR128(Ake, XOR128(Ame, Ase)))); \
    Ci = XOR128(Abi, XOR128(Agi, XOR128(Aki, XOR128(Ami, Asi)))); \
    Co = XOR128(Abo, XOR128(Ago, XOR128(Ako, XOR128(Amo, Aso)))); \
    Cu = XOR128(Abu, XOR128(Agu, XOR128(Aku, XOR128(Amu, Asu)))); \

/* --- Theta Rho Pi Chi Iota Prepare-theta */
/* --- 64-bit lanes mapped to 64-bit words */
#define thetaRhoPiChiIotaPrepareTheta(i, A, E) \
    Da = XOR128(Cu, ROL64in128(Ce, 1)); \
    De = XOR128(Ca, ROL64in128(Ci, 1)); \
    Di = XOR128(Ce, ROL64in128(Co, 1)); \
    Do = XOR128(Ci, ROL64in128(Cu, 1)); \
    Du = XOR128(Co, ROL64in128(Ca, 1)); \
\
    XOReq128(A##ba, Da); \
    Bba = A##ba; \
    XOReq128(A##ge, De); \
    Bbe = ROL64in128(A##ge, 44); \
    XOReq128(A##ki, Di); \
    Bbi = ROL64in128(A##ki, 43); \
    E##ba = XOR128(Bba, ANDnu128(Bbe, Bbi)); \
    XOReq128(E##ba, CONST128_64(KeccakP1600RoundConstants[i])); \
    Ca = E##ba; \
    XOReq128(A##mo, Do); \
    Bbo = ROL64in128(A##mo, 21); \
    E##be = XOR128(Bbe, ANDnu128(Bbi, Bbo)); \
    Ce = E##be; \
    XOReq128(A##su, Du); \
    Bbu = ROL64in128(A##su, 14); \
    E##bi = XOR128(Bbi, ANDnu128(Bbo, Bbu)); \
    Ci = E##bi; \
    E##bo = XOR128(Bbo, ANDnu128(Bbu, Bba)); \
    Co = E##bo; \
    E##bu = XOR128(Bbu, ANDnu128(Bba, Bbe)); \
    Cu = E##bu; \
\
    XOReq128(A##bo, Do); \
    Bga = ROL64in128(A##bo, 28); \
    XOReq128(A##gu, Du); \
    Bge = ROL64in128(A##gu, 20); \
    XOReq128(A##ka, Da); \
    Bgi = ROL64in128(A##ka, 3); \
    E##ga = XOR128(Bga, ANDnu128(Bge, Bgi)); \
    XOReq128(Ca, E##ga); \
    XOReq128(A##me, De); \
    Bgo = ROL64in128(A##me, 45); \
    E##ge = XOR128(Bge, ANDnu128(Bgi, Bgo)); \
    XOReq128(Ce, E##ge); \
    XOReq128(A##si, Di); \
    Bgu = ROL64in128(A##si, 61); \
    E##gi = XOR128(Bgi, ANDnu128(Bgo, Bgu)); \
    XOReq128(Ci, E##gi); \
    E##go = XOR128(Bgo, ANDnu128(Bgu, Bga)); \
    XOReq128(Co, E##go); \
    E##gu = XOR128(Bgu, ANDnu128(Bga, Bge)); \
    XOReq128(Cu, E##gu); \
\
    XOReq128(A##be, De); \
    Bka = ROL64in128(A##be, 1); \
    XOReq128(A##gi, Di); \
    Bke = ROL64in128(A##gi, 6); \
    XOReq128(A##ko, Do); \
    Bki = ROL64in128(A##ko, 25); \
    E##ka = XOR128(Bka, ANDnu128(Bke, Bki)); \
    XOReq128(Ca, E##ka); \
    XOReq128(A##mu, Du); \
    Bko = ROL64in128_8(A##mu); \
    E##ke = XOR128(Bke, ANDnu128(Bki, Bko)); \
    XOReq128(Ce, E##ke); \
    XOReq128(A##sa, Da); \
    Bku = ROL64in128(A##sa, 18); \
    E##ki = XOR128(Bki, ANDnu128(Bko, Bku)); \
    XOReq128(Ci, E##ki); \
    E##ko = XOR128(Bko, ANDnu128(Bku, Bka)); \
    XOReq128(Co, E##ko); \
    E##ku = XOR128(Bku, ANDnu128(Bka, Bke)); \
    XOReq128(Cu, E##ku); \
\
    XOReq128(A##bu, Du); \
    Bma = ROL64in128(A##bu, 27); \
    XOReq128(A##ga, Da); \
    Bme = ROL64in128(A##ga, 36); \
    XOReq128(A##ke, De); \
    Bmi = ROL64in128(A##ke, 10); \
    E##ma = XOR128(Bma, ANDnu128(Bme, Bmi)); \
    XOReq128(Ca, E##ma); \
    XOReq128(A##mi, Di); \
    Bmo = ROL64in128(A##mi, 15); \
    E##me = XOR128(Bme, ANDnu128(Bmi, Bmo)); \
    XOReq128(Ce, E##me); \
    XOReq128(A##so, Do); \
    Bmu = ROL64in128_56(A##so); \
    E##mi = XOR128(Bmi, ANDnu128(Bmo, Bmu)); \
    XOReq128(Ci, E##mi); \
    E##mo = XOR128(Bmo, ANDnu128(Bmu, Bma)); \
    XOReq128(Co, E##mo); \
    E##mu = XOR128(Bmu, ANDnu128(Bma, Bme)); \
    XOReq128(Cu, E##mu); \
\
    XOReq128(A##bi, Di); \
    Bsa = ROL64in128(A##bi, 62); \
    XOReq128(A##go, Do); \
    Bse = ROL64in128(A##go, 55); \
    XOReq128(A##ku, Du); \
    Bsi = ROL64in128(A##ku, 39); \
    E##sa = XOR128(Bsa, ANDnu128(Bse, Bsi)); \
    XOReq128(Ca, E##sa); \
    XOReq128(A##ma, Da); \
    Bso = ROL64in128(A##ma, 41); \
    E##se = XOR128(Bse, ANDnu128(Bsi, Bso)); \
    XOReq128(Ce, E##se); \
    XOReq128(A##se, De); \
    Bsu = ROL64in128(A##se, 2); \
    E##si = XOR128(Bsi, ANDnu128(Bso, Bsu)); \
    XOReq128(Ci, E##si); \
    E##so = XOR128(Bso, ANDnu128(Bsu, Bsa)); \
    XOReq128(Co, E##so); \
    E##su = XOR128(Bsu, ANDnu128(Bsa, Bse)); \
    XOReq128(Cu, E##su); \
\

/* --- Theta Rho Pi Chi Iota */
/* --- 64-bit lanes mapped to 64-bit words */
#define thetaRhoPiChiIota(i, A, E) \
    Da = XOR128(Cu, ROL64in128(Ce, 1)); \
    De = XOR128(Ca, ROL64in128(Ci, 1)); \
    Di = XOR128(Ce, ROL64in128(Co, 1)); \
    Do = XOR128(Ci, ROL64in128(Cu, 1)); \
    Du = XOR128(Co, ROL64in128(Ca, 1)); \
\
    XOReq128(A##ba, Da); \
    Bba = A##ba; \
    XOReq128(A##ge, De); \
    Bbe = ROL64in128(A##ge, 44); \
    XOReq128(A##ki, Di); \
    Bbi = ROL64in128(A##ki, 43); \
    E##ba = XOR128(Bba, ANDnu128(Bbe, Bbi)); \
    XOReq128(E##ba, CONST128_64(KeccakP1600RoundConstants[i])); \
    XOReq128(A##mo, Do); \
    Bbo = ROL64in128(A##mo, 21); \
    E##be = XOR128(Bbe, ANDnu128(Bbi, Bbo)); \
    XOReq128(A##su, Du); \
    Bbu = ROL64in128(A##su, 14); \
    E##bi = XOR128(Bbi, ANDnu128(Bbo, Bbu)); \
    E##bo = XOR128(Bbo, ANDnu128(Bbu, Bba)); \
    E##bu = XOR128(Bbu, ANDnu128(Bba, Bbe)); \
\
    XOReq128(A##bo, Do); \
    Bga = ROL64in128(A##bo, 28); \
    XOReq128(A##gu, Du); \
    Bge = ROL64in128(A##gu, 20); \
    XOReq128(A##ka, Da); \
    Bgi = ROL64in128(A##ka, 3); \
    E##ga = XOR128(Bga, ANDnu128(Bge, Bgi)); \
    XOReq128(A##me, De); \
    Bgo = ROL64in128(A##me, 45); \
    E##ge = XOR128(Bge, ANDnu128(Bgi, Bgo)); \
    XOReq128(A##si, Di); \
    Bgu = ROL64in128(A##si, 61); \
    E##gi = XOR128(Bgi, ANDnu128(Bgo, Bgu)); \
    E##go = XOR128(Bgo, ANDnu128(Bgu, Bga)); \
    E##gu = XOR128(Bgu, ANDnu128(Bga, Bge)); \
\
    XOReq128(A##be, De); \
    Bka = ROL64in128(A##be, 1); \
    XOReq128(A##gi, Di); \
    Bke = ROL64in128(A##gi, 6); \
    XOReq128(A##ko, Do); \
    Bki = ROL64in128(A##ko, 25); \
    E##ka = XOR128(Bka, ANDnu128(Bke, Bki)); \
    XOReq128(A##mu, Du); \
    Bko = ROL64in128_8(A##mu); \
    E##ke = XOR128(Bke, ANDnu128(Bki, Bko)); \
    XOReq128(A##sa, Da); \
    Bku = ROL64in128(A##sa, 18); \
    E##ki = XOR128(Bki, ANDnu128(Bko, Bku)); \
    E##ko = XOR128(Bko, ANDnu128(Bku, Bka)); \
    E##ku = XOR128(Bku, ANDnu128(Bka, Bke)); \
\
    XOReq128(A##bu, Du); \
    Bma = ROL64in128(A##bu, 27); \
    XOReq128(A##ga, Da); \
    Bme = ROL64in128(A##ga, 36); \
    XOReq128(A##ke, De); \
    Bmi = ROL64in128(A##ke, 10); \
    E##ma = XOR128(Bma, ANDnu128(Bme, Bmi)); \
    XOReq128(A##mi, Di); \
    Bmo = ROL64in128(A##mi, 15); \
    E##me = XOR128(Bme, ANDnu128(Bmi, Bmo)); \
    XOReq128(A##so, Do); \
    Bmu = ROL64in128_56(A##so); \
    E##mi = XOR128(Bmi, ANDnu128(Bmo, Bmu)); \
    E##mo = XOR128(Bmo, ANDnu128(Bmu, Bma)); \
    E##mu = XOR128(Bmu, ANDnu128(Bma, Bme)); \
\
    XOReq128(A##bi, Di); \
    Bsa = ROL64in128(A##bi, 62); \
    XOReq128(A##go, Do); \
    Bse = ROL64in128(A##go, 55); \
    XOReq128(A##ku, Du); \
    Bsi = ROL64in128(A##ku, 39); \
    E##sa = XOR128(Bsa, ANDnu128(Bse, Bsi)); \
    XOReq128(A##ma, Da); \
    Bso = ROL64in128(A##ma, 41); \
    E##se = XOR128(Bse, ANDnu128(Bsi, Bso)); \
    XOReq128(A##se, De); \
    Bsu = ROL64in128(A##se, 2); \
    E##si = XOR128(Bsi, ANDnu128(Bso, Bsu)); \
    E##so = XOR128(Bso, ANDnu128(Bsu, Bsa)); \
    E##su = XOR128(Bsu, ANDnu128(Bsa, Bse)); \
\

#define copyFromState(X, state) \
    X##ba = LOAD128(state[ 0]); \
    X##be = LOAD128(state[ 1]); \
    X##bi = LOAD128(state[ 2]); \
    X##bo = LOAD128(state[ 3]); \
    X##bu = LOAD128(state[ 4]); \
    X##ga = LOAD128(state[ 5]); \
    X##ge = LOAD128(state[ 6]); \
    X##gi = LOAD128(state[ 7]); \
    X##go = LOAD128(state[ 8]); \
    X##gu = LOAD128(state[ 9]); \
    X##ka = LOAD128(state[10]); \
    X##ke = LOAD128(state[11]); \
    X##ki = LOAD128(state[12]); \
    X##ko = LOAD128(state[13]); \
    X##ku = LOAD128(state[14]); \
    X##ma = LOAD128(state[15]); \
    X##me = LOAD128(state[16]); \
    X##mi = LOAD128(state[17]); \
    X##mo = LOAD128(state[18]); \
    X##mu = LOAD128(state[19]); \
    X##sa = LOAD128(state[20]); \
    X##se = LOAD128(state[21]); \
    X##si = LOAD128(state[22]); \
    X##so = LOAD128(state[23]); \
    X##su = LOAD128(state[24]); \

#define copyToState(state, X) \
    STORE128(state[ 0], X##ba); \
    STORE128(state[ 1], X##be); \
    STORE128(state[ 2], X##bi); \
    STORE128(state[ 3], X##bo); \
    STORE128(state[ 4], X##bu); \
    STORE128(state[ 5], X##ga); \
    STORE128(state[ 6], X##ge); \
    STORE128(state[ 7], X##gi); \
    STORE128(state[ 8], X##go); \
    STORE128(state[ 9], X##gu); \
    STORE128(state[10], X##ka); \
    STORE128(state[11], X##ke); \
    STORE128(state[12], X##ki); \
    STORE128(state[13], X##ko); \
    STORE128(state[14], X##ku); \
    STORE128(state[15], X##ma); \
    STORE128(state[16], X##me); \
    STORE128(state[17], X##mi); \
    STORE128(state[18], X##mo); \
    STORE128(state[19], X##mu); \
    STORE128(state[20], X##sa); \
    STORE128(state[21], X##se); \
    STORE128(state[22], X##si); \
    STORE128(state[23], X##so); \
    STORE128(state[24], X##su); \

#define copyStateVariables(X, Y) \
    X##ba = Y##ba; \
    X##be = Y##be; \
    X##bi = Y##bi; \
    X##bo = Y##bo; \
    X##bu = Y##bu; \
    X##ga = Y##ga; \
    X##ge = Y##ge; \
    X##gi = Y##gi; \
    X##go = Y##go; \
    X##gu = Y##gu; \
    X##ka = Y##ka; \
    X##ke = Y##ke; \
    X##ki = Y##ki; \
    X##ko = Y##ko; \
    X##ku = Y##ku; \
    X##ma = Y##ma; \
    X##me = Y##me; \
    X##mi = Y##mi; \
    X##mo = Y##mo; \
    X##mu = Y##mu; \
    X##sa = Y##sa; \
    X##se = Y##se; \
    X##si = Y##si; \
    X##so = Y##so; \
    X##su = Y##su; \

#ifdef KeccakP1600times2_fullUnrolling
#define FullUnrolling
#else
#define Unrolling KeccakP1600times2_unrolling
#endif

#if ((defined(FullUnrolling)) || (Unrolling == 12))
#define rounds12 \
    prepareTheta \
    thetaRhoPiChiIotaPrepareTheta(12, A, E) \
    thetaRhoPiChiIotaPrepareTheta(13, E, A) \
    thetaRhoPiChiIotaPrepareTheta(14, A, E) \
    thetaRhoPiChiIotaPrepareTheta(15, E, A) \
    thetaRhoPiChiIotaPrepareTheta(16, A, E) \
    thetaRhoPiChiIotaPrepareTheta(17, E, A) \
    thetaRhoPiChiIotaPrepareTheta(18, A, E) \
    thetaRhoPiChiIotaPrepareTheta(19, E, A) \
    thetaRhoPiChiIotaPrepareTheta(20, A, E) \
    thetaRhoPiChiIotaPrepareTheta(21, E, A) \
    thetaRhoPiChiIotaPrepareTheta(22, A, E) \
    thetaRhoPiChiIota(23, E, A) \

#elif (Unrolling == 6)
#define rounds12 \
    prepareTheta \
    for(i=12; i<24; i+=6) { \
        thetaRhoPiChiIotaPrepareTheta(i  , A, E) \
        thetaRhoPiChiIotaPrepareTheta(i+1, E, A) \
        thetaRhoPiChiIotaPrepareTheta(i+2, A, E) \
        thetaRhoPiChiIotaPrepareTheta(i+3, E, A) \
        thetaRhoPiChiIotaPrepareTheta(i+4, A, E) \
        thetaRhoPiChiIotaPrepareTheta(i+5, E, A) \
    } \

#elif (Unrolling == 4)
#define rounds12 \
    prepareTheta \
    for(i=12; i<24; i+=4) { \
        thetaRhoPiChiIotaPrepareTheta(i  , A, E) \
        thetaRhoPiChiIotaPrepareTheta(i+1, E, A) \
        thetaRhoPiChiIotaPrepareTheta(i+2, A, E) \
        thetaRhoPiChiIotaPrepareTheta(i+3, E, A) \
    } \

#elif (Unrolling == 3)
#define rounds12 \
    prepareTheta \
    for(i=12; i<24; i+=3) { \
        thetaRhoPiChiIotaPrepareTheta(i  , A, E) \
        thetaRhoPiChiIotaPrepareTheta(i+1, E, A) \
        thetaRhoPiChiIotaPrepareTheta(i+2, A, E) \
        copyStateVariables(A, E) \
    } \

#elif (Unrolling == 2)
#define rounds12 \
    prepareTheta \
    for(i=12; i<24; i+=2) { \
        thetaRhoPiChiIotaPrepareTheta(i  , A, E) \
        thetaRhoPiChiIotaPrepareTheta(i+1, E, A) \
    } \

#elif (Unrolling == 1)
#define rounds12 \
    prepareTheta \
    for(i=12; i<24; i++) { \
        thetaRhoPiChiIotaPrepareTheta(i  , A, E) \
        copyStateVariables(A, E) \
    } \

#else
#error "Unrolling is not correctly specified!"
#endif

void KeccakP1600times2_PermuteAll_12rounds(void *states)
{
    V128 *statesAsLanes = (V128 *)states;
    declareABCDE
    #ifndef KeccakP1600times2_fullUnrolling
    unsigned int i;
    #endif

    copyFromState(A, statesAsLanes)
    rounds12
    copyToState(statesAsLanes, A)
#if defined(UseMMX)
    _mm_empty();
#endif
}
#endif

#endif
