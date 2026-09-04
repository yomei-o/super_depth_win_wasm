/* The screen.  See gfx.h. */
#include "gfx.h"

#include <string.h>

void gfx_clear(Gfx *g, int colour)
{
    memset(g->px, colour & 0xff, sizeof g->px);
}

void gfx_palette(Gfx *g, const Dar *from)
{
    memcpy(g->pal, from->pal, sizeof g->pal);
}

void gfx_pat(Gfx *g, const Dar *d, int n, int x, int y)
{
    dar_draw(d, n, &g->px[0][0], GFX_W, GFX_W, GFX_H, x, y);
}
