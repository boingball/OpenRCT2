#!/usr/bin/env python3
"""Convert a fully linked m68k ELF (linked with demo/amiga.ld and
--emit-relocs) into an AmigaDOS hunk executable.

The linker script produces exactly two allocated sections:
  CODE  at VMA 0          -> HUNK_CODE (text + rodata + data)
  BSS   at VMA 0x40000000 -> HUNK_BSS

Every absolute 32-bit relocation (R_68K_32) found in the .rela sections
becomes a HUNK_RELOC32 entry.  PC-relative relocations are already resolved
by the link, which is only correct when they stay inside one hunk, so any
PC-relative reference into the other hunk is reported as an error.
"""
import struct
import sys

R_68K_32 = 1
R_68K_PC32, R_68K_PC16, R_68K_PC8 = 4, 5, 6
PC_RELOCS = (R_68K_PC32, R_68K_PC16, R_68K_PC8)

HUNK_CODE, HUNK_BSS = 0x3E9, 0x3EB
HUNK_RELOC32, HUNK_END, HUNK_HEADER = 0x3EC, 0x3F2, 0x3F3


def die(msg):
    sys.stderr.write("elf2hunk: %s\n" % msg)
    sys.exit(1)


def main():
    if len(sys.argv) != 3:
        die("usage: elf2hunk.py in.elf out-exe")
    data = open(sys.argv[1], "rb").read()
    if data[:4] != b"\x7fELF" or data[4] != 1 or data[5] != 2:
        die("not a 32-bit big-endian ELF")
    (e_type, e_machine, _, _, _, e_shoff, _, _, _, _,
     e_shentsize, e_shnum, e_shstrndx) = struct.unpack_from(
        ">HHIIIIIHHHHHH", data, 16)
    if e_machine != 4:
        die("not an m68k ELF")

    secs = []
    for i in range(e_shnum):
        secs.append(struct.unpack_from(">IIIIIIIIII", data,
                                       e_shoff + i * e_shentsize))
    shstr = secs[e_shstrndx]

    def name(sec):
        off = shstr[4] + sec[0]
        return data[off:data.index(b"\0", off)].decode()

    by_name = {name(s): (i, s) for i, s in enumerate(secs)}
    if "CODE" not in by_name:
        die("no CODE section")
    code_i, code = by_name["CODE"]
    code_base, code_size = code[3], code[5]
    code_bytes = bytearray(data[code[4]:code[4] + code_size])
    if "BSS" in by_name:
        bss_i, bss = by_name["BSS"]
        bss_base, bss_size = bss[3], bss[5]
    else:
        bss_i, bss_base, bss_size = -1, 0x40000000, 0

    def hunk_of(addr):
        if code_base <= addr <= code_base + code_size:
            return 0, addr - code_base
        if bss_base <= addr <= bss_base + bss_size:
            return 1, addr - bss_base
        die("relocation target 0x%x outside both hunks" % addr)

    relocs = {0: [], 1: []}
    for s in secs:
        if s[1] != 4:           # SHT_RELA
            continue
        if s[7] != code_i:      # sh_info: section the relocs apply to
            if s[7] == bss_i:
                die("relocations in BSS")
            continue
        symtab = secs[s[6]]
        for off in range(s[4], s[4] + s[5], 12):
            r_offset, r_info, r_addend = struct.unpack_from(">IIi", data, off)
            rtype, rsym = r_info & 0xFF, r_info >> 8
            st_value, _, _, _, st_shndx = struct.unpack_from(
                ">IIBBH", data, symtab[4] + rsym * 16 + 4)
            if rsym == 0:
                st_value = 0
            target = (st_value + r_addend) & 0xFFFFFFFF
            where = r_offset - code_base
            if rtype == R_68K_32:
                hunk, rel = hunk_of(target)
                struct.pack_into(">I", code_bytes, where, rel)
                relocs[hunk].append(where)
            elif rtype in PC_RELOCS:
                if hunk_of(target)[0] != 0:
                    die("PC-relative reference into BSS at 0x%x" % where)
            elif rtype != 0:
                die("unsupported relocation type %d at 0x%x" % (rtype, where))

    while len(code_bytes) % 4:
        code_bytes.append(0)
    nhunks = 2
    out = bytearray()
    out += struct.pack(">IIIII", HUNK_HEADER, 0, nhunks, 0, nhunks - 1)
    out += struct.pack(">II", len(code_bytes) // 4, (bss_size + 3) // 4)
    out += struct.pack(">II", HUNK_CODE, len(code_bytes) // 4)
    out += code_bytes
    if relocs[0] or relocs[1]:
        out += struct.pack(">I", HUNK_RELOC32)
        for hunk in (0, 1):
            offs = sorted(set(relocs[hunk]))
            if offs:
                out += struct.pack(">II", len(offs), hunk)
                out += b"".join(struct.pack(">I", o) for o in offs)
        out += struct.pack(">I", 0)
    out += struct.pack(">I", HUNK_END)
    out += struct.pack(">II", HUNK_BSS, (bss_size + 3) // 4)
    out += struct.pack(">I", HUNK_END)
    open(sys.argv[2], "wb").write(out)
    print("elf2hunk: %s: code %d bytes, bss %d bytes, %d relocs" % (
        sys.argv[2], len(code_bytes), bss_size,
        len(relocs[0]) + len(relocs[1])))


if __name__ == "__main__":
    main()
