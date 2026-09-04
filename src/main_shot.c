/* Draw something out of the game's data and write it as a PNG, so a render
 * can be checked without opening a window.
 *
 *     tmp/sd_shot.exe pat   disk/staff.dar out.png 0     one pattern
 *     tmp/sd_shot.exe sheet disk/depth1.dar out.png      all of them
 *     tmp/sd_shot.exe list  disk/depth.dar               names and sizes
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dar.h"
#include "gfx.h"
#include "png.h"

static Dar dar;
static Gfx gfx;

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
        fprintf(stderr, "usage: sd_shot pat|sheet|list <file.dar> [out.png] [n]\n");
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
    fprintf(stderr, "unknown mode %s\n", argv[1]);
    return 2;
}
