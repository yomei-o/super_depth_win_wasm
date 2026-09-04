/* The 8bpp surface the game draws into, and the things it draws:
 * patterns, filled rectangles and text.
 *
 * The size is the original's: WinGL is handed 0x280 x 0x1e0 = 640x480
 * (FUN_004178e0) and the window's client rectangle is set to the same.
 *
 * THE 32 PIXEL OFFSET.  FUN_00409000, which is what the game calls to put a
 * pattern on the screen, passes `y + 0x20` down to the blitter and clips with
 *
 *     y < 0x1f1                        (497)
 *     -0x21 < patternWidth + x         and x < 0x261   (609)
 *     -patternHeight <= y
 *
 * so the game's own y = 0 is 32 pixels down the surface, and objects are
 * allowed to sit a little way off every edge.  Everything here keeps those
 * numbers rather than tidying them up.  The 32 pixels are not spare: the end
 * of FUN_00401500 tiles pattern 0x9d9 along all four edges, which is the
 * frame the game runs inside (see game.c).
 *
 * TEXT.  FUN_00402980(col, row, s, base) and FUN_00402a20(col, y, s, base)
 * are the same routine twice over, the first taking the row in 16-pixel
 * units and the second the y in pixels.  Both
 *
 *     start at x = col * 8, step 16 pixels a glyph (col += 2)
 *     skip spaces
 *     draw pattern `base + (unsigned char)ch`
 *
 * which is why each 16x16 font bank in depth.dar is 256 patterns long: the
 * ASCII code indexes it directly.  The banks are listed below.  The small
 * font goes through FUN_00402c10 instead: x = col * 8, y = row * 8, pattern
 * `ch + 0xe0` - the same thing as FONT8 below, since 0xe0 + ' ' = 256.
 *
 * TWO ARCHIVES, ONE PALETTE.  A pattern slot number is global: the game
 * loads pic\depth.dar into slots 0.. and then overwrites the slots from
 * EXT_BASE = 0xb47 with whatever archive the scene needs (staff.dar,
 * depth1.dar, ...).  The screen palette is taken from pattern 0xb48 -
 * FUN_004178e0(surface, 0x280, 0x1e0, 0xb48, ...) at the end of the loader -
 * so it is the *scene* archive's palette, not depth.dar's.  That works
 * because of how the palettes are cut: indices 0..118 and 246..255 are
 * identical in every archive, 119..245 are each archive's own colours, and
 * depth.dar's patterns never use one of those (measured over all 2887:
 * 92.8% below 119, 7.2% at 246 and up, none in between).
 */
#ifndef SD_VIDEO_H
#define SD_VIDEO_H

#include "dar.h"

#define SCR_W 640
#define SCR_H 480
#define SCR_YOFF 0x20                   /* FUN_00409000's `y + 0x20` */
#define EXT_BASE 0xb47                  /* the scene archive's first slot */

/* The font banks in depth.dar.  base + ASCII, so a bank is 256 patterns. */
#define FNT_WHITE  384
#define FNT_RED    640
#define FNT_GREEN  896
#define FNT_BLUE  1152
#define FNT_YELLOW 1408
#define FNT_MAG   1664
#define FNT_CYAN  1920
#define FNT_BLACK 2177
/* The 8x8 font is not laid out that way: it starts at ' ' rather than 0. */
#define FONT8      256
#define FONT8_FIRST ' '

typedef struct {
    unsigned char px[SCR_H][SCR_W];
    const Dar *dar;                     /* pic\depth.dar, slots 0..EXT_BASE-1 */
    const Dar *ext;                     /* the scene archive, slots EXT_BASE.. */
} Video;

void vid_init(Video *v, const Dar *dar);
/* Point the slots from EXT_BASE at another archive, which also changes the
 * screen palette.  Passing 0 leaves only depth.dar. */
void vid_scene(Video *v, const Dar *ext);
/* The palette the surface is shown through, r,g,b per entry. */
const unsigned char (*vid_palette(const Video *v))[3];

void vid_clear(Video *v, int colour);
/* FUN_004183b0: a filled rectangle, right and bottom exclusive as in RECT. */
void vid_fill(Video *v, int left, int top, int right, int bottom, int colour);

/* Whichever archive holds this slot, and the pattern in it, or 0. */
const DarPat *vid_pat_info(const Video *v, int pat);

/* A pattern at the game's own coordinates - so 32 is added to y, and the
 * original's clipping applies. */
void vid_pat(Video *v, int x, int y, int pat);

/* Straight onto the surface, no offset and no clipping rules: for the title
 * pictures and anything else that is placed in surface coordinates. */
void vid_pat_raw(Video *v, int x, int y, int pat);

/* FUN_004219a0: centred on (cx, cy) in surface coordinates, which it does by
 * subtracting half the pattern's own width and height. */
void vid_pat_centre(Video *v, int cx, int cy, int pat);

/* FUN_004092a0 -> FUN_0041bad0: the same pattern with every row pushed
 * sideways by a sine, which is how the name-entry screen's backdrop waves.
 * `wave` is the wavelength - a negative one flips the amplitude on every
 * other row - `amp` the displacement and `phase` the scroll (the original
 * hands it the frame counter).  The sine is the original's own 256-byte
 * table at 0x442178, +-127.
 */
void vid_pat_wave(Video *v, int x, int y, int pat, int wave, int amp,
                  int phase);

/* FUN_00409090: the pattern's shape in colour 0xff, which is what the game
 * draws for one frame when something has been hit. */
void vid_pat_flash(Video *v, int x, int y, int pat);

/* FUN_00409120 -> FUN_0041b6f0 + FUN_0041b850: the pattern squeezed
 * sideways, `sx` in 8.8 and centred the way the original centres it (the
 * vertical scale is always 0x100 where the game uses this). */
void vid_pat_scale(Video *v, int x, int y, int pat, int sx, int sy);

/* FUN_00402980: the row is in 16-pixel units. */
void vid_text(Video *v, int col, int row, const char *s, int bank);
/* FUN_00402a20: y in pixels. */
void vid_text_at(Video *v, int col, int y, const char *s, int bank);
/* FUN_00402c10: the 8x8 font, col and row both in 8-pixel units. */
void vid_text8_at(Video *v, int col, int row, const char *s);
/* The same font placed by pixel, which is handy for the tools. */
void vid_text8(Video *v, int x, int y, const char *s);

#endif
