# K12 based on the eXtended Keccak Code Package (XKCP)
# https://github.com/XKCP/XKCP
#
# The Keccak-p permutations, designed by Guido Bertoni, Joan Daemen, Michaël Peeters and Gilles Van Assche.
#
# Implementation by Gilles Van Assche, hereby denoted as "the implementer".
# Core subroutine is based on one by Andy Polyakov, available
# at https://github.com/dot-asm/cryptogams. Used with permission.
#
# For more information, feedback or questions, please refer to the Keccak Team website:
# https://keccak.team/
#
# To the extent possible under law, the implementer has waived all copyright
# and related or neighboring rights to the source code in this file.
# http://creativecommons.org/publicdomain/zero/1.0/

.text

.balign 64  // strategic alignment and padding that allows to use
        // address value as loop termination condition...
    .quad   0,0,0,0,0,0,0,0
.ifdef macOS
.else
.type   iotas,%object
.endif
iotas:
    .quad   0x0000000000000001
    .quad   0x0000000000008082
    .quad   0x800000000000808a
    .quad   0x8000000080008000
    .quad   0x000000000000808b
    .quad   0x0000000080000001
    .quad   0x8000000080008081
    .quad   0x8000000000008009
    .quad   0x000000000000008a
    .quad   0x0000000000000088
    .quad   0x0000000080008009
    .quad   0x000000008000000a
iotas12:
    .quad   0x000000008000808b
    .quad   0x800000000000008b
    .quad   0x8000000000008089
    .quad   0x8000000000008003
    .quad   0x8000000000008002
    .quad   0x8000000000000080
    .quad   0x000000000000800a
    .quad   0x800000008000000a
    .quad   0x8000000080008081
    .quad   0x8000000000008080
    .quad   0x0000000080000001
    .quad   0x8000000080008008
.ifdef macOS
.else
.size   iotas,.-iotas
.endif

.ifdef macOS
.else
.type   KeccakP1600_ARMv8Asha3_Permute_12rounds_internal,%function
.endif
KeccakP1600_ARMv8Asha3_Permute_12rounds_internal:
.balign 32
    mov x9,#12
    adr x10,iotas12
    b   .Loop_ce
.balign 16
.Loop_ce:
    ////////////////////////////////////////////////// Theta
    eor3    v25.16b,v20.16b,v15.16b,v10.16b
    eor3    v26.16b,v21.16b,v16.16b,v11.16b
    eor3    v27.16b,v22.16b,v17.16b,v12.16b
    eor3    v28.16b,v23.16b,v18.16b,v13.16b
    eor3    v29.16b,v24.16b,v19.16b,v14.16b
    eor3    v25.16b,v25.16b,   v5.16b,v0.16b
    eor3    v26.16b,v26.16b,   v6.16b,v1.16b
    eor3    v27.16b,v27.16b,   v7.16b,v2.16b
    eor3    v28.16b,v28.16b,   v8.16b,v3.16b
    eor3    v29.16b,v29.16b,   v9.16b,v4.16b

    rax1    v30.2d,v25.2d,v27.2d            // D[1]
    rax1    v31.2d,v26.2d,v28.2d            // D[2]
    rax1    v27.2d,v27.2d,v29.2d            // D[3]
    rax1    v28.2d,v28.2d,v25.2d            // D[4]
    rax1    v29.2d,v29.2d,v26.2d            // D[0]

    ////////////////////////////////////////////////// Theta+Rho+Pi
    xar v25.2d,   v1.2d,v30.2d,#64-1 // C[0]=A[2][0]

    xar v1.2d,v6.2d,v30.2d,#64-44
    xar v6.2d,v9.2d,v28.2d,#64-20
    xar v9.2d,v22.2d,v31.2d,#64-61
    xar v22.2d,v14.2d,v28.2d,#64-39
    xar v14.2d,v20.2d,v29.2d,#64-18

    xar v26.2d,   v2.2d,v31.2d,#64-62 // C[1]=A[4][0]

    xar v2.2d,v12.2d,v31.2d,#64-43
    xar v12.2d,v13.2d,v27.2d,#64-25
    xar v13.2d,v19.2d,v28.2d,#64-8
    xar v19.2d,v23.2d,v27.2d,#64-56
    xar v23.2d,v15.2d,v29.2d,#64-41

    xar v15.2d,v4.2d,v28.2d,#64-27

    xar v28.2d,   v24.2d,v28.2d,#64-14 // D[4]=A[0][4]
    xar v24.2d,v21.2d,v30.2d,#64-2
    xar v8.2d,v8.2d,v27.2d,#64-55 // A[1][3]=A[4][1]
    xar v4.2d,v16.2d,v30.2d,#64-45 // A[0][4]=A[1][3]
    xar v16.2d,v5.2d,v29.2d,#64-36

    xar v5.2d,v3.2d,v27.2d,#64-28

    eor v0.16b,v0.16b,v29.16b

    xar v27.2d,   v18.2d,v27.2d,#64-21 // D[3]=A[0][3]
    xar v3.2d,v17.2d,v31.2d,#64-15 // A[0][3]=A[3][3]
    xar v30.2d,   v11.2d,v30.2d,#64-10 // D[1]=A[3][2]
    xar v31.2d,   v7.2d,v31.2d,#64-6 // D[2]=A[2][1]
    xar v29.2d,   v10.2d,v29.2d,#64-3 // D[0]=A[1][2]

    ////////////////////////////////////////////////// Chi+Iota
    bcax    v20.16b,v26.16b,   v22.16b,v8.16b   // A[1][3]=A[4][1]
    bcax    v21.16b,v8.16b,v23.16b,v22.16b  // A[1][3]=A[4][1]
    bcax    v22.16b,v22.16b,v24.16b,v23.16b
    bcax    v23.16b,v23.16b,v26.16b,   v24.16b
    bcax    v24.16b,v24.16b,v8.16b,v26.16b  // A[1][3]=A[4][1]

    ld1r    {v26.2d},[x10],#8

    bcax    v17.16b,v30.16b,   v19.16b,v3.16b   // A[0][3]=A[3][3]
    bcax    v18.16b,v3.16b,v15.16b,v19.16b  // A[0][3]=A[3][3]
    bcax    v19.16b,v19.16b,v16.16b,v15.16b
    bcax    v15.16b,v15.16b,v30.16b,   v16.16b
    bcax    v16.16b,v16.16b,v3.16b,v30.16b  // A[0][3]=A[3][3]

    bcax    v10.16b,v25.16b,   v12.16b,v31.16b
    bcax    v11.16b,v31.16b,   v13.16b,v12.16b
    bcax    v12.16b,v12.16b,v14.16b,v13.16b
    bcax    v13.16b,v13.16b,v25.16b,   v14.16b
    bcax    v14.16b,v14.16b,v31.16b,   v25.16b

    bcax    v7.16b,v29.16b,   v9.16b,v4.16b // A[0][4]=A[1][3]
    bcax    v8.16b,v4.16b,v5.16b,v9.16b // A[0][4]=A[1][3]
    bcax    v9.16b,v9.16b,v6.16b,v5.16b
    bcax    v5.16b,v5.16b,v29.16b,   v6.16b
    bcax    v6.16b,v6.16b,v4.16b,v29.16b    // A[0][4]=A[1][3]

    bcax    v3.16b,v27.16b,   v0.16b,v28.16b
    bcax    v4.16b,v28.16b,   v1.16b,v0.16b
    bcax    v0.16b,v0.16b,v2.16b,v1.16b
    bcax    v1.16b,v1.16b,v27.16b,   v2.16b
    bcax    v2.16b,v2.16b,v28.16b,   v27.16b

    eor v0.16b,v0.16b,v26.16b

    subs    x9,x9,#1
    bne .Loop_ce

    ret
.ifdef macOS
.else
.size   KeccakP1600_ARMv8Asha3_Permute_12rounds_internal,.-KeccakP1600_ARMv8Asha3_Permute_12rounds_internal
.endif

.ifdef macOS
.globl  _KeccakP1600_ARMv8Asha3_Permute_12rounds
_KeccakP1600_ARMv8Asha3_Permute_12rounds:
.else
.globl  KeccakP1600_ARMv8Asha3_Permute_12rounds
.type   KeccakP1600_ARMv8Asha3_Permute_12rounds,%function
KeccakP1600_ARMv8Asha3_Permute_12rounds:
.endif
.balign 32
    stp x29,x30,[sp,#-80]!
    add x29,sp,#0
    stp d8,d9,[sp,#16]      // per ABI requirement
    stp d10,d11,[sp,#32]
    stp d12,d13,[sp,#48]
    stp d14,d15,[sp,#64]
    ldp d0,d1,[x0,#8*0]
    ldp d2,d3,[x0,#8*2]
    ldp d4,d5,[x0,#8*4]
    ldp d6,d7,[x0,#8*6]
    ldp d8,d9,[x0,#8*8]
    ldp d10,d11,[x0,#8*10]
    ldp d12,d13,[x0,#8*12]
    ldp d14,d15,[x0,#8*14]
    ldp d16,d17,[x0,#8*16]
    ldp d18,d19,[x0,#8*18]
    ldp d20,d21,[x0,#8*20]
    ldp d22,d23,[x0,#8*22]
    ldr d24,[x0,#8*24]
    bl  KeccakP1600_ARMv8Asha3_Permute_12rounds_internal
    ldr x30,[sp,#8]
    stp d0,d1,[x0,#8*0]
    stp d2,d3,[x0,#8*2]
    stp d4,d5,[x0,#8*4]
    stp d6,d7,[x0,#8*6]
    stp d8,d9,[x0,#8*8]
    stp d10,d11,[x0,#8*10]
    stp d12,d13,[x0,#8*12]
    stp d14,d15,[x0,#8*14]
    stp d16,d17,[x0,#8*16]
    stp d18,d19,[x0,#8*18]
    stp d20,d21,[x0,#8*20]
    stp d22,d23,[x0,#8*22]
    str d24,[x0,#8*24]

    ldp d8,d9,[sp,#16]
    ldp d10,d11,[sp,#32]
    ldp d12,d13,[sp,#48]
    ldp d14,d15,[sp,#64]
    ldr x29,[sp],#80
    ret
.ifdef macOS
.else
.size   KeccakP1600_ARMv8Asha3_Permute_12rounds,.-KeccakP1600_ARMv8Asha3_Permute_12rounds
.endif

// size_t KeccakP1600_ARMv8Asha3_12rounds_FastLoop_Absorb(
//      void *state(x0),
//      unsigned int laneCount(x1) = 21,
//      const unsigned char *data(x2),
//      size_t dataByteLen(x3))
.ifdef macOS
.globl  _KeccakP1600_ARMv8Asha3_12rounds_FastLoop_Absorb
_KeccakP1600_ARMv8Asha3_12rounds_FastLoop_Absorb:
.else
.globl  KeccakP1600_ARMv8Asha3_12rounds_FastLoop_Absorb
.type   KeccakP1600_ARMv8Asha3_12rounds_FastLoop_Absorb,%function
KeccakP1600_ARMv8Asha3_12rounds_FastLoop_Absorb:
.endif
.balign 32
    stp x29,x30,[sp,#-80]!
    add x29,sp,#0
    stp d8,d9,[sp,#16]      // per ABI requirement
    stp d10,d11,[sp,#32]
    stp d12,d13,[sp,#48]
    stp d14,d15,[sp,#64]

    ldp d0,d1,[x0,#8*0]
    ldp d2,d3,[x0,#8*2]
    ldp d4,d5,[x0,#8*4]
    ldp d6,d7,[x0,#8*6]
    ldp d8,d9,[x0,#8*8]
    ldp d10,d11,[x0,#8*10]
    ldp d12,d13,[x0,#8*12]
    ldp d14,d15,[x0,#8*14]
    ldp d16,d17,[x0,#8*16]
    ldp d18,d19,[x0,#8*18]
    ldp d20,d21,[x0,#8*20]
    ldp d22,d23,[x0,#8*22]
    ldr d24,[x0,#8*24]

    // Prepare the return value
    mov x11, #0
    b .KeccakP1600_ARMv8Asha3_12rounds_FastLoop_Absorb_loop

.balign 16
.KeccakP1600_ARMv8Asha3_12rounds_FastLoop_Absorb_loop:
    subs x3, x3, #8*21
    b.cc .KeccakP1600_ARMv8Asha3_12rounds_FastLoop_Absorb_end

    // Lanes 0-3
    ld1 {v27.8b-v30.8b}, [x2], #32
    eor  v0.16b,  v0.16b, v27.16b
    eor  v1.16b,  v1.16b, v28.16b
    eor  v2.16b,  v2.16b, v29.16b
    eor  v3.16b,  v3.16b, v30.16b

    // Lanes 4-7
    ld1 {v27.8b-v30.8b}, [x2], #32
    eor  v4.16b,  v4.16b, v27.16b
    eor  v5.16b,  v5.16b, v28.16b
    eor  v6.16b,  v6.16b, v29.16b
    eor  v7.16b,  v7.16b, v30.16b

    // Lanes 8-11
    ld1 {v27.8b-v30.8b}, [x2], #32
    eor  v8.16b,  v8.16b, v27.16b
    eor  v9.16b,  v9.16b, v28.16b
    eor v10.16b, v10.16b, v29.16b
    eor v11.16b, v11.16b, v30.16b

    // Lanes 12-15
    ld1 {v27.8b-v30.8b}, [x2], #32
    eor v12.16b, v12.16b, v27.16b
    eor v13.16b, v13.16b, v28.16b
    eor v14.16b, v14.16b, v29.16b
    eor v15.16b, v15.16b, v30.16b

    // Lanes 16-20
    ld1 {v27.8b-v30.8b}, [x2], #32
    eor v16.16b, v16.16b, v27.16b
    eor v17.16b, v17.16b, v28.16b
    eor v18.16b, v18.16b, v29.16b
    eor v19.16b, v19.16b, v30.16b
    ld1 {v27.8b}, [x2], #8
    eor v20.16b, v20.16b, v27.16b

    bl KeccakP1600_ARMv8Asha3_Permute_12rounds_internal

    add x11, x11, #8*21

    b .KeccakP1600_ARMv8Asha3_12rounds_FastLoop_Absorb_loop
.KeccakP1600_ARMv8Asha3_12rounds_FastLoop_Absorb_end:

    stp d0,d1,[x0,#8*0]
    stp d2,d3,[x0,#8*2]
    stp d4,d5,[x0,#8*4]
    stp d6,d7,[x0,#8*6]
    stp d8,d9,[x0,#8*8]
    stp d10,d11,[x0,#8*10]
    stp d12,d13,[x0,#8*12]
    stp d14,d15,[x0,#8*14]
    stp d16,d17,[x0,#8*16]
    stp d18,d19,[x0,#8*18]
    stp d20,d21,[x0,#8*20]
    stp d22,d23,[x0,#8*22]
    str d24,[x0,#8*24]

    mov x0, x11

    ldr x30,[sp,#8]
    ldp d8,d9,[sp,#16]
    ldp d10,d11,[sp,#32]
    ldp d12,d13,[sp,#48]
    ldp d14,d15,[sp,#64]
    ldr x29,[sp],#80

    ret
.ifdef macOS
.else
.size   KeccakP1600_ARMv8Asha3_12rounds_FastLoop_Absorb,.-KeccakP1600_ARMv8Asha3_12rounds_FastLoop_Absorb
.endif

.ifdef macOS
.globl  _KeccakP1600times2_ARMv8Asha3_Permute_12rounds
_KeccakP1600times2_ARMv8Asha3_Permute_12rounds:
.else
.globl  KeccakP1600times2_ARMv8Asha3_Permute_12rounds
.type   KeccakP1600times2_ARMv8Asha3_Permute_12rounds,%function
KeccakP1600times2_ARMv8Asha3_Permute_12rounds:
.endif
.balign 32
    stp x29,x30,[sp,#-80]!
    add x29,sp,#0
    stp d8,d9,[sp,#16]      // per ABI requirement
    stp d10,d11,[sp,#32]
    stp d12,d13,[sp,#48]
    stp d14,d15,[sp,#64]

    ld1 { v0.2d,  v1.2d,  v2.2d,  v3.2d}, [x0], #64
    ld1 { v4.2d,  v5.2d,  v6.2d,  v7.2d}, [x0], #64
    ld1 { v8.2d,  v9.2d, v10.2d, v11.2d}, [x0], #64
    ld1 {v12.2d, v13.2d, v14.2d, v15.2d}, [x0], #64
    ld1 {v16.2d, v17.2d, v18.2d, v19.2d}, [x0], #64
    ld1 {v20.2d, v21.2d, v22.2d, v23.2d}, [x0], #64
    ld1 {v24.2d}, [x0]
    sub x0, x0, #64*6

    bl  KeccakP1600_ARMv8Asha3_Permute_12rounds_internal

    ldr x30,[sp,#8]
    st1 { v0.2d,  v1.2d,  v2.2d,  v3.2d}, [x0], #64
    st1 { v4.2d,  v5.2d,  v6.2d,  v7.2d}, [x0], #64
    st1 { v8.2d,  v9.2d, v10.2d, v11.2d}, [x0], #64
    st1 {v12.2d, v13.2d, v14.2d, v15.2d}, [x0], #64
    st1 {v16.2d, v17.2d, v18.2d, v19.2d}, [x0], #64
    st1 {v20.2d, v21.2d, v22.2d, v23.2d}, [x0], #64
    st1 {v24.2d}, [x0]

    ldp d8,d9,[sp,#16]
    ldp d10,d11,[sp,#32]
    ldp d12,d13,[sp,#48]
    ldp d14,d15,[sp,#64]
    ldr x29,[sp],#80

    ret
.ifdef macOS
.else
.size   KeccakP1600times2_ARMv8Asha3_Permute_12rounds,.-KeccakP1600times2_ARMv8Asha3_Permute_12rounds
.endif

.ifdef macOS
.globl  _KangarooTwelve_ARMv8Asha3_Process2Leaves
_KangarooTwelve_ARMv8Asha3_Process2Leaves:
.else
.globl  KangarooTwelve_ARMv8Asha3_Process2Leaves
.type   KangarooTwelve_ARMv8Asha3_Process2Leaves,%function
KangarooTwelve_ARMv8Asha3_Process2Leaves:
.endif
.balign 32
    stp x29,x30,[sp,#-80]!
    add x29,sp,#0
    stp d8,d9,[sp,#16]      // per ABI requirement
    stp d10,d11,[sp,#32]
    stp d12,d13,[sp,#48]
    stp d14,d15,[sp,#64]

    movi  v0.2d, #0
    movi  v1.2d, #0
    movi  v2.2d, #0
    movi  v3.2d, #0
    movi  v4.2d, #0
    movi  v5.2d, #0
    movi  v6.2d, #0
    movi  v7.2d, #0
    movi  v8.2d, #0
    movi  v9.2d, #0
    movi v10.2d, #0
    movi v11.2d, #0
    movi v12.2d, #0
    movi v13.2d, #0
    movi v14.2d, #0
    movi v15.2d, #0
    movi v16.2d, #0
    movi v17.2d, #0
    movi v18.2d, #0
    movi v19.2d, #0
    movi v20.2d, #0
    movi v21.2d, #0
    movi v22.2d, #0
    movi v23.2d, #0
    movi v24.2d, #0

    // x12 is input + chunkSize
    add x12, x0, #8192

    // Loop over the first 48 blocks
    mov x11, 48
    b .KangarooTwelve_ARMv8Asha3_Process2Leaves_blocks
.KangarooTwelve_ARMv8Asha3_Process2Leaves_blocks:

    // Lanes 0-3
    ld1 {v25.1d-v28.1d}, [x0], #32
    ld1 {v25.d}[1], [x12], #8
    ld1 {v26.d}[1], [x12], #8
    ld1 {v27.d}[1], [x12], #8
    ld1 {v28.d}[1], [x12], #8
#ifdef  __AARCH64EB__
    rev64 v25.16b, v25.16b
    rev64 v26.16b, v26.16b
    rev64 v27.16b, v27.16b
    rev64 v28.16b, v28.16b
#endif
    eor v0.16b, v0.16b, v25.16b
    eor v1.16b, v1.16b, v26.16b
    eor v2.16b, v2.16b, v27.16b
    eor v3.16b, v3.16b, v28.16b

    // Lanes 4-7
    ld1 {v25.1d-v28.1d}, [x0], #32
    ld1 {v25.d}[1], [x12], #8
    ld1 {v26.d}[1], [x12], #8
    ld1 {v27.d}[1], [x12], #8
    ld1 {v28.d}[1], [x12], #8
#ifdef  __AARCH64EB__
    rev64 v25.16b, v25.16b
    rev64 v26.16b, v26.16b
    rev64 v27.16b, v27.16b
    rev64 v28.16b, v28.16b
#endif
    eor v4.16b, v4.16b, v25.16b
    eor v5.16b, v5.16b, v26.16b
    eor v6.16b, v6.16b, v27.16b
    eor v7.16b, v7.16b, v28.16b

    // Lanes 8-11
    ld1 {v25.1d-v28.1d}, [x0], #32
    ld1 {v25.d}[1], [x12], #8
    ld1 {v26.d}[1], [x12], #8
    ld1 {v27.d}[1], [x12], #8
    ld1 {v28.d}[1], [x12], #8
#ifdef  __AARCH64EB__
    rev64 v25.16b, v25.16b
    rev64 v26.16b, v26.16b
    rev64 v27.16b, v27.16b
    rev64 v28.16b, v28.16b
#endif
    eor  v8.16b,  v8.16b, v25.16b
    eor  v9.16b,  v9.16b, v26.16b
    eor v10.16b, v10.16b, v27.16b
    eor v11.16b, v11.16b, v28.16b

    // Lanes 12-15
    ld1 {v25.1d-v28.1d}, [x0], #32
    ld1 {v25.d}[1], [x12], #8
    ld1 {v26.d}[1], [x12], #8
    ld1 {v27.d}[1], [x12], #8
    ld1 {v28.d}[1], [x12], #8
#ifdef  __AARCH64EB__
    rev64 v25.16b, v25.16b
    rev64 v26.16b, v26.16b
    rev64 v27.16b, v27.16b
    rev64 v28.16b, v28.16b
#endif
    eor v12.16b, v12.16b, v25.16b
    eor v13.16b, v13.16b, v26.16b
    eor v14.16b, v14.16b, v27.16b
    eor v15.16b, v15.16b, v28.16b

    // Lanes 16-20
    ld1 {v25.1d-v28.1d}, [x0], #32
    ld1 {v25.d}[1], [x12], #8
    ld1 {v26.d}[1], [x12], #8
    ld1 {v27.d}[1], [x12], #8
    ld1 {v28.d}[1], [x12], #8
    ld1 {v29.d}[0], [x0], #8
    ld1 {v29.d}[1], [x12], #8
#ifdef  __AARCH64EB__
    rev64 v25.16b, v25.16b
    rev64 v26.16b, v26.16b
    rev64 v27.16b, v27.16b
    rev64 v28.16b, v28.16b
    rev64 v29.16b, v29.16b
#endif
    eor v16.16b, v16.16b, v25.16b
    eor v17.16b, v17.16b, v26.16b
    eor v18.16b, v18.16b, v27.16b
    eor v19.16b, v19.16b, v28.16b
    eor v20.16b, v20.16b, v29.16b

    bl KeccakP1600_ARMv8Asha3_Permute_12rounds_internal

    subs x11, x11, #1
    bne .KangarooTwelve_ARMv8Asha3_Process2Leaves_blocks

    // Lanes 0-3
    ld1 {v25.1d-v28.1d}, [x0], #32
    ld1 {v25.d}[1], [x12], #8
    ld1 {v26.d}[1], [x12], #8
    ld1 {v27.d}[1], [x12], #8
    ld1 {v28.d}[1], [x12], #8
#ifdef  __AARCH64EB__
    rev64 v25.16b, v25.16b
    rev64 v26.16b, v26.16b
    rev64 v27.16b, v27.16b
    rev64 v28.16b, v28.16b
#endif
    eor v0.16b, v0.16b, v25.16b
    eor v1.16b, v1.16b, v26.16b
    eor v2.16b, v2.16b, v27.16b
    eor v3.16b, v3.16b, v28.16b

    // Lanes 4-7
    ld1 {v25.1d-v28.1d}, [x0], #32
    ld1 {v25.d}[1], [x12], #8
    ld1 {v26.d}[1], [x12], #8
    ld1 {v27.d}[1], [x12], #8
    ld1 {v28.d}[1], [x12], #8
#ifdef  __AARCH64EB__
    rev64 v25.16b, v25.16b
    rev64 v26.16b, v26.16b
    rev64 v27.16b, v27.16b
    rev64 v28.16b, v28.16b
#endif
    eor v4.16b, v4.16b, v25.16b
    eor v5.16b, v5.16b, v26.16b
    eor v6.16b, v6.16b, v27.16b
    eor v7.16b, v7.16b, v28.16b

    // Lanes 8-11
    ld1 {v25.1d-v28.1d}, [x0], #32
    ld1 {v25.d}[1], [x12], #8
    ld1 {v26.d}[1], [x12], #8
    ld1 {v27.d}[1], [x12], #8
    ld1 {v28.d}[1], [x12], #8
#ifdef  __AARCH64EB__
    rev64 v25.16b, v25.16b
    rev64 v26.16b, v26.16b
    rev64 v27.16b, v27.16b
    rev64 v28.16b, v28.16b
#endif
    eor  v8.16b,  v8.16b, v25.16b
    eor  v9.16b,  v9.16b, v26.16b
    eor v10.16b, v10.16b, v27.16b
    eor v11.16b, v11.16b, v28.16b

    // Lanes 12-15
    ld1 {v25.1d-v28.1d}, [x0], #32
    ld1 {v25.d}[1], [x12], #8
    ld1 {v26.d}[1], [x12], #8
    ld1 {v27.d}[1], [x12], #8
    ld1 {v28.d}[1], [x12], #8
#ifdef  __AARCH64EB__
    rev64 v25.16b, v25.16b
    rev64 v26.16b, v26.16b
    rev64 v27.16b, v27.16b
    rev64 v28.16b, v28.16b
#endif
    eor v12.16b, v12.16b, v25.16b
    eor v13.16b, v13.16b, v26.16b
    eor v14.16b, v14.16b, v27.16b
    eor v15.16b, v15.16b, v28.16b

    mov x13, #0x0B
    dup v25.2d, x13
    mov x13, #0x8000000000000000
    dup v26.2d, x13
    eor v16.16b, v16.16b, v25.16b
    eor v20.16b, v20.16b, v26.16b

    bl KeccakP1600_ARMv8Asha3_Permute_12rounds_internal

    st1 {v0.1d-v3.1d}, [x1], #32
    st1 {v0.d}[1], [x1], #8
    st1 {v1.d}[1], [x1], #8
    st1 {v2.d}[1], [x1], #8
    st1 {v3.d}[1], [x1], #8

    ldr x30,[sp,#8]
    ldp d8,d9,[sp,#16]
    ldp d10,d11,[sp,#32]
    ldp d12,d13,[sp,#48]
    ldp d14,d15,[sp,#64]
    ldr x29,[sp],#80

    ret
.ifdef macOS
.else
.size   KangarooTwelve_ARMv8Asha3_Process2Leaves,.-KangarooTwelve_ARMv8Asha3_Process2Leaves
.endif
