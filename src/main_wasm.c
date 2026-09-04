/* Emscripten front end.
 *
 * The page owns the clock, the keyboard and the audio, exactly as in the
 * author's windepth_wasm; everything else is the game (src/game.c) drawing
 * into the 640x480 8bpp surface (src/video.c) out of the .dar archives.
 *
 *   - the surface is expanded through the palette into an RGBA buffer and
 *     handed to the page, which does one putImageData a frame.  No WebGL:
 *     everything rasterises in software.
 *   - the data is baked in with --embed-file, so dar_load() and
 *     mus_load_file() keep using fopen() the way the native tools do.
 *   - the music is synthesised here (src/synth.c) and pulled by the page.
 *
 * Two extra views are kept alongside the game for looking at the material
 * that has been decoded - the fonts and sprites, and a sheet of every
 * pattern.  They are the port's own, not something the original has.
 */
#include <emscripten.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "dar.h"
#include "game.h"
#include "synth.h"
#include "video.h"

#define AUDIO_MAX 8192

static Dar g_dar;
static Video g_vid;
static Game g_game;
static unsigned char g_rgba[SCR_W * SCR_H * 4];
static int g_ready;
static unsigned g_frame;
static int g_view;                      /* 0 the game, 1 fonts, 2 sheet */

static Music g_mus;
static float g_al[AUDIO_MAX], g_ar[AUDIO_MAX];
static int g_song = 1, g_bgm_on = 1;
static char g_bgm[64];                  /* what the game last asked for */
static int g_bgm_mode;

static void music_apply(void);

/* ---- what the game asks of the front end ------------------------------ */

int plat_dar(Dar *d, const char *name)
{
    char path[96];

    sprintf(path, "/disk/%s", name);
    return dar_load(d, path);
}

/* FUN_00420980(mode, name): 0..3 start a song, 4 stops.  Modes 2 and 3 leave
 * the same song playing rather than restarting it (FUN_00420090's first
 * test).  Whether a mode also means "loop" is not read out of SMFDrv yet;
 * the call sites say it must - bgm01 is a 3.8 second logo jingle started
 * with 0, bgm02 the 43 second title music started with 1 - so 0 plays once
 * and the rest loop.  That is a reading of the call sites, not the driver. */
void plat_bgm(int mode, const char *name)
{
    if (mode == 4) {
        g_bgm[0] = 0;
        mus_stop(&g_mus);
        return;
    }
    if ((mode == 2 || mode == 3) && !strcmp(g_bgm, name)) return;
    strncpy(g_bgm, name, sizeof g_bgm - 1);
    g_bgm[sizeof g_bgm - 1] = 0;
    g_bgm_mode = mode;
    music_apply();
}

/* FUN_0041fd00(name): play one of the sound effects, which are plain RIFF
 * WAVs sitting next to the page in disk/.  The module has no way to make a
 * sound of its own, so the name is left here and the page picks it up. */
static char g_se[64];
static int g_se_pan;

void plat_se(const char *name, int pan)
{
    g_se_pan = pan;
    strncpy(g_se, name, sizeof g_se - 1);
    g_se[sizeof g_se - 1] = 0;
}

EMSCRIPTEN_KEEPALIVE int sd_se_pan(void) { return g_se_pan; }

int plat_read(const char *name, unsigned char *buf, int max)
{
    char path[96];
    FILE *f;
    int n;

    sprintf(path, "/disk/%s", name);
    f = fopen(path, "rb");
    if (!f) return -1;
    n = (int)fread(buf, 1, (size_t)max, f);
    fclose(f);
    return n;
}

EMSCRIPTEN_KEEPALIVE const char *sd_se_take(void)
{
    static char out[64];

    if (!g_se[0]) return 0;
    strcpy(out, g_se);
    g_se[0] = 0;
    return out;
}

/* ---- the surface ------------------------------------------------------ */

static void expand(void)
{
    unsigned *out = (unsigned *)g_rgba;
    const unsigned char *src = &g_vid.px[0][0];
    const unsigned char (*pal)[3] = vid_palette(&g_vid);
    unsigned lut[256];
    int i, n = SCR_W * SCR_H;

    for (i = 0; i < 256; i++)
        lut[i] = 0xff000000u | ((unsigned)pal[i][2] << 16) |
                 ((unsigned)pal[i][1] << 8) | pal[i][0];
    for (i = 0; i < n; i++) out[i] = lut[src[i]];
}

/* View 1: the fonts and a few sprites.  View 2: a sheet of the sprite banks.
 * Neither is in the original; they are here to show what has been decoded. */
static void draw_view(void)
{
    char line[64];
    int i;

    vid_clear(&g_vid, 0);
    if (g_view == 1) {
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
    game_init(&g_game, &g_vid);
    mus_init(&g_mus, 44100);
    g_ready = 1;
    /* No frame is run here: the original does nothing until its first timer
     * tick either, so the page gets a black screen for one frame. */
    expand();
    return 0;
}

EMSCRIPTEN_KEEPALIVE int sd_width(void) { return SCR_W; }
EMSCRIPTEN_KEEPALIVE int sd_height(void) { return SCR_H; }
EMSCRIPTEN_KEEPALIVE unsigned char *sd_framebuffer(void) { return g_rgba; }

/* The 8bpp surface itself, so a frame here can be compared byte for byte
 * with the one the native tools draw. */
EMSCRIPTEN_KEEPALIVE unsigned char *sd_surface(void) { return &g_vid.px[0][0]; }
EMSCRIPTEN_KEEPALIVE int sd_patterns(void) { return g_dar.count; }
EMSCRIPTEN_KEEPALIVE int sd_view(void) { return g_view; }
EMSCRIPTEN_KEEPALIVE int sd_state(void) { return g_game.state; }
EMSCRIPTEN_KEEPALIVE int sd_fps(void) { return g_game.fps; }
EMSCRIPTEN_KEEPALIVE void sd_set_pad(int pad) { game_set_pad(&g_game, (unsigned)pad); }

/* The score table, so the page can keep it: ten records of 40 bytes, laid
 * out exactly as the original writes them into the registry. */
EMSCRIPTEN_KEEPALIVE unsigned char *sd_rank_ptr(void)
{
    return (unsigned char *)g_game.rank;
}

EMSCRIPTEN_KEEPALIVE int sd_rank_len(void) { return (int)sizeof g_game.rank; }
EMSCRIPTEN_KEEPALIVE int sd_rank_stamp(void) { return g_game.rank_stamp; }

EMSCRIPTEN_KEEPALIVE void sd_set_view(int n)
{
    g_view = n < 0 ? 0 : (n > 2 ? 2 : n);
    if (g_view == 0) music_apply();     /* back to whatever the game wants */
}

EMSCRIPTEN_KEEPALIVE void sd_tick(void)
{
    if (!g_ready) return;
    if (g_view == 0) {
        time_t t = time(NULL);
        struct tm *lt = localtime(&t);
        /* GetLocalTime: the second drives the FPS counter and the date
         * is what a high score is stamped with. */
        game_set_second(&g_game, lt ? lt->tm_sec : 0);
        if (lt)
            game_set_date(&g_game, lt->tm_year + 1900, lt->tm_mon + 1,
                          lt->tm_mday);
        game_tick(&g_game);
    } else {
        draw_view();
    }
    expand();
}

/* ---- music ------------------------------------------------------------ */

static void music_apply(void)
{
    char path[96];

    mus_stop(&g_mus);
    if (!g_bgm_on) return;
    if (g_view == 0) {
        if (!g_bgm[0]) return;
        sprintf(path, "/disk/%s.mid", g_bgm);
        if (mus_load_file(&g_mus, path) == 0)
            mus_play(&g_mus, g_bgm_mode == 0 ? 0 : 1);
        return;
    }
    sprintf(path, "/disk/bgm%02d.mid", g_song);
    if (mus_load_file(&g_mus, path) == 0) mus_play(&g_mus, 1);
}

EMSCRIPTEN_KEEPALIVE int sd_song(void) { return g_song; }

EMSCRIPTEN_KEEPALIVE void sd_set_song(int n)
{
    if (n < 1) n = 1;
    if (n > 15) n = 15;
    g_song = n;
    music_apply();
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
