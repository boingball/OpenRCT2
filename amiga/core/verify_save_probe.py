#!/usr/bin/env python3
"""Check real fixtures, an explicit LE scenario envelope and rejected files."""
import argparse
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
    args = parser.parse_args()
    if bool(args.native) != bool(args.emulator):
        parser.error("--native and --emulator must be used together")
    work = ROOT / "build/save-verification"
    work.mkdir(parents=True, exist_ok=True)

    def chunk(data):
        return struct.pack("<BI", 0, len(data)) + data

    def checksummed(data):
        return data + struct.pack("<I", sum(data) & 0xffffffff)

    # This is a synthetic chunk envelope, not a usable park. It verifies
    # scenario-info ordering, the packed-object count and 16-byte entry skip.
    payloads = [struct.pack("<BBHII20x", 1, 0, 1, 120001, 0x31144), bytes(412),
                b"object payload", b"objects", b"time", b"tiles", b"state"]
    scenario = chunk(payloads[0]) + chunk(payloads[1]) + bytes(16)
    scenario += b"".join(chunk(p) for p in payloads[2:])
    (work / "scenario-envelope.sc6").write_bytes(checksummed(scenario))
    expected_hash = 2166136261
    for payload in payloads:
        for byte in payload:
            expected_hash = ((expected_hash ^ byte) * 16777619) & 0xffffffff
    fixtures = [
        ROOT / "test/tests/testdata/parks/bpb.sv6",
        ROOT / "test/tests/testdata/parks/small_park_car_ride_one_car.sv6",
        work / "scenario-envelope.sc6",
    ]
    bad = {
        "bad-checksum.sv6": checksummed(scenario)[:-1] + b"\xff",
        "bad-length.sv6": checksummed(bytes([0, 0, 0, 0, 32, 0])),
        "truncated.sv6": b"abc",
    }
    for name, data in bad.items():
        (work / name).write_bytes(data)

    platforms = [("host", [str(args.host.resolve())])]
    if args.native:
        platforms.append(("m68k", [str(args.emulator.resolve()), "--cpu", "68060", "--no-fpu",
            "--ram", "64M", "--stack", "256K", "-V", f"WORK:{ROOT}",
            "--cwd", "WORK:build/save-verification", str(args.native.resolve())]))
    results = {}
    for label, command in platforms:
        results[label] = {}
        for path in fixtures + [work / name for name in bad]:
            argument = str(path) if label == "host" else "WORK:" + path.relative_to(ROOT).as_posix()
            proc = subprocess.run(command + [argument], cwd=work, text=True,
                                  stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=120)
            (work / f"{label}-{path.name}.log").write_text(proc.stdout)
            if path.name in bad:
                assert proc.returncode == 20 and "FAIL:" in proc.stdout, proc.stdout
                assert "Save/byte-order checks: PASS" not in proc.stdout, proc.stdout
                continue
            assert proc.returncode == 0 and "Save/byte-order checks: PASS" in proc.stdout, proc.stdout
            wire = re.search(r"Wire fingerprint .*: ([0-9a-f]{8})", proc.stdout)
            decoded = re.search(r"Decoded chunks: (\d+); decoded-byte fingerprint: ([0-9a-f]{8})", proc.stdout)
            assert wire and wire[1] == "b9cdc2a8" and decoded, proc.stdout
            results[label][path.name] = {"chunks": int(decoded[1]), "fingerprint": decoded[2]}
        assert results[label]["scenario-envelope.sc6"] == {
            "chunks": 7, "fingerprint": f"{expected_hash:08x}"}
        assert results[label]["bpb.sv6"] == {"chunks": 5, "fingerprint": "08000478"}
        assert results[label]["small_park_car_ride_one_car.sv6"] == {
            "chunks": 5, "fingerprint": "8e1dcc1d"}
    if args.native:
        assert results["host"] == results["m68k"], results
    (work / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    print("Save decoding fixtures and corrupt-file rejection: PASS")
    if args.native:
        print("Little-endian host vs big-endian 68060 fingerprints: MATCH")
    print(json.dumps(results, indent=2))


if __name__ == "__main__":
    main()
