# AMAP version 1

Diagnostic terrain/path export from the partial S6 GameState importer. This
is not a save format and cannot restore a complete OpenRCT2 park. All integer
fields use explicit little-endian encoding; readers must not cast byte arrays
to native structs. Unaligned reads must be supported.

## Header (32 bytes)

| Offset | Type | Meaning |
| --- | --- | --- |
| 0 | 4 bytes | ASCII `AMAP` |
| 4 | u16 | Version, 1 |
| 6 | u16 | Map side, 5..256 |
| 8 | u32 | Cells, exactly side squared |
| 12 | u32 | Number of records, cells..196608 |
| 16 | u32 | Partial-state source fingerprint; not a whole-park checksum |
| 20 | u32 | FNV-1a over all bytes after the header |
| 24 | 8 bytes | Reserved, zero |

The payload begins with cells+1 u32 record offsets, indexed by x+y*side.
Offsets count records, not bytes. The first is zero; each following value is
strictly larger, and the last equals the record count. A cell must have exactly
one surface. Records follow immediately, eight bytes each, in imported tile
order. Trailing data is invalid. FNV-1a starts at 2166136261, XORs each byte,
then multiplies by 16777619 modulo 2^32; it detects accidental corruption,
not malicious edits.

## Records (8 bytes)

| Byte | Surface (type 0) | Path (type 1) |
| --- | --- | --- |
| 0 | 0 | 1 |
| 1 | Base height in world units / 8, 0..254 | Same |
| 2 | Engine ghost/invisible flags, mask 0x30 | Same |
| 3 | Top/right/bottom/left corner rises, 2 bits each | Edges low nibble, corners high nibble |
| 4 | Stable legacy terrain index, 0..31 | Direction bits 0..1; queue bit 2; sloped bit 3; wide bit 4 |
| 5 | Water height in world units / 8, 0..62; zero means none | Legacy path slot, 0..15 |
| 6 | Grass length | Reserved zero |
| 7 | Ownership low four bits | Reserved zero |

Corner rises are 0, 1 or 2, in units of 16 world units, computed by the actual
engine's GetSlopeRelativeCornerHeights. Value 3 is invalid. A path edge bit
uses the engine's legacy direction order; no pointer, native enum layout or
object manager index is serialized. Ghost/invisible records remain in the
file but are skipped by the viewer. Terrain and path slots identify legacy
entries, not loaded object graphics.

The viewer validates the complete payload before using it. A matching checksum
does not bypass its count, offset, enum, flag and reserved-field checks.

## Version 2: saved guest snapshots (viewer 0.9)

Version 2 retains the same header length and terrain/path records. Header byte
4 becomes u16 version 2; bytes 24..27 are u32 guest count (0..10000), and
28..31 remain reserved zero. After all terrain/path records, exactly guest
count 16-byte records follow. The payload checksum includes these records.
Readers accepting only v1 must reject v2 rather than ignore trailing data.
The 0.9 reader accepts both versions.

| Offset within guest record | Type | Meaning |
| --- | --- | --- |
| 0 | u16 | Legacy entity array slot, 0..9999; unique in this file |
| 2 | u16 | Saved world x, less than side * 32 |
| 4 | u16 | Saved world y, less than side * 32 |
| 6 | i16 | Saved world z, 0..2040 for this diagnostic |
| 8 | u8 | Direction, 0..31 |
| 9 | u8 | Legacy PeepState, 0..23 |
| 10 | u8 | Outside-of-park flag, 0 or 1 |
| 11 | u8 | Shirt colour index, 0..31 |
| 12 | u8 | Trousers colour index, 0..31 |
| 13 | u8 | Legacy animation group; retained without interpretation |
| 14 | 2 bytes | Reserved zero |

Records are sorted by `(floor(x/32)+floor(y/32))*256+floor(x/32)`, then by
`x+y`, then by slot. The viewer verifies this order and uniqueness before
using it. It draws markers after each map cell in that same painter order;
this is approximate occlusion, not the full engine paint algorithm.

The importer scans only actual legacy peep records of guest type. It skips
coordinates outside this diagnostic's bounds, unplaced guests (including the
null x sentinel) and invalid direction/state/flag/colour fields, reporting the
skip count. Staff are excluded. These constraints deliberately support a
bounded visual diagnostic, not every legacy state. On-ride state 3 and picked
state 9 remain in the file if placed, but are hidden by the viewer.

Guest data is separate from native EntityRegistry and is not tickable.
The header's partial-state fingerprint remains unchanged and does not include
guests; the payload checksum and file hash cover guest records. The saved
in-park guest counter can differ from the number of exported snapshots.
