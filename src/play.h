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
    int echain;             /* DAT_00463da8, chain reactions on/off */
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
