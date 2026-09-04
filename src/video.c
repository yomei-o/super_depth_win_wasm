/* The surface.  See video.h for where every number comes from. */
#include "video.h"

#include <string.h>

void vid_init(Video *v, const Dar *dar)
{
    memset(v, 0, sizeof *v);
    v->dar = dar;
}

void vid_clear(Video *v, int colour)
{
    memset(v->px, colour & 0xff, sizeof v->px);
}

void vid_pat_raw(Video *v, int x, int y, int pat)
{
    dar_draw(v->dar, pat, &v->px[0][0], SCR_W, SCR_W, SCR_H, x, y);
}

void vid_pat(Video *v, int x, int y, int pat)
{
    const DarPat *p;

    if (!v->dar || pat < 0 || pat >= v->dar->count) return;
    p = &v->dar->pat[pat];
    /* FUN_00409000's own clipping, kept as it is written there. */
    if (y >= 0x1f1) return;
    if (!(-0x21 < p->w + x && x < 0x261)) return;
    if (y < -p->h) return;
    vid_pat_raw(v, x, y + SCR_YOFF, pat);
}

static void text_px(Video *v, int col, int y, const char *s, int bank)
{
    int x = col * 8;

    for (; *s; s++, x += 0x10, col += 2) {
        if (col < -1 || col >= 0x50) continue;      /* the original's range */
        if (*s == ' ') continue;                    /* spaces draw nothing */
        vid_pat(v, x, y, bank + (unsigned char)*s);
    }
}

void vid_text(Video *v, int col, int row, const char *s, int bank)
{
    text_px(v, col, row << 4, s, bank);
}

void vid_text_at(Video *v, int col, int y, const char *s, int bank)
{
    text_px(v, col, y, s, bank);
}

void vid_text8(Video *v, int x, int y, const char *s)
{
    for (; *s; s++, x += 8) {
        if (*s == ' ') continue;
        vid_pat_raw(v, x, y, FONT8 + ((unsigned char)*s - FONT8_FIRST));
    }
}
