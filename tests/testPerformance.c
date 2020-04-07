/*
K12 based on the eXtended Keccak Code Package (XKCP)
https://github.com/XKCP/XKCP

KangarooTwelve, designed by Guido Bertoni, Joan Daemen, Michaël Peeters, Gilles Van Assche, Ronny Van Keer and Benoît Viguier.

Implementation by Gilles Van Assche and Ronny Van Keer, hereby denoted as "the implementer".

For more information, feedback or questions, please refer to the Keccak Team website:
https://keccak.team/

To the extent possible under law, the implementer has waived all copyright
and related or neighboring rights to the source code in this file.
http://creativecommons.org/publicdomain/zero/1.0/
*/

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "align.h"
#include "KangarooTwelve.h"
#include "KeccakP-1600-SnP.h"
#include "timing.h"
#include "testPerformance.h"

#define xstr(s) str(s)
#define str(s) #s

uint_32t measureKangarooTwelve(uint_32t dtMin, unsigned int inputLen)
{
    ALIGN(64) unsigned char input[2*1024*1024];
    ALIGN(64) unsigned char output[32];
    measureTimingDeclare

    assert(inputLen <= 2*1024*1024);

    memset(input, 0xA5, 16);

    measureTimingBeginDeclared
    KangarooTwelve(input, inputLen, output, 32, (const unsigned char *)"", 0);
    measureTimingEnd
}

void KangarooTwelve_SetProcessorCapabilities();

void printKangarooTwelvePerformanceHeader( void )
{
    KangarooTwelve_SetProcessorCapabilities();
    printf("*** KangarooTwelve ***\n");
    printf("Using Keccak-p[1600,12] implementations:\n");
    printf("- \303\2271: %s\n", KeccakP1600_GetImplementation());
    #if defined(KeccakP1600_12rounds_FastLoop_supported)
    printf("      + KeccakP1600_12rounds_FastLoop_Absorb()\n");
    #endif

#ifndef KeccakP1600_disableParallelism
    if (KeccakP1600times2_IsAvailable()) {
        printf("- \303\2272: %s\n", KeccakP1600times2_GetImplementation());
    #if defined(KeccakP1600times2_12rounds_FastLoop_supported)
        printf("      + KeccakP1600times2_12rounds_FastLoop_Absorb()\n");
    #endif
    }
    else
        printf("- \303\2272: not used\n");

    if (KeccakP1600times4_IsAvailable()) {
        printf("- \303\2274: %s\n", KeccakP1600times4_GetImplementation());
    #if defined(KeccakP1600times4_12rounds_FastLoop_supported)
        printf("      + KeccakP1600times4_12rounds_FastLoop_Absorb()\n");
    #endif
    }
    else
        printf("- \303\2274: not used\n");

    if (KeccakP1600times8_IsAvailable()) {
        printf("- \303\2278: %s\n", KeccakP1600times8_GetImplementation());
    #if defined(KeccakP1600times8_12rounds_FastLoop_supported)
        printf("      + KeccakP1600times8_12rounds_FastLoop_Absorb()\n");
    #endif
    }
    else
        printf("- \303\2278: not used\n");
#endif

    printf("\n");
}

void testKangarooTwelvePerformanceOne( void )
{
    const unsigned int chunkSize = 8192;
    unsigned halfTones;
    uint_32t calibration = calibrate();
    unsigned int chunkSizeLog = (unsigned int)floor(log(chunkSize)/log(2.0)+0.5);
    int displaySlope = 0;

    measureKangarooTwelve(calibration, 500000);
    for(halfTones=chunkSizeLog*12-28; halfTones<=13*12; halfTones+=4) {
        double I = pow(2.0, halfTones/12.0);
        unsigned int i  = (unsigned int)floor(I+0.5);
        uint_32t time, timePlus1Block, timePlus2Blocks, timePlus4Blocks, timePlus8Blocks;
        uint_32t timePlus168Blocks;
        time = measureKangarooTwelve(calibration, i);
        if (i == chunkSize) {
            displaySlope = 1;
            timePlus1Block  = measureKangarooTwelve(calibration, i+1*chunkSize);
            timePlus2Blocks = measureKangarooTwelve(calibration, i+2*chunkSize);
            timePlus4Blocks = measureKangarooTwelve(calibration, i+4*chunkSize);
            timePlus8Blocks = measureKangarooTwelve(calibration, i+8*chunkSize);
            timePlus168Blocks = measureKangarooTwelve(calibration, i+168*chunkSize);
        }
        printf("%8d bytes: %9d cycles, %6.3f cycles/byte\n", i, time, time*1.0/i);
        if (displaySlope) {
            printf("     +1 block:  %9d cycles, %6.3f cycles/byte (slope)\n", timePlus1Block, (timePlus1Block-(double)(time))*1.0/chunkSize/1.0);
            printf("     +2 blocks: %9d cycles, %6.3f cycles/byte (slope)\n", timePlus2Blocks, (timePlus2Blocks-(double)(time))*1.0/chunkSize/2.0);
            printf("     +4 blocks: %9d cycles, %6.3f cycles/byte (slope)\n", timePlus4Blocks, (timePlus4Blocks-(double)(time))*1.0/chunkSize/4.0);
            printf("     +8 blocks: %9d cycles, %6.3f cycles/byte (slope)\n", timePlus8Blocks, (timePlus8Blocks-(double)(time))*1.0/chunkSize/8.0);
            printf("   +168 blocks: %9d cycles, %6.3f cycles/byte (slope)\n", timePlus168Blocks, (timePlus168Blocks-(double)(time))*1.0/chunkSize/168.0);
            displaySlope = 0;
        }
    }
    for(halfTones=12*12; halfTones<=20*12; halfTones+=4) {
        double I = chunkSize + pow(2.0, halfTones/12.0);
        unsigned int i  = (unsigned int)floor(I+0.5);
        uint_32t time;
        time = measureKangarooTwelve(calibration, i);
        printf("%8d bytes: %9d cycles, %6.3f cycles/byte\n", i, time, time*1.0/i);
    }
    printf("\n\n");
}

void testKangarooTwelvePerformance()
{
    printKangarooTwelvePerformanceHeader();
    testKangarooTwelvePerformanceOne();
}
void testPerformance()
{
#ifndef KeccakP1600_disableParallelism
    // Read feature availability
    KangarooTwelve_EnableAllCpuFeatures();
    int cpu_has_AVX512 = KangarooTwelve_DisableAVX512();
    int cpu_has_AVX2 = KangarooTwelve_DisableAVX2();
    int cpu_has_SSSE3 = KangarooTwelve_DisableSSSE3();
#endif

    // Test without vectorization
    testKangarooTwelvePerformance();

#ifndef KeccakP1600_disableParallelism
    // Test with SSSE3 only if it's available
    if (cpu_has_SSSE3) {
        printf("\n");
        KangarooTwelve_EnableAllCpuFeatures();
        KangarooTwelve_DisableAVX512();
        KangarooTwelve_DisableAVX2();
        testKangarooTwelvePerformance();
    }
    // Test with SSSE3 and AVX2 if they're available
    if (cpu_has_AVX2) {
        printf("\n");
        KangarooTwelve_EnableAllCpuFeatures();
        KangarooTwelve_DisableAVX512();
        testKangarooTwelvePerformance();
    }
    // Finally, test with everything enabled if we have AVX512
    if (cpu_has_AVX512) {
        printf("\n");
        KangarooTwelve_EnableAllCpuFeatures();
        testKangarooTwelvePerformance();
    }
#endif
}
