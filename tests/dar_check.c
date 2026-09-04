/* Everything that must hold about the .dar archives.
 *
 *     tmp/dar_check.exe
 *
 * Run from the repo root; it reads disk/*.dar.
 *
 * The point of these is that the format is being read the way the game reads
 * it, not just plausibly:
 *
 *   * walking the patterns lands exactly on the end of every file - that is
 *     dar_load's own -4, and it is the check that the header sizes and the
 *     `height * stride` step are right
 *   * every row's runs add up to the pattern's width.  Miss the four-byte
 *     padding on the pixels and this fails on the first run whose length is
 *     not a multiple of four
 *   * the patterns whose names the game looks up are there, at the sizes the
 *     game asks for
 */
#include <stdio.h>
#include <string.h>

#include "dar.h"

static int fails;

static void ok(int cond, const char *what)
{
    if (!cond) { printf("FAIL %s\n", what); fails++; }
}

static void okn(int cond, const char *what, const char *who)
{
    if (!cond) { printf("FAIL %s (%s)\n", what, who); fails++; }
}

static const char *FILES[] = {
    "disk/depth.dar", "disk/depth1.dar", "disk/depth2.dar",
    "disk/space.dar", "disk/ending.dar", "disk/staff.dar"
};
static const int COUNTS[] = { 2887, 9, 9, 50, 2, 291 };

/* Do the runs of every row cover the width exactly? */
static int rows_add_up(const Dar *d, int n, int *badRow)
{
    const DarPat *p = &d->pat[n];
    int y;

    for (y = 0; y < p->h; y++) {
        const unsigned char *q = d->data + p->at + (long)y * p->stride;
        const unsigned char *end = q + p->stride;
        int x = 0;

        while (x < p->w && q + 4 <= end) {
            unsigned pair = (unsigned)q[0] | ((unsigned)q[1] << 8) |
                            ((unsigned)q[2] << 16) | ((unsigned)q[3] << 24);
            int run = (int)(pair >> 16);
            x += (int)(pair & 0xffff) + run;
            q += 4 + ((run + 3) & ~3);
        }
        if (x != p->w) { *badRow = y; return 0; }
    }
    return 1;
}

int main(void)
{
    static Dar d;
    int f, i, n;

    for (f = 0; f < (int)(sizeof FILES / sizeof *FILES); f++) {
        int rc = dar_load(&d, FILES[f]);
        int bad = 0, worst = -1;

        if (rc) {
            printf("FAIL %s: dar_load %d%s\n", FILES[f], rc,
                   rc == -4 ? " (the walk missed the end of the file)" : "");
            fails++;
            continue;
        }
        okn(d.count == COUNTS[f], "the pattern count is the one in the header",
            FILES[f]);

        for (i = 0; i < d.count; i++) {
            int row = -1;
            if (!rows_add_up(&d, i, &row)) {
                if (bad == 0) worst = i;
                bad++;
            }
            okn(d.pat[i].w > 0 && d.pat[i].h > 0, "the pattern has a size",
                FILES[f]);
            okn(d.pat[i].stride >= 4, "and room for one run", FILES[f]);
        }
        if (bad) {
            printf("FAIL %s: %d of %d patterns have rows that do not add up "
                   "(first #%d)\n", FILES[f], bad, d.count, worst);
            fails++;
        }
        printf("  %-18s %4d patterns, rows add up, walk exact\n",
               FILES[f], d.count);
        dar_free(&d);
    }

    /* The names the game asks for by hand.  `depth1.dar` is the sea. */
    if (dar_load(&d, "disk/depth1.dar") == 0) {
        n = dar_find(&d, "sea01");
        ok(n == 0, "sea01 is the first pattern of depth1.dar");
        if (n >= 0)
            ok(d.pat[n].w == 64 && d.pat[n].h == 64, "and it is 64x64");
        dar_free(&d);
    }
    if (dar_load(&d, "disk/staff.dar") == 0) {
        n = dar_find(&d, "biologo_staff");
        ok(n == 0, "biologo_staff is the first pattern of staff.dar");
        if (n >= 0)
            ok(d.pat[n].w == 300 && d.pat[n].h == 184, "and it is 300x184");
        n = dar_find(&d, "kgfwhite");
        ok(n >= 0, "kgfwhite is in staff.dar");
        if (n >= 0)
            ok(d.pat[n].stride == 4, "and it is one empty run a row");
        dar_free(&d);
    }
    if (dar_load(&d, "disk/depth.dar") == 0) {
        n = dar_find(&d, "sys16");
        ok(n == 0, "sys16 is the first pattern of depth.dar");
        if (n >= 0)
            ok(d.pat[n].clear == 254, "and 254 is its transparent colour");
        dar_free(&d);
    }

    if (fails) printf("%d checks failed\n", fails);
    else printf("all dar checks passed\n");
    return fails ? 1 : 0;
}
