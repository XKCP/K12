# -*- coding: utf-8 -*-
# Implementation by Gilles Van Assche, hereby denoted as "the implementer".
#
# For more information, feedback or questions, please refer to our website:
# https://keccak.team/
#
# To the extent possible under law, the implementer has waived all copyright
# and related or neighboring rights to the source code in this file.
# http://creativecommons.org/publicdomain/zero/1.0/

def ROL64(a, n):
    return ((a >> (64-(n%64))) + (a << (n%64))) % (1 << 64)

def load64(b):
    return sum((b[i] << (8*i)) for i in range(8))

def store64(a):
    return bytearray((a >> (8*i)) % 256 for i in range(8))

def hex2lane(hexstring):
    bytez = [int(token, 16) for token in hexstring.split()]
    return load64(bytez)

def KP(state):
    RC = [
        hex2lane("8B 80 00 80 00 00 00 00"),
        hex2lane("8B 00 00 00 00 00 00 80"),
        hex2lane("89 80 00 00 00 00 00 80"),
        hex2lane("03 80 00 00 00 00 00 80"),
        hex2lane("02 80 00 00 00 00 00 80"),
        hex2lane("80 00 00 00 00 00 00 80"),
        hex2lane("0A 80 00 00 00 00 00 00"),
        hex2lane("0A 00 00 80 00 00 00 80"),
        hex2lane("81 80 00 80 00 00 00 80"),
        hex2lane("80 80 00 00 00 00 00 80"),
        hex2lane("01 00 00 80 00 00 00 00"),
        hex2lane("08 80 00 80 00 00 00 80"),
    ]

    lanes = [[0 for _ in range(5)] for _ in range(5)]
    for x in range(5):
        for y in range(5):
            lanes[x][y] = load64(state[8*(x+5*y):8*(x+5*y)+8])

    for round in range(12):
        # theta
        C = [0]*5
        for x in range(5):
            C[x] = lanes[x][0]
            C[x] ^= lanes[x][1]
            C[x] ^= lanes[x][2]
            C[x] ^= lanes[x][3]
            C[x] ^= lanes[x][4]
        D = [0]*5
        for x in range(5):
            D[x] = C[(x+4) % 5] ^ ROL64(C[(x+1) % 5], 1)
        for y in range(5):
            for x in range(5):
                lanes[x][y] = lanes[x][y]^D[x]

        # rho and pi
        (x, y) = (1, 0)
        current = lanes[x][y]
        for t in range(24):
            (x, y) = (y, (2*x+3*y) % 5)
            (current, lanes[x][y]) = (lanes[x][y], ROL64(current, (t+1)*(t+2)//2))

        # chi
        for y in range(5):
            T = [0]*5
            for x in range(5):
                T[x] = lanes[x][y]
            for x in range(5):
                lanes[x][y] = T[x] ^((~T[(x+1) % 5]) & T[(x+2) % 5])

        # iota
        lanes[0][0] ^= RC[round]

    state = bytearray()
    for y in range(5):
        for x in range(5):
            state = state + store64(lanes[x][y])

    return state

def XOR(state1, state2):
    return [state1[i] ^ state2[i] for i in range(min(len(state1), len(state2)))]

def TurboSHAKE128(message, separationByte, outputByteLen):
    offset = 0
    state = [0x00]*200
    input = list(message) + [separationByte]

    # === Absorb complete blocks ===
    while offset < len(input) - 168:
        state = XOR(state, input[offset : offset + 168] + [0x00]*32)
        state = KP(state)
        offset += 168

    # === Absorb last block and treatment of padding ===
    LastBlockLength = len(input) - offset
    state = XOR(state, input[offset:] + [0x00]*(200-LastBlockLength))
    state = XOR(state, [0x00]*167 + [0x80] + [0x00]*32)
    state = KP(state)

    # === Squeeze ===
    output = bytearray()
    while outputByteLen > 168:
        output = output + state[0:168]
        outputByteLen -= 168
        state = KP(state)

    output = output + state[0:outputByteLen]
    return output

def TurboSHAKE256(message, separationByte, outputByteLen):
    offset = 0
    state = [0x00]*200
    input = list(message) + [separationByte]

    # === Absorb complete blocks ===
    while offset < len(input) - 136:
        state = XOR(state, input[offset : offset + 136] + [0x00]*64)
        state = KP(state)
        offset += 136

    # === Absorb last block and treatment of padding ===
    LastBlockLength = len(input) - offset
    state = XOR(state, input[offset:] + [0x00]*(200-LastBlockLength))
    state = XOR(state, [0x00]*135 + [0x80] + [0x00]*64)
    state = KP(state)

    # === Squeeze ===
    output = bytearray()
    while outputByteLen > 136:
        output = output + state[0:136]
        outputByteLen -= 136
        state = KP(state)

    output = output + state[0:outputByteLen]
    return output
