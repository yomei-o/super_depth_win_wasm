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

/* The sine at 0x442178: 256 signed bytes, a quarter-wave repeated and
 * negated, and slightly flat at the top - so it is written out as the
 * original has it rather than computed. */
static const signed char VID_SINE[256] = {
    0, 3, 6, 9, 12, 15, 18, 21, 24, 28, 31, 34, 37, 40, 43, 46,
    48, 51, 54, 57, 60, 63, 65, 68, 71, 73, 76, 78, 81, 83, 85, 88,
    90, 92, 94, 96, 98, 100, 102, 104, 106, 108, 109, 111, 112, 114, 115, 117,
    118, 119, 120, 121, 122, 123, 124, 124, 125, 126, 126, 127, 127, 127, 127, 127,
    127, 127, 127, 127, 127, 127, 126, 126, 125, 124, 124, 123, 122, 121, 120, 119,
    118, 117, 115, 114, 112, 111, 109, 108, 106, 104, 102, 100, 98, 96, 94, 92,
    90, 88, 85, 83, 81, 78, 76, 73, 71, 68, 65, 63, 60, 57, 54, 51,
    48, 46, 43, 40, 37, 34, 31, 28, 24, 21, 18, 15, 12, 9, 6, 3,
    0, -3, -6, -9, -12, -15, -18, -21, -24, -28, -31, -34, -37, -40, -43, -46,
    -48, -51, -54, -57, -60, -63, -65, -68, -71, -73, -76, -78, -81, -83, -85, -88,
    -90, -92, -94, -96, -98, -100, -102, -104, -106, -108, -109, -111, -112, -114, -115, -117,
    -118, -119, -120, -121, -122, -123, -124, -124, -125, -126, -126, -127, -127, -127, -127, -127,
    -127, -127, -127, -127, -127, -127, -126, -126, -125, -124, -124, -123, -122, -121, -120, -119,
    -118, -117, -115, -114, -112, -111, -109, -108, -106, -104, -102, -100, -98, -96, -94, -92,
    -90, -88, -85, -83, -81, -78, -76, -73, -71, -68, -65, -63, -60, -57, -54, -51,
    -48, -46, -43, -40, -37, -34, -31, -28, -24, -21, -18, -15, -12, -9, -6, -3,
};

void vid_pat_wave(Video *v, int x, int y, int pat, int wave, int amp,
                  int phase)
{
    const DarPat *p = vid_pat_info(v, pat);
    static short dx[SCR_H * 2];
    int flip, r, clip, a, n;
    const Dar *d;

    if (!p) return;
    /* FUN_004092a0's clipping, which is FUN_00409000's. */
    if (y >= 0x1f1) return;
    if (!(-0x21 < p->w + x && x < 0x261)) return;
    if (y < -p->h) return;

    flip = wave < 0;
    if (flip) wave = -wave;
    if (wave == 0) return;

    /* The original counts its rows from the first one that is not
     * clipped off the top, so the wave starts there too. */
    clip = y + SCR_YOFF < 0 ? -(y + SCR_YOFF) : 0;
    a = amp;
    for (r = 0; r < p->h && r < (int)(sizeof dx / sizeof *dx); r++) {
        if (r < clip) { dx[r] = 0; continue; }
        n = ((r - clip) * 0x200) / wave;
        dx[r] = (short)((VID_SINE[(phase + n) & 0xff] * a) >> 7);
        if (flip) a = -a;
    }
    d = pat >= EXT_BASE ? v->ext : v->dar;
    if (!d) return;
    dar_draw_wave(d, pat >= EXT_BASE ? pat - EXT_BASE : pat,
                  &v->px[0][0], SCR_W, SCR_W, SCR_H, x, y + SCR_YOFF, dx);
}

void vid_pat_centre(Video *v, int cx, int cy, int pat)
{
    const DarPat *p = vid_pat_info(v, pat);

    if (!p) return;
    vid_pat_raw(v, cx - p->w / 2, cy - p->h / 2, pat);
}

void vid_pat_flash(Video *v, int x, int y, int pat)
{
    const DarPat *p = vid_pat_info(v, pat);
    const Dar *d;

    if (!p) return;
    if (y >= 0x1f1) return;
    if (!(-0x21 < p->w + x && x < 0x261)) return;
    if (y < -p->h) return;
    d = pat >= EXT_BASE ? v->ext : v->dar;
    if (!d) return;
    dar_draw_solid(d, pat >= EXT_BASE ? pat - EXT_BASE : pat, &v->px[0][0],
                   SCR_W, SCR_W, SCR_H, x, y + SCR_YOFF, 0xff);
}

void vid_pat_scale(Video *v, int x, int y, int pat, int sx, int sy)
{
    const DarPat *p = vid_pat_info(v, pat);
    const Dar *d;
    int wide, tall;

    if (!p) return;
    if (y >= 0x1f1) return;
    if (!(-0x21 < p->w + x && x < 0x261)) return;
    if (y < -p->h) return;
    /* FUN_00409120 keeps the middle where it was: each edge moves in by half
     * of what that side lost (or out by half of what it gained). */
    wide = (p->w * sx) >> 8;
    tall = (p->h * sy) >> 8;
    x += p->w / 2 - wide / 2;
    y += p->h / 2 - tall / 2;
    d = pat >= EXT_BASE ? v->ext : v->dar;
    if (!d) return;
    dar_draw_scale(d, pat >= EXT_BASE ? pat - EXT_BASE : pat, &v->px[0][0],
                   SCR_W, SCR_W, SCR_H, x, y + SCR_YOFF, sx, sy);
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

/* FUN_00402b90: the same as vid_text, except the column arrives in pixels
 * and the original leaves off the range check there. */
void vid_text_px(Video *v, int x, int row, const char *s, int bank)
{
    for (; *s; s++, x += 0x10) {
        if (*s == ' ') continue;
        vid_pat(v, x, row << 4, bank + (unsigned char)*s);
    }
}

/* FUN_00414140: the staff roll's text, centred on 0x140 with the row in
 * pixels.  staff.dar's three fonts hold 96 glyphs each starting at the
 * space, so the glyph is `bank + char - 0x20` - not depth.dar's
 * `bank + char` - and the letter box is the bank pattern's own size. */
void vid_text_centre(Video *v, int y, const char *s, int bank)
{
    const DarPat *p = vid_pat_info(v, bank);
    int n = (int)strlen(s), i, x;

    if (!p) return;
    x = 0x140 - (n * p->w >> 1);
    for (i = 0; i < n; i++) {
        if (s[i] == ' ') continue;
        vid_pat(v, x + p->w * i, y, bank + (unsigned char)s[i] - 0x20);
    }
}

/* FUN_00408a40: FUN_0041b6f0 with nothing in front of it, so the pattern
 * grows out of its top-left corner - FUN_00409120 (vid_pat_scale) keeps the
 * middle still instead - and none of FUN_00409000's clipping runs. */
void vid_pat_scale_at(Video *v, int x, int y, int pat, int sx, int sy)
{
    const Dar *d = pat >= EXT_BASE ? v->ext : v->dar;

    if (!d) return;
    dar_draw_scale(d, pat >= EXT_BASE ? pat - EXT_BASE : pat, &v->px[0][0],
                   SCR_W, SCR_W, SCR_H, x, y + SCR_YOFF, sx, sy);
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
