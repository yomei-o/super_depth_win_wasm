/* Emscripten front end.
 *
 * The game logic is not ported yet, so what this shows is the part that is:
 * the .dar archives, the 640x480 8bpp surface, the original's fonts and its
 * sprites, and the BGM through the synthesiser.  The page owns the clock and
 * the audio callback, exactly as in the author's windepth_wasm.
 *
 *   - the surface is expanded through the archive's palette into an RGBA
 *     buffer and handed to the page, which does one putImageData a frame.
 *     No WebGL: everything rasterises in software.
 *   - the data is baked in with --embed-file, so dar_load() and
 *     mus_load_file() keep using fopen() the way the native tools do.
 *   - the music is synthesised here (src/synth.c) and pulled by the page.
 */
#include <emscripten.h>
#include <stdio.h>
#include <string.h>

#include "dar.h"
#include "synth.h"
#include "video.h"

#define AUDIO_MAX 8192

static Dar g_dar;
static Video g_vid;
static unsigned char g_rgba[SCR_W * SCR_H * 4];
static int g_ready;
static unsigned g_frame;
static int g_scene;                     /* which demo screen is up */

static Music g_mus;
static float g_al[AUDIO_MAX], g_ar[AUDIO_MAX];
static int g_song = 1, g_bgm_on;

static void expand(void)
{
    unsigned *out = (unsigned *)g_rgba;
    const unsigned char *src = &g_vid.px[0][0];
    unsigned lut[256];
    int i, n = SCR_W * SCR_H;

    for (i = 0; i < 256; i++)
        lut[i] = 0xff000000u | ((unsigned)g_dar.pal[i][2] << 16) |
                 ((unsigned)g_dar.pal[i][1] << 8) | g_dar.pal[i][0];
    for (i = 0; i < n; i++) out[i] = lut[src[i]];
}

/* Scene 0: the fonts and a few sprites, the way sd_shot's `text` mode draws
 * them.  Scene 1: a sheet of the 16x16 and 32x32 sprite banks. */
static void draw_scene(void)
{
    char line[64];
    int i;

    vid_clear(&g_vid, 0);
    if (g_scene == 0) {
        vid_text(&g_vid, 0x22, 5, "SUPER DEPTH", FNT_CYAN);
        vid_text(&g_vid, 0x1e, 7, "for Windows", FNT_WHITE);
        vid_text(&g_vid, 0x24, 10, "Ready", FNT_RED);
        vid_text(&g_vid, 0x20, 12, "Stage 3", FNT_YELLOW);
        if ((g_frame & 15) < 8)
            vid_text(&g_vid, 0x1c, 14, "DEMONSTRATION", FNT_GREEN);
        vid_text_at(&g_vid, 10, 0x168, "Score", FNT_WHITE);
        sprintf(line, "%06u", g_frame * 7u % 1000000u);
        vid_text_at(&g_vid, 0x3c, 0x168, line, FNT_GREEN);
        vid_text8(&g_vid, 8, 8, "the port so far: dar + palette + fonts"
                                " + sprites + midi");
        for (i = 0; i < 24; i++)
            vid_pat(&g_vid, 16 + i * 20, 240 + (i & 1 ? 4 : 0), 2433 + i);
        for (i = 0; i < 8; i++)
            vid_pat(&g_vid, 16 + i * 40, 290, 2505 + i);
        vid_pat(&g_vid, 440, 250, 2861);            /* boss1, 128x96 */
    } else {
        int x = 4, y = 4, rowh = 0;
        for (i = 2433; i < g_dar.count; i++) {
            const DarPat *p = &g_dar.pat[i];
            if (p->w > 200 || p->h > 120) continue;
            if (x + p->w + 2 > SCR_W) { x = 4; y += rowh + 2; rowh = 0; }
            if (y + p->h > SCR_H - 4) break;
            vid_pat_raw(&g_vid, x, y, i);
            x += p->w + 2;
            if (p->h > rowh) rowh = p->h;
        }
    }
    g_frame++;
}

EMSCRIPTEN_KEEPALIVE int sd_init(void)
{
    if (dar_load(&g_dar, "/disk/depth.dar") != 0) return -1;
    vid_init(&g_vid, &g_dar);
    mus_init(&g_mus, 44100);
    g_ready = 1;
    draw_scene();
    expand();
    return 0;
}

EMSCRIPTEN_KEEPALIVE int sd_width(void) { return SCR_W; }
EMSCRIPTEN_KEEPALIVE int sd_height(void) { return SCR_H; }
EMSCRIPTEN_KEEPALIVE unsigned char *sd_framebuffer(void) { return g_rgba; }
EMSCRIPTEN_KEEPALIVE int sd_patterns(void) { return g_dar.count; }
EMSCRIPTEN_KEEPALIVE int sd_scene(void) { return g_scene; }
EMSCRIPTEN_KEEPALIVE void sd_set_scene(int n) { g_scene = n ? 1 : 0; }
EMSCRIPTEN_KEEPALIVE int sd_song(void) { return g_song; }

EMSCRIPTEN_KEEPALIVE void sd_tick(void)
{
    if (!g_ready) return;
    draw_scene();
    expand();
}

/* --- music ------------------------------------------------------------- */

static void music_apply(void)
{
    char path[64];

    if (!g_bgm_on) { mus_stop(&g_mus); return; }
    sprintf(path, "/disk/bgm%02d.mid", g_song);
    if (mus_load_file(&g_mus, path) == 0) mus_play(&g_mus, 1);
}

EMSCRIPTEN_KEEPALIVE void sd_set_song(int n)
{
    if (n < 1) n = 1;
    if (n > 15) n = 15;
    g_song = n;
    if (g_bgm_on) music_apply();
}

EMSCRIPTEN_KEEPALIVE void sd_set_bgm(int on)
{
    g_bgm_on = on ? 1 : 0;
    music_apply();
}

EMSCRIPTEN_KEEPALIVE void sd_audio_init(int rate)
{
    mus_free(&g_mus);
    mus_init(&g_mus, rate);
    music_apply();
}

EMSCRIPTEN_KEEPALIVE float *sd_audio_left(void) { return g_al; }
EMSCRIPTEN_KEEPALIVE float *sd_audio_right(void) { return g_ar; }
EMSCRIPTEN_KEEPALIVE int sd_audio_max(void) { return AUDIO_MAX; }

EMSCRIPTEN_KEEPALIVE void sd_audio(int frames)
{
    if (frames > AUDIO_MAX) frames = AUDIO_MAX;
    if (frames < 0) frames = 0;
    mus_render(&g_mus, g_al, g_ar, frames);
}

int main(void)
{
    return 0;               /* the page drives everything through the exports */
}
