/* .dar - Bio_100%'s "pattern" archive, as superdepth.exe reads it.
 *
 * The whole format is in docs/format.md; the short version, from
 * FUN_004199c0 (the header) and FUN_00419700 (`PatEntryDAR`):
 *
 *     "DAR:8" | byte version | word patterns | dword 4 | 4 zero bytes
 *     RGBQUAD palette[256]                   1024 bytes, blue first
 *     patterns, from offset 0x40c
 *
 * and a pattern is a small header - width, height, the bytes a row, the
 * transparent colour, a name - followed by one run list a row.  A run is
 *
 *     dword  the transparent count in the low word,
 *            the opaque count in the high word
 *     bytes  the opaque pixels, padded to a multiple of four
 *
 * which is what FUN_0041a590 writes when it builds a run list out of a
 * bitmap.  Everything here is read straight out of the file: nothing is
 * decompressed, so a Dar is just the bytes plus an index.
 */
#ifndef SD_DAR_H
#define SD_DAR_H

#define DAR_PATTERNS 4096               /* 0xb48 = 2888 slots in the game */
#define DAR_NAME 32

typedef struct {
    int w, h;                           /* the pattern's size in pixels */
    int stride;                         /* bytes a row in the file */
    int clear;                          /* the transparent colour, or -1 */
    long at;                            /* where its runs start in the file */
    char name[DAR_NAME];
} DarPat;

typedef struct {
    unsigned char *data;                /* the file, owned */
    long len;
    int count;
    unsigned char pal[256][3];          /* r, g, b */
    DarPat pat[DAR_PATTERNS];
} Dar;

/* Read a .dar.  Returns 0, or -1 if the file is missing, -2 if it does not
 * start with "DAR:8", -3 if the version is over 1 (FUN_00419700 refuses
 * those too), -4 if walking the patterns does not land exactly on the end of
 * the file - which is the check that the format is being read right. */
int dar_load(Dar *d, const char *path);
void dar_free(Dar *d);

/* One pattern into a caller's buffer, `stride` bytes a row.  Pixels the
 * pattern leaves transparent are not written, so the buffer shows through.
 * Returns 0, or -1 if `n` is not a pattern in this archive. */
int dar_draw(const Dar *d, int n, unsigned char *out, int stride,
             int outW, int outH, int x, int y);

/* The same thing with each row shifted sideways: `rowdx[r]` is added to the
 * x of the pattern's row r.  That is how WinGL's FUN_0041bad0 draws its
 * wavy pictures; video.c works out the displacements. */
int dar_draw_wave(const Dar *d, int n, unsigned char *out, int stride,
                  int outW, int outH, int x, int y, const short *rowdx);

/* Every pixel written as `colour` instead of its own: the shape of the
 * pattern in one colour, which is how WinGL draws a thing that has just been
 * hit (FUN_00409090 hands the blitter a translate table that maps 0..0xfe to
 * 0xff, so the whole sprite comes out white). */
int dar_draw_solid(const Dar *d, int n, unsigned char *out, int stride,
                   int outW, int outH, int x, int y, int colour);

/* Squeezed sideways: `sx` is 8.8, so 0x100 is 1:1 and 0x80 is half width.
 * One destination column takes the first source pixel that reaches it, which
 * is the rule FUN_0041b850 uses. */
int dar_draw_scale(const Dar *d, int n, unsigned char *out, int stride,
                   int outW, int outH, int x, int y, int sx);

/* Which pattern carries this name, or -1.  The game looks patterns up by
 * name at load time (`PatEntryName('%s')`). */
int dar_find(const Dar *d, const char *name);

#endif
