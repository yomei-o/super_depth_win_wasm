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

#include "play.h"
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
#define FISH 9                          /* DAT_00449260, the title's swimmers */

/* The top score table at DAT_004bf9dc: ten records of 0x28 bytes, each of
 * them one registry value (`rank%d` under HKCU\Software\Bio_100%\SuperDepth,
 * written whole by FUN_004026f0).  FUN_00402610 fills in the defaults. */
#define RANKS 10

typedef struct {
    int score;                          /* +0x00, shown as "%05d0" */
    char name[16];                      /* +0x04 */
    char date[16];                      /* +0x14 */
    int stage;                          /* +0x24, shown as "%02d" */
} Rank;

/* DAT_004492c8, the routine the play states call every frame.  The original
 * keeps a code pointer (one of the thunks at 0x401100); here it is an id. */
enum { HOOK_NONE = 0,
       HOOK_PLAY = 1,                   /* LAB_00401168 -> FUN_00405c10 */
       HOOK_CLEAR = 2,                  /* LAB_00401235 -> FUN_00408210 */
       HOOK_OVER = 3,                   /* LAB_004011b3 -> FUN_0040bdb0 */
       HOOK_AIR = 4,                    /* LAB_004011ae -> FUN_0040c9e0 */
       HOOK_PAUSE = 5 };                /* LAB_00401041 -> FUN_0040b960 */

/* DAT_004bf164, WinGL's overlay hook, armed by FUN_004148f0(fn, 1). */
enum { DRAW_NONE = 0, DRAW_MENU = 1,    /* LAB_0040118b -> FUN_00414920 */
       DRAW_RECORD = 2 };               /* LAB_004010af -> FUN_00414b00 */

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

    int tick5, wob;                     /* DAT_00446508 / DAT_004492d4 */
    int fish_x[FISH];                   /* DAT_00449288 */
    int fish_y[FISH];                   /* DAT_00449260, 0 = the slot is free */
    int fish_vx[FISH];                  /* DAT_00446514 */
    int staff_step;                     /* DAT_004492f0 */
    int staff_col;                      /* DAT_004492b4 */
    int staff_wait;                     /* DAT_00446538 */
    int staff_line;                     /* DAT_004492c4, 0..7 */

    int draw, draw_new;                 /* DAT_004bf164 / DAT_004bf170 */
    int menu_cur;                       /* DAT_004bf168, 0..2 */
    int menu_idle;                      /* DAT_004bf16c, 0x708 -> the demo */
    int quit;                           /* state 0x5a's PostQuitMessage */

    Rank rank[RANKS];                   /* DAT_004bf9dc */

    Play p;                             /* the game itself, src/play.c */

    /* the recorded demo: one byte a frame, DEMO1.DAT (FUN_00403240) */
    unsigned char rec[10000];           /* DAT_00464ef8 */
    int reclen, recat;                  /* DAT_0046eb38 / DAT_0046eb3c */
    unsigned recpad;                    /* DAT_00464ee0.. spread back out */
} Game;

/* The three lines of the title menu (DAT_00441d68) and the eight staff lines
 * the title scrolls (0x43ed40, 0x25 bytes apiece). */
extern const char *const GAME_MENU[3];
extern const char *const GAME_STAFF[8];

/* The two things the game needs from the outside.  The front end provides
 * them: the paths differ between the native tools and the wasm build, and
 * the music is the page's business. */
int  plat_dar(Dar *d, const char *name);        /* load pic\<name> */
void plat_bgm(int mode, const char *name);      /* FUN_00420980(mode, name) */
/* FUN_0041fd00 / FUN_0041fdf0: a WAV by name, `pan` in the original's
 * hundredths of a decibel from the middle (-10000..10000, 0 = centre). */
void plat_se(const char *name, int pan);
/* Read a data file whole; returns how many bytes came back, or -1. */
int  plat_read(const char *name, unsigned char *buf, int max);

void game_init(Game *g, Video *v);
void game_set_pad(Game *g, unsigned pad);
/* The wall clock second, which is all the game uses the clock for (the FPS
 * counter at the end of the frame). */
void game_set_second(Game *g, int second);
/* One frame: FUN_00401500 followed by FUN_004209a0's own bookkeeping. */
void game_tick(Game *g);

/* FUN_0042691c: the MSVC rand(), which is the only random source. */
int  game_rand(Game *g);

/* What src/play.c needs from here.  Each of the button tests burns a random
 * number, exactly as the originals do, and reads the recorded demo instead
 * of the pad while one is playing. */
int  game_any_key(Game *g);             /* FUN_00402de0, BTN1 */
int  game_start_key(Game *g);           /* FUN_00402ec0, START */
int  game_btn2(Game *g);                /* FUN_00402e50, BTN2 */
int  game_left(Game *g);                /* FUN_00402fd0 */
int  game_right(Game *g);               /* FUN_00403020 */
int  game_up(Game *g);                  /* FUN_00402f30 */
int  game_down(Game *g);                /* FUN_00402f80 */
int  game_edge(const Game *g, unsigned bit);
void game_set_state(Game *g, int st);   /* FUN_00421da0 */
void game_scene(Game *g, const char *name, int count);

/* src/play.c */
void play_frame(Game *g);
void play_clear_frame(Game *g);         /* FUN_00408210, the stage clear */
void play_pause_frame(Game *g);         /* FUN_0040b960, the pause menu */

#endif
