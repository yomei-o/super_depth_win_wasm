/* Draw something out of the game's data and write it as a PNG, so a render
 * can be checked without opening a window.
 *
 *     tmp/sd_shot.exe pat   disk/staff.dar out.png 0     one pattern
 *     tmp/sd_shot.exe sheet disk/depth1.dar out.png      all of them
 *     tmp/sd_shot.exe list  disk/depth.dar               names and sizes
 *     tmp/sd_shot.exe game  disk/depth.dar out.png 30    the game, 30 frames
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dar.h"
#include "gfx.h"
#include "video.h"
#include "game.h"
#include "png.h"

static Dar dar;
static Gfx gfx;

/* What game.c wants from whoever is driving it.  The native tools read the
 * archives straight out of disk/ and have nowhere to put music. */
int plat_dar(Dar *d, const char *name)
{
    char path[96];

    sprintf(path, "disk/%s", name);
    return dar_load(d, path);
}

void plat_bgm(int mode, const char *name)
{
    printf("bgm: mode %d %s\n", mode, name);
}

void plat_se(const char *name)
{
    printf("se: %s\n", name);
}

static int save(const char *path, const Gfx *g, int w, int h)
{
    static unsigned char buf[GFX_H][GFX_W];
    int y;
    for (y = 0; y < h; y++) memcpy(buf[y], g->px[y], (size_t)w);
    /* png_write_indexed returns 1 for success; the callers here want 0. */
    return !png_write_indexed(path, w, h, &buf[0][0], GFX_W, g->pal, 256);
}

int main(int argc, char **argv)
{
    int rc;

    if (argc < 3) {
        fprintf(stderr, "usage: sd_shot pat|sheet|list|text <file.dar> [out.png] [n]\n");
        return 2;
    }
    rc = dar_load(&dar, argv[2]);
    if (rc) { fprintf(stderr, "%s: dar_load %d\n", argv[2], rc); return 1; }
    gfx_palette(&gfx, &dar);

    if (!strcmp(argv[1], "list")) {
        int i;
        printf("%s: %d patterns\n", argv[2], dar.count);
        for (i = 0; i < dar.count; i++) {
            const DarPat *p = &dar.pat[i];
            printf("  %4d  %-16s %4dx%-4d stride %4d clear %4d\n",
                   i, p->name, p->w, p->h, p->stride, p->clear);
        }
        return 0;
    }
    if (argc < 4) { fprintf(stderr, "need an output file\n"); return 2; }

    if (!strcmp(argv[1], "pat")) {
        int n = argc > 4 ? atoi(argv[4]) : 0;
        const DarPat *p;
        if (n < 0 || n >= dar.count) { fprintf(stderr, "no pattern %d\n", n); return 1; }
        p = &dar.pat[n];
        gfx_clear(&gfx, 0);
        gfx_pat(&gfx, &dar, n, 0, 0);
        if (save(argv[3], &gfx, p->w < GFX_W ? p->w : GFX_W,
                 p->h < GFX_H ? p->h : GFX_H)) {
            fprintf(stderr, "cannot write %s\n", argv[3]);
            return 1;
        }
        printf("%s #%d %s %dx%d -> %s\n", argv[2], n, p->name, p->w, p->h, argv[3]);
        return 0;
    }
    if (!strcmp(argv[1], "text")) {
        /* What the game itself prints, in its own fonts and at its own
         * coordinates: FUN_0040a8e0 draws "Ready" at (0x24, 10) with base
         * 0x280 = fntred, FUN_0040a840 "Stage" at (0x20, 10), and
         * FUN_004093d0 the score line at row 0x168 in pixels. */
        static Video vid;
        vid_init(&vid, &dar);
        vid_clear(&vid, 0);
        vid_text(&vid, 0x24, 10, "Ready", FNT_RED);
        vid_text(&vid, 0x20, 12, "Stage 3", FNT_YELLOW);
        vid_text(&vid, 0x1c, 14, "DEMONSTRATION", FNT_CYAN);
        vid_text(&vid, 0x1e, 16, "Game Over", FNT_MAG);
        vid_text_at(&vid, 10, 0x168, "Score", FNT_WHITE);
        vid_text_at(&vid, 0x3c, 0x168, "000000", FNT_GREEN);
        vid_text8(&vid, 8, 8, "the 8x8 font: 0123456789 ABCDEFGHIJKLM");
        vid_text8(&vid, 8, 20, "nopqrstuvwxyz !\"#$%&'()*+,-./:;<=>?@");
        {   /* a row of the 16x16 sprites, and the 128x96 boss */
            int i;
            for (i = 0; i < 24; i++) vid_pat(&vid, 16 + i * 20, 240, 2433 + i);
            for (i = 0; i < 8; i++) vid_pat(&vid, 16 + i * 40, 280, 2505 + i);
            vid_pat(&vid, 460, 260, 2861);
        }
        memcpy(gfx.px, vid.px, sizeof gfx.px);
        if (save(argv[3], &gfx, GFX_W, GFX_H)) {
            fprintf(stderr, "cannot write %s\n", argv[3]);
            return 1;
        }
        printf("%s -> %s (the game's own fonts and sprites)\n", argv[2], argv[3]);
        return 0;
    }
    if (!strcmp(argv[1], "sheet")) {
        int i, x = 0, y = 0, rowH = 0;
        gfx_clear(&gfx, 0);
        for (i = 0; i < dar.count; i++) {
            const DarPat *p = &dar.pat[i];
            if (p->w >= GFX_W || p->h >= GFX_H) continue;
            if (x + p->w + 1 > GFX_W) { x = 0; y += rowH + 1; rowH = 0; }
            if (y + p->h > GFX_H) break;
            gfx_pat(&gfx, &dar, i, x, y);
            x += p->w + 1;
            if (p->h > rowH) rowH = p->h;
        }
        if (save(argv[3], &gfx, GFX_W, GFX_H)) {
            fprintf(stderr, "cannot write %s\n", argv[3]);
            return 1;
        }
        printf("%s -> %s (%d of %d patterns fitted)\n", argv[2], argv[3], i, dar.count);
        return 0;
    }
    if (!strcmp(argv[1], "game")) {
        /* Run the state machine and shoot the frame it lands on, so a screen
         * can be looked at without a browser. */
        static Video vid;
        static Game game;
        int frames = argc > 4 ? atoi(argv[4]) : 1, i;

        vid_init(&vid, &dar);
        game_init(&game, &vid);
        for (i = 0; i < frames; i++) game_tick(&game);
        memcpy(gfx.px, vid.px, sizeof gfx.px);
        memcpy(gfx.pal, vid_palette(&vid), sizeof gfx.pal);
        if (save(argv[3], &gfx, GFX_W, GFX_H)) {
            fprintf(stderr, "cannot write %s\n", argv[3]);
            return 1;
        }
        printf("%d frames -> %s (state %02x, %d frames in it)\n",
               frames, argv[3], game.state, game.sub);
        return 0;
    }
    fprintf(stderr, "unknown mode %s\n", argv[1]);
    return 2;
}
