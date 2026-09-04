/* The screen: 640x480, one byte a pixel, and a 256-colour palette.
 *
 * That is what the game itself has.  WinGL is handed 0x280 x 0x1e0 at
 * FUN_004178e0, the window's client rectangle is set to the same
 * (RStack_120.right = 0x280, .bottom = 0x1e0), and everything reaches the
 * screen through GDI as a palettised DIB - CreateDIBitmap, StretchDIBits,
 * SetDIBitsToDevice - so there is nothing to convert here beyond handing the
 * palette to whoever draws.
 */
#ifndef SD_GFX_H
#define SD_GFX_H

#include "dar.h"

#define GFX_W 640
#define GFX_H 480

typedef struct {
    unsigned char px[GFX_H][GFX_W];
    unsigned char pal[256][3];
} Gfx;

void gfx_clear(Gfx *g, int colour);
void gfx_palette(Gfx *g, const Dar *from);

/* A pattern onto the screen, transparency and all. */
void gfx_pat(Gfx *g, const Dar *d, int n, int x, int y);

#endif
