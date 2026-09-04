#!/bin/sh
# Build the WASM front end with emscripten.
#
# Output lands in the repository root because GitHub Pages serves main:/ - the
# page, the module and the data all sit side by side there.
#
# depth.dar and the .mid files are baked in with --embed-file so dar_load()
# and mus_load_file() keep using fopen() exactly as the native tools do.  The
# MIDI is synthesised inside the module by src/synth.c, since a browser has no
# MIDI output and no soundfont we may redistribute.
set -e
cd "$(dirname "$0")/.."
EMSDK="${EMSDK:-/c/prog/emsdk/emsdk}"
EMCC="$EMSDK/upstream/emscripten/emcc.exe"
[ -f "$EMCC" ] || { echo "emcc not found at $EMCC" >&2; exit 1; }

EXPORTS=_main,_sd_init,_sd_tick,_sd_width,_sd_height,_sd_framebuffer
EXPORTS=$EXPORTS,_sd_patterns,_sd_view,_sd_set_view,_sd_song,_sd_set_song
EXPORTS=$EXPORTS,_sd_set_bgm,_sd_audio_init,_sd_audio,_sd_audio_left
EXPORTS=$EXPORTS,_sd_audio_right,_sd_audio_max,_sd_set_pad,_sd_debug,_sd_set_clock,_sd_music_on,_sd_se_on,_sd_stereo,_sd_demo_ptr,_sd_demo_len,_sd_demo_stamp,_sd_state
EXPORTS=$EXPORTS,_sd_fps,_sd_se_take,_sd_se_pan
EXPORTS=$EXPORTS,_sd_rank_ptr,_sd_rank_len,_sd_rank_stamp,_sd_surface

EMBED="--embed-file disk/depth.dar@/disk/depth.dar"
EMBED="$EMBED --embed-file disk/demo1.dat@/disk/demo1.dat"
# the space stage reads this every time it starts; without it
# FUN_0040f970 gives up and drops back to the title
EMBED="$EMBED --embed-file disk/stage3.bin@/disk/stage3.bin"
for n in staff depth1 depth2 space ending; do
    EMBED="$EMBED --embed-file disk/$n.dar@/disk/$n.dar"
done
for n in 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15; do
    EMBED="$EMBED --embed-file disk/bgm$n.mid@/disk/bgm$n.mid"
done
EMBED="$EMBED --embed-file disk/finst1.mid@/disk/finst1.mid"   # the staff roll

"$EMCC" -O2 -Wall -Wextra \
   -o superdepth.js \
   src/main_wasm.c src/game.c src/play.c src/air.c src/space.c \
   src/boss.c src/ending.c \
   src/video.c src/dar.c \
   src/smf.c src/synth.c \
   $EMBED \
   -s MODULARIZE=0 -s EXPORTED_RUNTIME_METHODS=HEAPU8,HEAPF32,UTF8ToString \
   -s ALLOW_MEMORY_GROWTH=1 -s ENVIRONMENT=web,node \
   -s EXPORTED_FUNCTIONS="$EXPORTS"
echo "built superdepth.js + superdepth.wasm"
