#!/usr/bin/env python3
"""Integration checks against the actual engine, with normal and small builds.

Usage: verify_profiles.py full-bench small-bench data-directory work-directory
Run from the repository root. The work directory must be disposable.
"""
import json
from pathlib import Path
import struct
import subprocess
import sys


def main():
    full, small, data, scratch = sys.argv[1:]
    scratch = Path(scratch).absolute()
    scratch.mkdir(parents=True, exist_ok=True)
    fixtures = Path("test/tests/testdata/parks")
    results = {}
    fields = ("Start tick", "End tick", "Guests", "Rides", "State fingerprint before", "State fingerprint after")
    for name, ticks in (("bpb.sv6", 1000), ("small_park_car_ride_one_car.sv6", 2000)):
        pair = []
        for label, executable in (("standard", full), ("small", small)):
            command = [str(Path(executable).absolute()), str(fixtures / name), str(ticks), data,
                       str(scratch / label), "40"]
            process = subprocess.run(command, capture_output=True, text=True, timeout=120)
            assert process.returncode == 0, process.stdout + process.stderr
            (scratch / f"{name}.{label}.log").write_text(process.stdout + process.stderr)
            values = dict(line.split(": ", 1) for line in process.stdout.splitlines() if ": " in line)
            assert int(values["End tick"]) - int(values["Start tick"]) == ticks + 40
            pair.append(values)
        assert all(pair[0][key] == pair[1][key] for key in fields), (name, pair)
        assert pair[0]["State fingerprint before"] != pair[0]["State fingerprint after"]
        assert int(pair[1]["GameState bytes"]) < int(pair[0]["GameState bytes"]) // 3
        results[name] = {"standard": pair[0], "small": pair[1]}
        print(f"Actual-core tick/state equivalence: PASS {name}")

    # A valid minimal Orca envelope whose map dimensions exceed the profile.
    # Test the real importer instead of only testing the capacity predicate.
    payload = struct.pack("<ii", 257, 257)
    h = ((0xCBF29CE484222325 ^ int.from_bytes(payload, "little")) * 0x100000001B3) & ((1 << 64) - 1)
    header = struct.pack("<IIIIQIQ8s20s", 0x4B524150, 63, 57, 1, len(payload), 0, len(payload),
                         h.to_bytes(8, "little"), bytes(20))
    bad_park = scratch / "map-too-large.park"
    bad_park.write_bytes(header + struct.pack("<IQQ", 0x30, 0, len(payload)) + payload)
    p = subprocess.run([str(Path(small).absolute()), str(bad_park), "1", data, str(scratch / "small"), "0"],
                       capture_output=True, text=True, timeout=120)
    assert p.returncode == 1 and "256-tile map limit" in p.stdout + p.stderr, p.stdout + p.stderr
    assert "Total ms:" not in p.stdout
    print("Oversized .park rejected without benchmark result: PASS")
    for ticks in ("0", "-1", "10000001", "not-an-integer"):
        p = subprocess.run([str(Path(small).absolute()), "unused.sv6", ticks, data, str(scratch / "small")],
                           capture_output=True, text=True, timeout=10)
        assert p.returncode == 2
    print("Invalid tick counts rejected: PASS")
    (scratch / "profile-comparison.json").write_text(json.dumps(results, indent=2) + "\n")


if __name__ == "__main__":
    main()
