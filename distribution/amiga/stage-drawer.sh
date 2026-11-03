#!/bin/sh
# Assemble a complete OpenRCT2 drawer for the Amiga from a cross build and a host data install.
#
#   usage: stage-drawer.sh <amiga-build-dir> <host-data-dir> <output-dir>
#
#   <amiga-build-dir>  the cross build (holds openrct2 and OpenRCT2Check)
#   <host-data-dir>    OpenRCT2's data folder from a native build of the SAME source tree:
#                      g2.dat, fonts.dat, palettes.dat, tracks.dat, language/, object/, sequence/, ...
#                      (DESTDIR=<dir> cmake --install <host-build> puts it in <dir>/usr/share/openrct2)
#   <output-dir>       receives OpenRCT2.info (drawer icon) and the OpenRCT2/ drawer
#
# Copy <output-dir>/OpenRCT2 and <output-dir>/OpenRCT2.info to the Amiga side by side. The original
# RCT2 data is not included; assign RCT2: to it (see README.txt).
set -e
BUILD="${1:?amiga build dir required}"; DATA="${2:?host data dir required}"; OUT="${3:?output dir required}"
HERE="$(cd "$(dirname "$0")" && pwd)"
for f in "$BUILD/openrct2" "$BUILD/OpenRCT2Check" "$DATA/g2.dat" "$DATA/fonts.dat" "$DATA/palettes.dat" "$DATA/tracks.dat"; do
    [ -f "$f" ] || { echo "missing $f"; exit 1; }
done
[ -d "$DATA/language" ] || { echo "missing $DATA/language (is this the share/openrct2 folder?)"; exit 1; }
[ -d "$DATA/object" ] || echo "warning: $DATA/object missing; configure the host build with DOWNLOAD_OBJECTS=ON"

D="$OUT/OpenRCT2"
rm -rf "$D"; mkdir -p "$D/bin" "$D/user"
cp "$BUILD/openrct2" "$BUILD/OpenRCT2Check" "$D/bin/"
cp -R "$DATA" "$D/data"
# Launch scripts with their icons; OpenRCT2-launcher.info is the icon of the main OpenRCT2 script.
for s in OpenRCT2 OpenRCT2-AGA OpenRCT2-AGA-Paula OpenRCT2-Paula OpenRCT2-A1200 OpenRCT2-A1200-Offline \
         OpenRCT2-Experimental OpenRCT2-Experimental-AGA TraceStartup TraceStartup-NoAudio; do
    cp "$HERE/$s" "$D/"
    [ -f "$HERE/$s.info" ] && cp "$HERE/$s.info" "$D/"
done
cp "$HERE/OpenRCT2-launcher.info" "$D/OpenRCT2.info"
cp "$HERE/OpenRCT2-drawer.info" "$OUT/OpenRCT2.info"
cp "$HERE/README.txt" "$HERE/README.txt.info" "$D/"
cp "$HERE/user/config.ini" "$D/user/config.ini"
# The Amiga title sequence: a zip holding script.txt, selected in config.ini by its name "Amiga".
mkdir -p "$D/data/sequence"
rm -f "$D/data/sequence/Amiga.parkseq"
(cd "$HERE/title-sequence" && zip -q -X "$D/data/sequence/Amiga.parkseq" script.txt)
printf 'OpenRCT2 AmigaOS/68k build (git %s), staged %s\n' \
    "$(git -C "$HERE" log -1 --format=%h 2>/dev/null || echo unknown)" "$(date -u +%Y-%m-%dT%H:%MZ)" > "$D/VERSION"
echo "staged $D ($(du -sh "$D" | cut -f1))"
echo "the experimental launchers (OpenRCT2-A1200, OpenRCT2-Experimental*) need bin/openrct2-experimental from a threaded build"
