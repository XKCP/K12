# -*- coding: utf-8 -*-
# Implementation by Gilles Van Assche, hereby denoted as "the implementer".
#
# For more information, feedback or questions, please refer to our website:
# https://keccak.team/
#
# To the extent possible under law, the implementer has waived all copyright
# and related or neighboring rights to the source code in this file.
# http://creativecommons.org/publicdomain/zero/1.0/

from KangarooTwelve import KT128, KT256
from Utils import hexStringSpecial, numberStringSpecial, printTestVectorOutput

def printKT128TestVectors():
    print("  KT128(M=`00`^0, C=`00`^0, 32):")
    printTestVectorOutput(KT128(b'', b'', 32))
    print("  KT128(M=`00`^0, C=`00`^0, 64):")
    printTestVectorOutput(KT128(b'', b'', 64))
    print("  KT128(M=`00`^0, C=`00`^0, 10032), last 32 bytes:")
    printTestVectorOutput(KT128(b'', b'', 10032)[10000:])
    for i in range(7):
        C = b''
        M = bytearray([(j % 251) for j in range(17**i)])
        print("  KT128(M=ptn({0:s} bytes), C=`00`^0, 32):".format(numberStringSpecial(17, i)))
        printTestVectorOutput(KT128(M, C, 32))
    for i in range(4):
        M = bytearray([0xFF for j in range(2**i-1)])
        C = bytearray([(j % 251) for j in range(41**i)])
        print("  KT128({0:s}, C=ptn({1:s} bytes), 32):".format(hexStringSpecial(M), numberStringSpecial(41, i)))
        printTestVectorOutput(KT128(M, C, 32))
    # We test for 8191 bytes of M because right_encode of empty C is 1 byte, so S is exactly 8192 bytes
    print("  KT128(M=ptn(8191 bytes), C=`00`^0, 32):")
    printTestVectorOutput(KT128(bytearray([(j % 251) for j in range(8191)]), b'', 32))
    # We test for 8192 bytes of M because right_encode of empty C is 1 byte so this put a full new block
    print("  KT128(M=ptn(8192 bytes), C=`00`^0, 32):")
    printTestVectorOutput(KT128(bytearray([(j % 251) for j in range(8192)]), b'', 32))
    # We test with 8192 bytes of M + 8189 bytes of C because 8189 = 3 bytes of Right_ecnode thus S is exactly 2 * 8192 bytes
    # We test with 8192 bytes of M + 8190 bytes of C because 8189 = 3 bytes of Right_ecnode thus S is exactly 2 * 8192 + 1 bytes
    for c in [8189, 8190]:
        C = bytearray([(j % 251) for j in range(c)])
        print("  KT128(M=ptn(8192 bytes), C=ptn({0:d} bytes), 32):".format(c))
        printTestVectorOutput(KT128(bytearray([(j % 251) for j in range(8192)]), C, 32))

def printKT256TestVectors():
    print("  KT256(M=`00`^0, C=`00`^0, 64):")
    printTestVectorOutput(KT256(b'', b'', 64))
    print("  KT256(M=`00`^0, C=`00`^0, 128):")
    printTestVectorOutput(KT256(b'', b'', 128))
    print("  KT256(M=`00`^0, C=`00`^0, 10064), last 64 bytes:")
    printTestVectorOutput(KT256(b'', b'', 10064)[10000:])
    for i in range(7):
        C = b''
        M = bytearray([(j % 251) for j in range(17**i)])
        print("  KT256(M=ptn({0:s} bytes), C=`00`^0, 64):".format(numberStringSpecial(17, i)))
        printTestVectorOutput(KT256(M, C, 64))
    for i in range(4):
        M = bytearray([0xFF for j in range(2**i-1)])
        C = bytearray([(j % 251) for j in range(41**i)])
        print("  KT256({0:s}, C=ptn({1:s} bytes), 64):".format(hexStringSpecial(M), numberStringSpecial(41, i)))
        printTestVectorOutput(KT256(M, C, 64))
    # We test for 8191 bytes of M because right_encode of empty C is 1 byte, so S is exactly 8192 bytes
    print("  KT256(M=ptn(8191 bytes), C=`00`^0, 64):")
    printTestVectorOutput(KT256(bytearray([(j % 251) for j in range(8191)]), b'', 64))
    # We test for 8192 bytes of M because right_encode of empty C is 1 byte so this put a full new block
    print("  KT256(M=ptn(8192 bytes), C=`00`^0, 64):")
    printTestVectorOutput(KT256(bytearray([(j % 251) for j in range(8192)]), b'', 64))
    # We test with 8192 bytes of M + 8189 bytes of C because 8189 = 3 bytes of Right_ecnode thus S is exactly 2 * 8192 bytes
    # We test with 8192 bytes of M + 8190 bytes of C because 8189 = 3 bytes of Right_ecnode thus S is exactly 2 * 8192 + 1 bytes
    for c in [8189, 8190]:
        C = bytearray([(j % 251) for j in range(c)])
        print("  KT256(M=ptn(8192 bytes), C=ptn({0:d} bytes), 64):".format(c))
        printTestVectorOutput(KT256(bytearray([(j % 251) for j in range(8192)]), C, 64))

printKT128TestVectors()
printKT256TestVectors()
