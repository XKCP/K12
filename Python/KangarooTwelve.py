# -*- coding: utf-8 -*-
# Implementation by Gilles Van Assche, hereby denoted as "the implementer".
#
# For more information, feedback or questions, please refer to our website:
# https://keccak.team/
#
# To the extent possible under law, the implementer has waived all copyright
# and related or neighboring rights to the source code in this file.
# http://creativecommons.org/publicdomain/zero/1.0/

from TurboSHAKE import TurboSHAKE128, TurboSHAKE256
from Utils import outputHex

def length_encode(x):
    S = bytearray()
    while x > 0:
        S = bytearray([x % 256]) + S
        x = x//256
    S = S + bytearray([len(S)])
    return S

# inputMessage and customizationString must be of type byte string or byte array
def KT128(inputMessage, customString, outputByteLen):
    S = inputMessage + customString
    S = S + length_encode(len(customString))

    if len(S) <= 8192:
        return TurboSHAKE128(S, 0x07, outputByteLen)
    else:
        # === Kangaroo hopping ===
        FinalNode = S[0:8192] + bytearray([0x03] + [0x00]*7)
        offset = 8192
        numBlock = 0
        while offset < len(S):
            blockSize = min(len(S) - offset, 8192)
            CV = TurboSHAKE128(S[offset : offset + blockSize], 0x0B, 32)
            FinalNode = FinalNode + CV
            numBlock += 1
            offset   += blockSize
        FinalNode = FinalNode + length_encode( numBlock ) + bytearray([0xFF, 0xFF])
        return TurboSHAKE128(FinalNode, 0x06, outputByteLen)

def KT256(inputMessage, customString, outputByteLen):
    S = inputMessage + customString
    S = S + length_encode(len(customString))

    if len(S) <= 8192:
        return TurboSHAKE256(S, 0x07, outputByteLen)
    else:
        # === Kangaroo hopping ===
        FinalNode = S[0:8192] + bytearray([0x03] + [0x00]*7)
        offset = 8192
        numBlock = 0
        while offset < len(S):
            blockSize = min(len(S) - offset, 8192)
            CV = TurboSHAKE256(S[offset : offset + blockSize], 0x0B, 64)
            FinalNode = FinalNode + CV
            numBlock += 1
            offset   += blockSize
        FinalNode = FinalNode + length_encode( numBlock ) + bytearray([0xFF, 0xFF])
        return TurboSHAKE256(FinalNode, 0x06, outputByteLen)
