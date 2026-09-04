#!/bin/sh
# Build everything.  There is no make on this machine, so this is the thing
# that runs rather than a Makefile.
#
#   sh tools/build.sh          native tools and checks
#   sh tools/build.sh check    build, run every check, and shoot the PNGs
set -e
cd "$(dirname "$0")/.."

CC="sh tools/cc.sh -O2 -Wall -Isrc"
CORE="src/dar.c src/gfx.c src/video.c"

mkdir -p tmp

what=${1:-all}

if [ "$what" = all ] || [ "$what" = native ] || [ "$what" = check ]; then
    echo "== native"
    $CC -o tmp/sd_shot.exe   src/main_shot.c src/png.c $CORE
    $CC -o tmp/dar_check.exe tests/dar_check.c $CORE
    $CC -o tmp/unlib.exe     tools/unlib.c tools/blast.c -Itools
fi

if [ "$what" = check ]; then
    echo "== checks"
    ./tmp/dar_check.exe
    echo "== shots"
    ./tmp/sd_shot.exe sheet disk/depth1.dar tmp/sheet_sea.png
    ./tmp/sd_shot.exe pat   disk/staff.dar  tmp/biologo.png 0
    ./tmp/sd_shot.exe pat   disk/staff.dar  tmp/depthlogo.png 1
    ./tmp/sd_shot.exe pat   disk/ending.dar tmp/earth.png 0
    ./tmp/sd_shot.exe text  disk/depth.dar  tmp/text.png
fi

echo "-> tmp/"
