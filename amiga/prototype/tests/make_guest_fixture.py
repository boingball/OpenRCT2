#!/usr/bin/env python3
"""Synthetic G1-shaped guest test data; contains no original game artwork."""
from pathlib import Path
import struct
import sys
count=29294
pixels=bytearray(9*20)
for y in range(20):
    for x in range(9):
        value=0
        if 1<=y<=4 and 3<=x<=5:value=58
        elif 5<=y<=13 and 2<=x<=6:value=243+(y-5)
        elif 14<=y<=19 and x in (3,5):value=202+(y-14)
        pixels[y*9+x]=value
entries=bytearray(count*16)
for direction in range(4):
    struct.pack_into('<IhhhhHH',entries,(6409+direction)*16,0,9,20,-4,-20,1,0)
Path(sys.argv[1]).write_bytes(struct.pack('<II',count,len(pixels))+entries+pixels)
