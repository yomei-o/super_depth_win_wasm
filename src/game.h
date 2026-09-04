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
    ST_PLAY    = 0x32,      /* playing; all three run the hook */
    ST_DEMO    = 0x33,      /* the demo playing back */
    ST_RECORD  = 0x34,      /* recording one (the debug menu's own entries) */
    ST_PLAY_END = 0x35,     /* goes back to ST_LOGO, which is the demo loop */
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
       HOOK_PAUSE = 5,                  /* LAB_00401041 -> FUN_0040b960 */
       HOOK_AIRCLEAR = 6,               /* LAB_004010d2 -> FUN_0040f490 */
       HOOK_SPACE = 7,                  /* LAB_0040110e -> FUN_0040f970 */
       HOOK_BOSS = 8,                   /* LAB_004011c7 -> FUN_00403dc0 */
       HOOK_END = 9,                    /* LAB_0040113b -> FUN_00408650 */
       HOOK_CAST = 10,                  /* 0x401136     -> FUN_00408a80 */
       HOOK_STAFF = 11 };               /* LAB_00401091 -> FUN_00414210 */

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
    int year, month, day;               /* DAT_004bf878 / 87c / 880 */
    int clear_next;                     /* DAT_004492cc, clear at the end */
    int fps, fps_count;                 /* DAT_004bf8a0 / DAT_004bf8a4 */

    int flash;                          /* DAT_0046217c, white frames left */
    int fullscreen;                     /* DAT_004bf8b8, the registry setting */
    int demo;                           /* DAT_00464ed8, 0 = a person plays */
    int hook, hook_arg;                 /* DAT_004492c8 / DAT_004492ac */
    int boxes;                          /* DAT_00462168, the debug outlines */
    /* DAT_00463da8: whether one wreck sets off the next.  FUN_00402480 (the
     * window being made) sets it to 1 and nothing clears it - it is outside
     * everything FUN_0040b250 wipes - so the chains are on for the whole run
     * unless the debug menu's 0x86a turns them off.  It was a Play field
     * here, which meant play_clear zeroed it and no chain ever happened. */
    int echain;
    /* DAT_00463dc4: 死なない, the other one FUN_00402480 sets once (to 0)
     * and only the menu's 0x867 changes.  Nothing can hit the ship while it
     * is on - which is what the checks here use it for. */
    int nodie;
    /* The registry's own three, which FUN_004026f0 reads at startup and the
     * オプション menu toggles: Music (DAT_004bf8b0), Sound (DAT_004bf8b4)
     * and WavePan (DAT_004bf8c4).  The defaults FUN_00402610 writes are all
     * 1.  It is the sound layer that reads them in the original, so it is
     * the front end that reads them here. */
    int music_on, se_on, stereo;

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
    /* Not the original: it writes each record straight into the registry
     * (FUN_004026f0).  A browser has no registry, so the front end watches
     * this counter and keeps the table wherever it can. */
    int rank_stamp;

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
/* FUN_004031d0 writing DEMO1.DAT back out at the end of a recording.  The
 * native checks do not put it on the disk (`disk/demo1.dat` is the
 * original's own recording); the page hands it to the browser to save. */
void plat_write(const char *name, const unsigned char *buf, int n);

void game_init(Game *g, Video *v);
void game_set_pad(Game *g, unsigned pad);
/* GetLocalTime's fields, which the game uses for the FPS counter and for
 * the date it stamps a high score with. */
void game_set_second(Game *g, int second);
void game_set_date(Game *g, int year, int month, int day);
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
void play_over_frame(Game *g);          /* FUN_0040bdb0, the name entry */
/* FUN_0040bbb0 and its wrapper FUN_0040bb60: one row of the score table. */
void play_rank_row_of(Game *g, int rank, int score, int stage,
                      const char *name, const char *date, int bank);
void play_rank_row(Game *g, int rank);

/* What src/air.c borrows from src/play.c: the pieces both stages share. */
void play_field_build(Game *g);         /* FUN_0040aa20 */
void play_enemy_hit(Game *g, int i, int chain);         /* FUN_0040acf0 */
void play_item_pick(Game *g);           /* FUN_0040aed0 */
void play_item_apply(Play *p);          /* FUN_0040aab0 */
void play_item_name(Game *g, int x, int y, int kind);   /* FUN_0040b1c0 */
void play_boom(Video *v, int x, int y, int frame);      /* FUN_0040ae50 */
void play_status_bar(Game *g);          /* FUN_004093d0 */
void play_clear_banners(Game *g);       /* FUN_0040a9f0 */
void play_box(Game *g, int x, int y, int w, int h);     /* FUN_00403520 */
int  play_score_of(const Play *p, int kind);            /* 0x43fe70's table */

/* src/air.c */
void air_frame(Game *g);                /* FUN_0040c9e0, the stage after one */
void air_clear_frame(Game *g);          /* FUN_0040f490, its own clear */

/* src/space.c */
void space_frame(Game *g);              /* FUN_0040f970, the stage after that */

/* src/boss.c */
void boss_frame(Game *g);               /* FUN_00403dc0, the fourth kind */

/* src/ending.c - the three screens only the debug menu can reach */
void end_frame(Game *g);                /* FUN_00408650 */
void cast_frame(Game *g);               /* FUN_00408a80 */
void staff_frame(Game *g);              /* FUN_00414210 */

/* The debug menu's commands (the MENU resource the release build drops),
 * by the resource's own ids.  Answers non-zero when the command was one of
 * them.  See the head of src/ending.c. */
enum { MENU_MUSIC = 0x85d,              /* オプション, in both menus */
       MENU_SE = 0x85e,
       MENU_RESET = 0x860,              /* this one is in both menus */
       MENU_STEREO = 0x864,             /* 効果音をステレオ, debug menu */
       DBG_NODIE = 0x867,               /* 死なない, off at startup */
       DBG_CHAIN = 0x86a,               /* 誘爆, on at startup */
       DBG_BOX_ON = 0x84c, DBG_BOX_OFF = 0x84d,
       DBG_STAGE01 = 0x84e, DBG_STAGE02 = 0x84f, DBG_STAGE03 = 0x850,
       DBG_STAGE04 = 0x851, DBG_STAGE05 = 0x852, DBG_STAGE06 = 0x853,
       DBG_FULLPOWER = 0x85f,
       DBG_LOGO = 0x86d, DBG_TITLE = 0x86e,
       DBG_ENDING = 0x86f, DBG_STAFF = 0x870,
       DBG_STAGE07 = 0x872, DBG_STAGE12 = 0x877,
       DBG_DEMO_REC = 0x878, DBG_DEMO_PLAY = 0x87b };
int  game_debug(Game *g, int cmd);

#endif
