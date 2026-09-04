/* The surface.  See video.h for where every number comes from. */
#include "video.h"

#include <string.h>

void vid_init(Video *v, const Dar *dar)
{
    memset(v, 0, sizeof *v);
    v->dar = dar;
}

void vid_scene(Video *v, const Dar *ext)
{
    v->ext = ext;
}

const unsigned char (*vid_palette(const Video *v))[3]
{
    /* FUN_004178e0 is handed pattern 0xb48, which belongs to the scene
     * archive, so that is where the screen palette comes from. */
    if (v->ext) return v->ext->pal;
    return v->dar->pal;
}

void vid_clear(Video *v, int colour)
{
    memset(v->px, colour & 0xff, sizeof v->px);
}

void vid_fill(Video *v, int left, int top, int right, int bottom, int colour)
{
    int y;

    /* FUN_004183b0 starts with IntersectRect against the surface. */
    if (left < 0) left = 0;
    if (top < 0) top = 0;
    if (right > SCR_W) right = SCR_W;
    if (bottom > SCR_H) bottom = SCR_H;
    for (y = top; y < bottom; y++)
        if (right > left) memset(&v->px[y][left], colour & 0xff,
                                 (size_t)(right - left));
}

/* Which archive owns a slot.  `local` comes back as the index inside it. */
static const Dar *owner(const Video *v, int pat, int *local)
{
    if (pat < 0) return 0;
    if (pat >= EXT_BASE) {
        *local = pat - EXT_BASE;
        if (!v->ext || *local >= v->ext->count) return 0;
        return v->ext;
    }
    *local = pat;
    if (!v->dar || pat >= v->dar->count) return 0;
    return v->dar;
}

const DarPat *vid_pat_info(const Video *v, int pat)
{
    int n;
    const Dar *d = owner(v, pat, &n);

    return d ? &d->pat[n] : 0;
}

void vid_pat_raw(Video *v, int x, int y, int pat)
{
    int n;
    const Dar *d = owner(v, pat, &n);

    if (d) dar_draw(d, n, &v->px[0][0], SCR_W, SCR_W, SCR_H, x, y);
}

void vid_pat(Video *v, int x, int y, int pat)
{
    const DarPat *p = vid_pat_info(v, pat);

    if (!p) return;
    /* FUN_00409000's own clipping, kept as it is written there. */
    if (y >= 0x1f1) return;
    if (!(-0x21 < p->w + x && x < 0x261)) return;
    if (y < -p->h) return;
    vid_pat_raw(v, x, y + SCR_YOFF, pat);
}

void vid_pat_centre(Video *v, int cx, int cy, int pat)
{
    const DarPat *p = vid_pat_info(v, pat);

    if (!p) return;
    vid_pat_raw(v, cx - p->w / 2, cy - p->h / 2, pat);
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

void vid_text8_at(Video *v, int col, int row, const char *s)
{
    vid_text8(v, col * 8, row * 8, s);
}
