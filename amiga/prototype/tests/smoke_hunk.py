#!/usr/bin/env python3
"""Run the actual HUNK and Amiga library-call glue with mock OS vectors.

Requires machine68k. This catches relocation/startup/ABI mistakes; the mock
graphics library is NOT P96/CGX and this is not a hardware performance test.
"""
import struct
import sys
from pathlib import Path
import machine68k as m


def run(executable, args, w, h, map_file=None, expected_preview=None, expected_fail=False):
    mc = m.Machine(m.CPUType.M68040, 8192)
    mem, cpu = mc.mem, mc.cpu
    def invalid(mode, width, address):
        raise AssertionError(("invalid emulator memory access", mode, width, hex(address), hex(cpu.r_pc())))
    mem.set_invalid_func(invalid)
    blob = Path(executable).read_bytes()
    position = 0

    def word():
        nonlocal position
        n = struct.unpack_from(">I", blob, position)[0]
        position += 4
        return n

    assert [word() for _ in range(5)] == [0x3F3, 0, 2, 0, 1]
    sizes = [word(), word()]
    bases = [0x100000, 0x200000]
    assert word() == 0x3E9
    length = word() * 4
    code = bytearray(blob[position:position + length])
    position += length
    tag = word()
    if tag == 0x3EC:
        while True:
            count = word()
            if not count:
                break
            target = word()
            for _ in range(count):
                off = word()
                val = struct.unpack_from(">I", code, off)[0] + bases[target]
                struct.pack_into(">I", code, off, val)
        tag = word()
    assert tag == 0x3F2 and word() == 0x3EB and word() == sizes[1] and word() == 0x3F2
    assert position == len(blob)
    mem.w_block(bases[0], bytes(code))

    libs = {"exec.library": 0x10000, "dos.library": 0x18000,
            "graphics.library": 0x20000, "intuition.library": 0x28000,
            "timer.device": 0x30000}
    process, screen, bitmap, window, port, message = 0x40000, 0x41000, 0x42000, 0x43000, 0x44000, 0x45000
    handlers = {}
    for name, vectors in {
        "exec.library": [-132, -138, -198, -210, -294, -372, -378, -384, -414, -444, -450, -552],
        "dos.library": [-30, -36, -42, -48, -60, -66],
        "graphics.library": [-198, -270, -732, -756, -882, -1056],
        "intuition.library": [-66, -72, -606, -612],
        "timer.device": [-60],
    }.items():
        for lvo in vectors:
            at = libs[name] + lvo
            mem.w16(at, 0x4E4F)  # trap #15, intercepted below
            mem.w16(at + 2, 0x4E75)
            handlers[at] = (name, lvo)
    vectors = 0x60000
    for vector in range(2, 64):
        mem.w32(vector * 4, vectors + vector * 4)
        mem.w16(vectors + vector * 4, 0x60FE)
    mem.w32(4, libs["exec.library"])
    mem.w8(libs["exec.library"] + 297, 0x80)
    mem.w32(process + 172, 1)  # Shell launch
    mem.w32(screen + 88, bitmap)
    mem.w32(window + 86, port)
    argument_addr = 0x46000
    mem.w_block(argument_addr, args.encode("ascii"))
    mem.w32(0x7F0000, 0x70000)
    mem.w16(0x70000, 0x60FE)
    cpu.w_pc(bases[0])
    cpu.w_sr(0x2700)
    cpu.w_isp(0x7F0000)
    cpu.w_sp(0x7F0000)
    cpu.w_reg(m.Register.A0, argument_addr)
    cpu.w_reg(m.Register.D0, len(args))
    map_bytes = Path(map_file).read_bytes() if map_file else None
    if expected_fail:
        map_bytes=map_bytes[:-1]+bytes([map_bytes[-1]^1])
    file_position = 0
    file_open = False
    allocation = 0x300000
    live = {}
    frame = bytearray(w * h)
    palette = [(0,0,0)]*256
    frames = 0
    copies = 0
    clock = 0
    sent_quit = False
    closed = set()
    opened = set()
    console = []
    mode = 0x50011000

    def reg(name):
        return cpu.r_reg(getattr(m.Register, name))

    def string(addr, limit=600):
        result = bytearray()
        for i in range(limit):
            c = mem.r8(addr + i)
            if c == 0:
                return result.decode("ascii")
            result.append(c)
        raise AssertionError("unterminated string")

    for _ in range(12000):
        mc.execute(100000)
        pc = cpu.r_pc()
        if pc == 0x70000:
            if expected_fail:
                assert reg("D0")==20 and not live and not file_open and frames==0
                assert b"Cannot load imported MAP" in b"".join(console)
                print("Corrupt MAP rejected / allocations and DOS file cleaned up: PASS")
                mc.cleanup();return
            assert reg("D0") == 0
            assert not live, live
            assert {"screen", "window", "timer"}.issubset(closed)
            assert frames == 3 and copies > 0
            assert len(set(frame)) > (4 if map_file else 15)
            assert not file_open
            if expected_preview:
                header1,dimensions,maximum,pixels=Path(expected_preview).read_bytes().split(b"\n",3)
                assert header1==b"P6" and dimensions==f"{w} {h}".encode() and maximum==b"255"
                native_pixels=b"".join(bytes(palette[index]) for index in frame[48*w:(h-48)*w])
                assert native_pixels==pixels[48*w*3:(h-48)*w*3], "Native viewport differs from host"
                print("Native RTG viewport pixels match host: PASS")
            print(f"HUNK/relocations/Amiga startup/OS ABI/RTG copy/cleanup: PASS {w}x{h}")
            mc.cleanup()
            return
        if not vectors <= pc < vectors + 256:
            continue
        vector = (pc - vectors) // 4
        sp = cpu.r_sp()
        stacked_pc = mem.r32(sp + 2)
        assert vector == 47, ("unexpected CPU exception", vector, hex(stacked_pc))
        name, lvo = handlers[stacked_pc - 2]
        result = 0
        if name == "exec.library":
            if lvo == -294:
                result = process
            elif lvo == -552:
                libname = string(reg("A1"))
                assert libname in libs, libname
                opened.add(libname)
                result = libs[libname]
            elif lvo == -198:
                n, flags = reg("D0"), reg("D1")
                assert flags & 4 and (n == w * h or (map_bytes is not None and n == len(map_bytes)))
                result = allocation
                live[result] = n
                allocation += (n + 15) & ~15
            elif lvo == -210:
                assert live.pop(reg("A1")) == reg("D0")
            elif lvo == -444:
                assert string(reg("A0")) == "timer.device"
                assert mem.r16(reg("A1") + 18) == 48
                mem.w32(reg("A1") + 20, libs["timer.device"])
            elif lvo == -450:
                closed.add("timer")
            elif lvo == -372:
                assert reg("A0") == port
                if frames >= 3 and not sent_quit:
                    sent_quit = True
                    mem.w32(message + 20, 0x400)
                    mem.w16(message + 24, 0x45)
                    result = message
            elif lvo == -378:
                assert reg("A1") == message
        elif name == "dos.library":
            if lvo == -30:
                if map_bytes is not None and string(reg("D1")) == "TestPark.amap":
                    file_open=True;file_position=0;result=42
                else: result=0
            elif lvo == -36:
                assert reg("D1")==42 and file_open
                file_open=False
            elif lvo == -66:
                assert reg("D1")==42 and file_open
                result=file_position
                origin=reg("D3");offset=reg("D2")
                if offset & 0x80000000:offset-=0x100000000
                file_position=(len(map_bytes) if origin==1 else 0 if origin==0xffffffff else file_position)+offset
            elif lvo == -42:
                assert reg("D1")==42 and file_open
                data=map_bytes[file_position:file_position+reg("D3")]
                mem.w_block(reg("D2"),data);file_position+=len(data);result=len(data)
            elif lvo == -60:
                result = 1
            elif lvo == -48:
                console.append(mem.r_block(reg("D2"), reg("D3")))
                result = reg("D3")
            else:
                raise AssertionError((name, lvo))
        elif name == "graphics.library":
            if lvo == -732:
                result = mode if reg("D0") == 0xFFFFFFFF else 0xFFFFFFFF
            elif lvo == -756:
                out, tag = reg("A1"), reg("D1")
                assert reg("D2") == mode
                if tag == 0x80000000:
                    mem.w32(out + 18, 0x80000000)
                else:
                    assert tag == 0x80001000
                    mem.w16(out + 16, 8)
                    mem.w16(out + 30, w - 1)
                    mem.w16(out + 32, h - 1)
                result = reg("D0")
            elif lvo == -270:
                frames += 1
            elif lvo == -882:
                assert reg("A0") == screen + 44
                assert mem.r32(reg("A1")) == 256 << 16
                palette=[tuple(mem.r32(reg("A1")+4+(i*3+k)*4)>>24 for k in range(3)) for i in range(256)]
            elif lvo == -1056:
                rp = reg("A0")
                assert mem.r32(rp + 4) == bitmap
                x0, y0, x1, y1 = [reg(f"D{i}") for i in range(4)]
                stride, src = reg("D4"), reg("A2")
                assert 0 <= x0 <= x1 < w and 0 <= y0 <= y1 < h and stride == w
                for row in range(y1 - y0 + 1):
                    data = mem.r_block(src + row * stride, x1 - x0 + 1)
                    frame[(y0 + row) * w + x0:(y0 + row) * w + x1 + 1] = data
                copies += 1
        elif name == "intuition.library":
            if lvo == -612:
                result = screen
            elif lvo == -606:
                result = window
            elif lvo == -66:
                assert reg("A0") == screen
                closed.add("screen")
            elif lvo == -72:
                assert reg("A0") == window
                closed.add("window")
        else:
            assert name == "timer.device" and lvo == -60
            clock += 1000
            mem.w32(reg("A0"), 0)
            mem.w32(reg("A0") + 4, clock)
            result = 1000000
        cpu.w_reg(m.Register.D0, result)
        cpu.w_sr(mem.r16(sp))
        cpu.w_sp(sp + 8)  # 68040 format-0 exception frame includes the format word
        cpu.w_pc(stacked_pc)
    raise AssertionError("HUNK smoke test did not terminate")


if __name__ == "__main__":
    executable = sys.argv[1]
    run(executable, "DIAGNOSTIC\n", 640, 480)
    run(executable, "LOW DIAGNOSTIC\n", 320, 240)
    if len(sys.argv)>2:
        run(executable,"DIAGNOSTIC MAP=TestPark.amap\n",640,480,sys.argv[2],sys.argv[3] if len(sys.argv)>3 else None)
        run(executable,"LOW DIAGNOSTIC MAP=TestPark.amap\n",320,240,sys.argv[2],sys.argv[4] if len(sys.argv)>4 else None)

    if len(sys.argv)>2:
        run(executable,"DIAGNOSTIC MAP=TestPark.amap\n",640,480,sys.argv[2],expected_fail=True)
