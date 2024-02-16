#!/usr/bin/python3

import sys

try:
    fp = open('../builds/chroma74y/main.map',mode='r')
except OSError as err:
    print(err)
    exit(code=err.errno)

LastAdr = -1
while True:
    line=fp.readline()
    if len(line) == 0:
        break
    if line.startswith('D:'):
        fields=line.strip().split()
        address=int(fields[1],16)
        if LastAdr != -1:
            Length = address - LastAdr
            print(f'{Length}\tD: {LastName} @ 0x{LastAdr:04x}')
        LastAdr = address
        LastName=fields[2]

fp.seek(0,0)

LastAdr = -1
while True:
    line=fp.readline()
    if len(line) == 0:
        break
    if line.startswith('C:'):
        fields=line.strip().split()
        address=int(fields[1],16)
        if LastAdr != -1:
            Length = address - LastAdr
            print(f'{Length}\tC: {LastName} @ 0x{LastAdr:04x}')
        LastAdr = address
        LastName=fields[2]

