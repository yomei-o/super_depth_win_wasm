/* The game itself: FUN_00401500, the one function WinGL calls once a frame,
 * and the globals it works out of.
 *
 * The original is a single `switch (DAT_004bf894)` with one case a screen.
 * FUN_00421da0(n) puts the next state in and raises the "just entered" flag
 * (and zeroes 0xe100 dwords of object area at DAT_004c0070, which here is
 * covered by each state owning its own fields).  Field comments give the
 * original's address so the two can be read side by side.
 *
 * WHAT RUNS EVERY FRAME.  FUN_004209a0 is the window's frame handler:
 *
 *     FUN_00430f50(pad)        the key table at DAT_00444c20 -> pad bits
 *     GetLocalTime             -> the clock globals
 *     thunk_FUN_00401500()     the game frame, which is what game_tick is
 *     FUN_00417e70()           present
 *     frame = (frame + 1) & 0x800000ff
 *     the pad block is copied to DAT_004bfc18 for next frame's edges
 *
 * on a 33ms timer (FUN_00424870(this, 0x21, 200, 0) in WinMain).
 */
#ifndef SD_GAME_H
#define SD_GAME_H

#include "video.h"

/* The states, from the switch in FUN_00401500. */
enum {
    ST_BOOT    = 0x0a,      /* one frame of setting up, then ST_LOGO */
    ST_LOGO0   = 0x0f,      /* the Bio_100% logo; 0x0f and 0x10 are the same */
    ST_LOGO    = 0x10,
    ST_TITLE   = 0x1e,      /* the sea title */
    ST_TITLE2  = 0x32,
    ST_TITLE3  = 0x33,
    ST_TITLE4  = 0x34,
    ST_TITLE5  = 0x35,      /* goes back to ST_LOGO, which is the demo loop */
    ST_SOUND   = 0x46,      /* SOUND TEST */
    ST_VERSION = 0x5a       /* version, credits, then PostQuitMessage */
};

/* The pad bits FUN_00430f50 packs, which WinGL then spreads one to an int
 * from DAT_004bf83c up.  The addresses are the spread copies. */
#define PAD_UP    0x0001                /* DAT_004bf844 */
#define PAD_DOWN  0x0002                /* DAT_004bf848 */
#define PAD_LEFT  0x0004                /* DAT_004bf83c */
#define PAD_RIGHT 0x0008                /* DAT_004bf840 */
#define PAD_BTN1  0x0010                /* DAT_004bf850  Z / Space */
#define PAD_BTN2  0x0020                /* DAT_004bf854  X / Enter */
#define PAD_BTN3  0x0040                /* Shift */
#define PAD_BTN4  0x0080                /* Q */
#define PAD_JOY7  0x0400                /* DAT_004bf868, joystick only */
#define PAD_START 0x1000                /* F2 */
#define PAD_ESC   0x8000                /* DAT_004bf84c */

#define LOGO_ROWS 0xb8                  /* DAT_0044653c, one int a pixel row */

typedef struct {
    Video *v;
    Dar scene;                          /* whatever sits at EXT_BASE now */
    char scene_name[64];                /* DAT_00470a94, so it is loaded once */

    int state;                          /* DAT_004bf894 */
    int entered;                        /* DAT_004bf89c */
    int sub;                            /* DAT_004bf898, frames in this state */
    unsigned frame;                     /* DAT_004bf820 */
    unsigned seed;                      /* DAT_00443d44, the MSVC rand seed */

    unsigned pad, pad_prev;             /* the spread block and its copy */
    int second, second_prev;            /* DAT_004bf88c / DAT_004bfc94 */
    int fps, fps_count;                 /* DAT_004bf8a0 / DAT_004bf8a4 */

    int flash;                          /* DAT_0046217c, white frames left */
    int fullscreen;                     /* DAT_004bf8b8, the registry setting */
    int demo;                           /* DAT_00464ed8, 0 = a person plays */
    int hook, hook_arg;                 /* DAT_004492c8 / DAT_004492ac */

    int logo_row[LOGO_ROWS];            /* DAT_0044653c: 1 = that row is hidden */
    int logo_left;                      /* DAT_004492b8, rows still hidden */
    int logo_phase;                     /* DAT_004492c0: 0 = show, 2 = hide */
    int logo_timer;                     /* DAT_0045cb78 */
} Game;

/* The two things the game needs from the outside.  The front end provides
 * them: the paths differ between the native tools and the wasm build, and
 * the music is the page's business. */
int  plat_dar(Dar *d, const char *name);        /* load pic\<name> */
void plat_bgm(int mode, const char *name);      /* FUN_00420980(mode, name) */

void game_init(Game *g, Video *v);
void game_set_pad(Game *g, unsigned pad);
/* The wall clock second, which is all the game uses the clock for (the FPS
 * counter at the end of the frame). */
void game_set_second(Game *g, int second);
/* One frame: FUN_00401500 followed by FUN_004209a0's own bookkeeping. */
void game_tick(Game *g);

/* FUN_0042691c: the MSVC rand(), which is the only random source. */
int  game_rand(Game *g);

#endif
