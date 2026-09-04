/* The game proper: FUN_00405c10, the routine states 0x32..0x34 call through
 * DAT_004492c8 once a frame, and the objects it works on.
 *
 * You are a destroyer on the surface (patterns 0xa05..) dropping depth
 * charges on submarines.  LEFT and RIGHT move, BTN1 drops a charge off the
 * bow and BTN2 off the stern.  The stage is over when the kill quota is met,
 * and a hit costs a life.
 *
 * EVERY OBJECT IS 28 INTS.  The original keeps five arrays of the same
 * 0x1c-int record and only uses the fields each kind needs; the field
 * comments give the original's index so the two can be read side by side.
 *
 *     enemies         DAT_004621a8, 64 slots (the stage uses `nenemy` of them)
 *     depth charges   DAT_004a5490, 16
 *     torpedoes       DAT_004a4d88, 16   (what the submarines shoot upward)
 *     enemy shells    DAT_004a5b90, 8    (what the surface ships lob)
 *     splashes        DAT_004a5fa4, 64
 */
#ifndef SD_PLAY_H
#define SD_PLAY_H

#include "video.h"

#define ENEMIES 64
#define CHARGES 16
#define TORPS   16
#define ESHOTS   8
#define SPLASHES 64

/* The enemy kinds, which are what the stage table at 0x43fae8 holds.  Only
 * these six appear in FUN_00405c10's switch; the beta's stages 4, 8 and 12
 * ask for kinds 4, 8 and 12, and 8 and 12 have no code at all - which is why
 * the readme calls those stages unplayable. */
enum {
    EK_SUB      = 1,        /* the common submarine, 0xa1d / 0xa15 */
    EK_SUB2     = 2,        /* 0xa0d, lobs shells */
    EK_MINI     = 3,        /* 0x9cd, small, shoots torpedoes */
    EK_SHIP     = 4,        /* 0xa05, a surface ship: stops and fires four */
    EK_SUB3     = 5,        /* 0xa0b, three frames of animation */
    EK_HOMING   = 9         /* 0x9c9, steers toward a target depth */
};

typedef struct {
    int x, y;               /* [0] [1]   y == 0 means the slot is free */
    int vx, vy;             /* [2] [3] */
    int w, h;               /* [6] [7]   the box the sonar and hits use */
    int state;              /* [11]  10 = alive, 9..0 = blowing up */
    int aim;                /* [12]  kind 4's aim counter, kind 9's depth */
    int kind;               /* [13] */
    int chain;              /* [16]  how many went up with it */
    int anim, animt;        /* [20] [21]  kind 5's three frames */
    int face;               /* [22]  kind 1's second sprite pair */
    /* the space stage uses more of the record (src/space.c) */
    int mirror;             /* [-2] one frame of white after a hit */
    int hp;                 /* [15] the kinds that take more than one */
    int c1, c2;             /* [17] [18] */
    int phase;              /* [23] which step of its own dance */
    int tick;               /* [24] */
    int layer;              /* [25] which of the two draw passes */
} Enemy;

typedef struct {            /* DAT_004a5490 */
    int x, y;               /* [0] [1]   y == 0x134 means the slot is free */
    int vx;                 /* [2] */
    int vy;                 /* [3]  always 2 - the original writes it once */
    int tx;                 /* [4]  where it was dropped, for the homing item */
} Charge;

typedef struct { int x, y; } Torp;          /* DAT_004a4d88, y <= 0x20 free */
typedef struct { int x, y, vx; } EShot;     /* DAT_004a5b90, y < -0xf free */
typedef struct { int frame, x, y; } Splash; /* DAT_004a5fa4, frame 4 = free */

typedef struct {            /* DAT_004a8548, 5 ints, 64 of them */
    int t;                  /* [0]  0x3c down to 0, and 0 = free */
    int value;              /* [1]  the points, already multiplied by ten */
    int x, y;               /* [2] [3] */
    int chain;              /* [4]  1 = show the number on its own */
} Popup;

#define POPUPS 64

typedef struct { int x, y, vx, kind; } Star;  /* DAT_004a9110, 256 of them */

/* The air stage (src/air.c).  The ship shoots upward and the aircraft drop
 * two kinds of bomb; the arrays are the same 28-int records again. */
typedef struct {            /* DAT_00461358, y < -0xf means free */
    int x, y;
    int dx;                 /* the up/down spread: the air stage's +0x20,
                             * the space stage's +0x24 - each stage sets the
                             * array up itself, so one field does for both */
    int vx;                 /* +0x08, only the space stage (it shoots
                             * sideways) */
} UpShot;
typedef struct { int x, y, vy; } Bomb;      /* DAT_00461a70, y >= 0x160 free */
typedef struct {            /* DAT_00463dd8, y >= 0x160 means free */
    int x, y, vx, vy;
    int t;                  /* +0x14, the boss stage's homing counter -
                             * nothing in this build ever sets it */
} ABomb;
/* DAT_004a8a58: 64 records of four ints.  The air stage keeps its rain in
 * them and the space and boss stages their starfield - in the original that
 * is one and the same memory, and the ending draws whatever the stage before
 * it left there, so they are one array here too. */
typedef struct { int x, y, kind; } Cloud;

/* The space stage (src/space.c).  The script is stage3.bin turned into
 * FUN_00413df0's runtime form: six ints an entry, and the entry after the
 * last one has type 0xff. */
typedef struct { int type, v, a, b, c, d; } Script;
typedef struct { int x, y, vx, on; } Big;   /* DAT_004bb148, the big shots */
typedef struct {                            /* DAT_004ba940, FUN_00413ae0 */
    int k1, k2, k3, k4;
    int x, y, vy, a;
} Dust;

typedef struct {            /* DAT_004a4858, 8 ints, 64 of them */
    int on;
    int x, y;
    int frame, sub;         /* it steps every third frame */
    int kind;               /* 0x10 small, 0xc0 the big one at three times */
} Boom;

#define SCRIPTS 320
#define BOOMS   64
#define BIGS    64
#define STARS2  64      /* the same 64 records as CLOUDS */
#define DUSTS   64

#define UPSHOTS 16
#define BOMBS   16
#define ABOMBS  16
#define CLOUDS  64

#define STARS 256

typedef struct {
    /* the player - a ship, so only x moves */
    int px, py;             /* DAT_004644dc / DAT_004644d8 */
    int pdx;                /* DAT_00461340, this frame's step */
    int speed;              /* DAT_00463db0, 2 and up to 8 with the item */
    int life;               /* DAT_00461a64, 10 = alive, then it counts down */
    int lives;              /* DAT_00462194, 2 at the start */
    int hit;                /* DAT_00463dc4, no collisions while it is set */

    int score;              /* DAT_00463dcc, shown times ten */
    int stage;              /* DAT_00463da4 */
    int loaded;             /* DAT_00461334, the stage the field was built for */
    int cycle;              /* DAT_00462198, ((stage - 1) % 4) + 1 */
    int nenemy;             /* DAT_00463dac, 10 */
    int kills;              /* DAT_00461a60 */
    int quota;              /* DAT_00462180, kills needed to clear the stage */
    int onscreen;           /* DAT_00462184 */

    int charges;            /* DAT_00462190, how many the ship carries */
    int inflight;           /* DAT_00461344 */
    int ntorp;              /* DAT_00461a5c */
    int neshot;             /* DAT_00462188 */

    int swell, swellt;      /* DAT_004a5484 / DAT_004a5f9c, the 1px sea swell */
    int canim, cframe;      /* DAT_004a5f0c / DAT_004a5f94, the charge sprite */
    int flip;              /* DAT_0046216c, flips every frame */
    int powerA, powerB;     /* DAT_004644d4 / DAT_0046134c, the two power-ups */
    int sunk;               /* DAT_00463dc8, charges that reached the bottom */

    int item;               /* DAT_00463dc0, 0 = none, else the kind */
    int itemx, itemy;       /* DAT_00461a54 / DAT_00461a58 */
    int itemt;              /* DAT_00463da0 */
    int itemvy;             /* DAT_00463db8 */
    int itemk;              /* DAT_00463db4 */

    int demo;               /* DAT_0046218c, 1 while the attract demo runs */
    int banner;             /* DAT_004a8544, the "STAGE" banner's frames */
    int over;               /* DAT_004a8540, the game over banner's frames */
    int announce;           /* DAT_004a8a48 / DAT_004a8a4c */
    int announce_stage;

    Enemy e[ENEMIES];
    Charge c[CHARGES];
    Torp t[TORPS];
    EShot s[ESHOTS];
    Splash sp[SPLASHES];
    Popup pop[POPUPS];

    /* the name entry (FUN_0040bdb0) */
    int rankin;             /* DAT_004a9038, which row the score takes */
    int rcurx, rcury;       /* DAT_004a9044 / DAT_004a9048 */
    int repeat;             /* DAT_004a9060, the key-repeat delay */
    int namelen;            /* DAT_004a903c */
    char nm[16];            /* DAT_004b01e8, what has been typed */
    char date[16];          /* DAT_004a9050, "YY/MM/DD" */
    int nnames, pickname;   /* DAT_004a9040 / DAT_004a904c, the DUP list */
    char names[16][16];     /* DAT_004a9064 */
    Star star[STARS];

    /* the air stage */
    UpShot up[UPSHOTS];
    Bomb bomb[BOMBS];
    ABomb ab[ABOMBS];
    int nbomb, nab;         /* DAT_004b18c0 / DAT_004b18b8 */
    int wob2;               /* DAT_004b18bc, the air stage's own 0/1 */
    Cloud cloud[CLOUDS];
    int ncloud;             /* DAT_004b18d0 */
    int ac_timer;           /* DAT_004b18c8 */
    int ac_scroll;          /* DAT_004b18cc */

    /* the space stage */
    Script script[SCRIPTS];
    int nscript;
    int sc_at;              /* DAT_004ba92c, where the script is up to */
    int sc_wait;            /* DAT_004b78dc */
    Big big[BIGS];
    int big_timer;          /* DAT_004ba938 */
    int flash2;             /* DAT_004ba930, one white frame */
    int emerg;              /* DAT_004ba934, the EMERGENCY countdown */
    int drift_x, drift_y;   /* DAT_004a8a50 / DAT_004a8a54 */
    Dust dust[DUSTS];
    int scroll_n;           /* DAT_004b78d8, how fast the sky goes past */
    int spacex[2], spacey[2], spacevx[2];   /* DAT_004ba8e8, the two pictures */

    /* the boss stage (src/boss.c) */
    Boom boom[BOOMS];
    int gun_dx[4], gun_x[4], gun_y[4];  /* DAT_004a4c58 / 68 / 78 */
    int gunfire;            /* DAT_004a4850, how many ports are out */
    int boss_hits;          /* DAT_004a4c88, thirty and it is finished */
    int boss_phase;         /* DAT_004a4c8c */
    int boss_cool;          /* DAT_004a4c90 */
    int boss_live;          /* DAT_00461330, which the sonar reads */

    /* the ending and what follows it (src/ending.c) */
    int end_hold;           /* DAT_004a84fc, frames since the earth arrived */
    int cast_state;         /* DAT_004a8504 */
    int cast_i;             /* DAT_004a8528, which of the twenty */
    int cast_x;             /* DAT_004a8510, the slide */
    int cast_t;             /* DAT_004a8514 */
    int cast_anim;          /* DAT_004a8524 */
    int staff_y;            /* DAT_004bf154, the roll's scroll */
    int staff_hold;         /* DAT_004bf158 */
    int staff_t;            /* DAT_004bf15c, and it is on the screen */

    /* the pause menu (FUN_0040b960) */
    int pause_cur;          /* DAT_004a902c, 0 = CONTINUE, 1 = EXIT */
    int saved_hook;         /* DAT_004a9030, what to go back to */

    /* the stage-clear pan (FUN_00408210) */
    int cl_step;            /* DAT_004a84e4 */
    int cl_timer;           /* DAT_004a84ec */
    int cl_sky;             /* DAT_004a84e0 */
    int cl_ground;          /* DAT_004a84e8 */
    int cl_row;             /* DAT_004a84f0 */
} Play;

#endif
