/* The ending and the two screens behind it: FUN_00408650 (the earth),
 * FUN_00408a80 (CAST) and FUN_00414210 (the staff roll).
 *
 * None of the three can be reached by playing.  The only code that hands the
 * hook to them is the WM_COMMAND handler at 0x4265a5 and 0x426638, which the
 * debug menu drives - and FUN_00421120 registers the window class with
 * `menu_release`, the menu that has no debug entries in it.  The other menu
 * is still in the executable's resources, and it names them:
 *
 *     デバッグ -> モードセレクト -> Bio_100% ロゴ    id 0x86d  state 0x0f
 *                                  タイトル画面      id 0x86e  state 0x1e
 *                                  エンディング      id 0x86f  this file
 *                                  スタッフロール    id 0x870  this file
 *              -> ステージセレクト  STAGE 01..12     id 0x84e..0x877
 *
 * so game_debug() offers the same commands and the screens are ported here.
 *
 * The ending itself: the Yamaboku flies to the top left, another earth
 * drifts in from the right, and once it is in place "Congratulation!!" and
 * "Yamaboku find another earth." sit under it for 150 frames.  Then twenty
 * of the game's creatures walk past with their names, and then the staff
 * roll scrolls up over the Bio_100% logo.
 */
#include "game.h"

#include <stdio.h>
#include <string.h>

static int sgn(int v) { return v > 0 ? 1 : (v < 0 ? -1 : 0); }

/* ---- FUN_00408650: the earth ------------------------------------------ */

void end_frame(Game *g)
{
    Play *p = &g->p;
    Video *v = g->v;
    Enemy *e = &p->e[0];
    const DarPat *pic;
    int i, target;

    if (g->hook_arg) {
        g->hook_arg = 0;
        game_scene(g, "ending.dar", 2);
        play_field_build(g);            /* FUN_0040aa20 */
        play_clear_banners(g);          /* FUN_0040a9f0 */
        p->end_hold = 0;                /* DAT_004a84fc */
        p->onscreen = 0;
        p->kills = 0;
        plat_bgm(0, "bgm14");
        pic = vid_pat_info(v, EXT_BASE);
        e->x = 900 - (pic ? pic->w : 0);
        p->inflight = 0;
        p->quota = 999;
        p->itemt = 0;
        p->itemk = 0;
        p->itemvy = 0;
        p->itemy = 0;
        p->itemx = 0;
        p->item = 0;
        p->boss_live = 0;
        /* _DAT_00462174 and _DAT_00462178 are zeroed here as well, as they
         * are at the top of every stage; nothing in this build reads them. */
    }

    /* The ship walks one pixel a frame to the top left corner. */
    p->px += sgn(0x80 - p->px);
    p->py += sgn(0xc4 - p->py);

    /* The starfield the debug command seeded, drifting if the stage before
     * it left a drift behind - the ending never sets one itself. */
    if (p->drift_x != 0 || p->drift_y != 0)
        for (i = 0; i < STARS2; i++) {
            Cloud *st = &p->cloud[i];
            st->x += p->drift_x;
            st->y += p->drift_y;
            if (p->drift_x != 0)
                st->x += p->drift_x < 0 ? -st->kind : st->kind;
            if (p->drift_y != 0)
                st->y += p->drift_y < 0 ? -st->kind : st->kind;
            if (st->x < 0) { st->x += 0x280; st->y = game_rand(g) % 0x160; }
            if (st->x > 0x280) { st->x -= 0x280; st->y = game_rand(g) % 0x160; }
            if (st->y < 0) { st->y += 0x160; st->x = game_rand(g) % 0x280; }
            if (st->y > 0x160) { st->y -= 0x160; st->x = game_rand(g) % 0x280; }
        }
    for (i = 0; i < STARS2; i++)
        vid_pat(v, p->cloud[i].x, p->cloud[i].y, p->cloud[i].kind + 0xb30);

    /* The earth is enemy slot 0, so it shows up on the sonar as well
     * (FUN_004097a0's last branch draws 0xb48 for it). */
    e->y = 0x90;
    pic = vid_pat_info(v, EXT_BASE);
    target = 0x200 - (pic ? pic->w : 0);
    e->x += sgn(target - e->x);
    vid_pat(v, e->x, 0x90, EXT_BASE);
    vid_pat(v, p->px, p->py, 0xa05);    /* the Yamaboku */
    play_status_bar(g);

    if (e->x == target) {
        p->end_hold++;
        vid_text(v, 0x18, 4, "Congratulation!!", FNT_RED);
        vid_text(v, 0xc, 0x12, "Yamaboku find another earth.", FNT_CYAN);
        if (p->end_hold == 0x96) {
            g->hook = HOOK_CAST;
            g->hook_arg = 1;
        }
    }
    p->flip ^= 1;
}

/* ---- FUN_00408a80: CAST ----------------------------------------------- */

/* 0x43f870, sixteen bytes each and centred with spaces by hand; 0x43f9b0
 * the pattern, 0x43fa04 what to do with it.  The twenty-first row is what
 * the original reads on the frame it hands over to the staff roll: the name
 * comes out of the pattern table's own bytes and the pattern is 0. */
static const char *const CAST_NAME[21] = {
    "    Tiddler    ", "   Asthmatic   ", "     Coypu     ",
    "     Wigwam    ", "    Eyewash    ", "     Spooky    ",
    "   Fratricide  ", "    Scourge    ", "      Mean     ",
    "    Chirstie   ", "     Poppy     ", "      Rob      ",
    "      Hoot     ", " Strayed Brain ", " Guest Finalty ",
    " Guest Finalty ", "   Eerie Core  ", " Lunatic Noddle",
    "    B.P.S.M.   ", "    Yamaboku   ", "\x1d\x0a"
};
static const int CAST_PAT[21] = {
    0xa1d, 0xa0d, 0x9ce, 0xa0a, 0x9e3, 0xa25, 0xa21, 0xa33, 0x9cf, 0xa23,
    0x991, 0xa07, 0x9c9, 0x9d5, 0xa0b, 0xac4, 0xb2d, 0xb2d, 0xb2d, 0xa05, 0
};
static const int CAST_KIND[21] = {
    0x1e, 0x1e, 0x14, 0x20, 0x15, 0x1e, 0x20, 0x1e, 0x14, 0x1e,
    0x0b, 0x20, 0x14, 0x15, 0x21, 0x14, 0x80, 0x80, 0x80, 0x1e, 0
};

void cast_frame(Game *g)
{
    Play *p = &g->p;
    Video *v = g->v;
    int x, y, pat, off = 0;

    if (g->hook_arg) {
        g->hook_arg = 0;
        play_clear_banners(g);          /* FUN_0040a9f0, no archive change */
        p->onscreen = 0;
        p->kills = 0;
        p->inflight = 0;
        p->quota = 999;
        p->item = 0;
        p->itemt = 0;
        p->cast_state = 0;
    }
    vid_text(v, 0x24, 2, "CAST", FNT_WHITE);

    switch (p->cast_state) {
    case 0:
        p->cast_state = 8;
        p->cast_i = 0;
        p->cast_x = -0x100;
        break;
    case 5:
        p->cast_i++;
        p->cast_anim = 0;
        if (p->cast_i == 0x14) {        /* the twentieth has gone past */
            g->hook = HOOK_STAFF;
            g->hook_arg = 1;
        }
        /* falls into case 8, exactly as the original does */
    case 8:
        p->cast_state = 10;
        p->cast_t = 0;
        p->cast_x = -0x100;
        break;
    case 10:
        if (p->cast_x < 0xc0) {
            p->cast_x += 0x10;
        } else if (++p->cast_t > 0x4a) {
            p->cast_t = 0;
            p->cast_state = 0x14;
        }
        break;
    case 0x14:
        if (p->cast_x < 0x280) {
            p->cast_x += 0x10;
        } else if (++p->cast_t >= 0) {  /* which it always is */
            p->cast_state = 5;
        }
        break;
    }

    x = p->cast_x + 0x80;
    y = 0xb0;
    pat = CAST_PAT[p->cast_i];
    switch (CAST_KIND[p->cast_i]) {
    case 0xb:
        x = p->cast_x + 0x70;
        y = 0xa0;
        if (g->frame % 8 < 4) off = 1;
        break;
    case 0x14:
        y = 0x90;
        x = p->cast_x + 0x60;
        break;
    case 0x15:
        y = 0x90;
        x = p->cast_x + 0x60;
        if (g->frame % 8 < 4) off = 1;
        break;
    case 0x1e:
        y = 0x90;
        x = p->cast_x + 0x40;
        break;
    case 0x1f:
        y = 0x90;
        x = p->cast_x + 0x40;
        off = (int)(g->frame % 2);
        break;
    case 0x20:
        y = 0x90;
        x = p->cast_x + 0x40;
        if (g->frame % 8 < 4) off = 8;
        break;
    case 0x21:
        y = 0x90;
        x = p->cast_x + 0x40;
        p->cast_anim = (p->cast_anim + 1) % 9;
        off = p->cast_anim / 3 * 8;
        break;
    case 0x80:
        y = 0x50;
        x = p->cast_x;
        break;
    }
    /* The name slides with it, but from the left edge of the slide rather
     * than from where the creature is drawn. */
    vid_text_px(v, p->cast_x, 0x14, CAST_NAME[p->cast_i], FNT_WHITE);
    vid_pat_scale_at(v, x, y, pat + off, 0x200, 0x200);
}

/* ---- FUN_00414210: the staff roll ------------------------------------- */

/* 0x44082c, two ints an entry: 0 leaves a gap, 1 draws a pattern, 2 is one
 * of the blocks of text below, 0xff ends the list.  The block at 0x32
 * ("Special Thanks", yosi, steelman, kobi) has code but the list never
 * asks for it. */
static const int ROLL[][2] = {
    { 2, 0 }, { 1, 0xb48 }, { 2, 1 }, { 0, 0x80 },
    { 2, 2 }, { 0, 0x80 }, { 2, 3 }, { 0, 0x80 },
    { 2, 4 }, { 0, 0x80 }, { 2, 5 }, { 0, 0x80 },
    { 2, 6 }, { 0, 0x80 }, { 2, 7 }, { 0, 0x80 },
    { 2, 8 }, { 0, 0x80 }, { 2, 10 }, { 0, 0x80 },
    { 2, 9 }, { 0, 0xc0 }, { 1, 0xb47 }, { 2, 100 },
    { 0xff, 0 }
};

#define ROLL_YELLOW (EXT_BASE + 0xc2)   /* 0xc09, staff.dar's kgfyellow */
#define ROLL_RED    (EXT_BASE + 0x62)   /* 0xba9, kgfred */
#define ROLL_WHITE  (EXT_BASE + 2)      /* 0xb49, kgfwhite */

/* The height of one line of a font, which is what the original reads out of
 * the pattern table (DAT_00497dae and the two next to it). */
static int line_h(Video *v, int bank)
{
    const DarPat *p = vid_pat_info(v, bank);

    return p ? p->h : 0;
}

/* One line, and how far down the next one starts. */
static int roll_line(Video *v, int y, const char *s, int bank)
{
    vid_text_centre(v, y, s, bank);
    return line_h(v, bank);
}

void staff_frame(Game *g)
{
    Play *p = &g->p;
    Video *v = g->v;
    char line[64];
    int at, acc = 0;

    if (g->hook_arg) {
        g->hook_arg = 0;
        game_scene(g, "staff.dar", 0x123);
        plat_bgm(0, "finst1");
        p->staff_y = 0x1c0;
        p->staff_hold = 0;
        p->staff_t = 0;
    }

    for (at = 0; ROLL[at][0] != 0xff; at++) {
        int what = ROLL[at][0], val = ROLL[at][1];
        int y = p->staff_y + acc;

        if (what == 0) {
            acc += val;
            continue;
        }
        if (what == 1) {                /* a pattern, centred */
            const DarPat *q = vid_pat_info(v, val);

            if (q) {
                vid_pat(v, 0x140 - q->w / 2, y, val);
                acc += q->h;
            }
            continue;
        }
        if (what != 2) {                /* which the list never holds */
            acc += roll_line(v, y, "ERROR CODE", ROLL_RED);
            continue;
        }
        switch (val) {
        case 0:
            acc += roll_line(v, y, "Bio_100% Presents", ROLL_YELLOW);
            break;
        case 1:
            acc += roll_line(v, y, "Staff", ROLL_RED);
            break;
        case 2:
            acc += roll_line(v, y, "Game Design", ROLL_YELLOW);
            acc += roll_line(v, p->staff_y + acc, "alty & tacox", ROLL_WHITE);
            break;
        case 3:
            acc += roll_line(v, y, "Character Design", ROLL_YELLOW);
            acc += roll_line(v, p->staff_y + acc, "tacox & alty", ROLL_WHITE);
            break;
        case 4:
            acc += roll_line(v, y, "Music Composition", ROLL_YELLOW);
            acc += roll_line(v, p->staff_y + acc, "FIN & CLAUDE", ROLL_WHITE);
            break;
        case 5:
            acc += roll_line(v, y, "Font Design", ROLL_YELLOW);
            acc += roll_line(v, p->staff_y + acc, "tacox", ROLL_WHITE);
            break;
        case 6:
            acc += roll_line(v, y, "Programming", ROLL_YELLOW);
            acc += roll_line(v, p->staff_y + acc, "nag & alty", ROLL_WHITE);
            break;
        case 7:
            acc += roll_line(v, y, "Update Program - IPatcher", ROLL_YELLOW);
            acc += roll_line(v, p->staff_y + acc, "A.Koizuka", ROLL_WHITE);
            break;
        case 8:
            acc += roll_line(v, y, "MIDI Sound Driver - SMFDrv", ROLL_YELLOW);
            acc += roll_line(v, p->staff_y + acc, "Ajax", ROLL_WHITE);
            break;
        case 9:
            acc += roll_line(v, y, "TSW WinGL Prototype L3", ROLL_YELLOW);
            acc += roll_line(v, p->staff_y + acc, "tarbo", ROLL_WHITE);
            break;
        case 10:
            acc += roll_line(v, y, "WinGL L1 Version 0.11e", ROLL_YELLOW);
            acc += roll_line(v, p->staff_y + acc, "A.Koizuka & alty",
                             ROLL_WHITE);
            break;
        case 0x32:
            acc += roll_line(v, y, "Special Thanks", ROLL_YELLOW);
            acc += roll_line(v, p->staff_y + acc, "yosi", ROLL_WHITE);
            acc += roll_line(v, p->staff_y + acc, "steelman", ROLL_WHITE);
            acc += roll_line(v, p->staff_y + acc, "kobi", ROLL_WHITE);
            break;
        case 100:
            acc += roll_line(v, y, "(C)1991 alty & tacox / Bio_100%",
                             ROLL_WHITE);
            acc += roll_line(v, p->staff_y + acc, "(C)1998 Bio_100% Inc.",
                             ROLL_WHITE);
            break;
        default:
            break;
        }
    }

    /* It creeps up a pixel every other frame until the last line is above
     * the panel, then waits a second. */
    if (g->frame % 2 != 0) {
        if (p->staff_y + acc > 0x160) {
            p->staff_y--;
        } else if (++p->staff_hold > 0x3c) {
            g->hook = HOOK_OVER;
            g->hook_arg = 1;
        }
    }
    /* finend2 is meant to take over here, but the installer has no
     * finend2.mid - only finst1.mid and bgm01..bgm15. */
    if (p->staff_t == 0xac8) plat_bgm(0, "finend2");
    sprintf(line, "%5d", p->staff_t);
    vid_text8_at(v, 9, 5, line);        /* a frame counter, left in */
    p->staff_t++;
}
