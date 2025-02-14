# -*- coding: utf-8 -*-
# Implementation by Gilles Van Assche and Benoit Viguier, hereby denoted as "the implementers".
#
# For more information, feedback or questions, please refer to our website:
# https://keccak.team/
#
# To the extent possible under law, the implementers has waived all copyright
# and related or neighboring rights to the source code in this file.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import print_function
from TurboSHAKE import TurboSHAKE256
from Utils import hexString, printTestVectorOutput

def generateSimpleRawMaterial(length, seed1, seed2):
    seed2 = seed2 % 8
    return bytes([(seed1 + 161*length - ((i%256) << seed2) - ((i%256) >> (8-seed2)) + i)%256 for i in range(length)])

customizationByteSize = 32

def performTestTurboSHAKE256OneInput(inputLen, outputLen, customLen):
    customization = 97
    inputMessage = generateSimpleRawMaterial(inputLen, outputLen, inputLen + customLen)
    print("outputLen {0:5d}, inputLen {1:5d}, customLen {2:3d}".format(outputLen, inputLen, customLen))
    output = TurboSHAKE256(inputMessage, customization, outputLen)
    print("Kangaroo-Twelve")
    print("Input of {0:d} bytes:".format(inputLen), end='')
    for i in range(min(inputLen, 16)):
        print(" {0:02x}".format(inputMessage[i]), end='')
    if (inputLen > 16):
        print(" ...", end='')
    print("")
    print("Output of {0:d} bytes:".format(outputLen), end='')
    for i in range(outputLen):
        print(" {0:02x}".format(output[i]), end='')
    print("")
    print("")

def performTestTurboSHAKE256():
    cBlockSize = 8192
    outputLen = 256//8
    customLen = 0
    for inputLen in range(cBlockSize*9+124):
        performTestTurboSHAKE256OneInput(inputLen, outputLen, customLen)

    outputLen = 128//8
    while(outputLen <= 512//8):
        inputLen = 0
        while(inputLen <= (3*cBlockSize)):
            customLen = 0
            while(customLen <= customizationByteSize):
                performTestTurboSHAKE256OneInput(inputLen, outputLen, customLen)
                customLen += 7
            inputLen = (inputLen + 167) if (inputLen > 0) else 1
        outputLen = outputLen*2

def performShortTestTurboSHAKE256():
    cBlockSize = 8192
    outputLen = 256//8
    customLen = 0
    for inputLen in range(4):
        performTestTurboSHAKE256OneInput(inputLen, outputLen, customLen)
    performTestTurboSHAKE256OneInput(27121, outputLen, customLen)

#performTestTurboSHAKE256()
#performShortTestTurboSHAKE256()

def printTestVectors():
    print("  TurboSHAKE256(M=`00`^0, D=`1F`, 64):")
    printTestVectorOutput(TurboSHAKE256(b'', 0x1F, 64))
    print("  TurboSHAKE256(M=`00`^0, D=`1F`, 10032), last 32 bytes:")
    printTestVectorOutput(TurboSHAKE256(b'', 0x1F, 10032)[10000:])
    for i in range(7):
        M = bytearray([(j % 251) for j in range(17**i)])
        print("  TurboSHAKE256(M=ptn(17**{0:d} bytes), D=`1F`, 64):".format(i))
        printTestVectorOutput(TurboSHAKE256(M, 0x1F, 64))
    for D in [0x01, 0x06, 0x07, 0x0B, 0x30, 0x7F]:
        i = D%3 + 1
        M = bytearray([0xFF for j in range(2**i-1)])
        print("  TurboSHAKE256(M=`{0}`, D=`{1:02X}`, 64):".format(hexString(M), D))
        printTestVectorOutput(TurboSHAKE256(M, D, 64))

printTestVectors()
