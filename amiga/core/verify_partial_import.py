#!/usr/bin/env python3
"""Compare partial GameState imports and reject malformed legacy saves."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("host", type=Path)
    parser.add_argument("--native", type=Path)
    parser.add_argument("--emulator", type=Path)
    parser.add_argument("--work-name", default="import-verification")
    args = parser.parse_args()
    if bool(args.native) != bool(args.emulator):
        parser.error("--native and --emulator must be specified together")
    if not re.fullmatch(r"[a-zA-Z0-9_-]+", args.work_name):
        parser.error("--work-name must be a simple directory name")
    work = ROOT / "build" / args.work_name
    work.mkdir(parents=True, exist_ok=True)

    def chunk(payload):
        return struct.pack("<BI", 0, len(payload)) + payload

    def checksummed(data):
        return data + struct.pack("<I", sum(data) & 0xffffffff)

    # Independently laid out wire fixture. These are protocol offsets after
    # the classic 10,000 x 256-byte entity block, not native offsetof values.
    header = struct.pack("<BBHII20x", 0, 0, 0, 120001, 0x31144)
    dates = struct.pack("<HHIII", 27, 64512, 54321, 0x01234567, 0x89abcdef)
    tiles = bytearray(0x30000 * 8)
    surface = bytes([0, 0x80, 2, 2, 0, 0, 0, 0])
    tiles[:65537 * 8] = surface * 65537
    tiles[1] = 0x0f  # first tile continues to its queue path
    tiles[8:16] = bytes([5, 0x80, 2, 4, 0x3e, 0xa1, 0x5a, 0xfe])
    saved = bytearray(488816 + 10000 * 256)
    post = 4 + 10000 * 256
    for offset, fmt, value in [(5240, "H", 5), (1160, "H", 9), (2292, "H", 321),
                               (32, "i", 123450), (36, "i", 10000), (40, "I", 4),
                               (44, "h", 30), (46392, "I", 54321)]:
        struct.pack_into("<" + fmt, saved, post + offset, value)
    encrypted_cash = (((123456 >> 13) | (123456 << 19)) & 0xffffffff) ^ 0xf4ec9621
    struct.pack_into("<I", saved, post + 5180, encrypted_cash)
    label = b"Native endian fixture\0"
    saved[post + 7758:post + 7758 + len(label)] = label

    def envelope(state, tile_data=tiles, header_data=header):
        return checksummed(chunk(header_data) + chunk(bytes(721 * 16))
                           + chunk(dates) + chunk(tile_data) + chunk(state))

    good = work / "wire-fixture.sv6"
    good.write_bytes(envelope(saved))
    oversized = bytearray(saved)
    struct.pack_into("<H", oversized, post + 5240, 257)
    no_end = bytearray(tiles)
    no_end[1::8] = bytes(len(no_end) // 8)
    no_end[2::8] = bytes([255]) * (len(no_end) // 8)
    scenario_header = bytearray(header)
    scenario_header[0] = 1
    bad = {
        "oversized.sv6": envelope(oversized),
        "no-end-marker.sv6": envelope(saved, no_end),
        "scenario-not-supported.sv6": envelope(saved, header_data=scenario_header),
        "bad-checksum.sv6": envelope(saved)[:-1] + bytes([envelope(saved)[-1] ^ 1]),
        "truncated.sv6": b"abc",
    }
    for name, data in bad.items():
        (work / name).write_bytes(data)
    negative = bytearray(saved)
    negative_cash = (-123456) & 0xffffffff
    negative_encrypted = (((negative_cash >> 13) | (negative_cash << 19)) & 0xffffffff) ^ 0xf4ec9621
    struct.pack_into("<I", negative, post + 5180, negative_encrypted)
    negative_file = work / "negative-cash.sv6"
    negative_file.write_bytes(envelope(negative))
    fixtures = [ROOT / "test/tests/testdata/parks/small_park_car_ride_one_car.sv6",
                ROOT / "test/tests/testdata/parks/bpb.sv6", good, negative_file]
    expected = {
        fixtures[0].name: (15, 225, 24),
        "bpb.sv6": (128, 16384, 3400),
        good.name: (5, 25, 1),
        negative_file.name: (5, 25, 1),
    }
    platforms = [("host", [str(args.host.resolve())])]
    if args.native:
        platforms.append(("m68k", [str(args.emulator.resolve()), "--cpu", "68060", "--no-fpu",
            "--ram", "64M", "--stack", "256K", "-V", f"WORK:{ROOT}"]))
    results = {}
    for platform, prefix in platforms:
        results[platform] = {}
        for source in fixtures + [work / n for n in bad]:
            case = work / platform / source.stem
            case.mkdir(parents=True, exist_ok=True)
            overview = case / "ImportedMap.ppm"
            overview.unlink(missing_ok=True)
            before = hashlib.sha256(source.read_bytes()).hexdigest()
            if platform == "host":
                command = prefix + [str(source)]
            else:
                command = prefix + ["--cwd", "WORK:" + case.relative_to(ROOT).as_posix(),
                    str(args.native.resolve()), "WORK:" + source.relative_to(ROOT).as_posix()]
            run = subprocess.run(command, cwd=case, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                 text=True, timeout=120)
            (case / "run.log").write_text(run.stdout)
            assert hashlib.sha256(source.read_bytes()).hexdigest() == before, "Input save was modified"
            if source.name in bad:
                assert run.returncode == 20 and "FAIL:" in run.stdout and not overview.exists(), run.stdout
                continue
            assert run.returncode == 0 and "Partial GameState import: PASS" in run.stdout, run.stdout
            assert "Native path status/RideId union: PASS" in run.stdout, run.stdout
            match = re.search(r"Map: (\d+) x (\d+); surfaces: (\d+); paths: (\d+)", run.stdout)
            assert match, run.stdout
            assert tuple(map(int, (match[1], match[3], match[4]))) == expected[source.name], run.stdout
            assert match[1] == match[2], run.stdout
            fp = re.search(r"Partial native state fingerprint: ([0-9a-f]{8})", run.stdout)
            assert fp and overview.exists(), run.stdout
            result = {"size": int(match[1]), "surfaces": int(match[3]), "paths": int(match[4]),
                      "fingerprint": fp[1], "overview_sha256": hashlib.sha256(overview.read_bytes()).hexdigest()}
            results[platform][source.name] = result
            if source == negative_file:
                assert "Cash (tenths): -123456; loan (tenths): 10000" in run.stdout, run.stdout
            if source == good:
                for text in ["Ticks: 54321; elapsed months: 27; month ticks: 64512",
                             "Saved guest count: 9; park rating: 321",
                             "RNG seeds: 01234567 89abcdef",
                             "Initial cash (tenths): 123450; fee: 30; flags: 00000004",
                             "Cash (tenths): 123456; loan (tenths): 10000",
                             "Scenario label (legacy bytes): Native endian fixture"]:
                    assert text in run.stdout, run.stdout
                assert overview.read_bytes() == b"P6\n5 5\n255\n" + bytes([72, 144, 210]) + bytes([32, 91, 35]) * 24
        print(platform + " partial GameState import, wire fixture and rejection checks: PASS", flush=True)
    if args.native:
        assert results["host"] == results["m68k"], results
        print("Host/68060 imported fields and overview bytes: MATCH")
    (work / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    print(json.dumps(results, indent=2))


if __name__ == "__main__":
    main()
