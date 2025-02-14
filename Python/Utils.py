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

def hexString(s):
    r = ''
    for i in range(len(s)):
        if r != '': r = r + ' '
        r = r + "{0:02X}".format(s[i])
    return r

def hexStringSpecial(s):
    if len(s) == 0:
        return "`00`^0"
    else:
        return "`"+hexString(s)+"`"

def numberStringSpecial(base, exponent):
    if exponent == 0:
        return "1"
    elif exponent == 1:
        return "{0:d}".format(base)
    else:
        return "{0:d}**{1:d}".format(base, exponent)

def outputHex(s):
    for i in range(len(s)):
        print("{0:02X}".format(s[i]), end=' ')
        if i % 16 == 15:
            print()
    print()
    print()

def printTestVectorOutput(s):
    print('    `', end='')
    for i in range(len(s)):
        print("{0:02X}".format(s[i]), end=('`' if i == len(s) - 1 else ' '))
        if i % 16 == 15:
            print()
            print('     ', end='')
    print()
