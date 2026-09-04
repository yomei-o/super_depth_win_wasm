/* FUN_00403dc0 - the fourth kind of stage: one big thing, in space, with
 * `disk/depth5.jpg` for a picture of it.
 *
 * The ship flies as it does in the space stage.  The boss is enemy slot 0
 * and nothing else is on the field until it dies, when it breaks into the
 * eight pieces the tables at 0x43f6d4 and 0x43f71c describe.  Beating it
 * sets the stage number back to 1 and hands over to the sea stage, so the
 * four kinds of stage go round for ever.
 *
 * The background is the same dust as the space stage's, drawn as 48x1
 * strips out of space.dar - the coloured streaks in the screenshot.
 */
#include "game.h"

#include <stdio.h>
#include <string.h>

static int sgn(int v) { return v > 0 ? 1 : (v < 0 ? -1 : 0); }
static int absi(int v) { return v < 0 ? -v : v; }

/* 0x43f6d4 and 0x43f71c: where the eight pieces start and where they go. */
static const int PIECE_X[8] = { 0, 42, 84, 0, 42, 84, 0, 42 };
static const int PIECE_Y[8] = { 0, 0, 0, 32, 32, 32, 64, 64 };
static const int PIECE_VX[8] = { -12, 0, 12, -16, 0, 16, -12, 0 };
static const int PIECE_VY[8] = { -12, -16, -12, 0, 0, 0, 12, 16 };

/* FUN_00403c30: nothing is blowing up yet. */
static void boom_clear(Play *p)
{
    int i;

    for (i = 0; i < BOOMS; i++) {
        p->boom[i].on = 0;
        p->boom[i].x = -0x20;
        p->boom[i].y = -0x20;
        p->boom[i].frame = 0;
        p->boom[i].sub = 0;
        p->boom[i].kind = 0;
    }
}

/* FUN_00403c70: one more explosion.  `kind` is 0x10 for the small four
 * frame one and 0xc0 for the eight frame one drawn at three times size. */
static void boom_add(Play *p, int x, int y, int kind)
{
    int i;

    for (i = 0; i < BOOMS; i++) {
        if (p->boom[i].on != 0) continue;
        p->boom[i].on = 1;
        p->boom[i].x = x;
        p->boom[i].y = y;
        p->boom[i].sub = 0;
        p->boom[i].frame = 0;
        p->boom[i].kind = kind;
        return;
    }
}

/* FUN_00403cf0: they step on every third frame. */
static void boom_draw(Game *g)
{
    Play *p = &g->p;
    int i;

    for (i = 0; i < BOOMS; i++) {
        Boom *b = &p->boom[i];

        if (b->on == 0) continue;
        b->sub = (b->sub + 1) % 3;
        if (b->sub == 0) b->frame++;
        if (b->kind == 0x10) {
            if (b->frame < 4) vid_pat(g->v, b->x, b->y, b->frame + 0x997);
            else b->on = 0;
        } else if (b->kind == 0xc0) {
            if (b->frame < 8)
                vid_pat_scale(g->v, b->x, b->y, b->frame + 0xa35, 0x300, 0x300);
            else b->on = 0;
        }
    }
}

/* FUN_00413bb0: the streaks slide left, and lean when the ship climbs. */
static void dust_move(Game *g)
{
    Play *p = &g->p;
    int i;

    for (i = 0; i < DUSTS; i++) {
        Dust *d = &p->dust[i];
        int lean = 0;

        if (game_up(g)) lean = 2 - (d->vy + 0x18) / 2;
        if (game_down(g)) lean = (d->vy + 0x18) / 2 - 2;
        if (game_up(g) && game_down(g)) lean = 0;
        d->x += d->vy;                  /* the field is the x step, despite
                                         * where it sits in the record */
        d->y += d->a + lean;
        if (d->k1 * 0x30 + d->vy + d->x < 0) {
            d->x += 700;
            d->y = game_rand(g) % 0x160;
            d->k1 = game_rand(g) % 3;
            d->k2 = game_rand(g) % 0xf;
            d->k3 = game_rand(g) % 2;
            d->k4 = game_rand(g) % 2;
        }
    }
}

/* FUN_00413d60: one or two 48x1 strips each, out of three colour banks. */
static void dust_draw(Game *g)
{
    Play *p = &g->p;
    int i, j;

    for (i = 0; i < DUSTS; i++) {
        const Dust *d = &p->dust[i];
        int base = EXT_BASE;            /* space.dar's ST3BLINE */

        if (d->k1 == 1) base = EXT_BASE + 0x10;
        if (d->k1 == 2) base = EXT_BASE + 0x20;
        if (d->k4 < 0) continue;
        for (j = 0; j <= d->k4; j++)
            vid_pat(g->v, d->x + j * 0x30, d->y, d->k2 + base);
    }
}

/* The starfield the space stage shares, when the boss has gone. */
static void star_move(Game *g)
{
    Play *p = &g->p;
    int i;

    if (p->drift_x == 0 && p->drift_y == 0) return;
    for (i = 0; i < STARS2; i++) {
        Cloud *st = &p->cloud[i];
        st->x += p->drift_x;
        st->y += p->drift_y;
        if (p->drift_x != 0) st->x += p->drift_x < 0 ? -st->kind : st->kind;
        if (p->drift_y != 0) st->y += p->drift_y < 0 ? -st->kind : st->kind;
        if (st->x < 0) { st->x += 0x280; st->y = game_rand(g) % 0x160; }
        if (st->x > 0x280) { st->x -= 0x280; st->y = game_rand(g) % 0x160; }
        if (st->y < 0) { st->y += 0x160; st->x = game_rand(g) % 0x280; }
        if (st->y > 0x160) { st->y -= 0x160; st->x = game_rand(g) % 0x280; }
    }
}

/* FUN_0040b080, the part this stage uses: which way the stars lean. */
static void drift(Play *p, int which)
{
    switch (which) {
    case 3: p->drift_x--; break;
    case 9:
        if (p->drift_x > 0) p->drift_x--;
        if (p->drift_x < 0) p->drift_x++;
        break;
    }
}

/* ---- the frame -------------------------------------------------------- */

void boss_frame(Game *g)
{
    Play *p = &g->p;
    Video *v = g->v;
    Enemy *b = &p->e[0];
    int cycle4 = p->stage / 4;
    int destroyed = 0;
    int i, j, k, n;

    if (g->hook_arg) {
        g->hook_arg = 0;
        game_scene(g, "space.dar", 0x32);
        play_field_build(g);
        play_clear_banners(g);          /* FUN_0040a9f0 */
        p->nenemy = 10;
        boom_clear(p);
        g->flash = 6;
        p->itemvy = 0;
        p->itemk = 0;
        p->itemy = 0;
        p->itemx = 0;
        p->item = 0;
        p->onscreen = 0;
        p->kills = 0;
        if (p->loaded == p->stage) {
            p->py = 0xaa;
            p->speed = 4;
            p->charges = 4;
            p->powerB = 0;
            p->powerA = 0;
            p->life = 10;
            p->px = 0x40;
            for (i = 0; i < STARS2; i++) {
                p->cloud[i].x = game_rand(g) % 0x280;
                p->cloud[i].y = game_rand(g) % 0x160;
                p->cloud[i].kind = game_rand(g) % 6;
            }
            for (i = 0; i < DUSTS; i++) {   /* FUN_00413ae0 */
                p->dust[i].x = game_rand(g) % 0x280;
                p->dust[i].y = game_rand(g) % 0x160;
                p->dust[i].vy = -0x1c - game_rand(g) % 8;
                p->dust[i].a = 0;
                p->dust[i].k1 = game_rand(g) % 3;
                p->dust[i].k2 = game_rand(g) % 0xf;
                p->dust[i].k3 = game_rand(g) % 2;
                p->dust[i].k4 = game_rand(g) % 2;
            }
            p->banner = 0x96;
        } else {
            p->announce = 0x96;
            p->announce_stage = p->stage;
        }
        p->loaded = p->stage;
        plat_bgm(3, "bgm06");
        p->inflight = 0;
        for (i = 0; i < UPSHOTS; i++) { p->up[i].y = -0x10; p->up[i].dx = 0; }
        for (i = 0; i < ENEMIES; i++) p->e[i].y = -0x20;
        for (i = 0; i < 4; i++) {
            p->gun_dx[i] = 0;
            p->gun_y[i] = 0x160;
        }
        p->quota = 999;
        p->itemt = 0;
        p->drift_x = 0;
        p->drift_y = 0;
        p->boss_live = 1;               /* DAT_00461330, which the sonar reads */
        p->boss_phase = 0;
        p->boss_hits = 0;
        p->boss_cool = 100;
        b->x = 800;
        b->y = 0;
        b->vx = -0x14;
        b->vy = 0x14;
        b->tick = 0;
        p->gunfire = 0;
        p->nab = 0;
        for (i = 0; i < ABOMBS; i++) {
            p->ab[i].t = 0;
            p->ab[i].y = 0x160;
            p->ab[i].vx = 0;
            p->ab[i].vy = 0;
        }
    }

    /* --- the ship ------------------------------------------------------ */
    if (p->life < 10) {
        if (g->frame % 2 != 0) p->life--;
        if (p->life < 1) {
            if (p->lives == 0) {
                if (p->life == 0) {
                    p->announce = 0;
                    p->over = 300;
                    plat_bgm(0, "bgm07");
                }
                if (p->life < -0x59) {
                    g->hook = HOOK_OVER;
                    g->hook_arg = 1;
                }
            } else {
                g->hook_arg = 1;
                p->lives--;
            }
        }
    } else {
        if (p->boss_phase < 200) {      /* once it is dying, the keys stop */
            if (game_right(g)) {
                n = p->px + p->speed;
                p->px = n < 0x20 ? 0x20 : (n < 0x221 ? n : 0x220);
            }
            if (game_left(g)) {
                n = p->px - p->speed;
                p->px = n < 0x20 ? 0x20 : (n < 0x221 ? n : 0x220);
            }
            if (game_up(g)) {
                n = p->py - p->speed;
                p->py = n < 0 ? 0 : (n < 0x141 ? n : 0x140);
            }
            if (game_down(g)) {
                n = p->py + p->speed;
                p->py = n < 0 ? 0 : (n < 0x141 ? n : 0x140);
            }
        }
        if (game_any_key(g) && p->inflight < p->charges + p->powerA * -2 &&
            p->boss_phase < 200) {
            int vx = (-2 - p->powerB) * 6;
            for (i = 0; i < p->charges && i < UPSHOTS; i++)
                if (p->up[i].y < -0xf) break;
            if (i < UPSHOTS) {
                p->up[i].x = p->px + 5;
                p->up[i].y = p->py + 0xc;
                p->up[i].vx = vx;
                p->up[i].dx = 0;
                p->inflight++;
                if (p->powerA == 1)
                    for (j = 0; j < 2; j++) {
                        for (k = 0; k < p->charges && k < UPSHOTS; k++)
                            if (p->up[k].y < -0xf) break;
                        if (k >= UPSHOTS) break;
                        p->up[k].x = p->px + 5;
                        p->up[k].y = p->py + 0xc;
                        p->up[k].vx = vx;
                        p->up[k].dx = j == 0 ? -1 : 1;
                        p->inflight++;
                    }
                plat_se("depth05", (p->up[i].x - 0x140) * 0x1f);
            }
        }
        if (game_btn2(g) && p->inflight < p->charges + p->powerA * -2 &&
            p->boss_phase < 200) {
            int vx = (p->powerB * 3 + 6) * 2;
            for (i = 0; i < p->charges && i < UPSHOTS; i++)
                if (p->up[i].y < -0xf) break;
            if (i < UPSHOTS) {
                p->up[i].x = p->px + 0x2c;
                p->up[i].y = p->py + 0xc;
                p->up[i].vx = vx;
                p->up[i].dx = 0;
                p->inflight++;
                if (p->powerA == 1)
                    for (j = 0; j < 2; j++) {
                        for (k = 0; k < p->charges && k < UPSHOTS; k++)
                            if (p->up[k].y < -0xf) break;
                        if (k >= UPSHOTS) break;
                        p->up[k].x = p->px + 0x2c;
                        p->up[k].y = p->py + 0xc;
                        p->up[k].vx = vx;
                        p->up[k].dx = j == 0 ? -1 : 1;
                        p->inflight++;
                    }
                plat_se("depth05", (p->up[i].x - 0x140) * 0x1f);
            }
        }
    }

    /* --- the boss ------------------------------------------------------- */
    p->onscreen = 0;
    if (p->boss_hits < 0x1e) {
        p->boss_cool++;
        if (p->boss_hits < 0xc) {
            /* It works its way in to one x and then the other, rocking up
             * and down, and opens its four ports now and then. */
            int fire = 1;

            if (p->boss_phase == 0) {
                if (p->boss_cool > 100 && p->gunfire == 0) {
                    b->vx = sgn(0x1d2 - b->x) * 7;
                    b->vy -= sgn(b->vy);
                    if (absi(b->vx - 0x1d2 + b->x) < 9) {
                        p->boss_phase = 1;
                        p->boss_cool = 0;
                        b->vy = (game_rand(g) % 2) * 8 - 4;
                        b->vx = 0;
                        fire = 0;
                    } else {
                        fire = p->boss_cool < 0x65;
                    }
                } else {
                    fire = p->boss_cool < 0x65 ? 1 : (p->gunfire == 0 ? 0 : 1);
                    if (p->boss_cool >= 0x65 && p->gunfire != 0) fire = 1;
                }
            }
            if (p->boss_phase == 1) {
                if (p->boss_cool >= 0x65 && p->gunfire == 0) {
                    b->vx = sgn(0x2e - b->x) * 6;
                    b->vy -= sgn(b->vy);
                    if (absi(b->vx - 0x2e + b->x) < 9) {
                        p->boss_phase = 0;
                        p->boss_cool = 0;
                        b->vy = (game_rand(g) % 2) * 8 - 4;
                        b->vx = 0;
                    }
                }
                fire = p->boss_cool < 0x65;
            }
            if (fire) {
                b->vx = 0;
                if (game_rand(g) % 10 == 0 && p->gunfire == 0) {
                    /* the four ports open, up or down depending on which
                     * side of its walk it is on */
                    if (p->boss_phase == 0) {
                        p->gun_x[0] = b->x + 0x50;
                        p->gun_x[1] = b->x + 0x68;
                        p->gun_dx[0] = 8;
                        p->gun_dx[1] = 8;
                        p->gun_dx[2] = 8;
                        p->gun_dx[3] = 8;
                    } else {
                        p->gun_x[0] = b->x + 0x10;
                        p->gun_x[1] = b->x - 8;
                        p->gun_dx[0] = -8;
                        p->gun_dx[1] = -8;
                        p->gun_dx[2] = -8;
                        p->gun_dx[3] = -8;
                    }
                    p->gunfire = 4;
                    p->gun_y[0] = b->y + 6;
                    p->gun_y[1] = b->y + 0x18;
                    p->gun_y[2] = b->y + 0x39;
                    p->gun_y[3] = b->y + 0x4b;
                    p->gun_x[2] = p->gun_x[0];
                    p->gun_x[3] = p->gun_x[1];
                }
            }
            if (b->vy + b->y < 1 || b->vy + b->y > 0xff) b->vy = -b->vy;
        } else {
            /* Past a dozen hits it comes after the ship and lobs bombs. */
            b->tick = (b->tick + 1) % 4;
            if (b->tick == 0) {
                int lim = p->speed - 2;

                if (p->px + 0x28 < b->x + 0x30) {
                    n = b->vx - 1;
                    b->vx = n < -lim ? -lim : (n > lim ? lim : n);
                }
                if (b->x + 0x30 < p->px + 8) {
                    n = b->vx + 1;
                    b->vx = n < -lim ? -lim : (n > lim ? lim : n);
                }
                if (p->py + 0x1c < b->y + 0x30) {
                    n = b->vy - 1;
                    b->vy = n < -lim ? -lim : (n > lim ? lim : n);
                }
                if (b->y + 0x30 < p->py + 4) {
                    n = b->vy + 1;
                    b->vy = n < -lim ? -lim : (n > lim ? lim : n);
                }
            }
            if (g->frame % 8 == 0 && game_rand(g) % (10 - p->speed) == 0 &&
                p->nab < 0x10 && b->x > 0 && b->x < 0x201) {
                int slot = 0, d;

                for (i = 0; i < ABOMBS; i++)
                    if (p->ab[i].y > 0x15f) slot = i;
                p->ab[slot].y = b->y + 0x20 + (game_rand(g) % 2) * 0x18;
                p->ab[slot].x = b->x + 0x20 + (game_rand(g) % 2) * 0x38;
                d = p->px - p->ab[slot].x;
                if (absi(d + 0x20) < 0x33) {
                    p->ab[slot].vx = game_rand(g) % 2 - 1;
                } else {
                    n = sgn(d);
                    p->ab[slot].vx = (game_rand(g) % 5 + 3) * n;
                }
                d = p->py - p->ab[slot].y;
                if (absi(d + 0x10) < 0x33) {
                    p->ab[slot].vy = game_rand(g) % 2 - 1;
                } else {
                    n = sgn(d);
                    p->ab[slot].vy = (game_rand(g) % 5 + 3) * n;
                }
                p->nab++;
            }
        }
    } else {
        /* It is coming apart. */
        b->vy = 0;
        p->boss_phase++;
        b->vx = 0;
        if (p->boss_phase < 0x78) {
            int ey = game_rand(g) % 0x7a - 0x10 + b->y;
            boom_add(p, game_rand(g) % 0x90 - 0x10 + b->x, ey, 0x10);
            if (p->boss_phase % 0x14 == 0)
                plat_se("burn", (b->x - 0x140) * 0x1f);
        }
        if (p->boss_phase == 0x6e) {
            g->flash = 0x14;
            boom_add(p, b->x + 0x18, b->y + 0x18, 0xc0);
        }
        if (p->boss_phase == 0x78) {
            p->boss_live = 0;
            for (i = 0; i < DUSTS; i++) {       /* FUN_00413ae0 again */
                p->dust[i].x = game_rand(g) % 0x280;
                p->dust[i].y = game_rand(g) % 0x160;
                p->dust[i].vy = -0x1c - game_rand(g) % 8;
                p->dust[i].a = 0;
                p->dust[i].k1 = game_rand(g) % 3;
                p->dust[i].k2 = game_rand(g) % 0xf;
                p->dust[i].k3 = game_rand(g) % 2;
                p->dust[i].k4 = game_rand(g) % 2;
            }
            p->score += play_score_of(p, 1);
            plat_se("burn", 0);
            for (i = 0; i < 8; i++) {           /* the eight pieces */
                Enemy *q = &p->e[i + 1];
                q->state = 9;
                q->x = PIECE_X[i] + b->x;
                q->y = b->y + PIECE_Y[i];
                q->vx = PIECE_VX[i];
                q->vy = PIECE_VY[i];
            }
        }
        if (p->boss_phase == 200) plat_bgm(0, "bgm11");
        destroyed = p->boss_phase > 199;
        if (p->boss_phase == 0x1b8) {
            p->stage = 1;               /* round we go again */
            g->hook = HOOK_PLAY;
            g->hook_arg = 1;
        }
    }

    /* --- the ship's shots ----------------------------------------------- */
    for (k = 0; k < p->charges && k < UPSHOTS; k++) {
        UpShot *s = &p->up[k];

        if (s->y <= -0x10) continue;
        s->x += s->vx;
        s->y += (p->powerB + 2) * s->dx * 2;
        /* `stage / 4 == 1` is what the original tests, so at stage 8 and 12
         * the boss cannot be hit at all.  Kept as it is. */
        if (cycle4 == 1 && s->x >= b->x && s->x <= b->x + 0x70 &&
            s->y >= b->y - 8 && s->y <= b->y + 0x58 && p->boss_hits < 0x1e) {
            if (s->y >= b->y + 0x1c && s->y <= b->y + 0x34) {
                plat_se("depth06", (s->x + 8 - 0x140) * 0x1f);
                boom_add(p, s->x, s->y, 0x10);
                p->boss_hits++;
                p->score += play_score_of(p, 2);
            }
            s->y = -0x10;
        }
        if (s->x < 0 || s->x > 0x26f || s->y > 0x15f) s->y = -0x10;
        if (s->y < -0xf) p->inflight--;
    }

    /* --- what it throws ------------------------------------------------- */
    if (p->nab >= 1 && p->boss_phase < 200) {
        for (i = 0; i < ABOMBS; i++) {
            ABomb *a = &p->ab[i];

            if (a->y < 0) a->y = 0x160;
            if (a->y >= 0x160) continue;
            if (a->t > 0) {             /* nothing ever sets this in the beta */
                a->t--;
                a->vx += sgn((p->px - a->x) + 0x20) * p->flip;
                a->vy += sgn((p->py - a->y) + 0x10) * p->flip;
                if (absi(a->vx) > 8) a->vx = sgn(a->vx) * 8;
                if (absi(a->vy) > 8) a->vy = sgn(a->vy) * 8;
            }
            a->y += a->vy;
            a->x += a->vx;
            if (a->y < 0 || a->x < 0 || a->x > 0x277) {
                a->y = 0x160;
                p->nab--;
                continue;
            }
            if (p->life == 10 && a->x >= p->px && a->x <= p->px + 0x3a &&
                a->y >= p->py + 6 && a->y <= p->py + 0x1a && g->nodie == 0) {
                p->life = 9;
                a->y = 0x160;
                plat_se("burn", (p->px + 0x20 - 0x140) * 0x1f);
            }
            if (a->y >= 0x160 || a->y < 0) p->nab--;
        }
    }
    /* the four ports, which fly out sideways */
    if (p->boss_phase < 200 && cycle4 == 1 && p->gunfire > 0)
        for (i = 0; i < 4; i++) {
            if (p->gun_y[i] >= 0x160) continue;
            p->gun_x[i] += p->gun_dx[i];
            if (p->gun_x[i] < -0x20 || p->gun_x[i] > 0x25f) {
                p->gun_y[i] = 0x160;
                p->gunfire--;
            } else if (p->life == 10 && g->nodie == 0 &&
                       p->gun_x[i] >= p->px - 0x34 &&
                       p->gun_x[i] <= p->px + 0x34 &&
                       p->gun_y[i] >= p->py + 6 &&
                       p->gun_y[i] <= p->py + 0x1a) {
                p->life = 9;
                plat_se("burn", (p->px + 0x20 - 0x140) * 0x1f);
            }
        }

    /* --- running into it ------------------------------------------------ */
    if (g->nodie == 0 && cycle4 == 1 &&
        p->px > b->x - 0x30 && p->px < b->x + 0x70 &&
        p->py > b->y - 0x14 && p->py < b->y + 0x54) {
        if (p->boss_phase < 0x78) {
            if (p->life > 9) {
                p->life = 9;
                plat_se("burn", (p->px + 0x20 - 0x140) * 0x1f);
            }
            drift(p, 3);
        } else if (g->frame % 4 == 0 && p->drift_x < -2) {
            drift(p, 9);
        }
    } else if (p->boss_phase > 0x77) {
        if (g->frame % 4 == 0 && p->drift_x < -2) drift(p, 9);
    } else {
        drift(p, 3);
    }
    star_move(g);

    /* --- the background ------------------------------------------------- */
    if (p->boss_phase < 0x78) {
        dust_move(g);
        dust_draw(g);
    } else {
        for (i = 0; i < STARS2; i++)
            vid_pat(v, p->cloud[i].x, p->cloud[i].y, p->cloud[i].kind + 0xb30);
    }

    /* --- the pieces ----------------------------------------------------- */
    for (i = 1; i < 8; i++) {
        Enemy *q = &p->e[i];

        if (q->state >= 10) continue;
        if (g->frame % 4 == 0) q->state--;
        if (q->state < 0) {
            q->x = 0;
            q->y = 0;
            q->vx = 0;
            q->vy = 0;
            q->state = 10;
        } else {
            q->x += q->vx;
            q->y += q->vy;
            play_boom(v, q->x, q->y, 9 - q->state);
        }
    }

    /* --- and everything else -------------------------------------------- */
    for (i = 0; i < 4; i++)
        if (p->gun_y[i] < 0x160)
            vid_pat(v, p->gun_x[i], p->gun_y[i], 0xa47);
    if (p->boss_phase < 0x78) {
        int jx = 0, jy = 0;
        if (p->boss_phase > 1) {
            jx = (game_rand(g) % 5) * 2 - 4;
            jy = (game_rand(g) % 5) * 2 - 4;
        }
        vid_pat(v, b->x + jx, b->y + jy, 0xb2d);        /* depth.dar's boss1 */
        b->x += b->vx;
        b->y += b->vy;
    }
    boom_draw(g);
    for (i = 0; i < p->charges && i < UPSHOTS; i++)
        if (p->up[i].y > -0x10) {
            n = p->up[i].dx + 0x9a9 + p->powerB * 9;
            if (p->up[i].vx > 0) n += 3;
            vid_pat(v, p->up[i].x, p->up[i].y, n);
        }
    for (i = 0; i < ABOMBS; i++)
        if (p->ab[i].y < 0x160 && p->ab[i].y >= 0 && p->boss_phase < 200)
            vid_pat(v, p->ab[i].x, p->ab[i].y, p->flip + 0xa41);
    if (p->life < 10) play_boom(v, p->px, p->py - 0x10, 9 - p->life);
    else vid_pat(v, p->px, p->py, 0xa05);
    if (destroyed) vid_text(v, 0x1e, 0xb, "Destroyed!", FNT_CYAN);

    play_status_bar(g);
    p->flip ^= 1;

    if (game_edge(g, PAD_ESC) || game_start_key(g)) {
        if (g->demo == 0) {
            p->saved_hook = g->hook;
            g->hook = HOOK_PAUSE;
            g->hook_arg = 1;
        } else if (g->demo == 1) {
            game_set_state(g, ST_PLAY_END);
            g->hook = HOOK_NONE;
            g->hook_arg = 1;
        } else if (g->demo == 2) {
            game_set_state(g, ST_LOGO);
            g->hook = HOOK_NONE;
            g->hook_arg = 1;
        }
    }
}
