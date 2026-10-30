#!/usr/bin/env python3
"""Fail if a 68k ELF uses the integer instructions a 68060 lacks.

    tools/check_060.py build/draw_68020
    OBJDUMP=m68k-amigaos-objdump tools/check_060.py build/Game   (a hunk file)

(A hunk file doesn't say which CPU it's for: it is disassembled as a
68020's, or the 32-bit multiplies and divides wouldn't be seen at all.)

The 68060 dropped the 32x32->64 bit multiply (muls.l/mulu.l into a
register pair) and the 64/32 bit divide (divs.l/divu.l from a pair) of the
68020.  Code using them still runs, but each one traps to 68060.library
and is done in software, hundreds of cycles a go: gcc -m68020 uses the
multiply for every division by a constant, so a game can crawl on an
'060 that flies on an '030.  gcc -m68020-60 keeps clear of them; this
checks nothing (libgcc, an assembler routine) has brought them back.
"""
import os
import re
import subprocess
import sys

elf = sys.argv[1]
objdump = os.environ.get("OBJDUMP", "m68k-linux-gnu-objdump")
arch = [] if objdump == "m68k-linux-gnu-objdump" else ["-m", "m68k:68020"]
dis = subprocess.run([objdump, "-d"] + arch + [elf],
                     capture_output=True, text=True, check=True).stdout
# Freestanding games may merge text, rodata and data into one executable
# hunk. A linker-provided boundary prevents strings such as "WORLD: FLAT"
# from being decoded as a register-pair divide. Other ELF/hunk layouts
# retain the existing full-disassembly check.
symbols = subprocess.run([objdump, "-t", elf], capture_output=True, text=True, check=True).stdout
boundary = re.search(r"^([0-9a-f]+)\s+.*\s__text_end$", symbols, re.MULTILINE)
text_end = int(boundary.group(1), 16) if boundary else None
func, bad = None, []
for line in dis.splitlines():
    m = re.match(r"^[0-9a-f]+ <(.+)>:", line)
    if m:
        func = m.group(1)
        continue
    m = re.match(r"^\s*([0-9a-f]+):\s+((?:[0-9a-f]{4} )+)\s*(\S+)", line)
    if not m:
        continue
    if text_end is not None and int(m.group(1), 16) >= text_end:
        continue
    words, op = m.group(2).split(), m.group(3)
    # mul.l / div.l: 0x4c0x / 0x4c4x, then an extension word whose bit 10
    # picks the 64-bit form
    if (op.startswith(("muls", "mulu", "divs", "divu")) and len(words) >= 2
            and words[0].startswith("4c") and int(words[1], 16) & 0x0400):
        bad.append("%s in %s at %s" % (op, func, m.group(1)))
for b in bad:
    print("68060 traps on:", b)
print("68060: %s" % ("FAIL" if bad else "ok, no 64-bit multiply or divide"))
sys.exit(1 if bad else 0)
