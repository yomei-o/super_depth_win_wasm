/* The 8bpp surface the game draws into, and the two things it draws:
 * patterns and text.
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
 * numbers rather than tidying them up.
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
 * ASCII code indexes it directly.  The banks are listed below.
 */
#ifndef SD_VIDEO_H
#define SD_VIDEO_H

#include "dar.h"

#define SCR_W 640
#define SCR_H 480
#define SCR_YOFF 0x20                   /* FUN_00409000's `y + 0x20` */

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
    const Dar *dar;
} Video;

void vid_init(Video *v, const Dar *dar);
void vid_clear(Video *v, int colour);

/* A pattern at the game's own coordinates - so 32 is added to y, and the
 * original's clipping applies. */
void vid_pat(Video *v, int x, int y, int pat);

/* Straight onto the surface, no offset and no clipping rules: for the title
 * pictures and anything else that is placed in surface coordinates. */
void vid_pat_raw(Video *v, int x, int y, int pat);

/* FUN_00402980: the row is in 16-pixel units. */
void vid_text(Video *v, int col, int row, const char *s, int bank);
/* FUN_00402a20: y in pixels. */
void vid_text_at(Video *v, int col, int y, const char *s, int bank);
/* The 8x8 font, which the game itself uses for the small print. */
void vid_text8(Video *v, int x, int y, const char *s);

#endif
