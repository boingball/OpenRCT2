# Source acknowledgements

The G1 sprite definitions and RLE rendering approach are adapted from
OpenRCT2, copyright 2014–2026 OpenRCT2 developers:

- `src/openrct2/drawing/G1Element.h`
- `src/openrct2/drawing/Drawing.Sprite.RLE.cpp`
- `src/openrct2/drawing/Drawing.Sprite.BMP.cpp`
- `src/openrct2/drawing/Palette.cpp`
- `src/openrct2/SpriteIds.h`

The original grass and visitor sprite indices correspond to OpenRCT2/objects'
`rct2.terrain_surface.grass` and `rct2.peep_animations.guest` definitions.

Amiga startup, generic library-call glue, assembler definitions, linker script,
ELF-to-HUNK converter and the 68060 instruction check come from the user's
existing Ami3D/MinTee Golf projects. The RTG backend follows the working
MinTee Golf Intuition/graphics.library approach. This directory is distributed
under GPL version 3, as part of this OpenRCT2 fork.

The actual OpenRCT2 simulation is not compiled into this runner. Generated
diagnostic shapes and paths are new test graphics. Original RCT2 graphics are
loaded only from a file supplied by the user; none are bundled.

The optional 0.11 guest sprite decoder adapts the primary/secondary palette
mapping conventions from OpenRCT2 Drawing.Sprite.cpp and Paint.Guest.cpp.
The 32 legacy clothing ramps in guest_palette.h are generated from OpenRCT2's
resources/palettes/map_base PNGs (OpenRCT2 developers, 2014-2026; GPL-3.0).
Synthetic guest test fixtures contain generated test pixels, not original
RCT2 guest artwork. Users must supply their own original G1.DAT.
