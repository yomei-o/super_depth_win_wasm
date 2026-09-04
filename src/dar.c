/* Reading a .dar.  See dar.h and docs/format.md. */
#include "dar.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned word(const unsigned char *p) { return p[0] | (p[1] << 8); }
static unsigned dword(const unsigned char *p)
{
    return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) |
           ((unsigned)p[3] << 24);
}

int dar_load(Dar *d, const char *path)
{
    FILE *f;
    long n;
    int i;
    long at;

    memset(d, 0, sizeof *d);
    f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    d->data = (unsigned char *)malloc((size_t)n);
    if (!d->data || fread(d->data, 1, (size_t)n, f) != (size_t)n) {
        fclose(f);
        free(d->data);
        d->data = NULL;
        return -1;
    }
    fclose(f);
    d->len = n;

    if (n < 0x40c || memcmp(d->data, "DAR:8", 5) != 0) return -2;
    if (d->data[5] > 1) return -3;                       /* FUN_00419700 */
    d->count = (int)word(d->data + 6);

    /* The colour table starts at 0x0c - 0x0c + 256 * 4 = 0x40c, where the
     * patterns begin - and is stored the way a DIB stores one, blue first. */
    for (i = 0; i < 256; i++) {
        d->pal[i][0] = d->data[0x0c + i * 4 + 2];
        d->pal[i][1] = d->data[0x0c + i * 4 + 1];
        d->pal[i][2] = d->data[0x0c + i * 4 + 0];
    }

    at = 0x40c;
    for (i = 0; i < d->count; i++) {
        long head, hlen;
        DarPat *p;

        if (i >= DAR_PATTERNS || at + 8 > n) return -4;
        if (d->data[5]) {
            hlen = (long)word(d->data + at);
            head = at + 2;
        } else {
            hlen = 8;                                    /* version 0 */
            head = at;
        }
        if (head + hlen > n) return -4;
        p = &d->pat[i];
        p->w = (int)word(d->data + head);
        p->h = (int)word(d->data + head + 2);
        p->stride = (int)word(d->data + head + 4);
        p->clear = (short)word(d->data + head + 6);      /* -1 = opaque */
        p->at = head + hlen;
        p->name[0] = 0;
        if (hlen > 15) {
            /* The length byte counts itself, so the text is one shorter. */
            int nl = d->data[head + 14];
            int k, m = nl - 1;
            if (m > DAR_NAME - 1) m = DAR_NAME - 1;
            for (k = 0; k < m; k++) p->name[k] = (char)d->data[head + 15 + k];
            p->name[m > 0 ? m : 0] = 0;
        }
        at = p->at + (long)p->h * p->stride;
    }
    /* The walk has to land exactly on the end of the file.  It does for all
     * six archives, which is what says the layout above is right. */
    if (at != n) return -4;
    return 0;
}

void dar_free(Dar *d)
{
    free(d->data);
    d->data = NULL;
}

int dar_find(const Dar *d, const char *name)
{
    int i;
    for (i = 0; i < d->count; i++)
        if (strcmp(d->pat[i].name, name) == 0) return i;
    return -1;
}

static int draw_rows(const Dar *d, int n, unsigned char *out, int stride,
                     int outW, int outH, int x0, int y0, const short *rowdx)
{
    const DarPat *p;
    int y;

    if (n < 0 || n >= d->count) return -1;
    p = &d->pat[n];
    for (y = 0; y < p->h; y++) {
        /* The rows are stored bottom-up, the way a Windows DIB is: stored row
         * 0 is the picture's last line.  Read them in the other order and
         * every pattern comes out upside down - which is easy to miss on a
         * logo and impossible to miss on the font. */
        const unsigned char *q = d->data + p->at +
                                 (long)(p->h - 1 - y) * p->stride;
        const unsigned char *end = q + p->stride;
        int ty = y0 + y;
        int shift = rowdx ? rowdx[y] : 0;
        int x = 0;

        while (x < p->w && q + 4 <= end) {
            unsigned pair = dword(q);
            int clear = (int)(pair & 0xffff);
            int run = (int)(pair >> 16);
            int i;

            q += 4;
            x += clear;
            if (run == 0) break;                /* a zero run ends the row */
            for (i = 0; i < run; i++, x++) {
                int tx = x0 + shift + x;
                if (x >= p->w) break;
                if (ty < 0 || ty >= outH || tx < 0 || tx >= outW) continue;
                out[(long)ty * stride + tx] = q[i];
            }
            q += (run + 3) & ~3;        /* the pixels are padded to four */
        }
    }
    return 0;
}

int dar_draw(const Dar *d, int n, unsigned char *out, int stride,
             int outW, int outH, int x0, int y0)
{
    return draw_rows(d, n, out, stride, outW, outH, x0, y0, NULL);
}

int dar_draw_solid(const Dar *d, int n, unsigned char *out, int stride,
                   int outW, int outH, int x0, int y0, int colour)
{
    const DarPat *p;
    int y;

    if (n < 0 || n >= d->count) return -1;
    p = &d->pat[n];
    for (y = 0; y < p->h; y++) {
        const unsigned char *q = d->data + p->at +
                                 (long)(p->h - 1 - y) * p->stride;
        const unsigned char *end = q + p->stride;
        int ty = y0 + y;
        int x = 0;

        while (x < p->w && q + 4 <= end) {
            unsigned pair = dword(q);
            int clear = (int)(pair & 0xffff);
            int run = (int)(pair >> 16);
            int i;

            q += 4;
            x += clear;
            if (run == 0) break;
            for (i = 0; i < run; i++, x++) {
                int tx = x0 + x;
                if (x >= p->w) break;
                if (ty < 0 || ty >= outH || tx < 0 || tx >= outW) continue;
                out[(long)ty * stride + tx] = (unsigned char)colour;
            }
            q += (run + 3) & ~3;
        }
    }
    return 0;
}

int dar_draw_scale(const Dar *d, int n, unsigned char *out, int stride,
                   int outW, int outH, int x0, int y0, int sx)
{
    const DarPat *p;
    int y;

    if (n < 0 || n >= d->count) return -1;
    if (sx <= 0) return 0;
    p = &d->pat[n];
    for (y = 0; y < p->h; y++) {
        const unsigned char *q = d->data + p->at +
                                 (long)(p->h - 1 - y) * p->stride;
        const unsigned char *end = q + p->stride;
        int ty = y0 + y;
        int x = 0;
        long at = 0;                    /* where we are in the source, 8.8 */
        int last = -1;                  /* the destination column just done */

        while (x < p->w && q + 4 <= end) {
            unsigned pair = dword(q);
            int clear = (int)(pair & 0xffff);
            int run = (int)(pair >> 16);
            int i;

            q += 4;
            x += clear;
            at += (long)clear * sx;
            if (run == 0) break;
            for (i = 0; i < run; i++, x++, at += sx) {
                int tx = x0 + (int)(at >> 8);
                if (x >= p->w) break;
                if ((int)(at >> 8) == last) continue;
                last = (int)(at >> 8);
                if (ty < 0 || ty >= outH || tx < 0 || tx >= outW) continue;
                out[(long)ty * stride + tx] = q[i];
            }
            q += (run + 3) & ~3;
        }
    }
    return 0;
}

int dar_draw_wave(const Dar *d, int n, unsigned char *out, int stride,
                  int outW, int outH, int x0, int y0, const short *rowdx)
{
    return draw_rows(d, n, out, stride, outW, outH, x0, y0, rowdx);
}
