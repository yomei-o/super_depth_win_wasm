/* FUN_0040c9e0 and FUN_0040f490 - the stage after a sea stage, and its own
 * clear.  Same ship, same score, same lives; the sea is at the bottom of the
 * screen now and the enemies are aircraft, so the ship shoots upward instead
 * of dropping charges.  `disk/depth3.jpg` is a screenshot of it.
 *
 * The enemies live in the same 64 slots as the sea stage (FUN_0040aa20
 * builds them), with one difference that matters everywhere: **a free slot
 * has y <= -0x20 here**, not y == 0.  Three new arrays hold what flies:
 *
 *     the ship's shots    DAT_00461358, 16   (y < -0xf is free)
 *     dropped bombs       DAT_00461a70, 16   (y >= 0x160 is free)
 *     aimed bombs         DAT_00463dd8, 16   (y >= 0x160 is free)
 */
#include "game.h"

#include <stdio.h>
#include <string.h>

static int sgn(int v) { return v > 0 ? 1 : (v < 0 ? -1 : 0); }

/* FUN_0040b850: do two boxes touch.  Written as the four corner tests the
 * original does rather than as one comparison. */
static int overlap(int x1, int y1, int x2, int y2, int w1, int h1,
                   int w2, int h2)
{
    if (x1 <= x2 && x2 < x1 + w1 && y1 <= y2 && y2 < y1 + h1) return 1;
    if (x2 <= x1 && x1 < x2 + w2 && y2 <= y1 && y1 < y2 + h2) return 1;
    if (x1 <= x2) {
        if (x2 < x1 + w1 && y2 <= y1 && y1 < y2 + h2) return 1;
        if (x1 < x2) return 0;
    }
    if (x1 < x2 + w2 && y1 <= y2 && y2 < y1 + h1) return 1;
    return 0;
}

/* The scans the original writes inline, which keep looking after they have
 * found a free slot - so they end up with the last one.  `slot` carries the
 * register's previous value in, exactly as over there. */
static int last_free_bomb(Play *p, int slot)
{
    int i;

    for (i = 0; i < BOMBS; i++)
        if (p->bomb[i].y > 0x15f) slot = i;
    return slot;
}

static int last_free_abomb(Play *p, int slot)
{
    int i;

    for (i = 0; i < ABOMBS; i++)
        if (p->ab[i].y > 0x15f) slot = i;
    return slot;
}

/* The ship's own shots are looked for the other way round: the first free
 * one, and the register keeps the previous value when there is none. */
static int first_free_shot(Play *p, int slot)
{
    int i;

    for (i = 0; i < p->charges && i < UPSHOTS; i++)
        if (p->up[i].y < -0xf) return i;
    return slot;
}

/* The jittery homing kind 1 and kind 5 use: two random numbers deep. */
static int nudge(Game *g, int ex)
{
    Play *p = &g->p;
    int t = (game_rand(g) % 16 - ex) + p->px;

    if (t >= 1) return 1;
    t = (game_rand(g) % 16 - ex) + p->px;
    return t >= 0 ? 0 : -1;
}

static void fire(Game *g, int x, int y)
{
    Play *p = &g->p;
    int k, slot = 0;

    if (p->inflight >= p->charges + p->powerA * -2) return;
    k = first_free_shot(p, slot);
    p->up[k].y = y;
    p->up[k].x = x;
    p->up[k].dx = 0;
    p->inflight++;
    if (p->powerA == 1) {               /* three at a time */
        k = first_free_shot(p, k);
        p->up[k].y = y;
        p->up[k].x = x;
        p->up[k].dx = -1;
        k = first_free_shot(p, k);
        p->up[k].y = y;
        p->up[k].x = x;
        p->up[k].dx = 1;
        p->inflight += 2;
    }
    plat_se("depth05", ((x - 0x140) * 0x1f > 10000) ? 10000 :
                       (((x - 0x140) * 0x1f < -10000) ? -10000 : (x - 0x140) * 0x1f));
}

/* ---- the enemies ------------------------------------------------------ */

static void air_enemy(Game *g, int i, int *slot)
{
    Play *p = &g->p;
    Enemy *e = &p->e[i];
    int j, d, t, k;

    switch (e->kind) {
    case 1:
    case 5:                             /* the same flight, kind 5 dodges */
        if (game_rand(g) % 0x78 == 0 && e->y < -0x1f) {
            if (p->kills < p->quota) {
                e->y = game_rand(g) % 100 + 0x3c;
                e->x = (game_rand(g) % 2) * 0x4e0 - 0x140;
                e->vx = (game_rand(g) % 3 + 1) * sgn(0x140 - e->x);
                e->vy = game_rand(g) % 4 + 2;
                e->aim = game_rand(g) % 0x32 + (0x32 - e->vy) * 3;
                e->w = 0x20;
                e->h = 0x20;
                if (p->quota <= p->kills) e->vy = -8;
            } else {
                e->vy = -8;
            }
        } else if (p->quota <= p->kills) {
            e->vy = -8;
        }
        if (e->vx + e->x < -0x13f || e->vx + e->x > 0x39f) {
            e->y = -0x20;
            e->vx += nudge(g, e->x);
        }
        if (e->y > -0x20) {
            if (p->quota <= p->kills) e->vx = sgn(e->vx) * 10;
            if (game_rand(g) % ((10 - p->powerA) * 0xf + p->powerB * -0x14) == 0 &&
                p->nbomb < 0x10 && e->x > 0 && e->x < 0x261) {
                *slot = last_free_bomb(p, *slot);
                p->bomb[*slot].y = e->y + 0xc;
                p->bomb[*slot].x = e->x + 8;
                p->bomb[*slot].vy = 0;
                p->nbomb++;
            }
            if (sgn(e->vx) != sgn((p->px - e->x) + 0x10) || p->flip != 0)
                e->vx += nudge(g, e->x);
            if (e->vx > 10 || e->vx < -10) e->vx -= sgn(e->vx);
            if (e->aim < e->y) e->vy--;
            if (e->y < 0x1e) e->vy++;
            if (e->kind == 5) {
                /* it leans away from anything coming up at it */
                for (j = 0; j < p->charges && j < UPSHOTS; j++) {
                    if (p->up[j].y <= -0x10) continue;
                    if (overlap(e->x, e->y, p->up[j].x - 0x70, p->up[j].y - 0x78,
                                4, 4, 0x98, 0x88)) {
                        int v = e->vx - 2;
                        e->vx = v;
                        if (v < -6) e->vx = e->vx + 1;
                    }
                    if (overlap(e->x, e->y, p->up[j].x + 8, p->up[j].y - 0x78,
                                4, 4, 0x90, 0x88)) {
                        int v = e->vx + 2;
                        e->vx = v;
                        if (v > 6) e->vx = e->vx - 1;
                    }
                }
            }
        }
        break;

    case 2:
        if (game_rand(g) % 0x1e == 0 && e->y < -0x1f && p->kills < p->quota) {
            e->y = (game_rand(g) % 3 + 1) * 0x20;
            e->x = (game_rand(g) % 2) * 0x4c0 - 0x140;
            e->vx = (game_rand(g) % 4 + 2) * sgn(0x140 - e->x);
            e->vy = 0;
            e->w = 0x40;
            e->h = 0x20;
        }
        if (e->x + e->vx < -0x13f || e->x + e->vx > 0x37f) e->y = -0x20;
        if (e->y > -0x20) {
            if (p->quota <= p->kills) e->vx = sgn(e->vx) * 10;
            if (game_rand(g) % ((0x12 - p->powerA) * 10 + p->powerB * -0xd) == 0 &&
                p->nbomb < 0x10 && e->x > 0 && e->x < 0x241) {
                *slot = last_free_bomb(p, *slot);
                p->bomb[*slot].y = e->y + 0x14;
                p->bomb[*slot].x = e->x + 0x10;
                p->bomb[*slot].vy = 0;
                p->nbomb++;
            }
        }
        break;

    case 3:                             /* dives in from the top and pulls up */
        if (game_rand(g) % ((0xe - p->powerB) * 0x1e + p->powerA * -0x14) == 0 &&
            e->y < -0x1f && p->kills < p->quota) {
            e->y = -0x1f;
            e->x = game_rand(g) % 0x200 + 0x20;
            e->vx = (game_rand(g) * sgn(0x140 - e->x)) % 4;
            e->vy = game_rand(g) % 5 + 3;
            e->w = 0x40;
            e->h = 0x20;
        }
        if (e->y > -0x20) {
            if (e->y > 0x32 && g->frame % 4 == 0) e->vy--;
            if (p->quota <= p->kills) e->vx = sgn(e->vx) * 10;
            if (game_rand(g) % ((9 - p->powerA) * 10 + p->powerB * -0xd) == 0 &&
                p->nab < 0x10) {
                *slot = last_free_abomb(p, *slot);
                k = *slot;
                p->ab[k].y = e->y + 0x18;
                p->ab[k].x = e->x + 0x20;
                t = p->px - e->x;
                if (t < 0) t = -t;
                if (t < 0x21) {
                    p->ab[k].vy = game_rand(g) % 2 + 2;
                    p->ab[k].vx = game_rand(g) % 3 - 1;
                } else {
                    p->ab[k].vy = game_rand(g) % 4 + 3;
                    d = sgn(p->px - p->ab[k].x);
                    p->ab[k].vx = (game_rand(g) % 3 + 1) * d;
                }
                p->nab++;
            }
        }
        break;

    case 4:                             /* crosses to a mark and lets five go */
        if (game_rand(g) % 0x3c == 0 && e->y < -0x1f && p->kills < p->quota) {
            e->y = (game_rand(g) % 5 + 1) * 0x20;
            e->x = (game_rand(g) % 2) * 0x4c0 - 0x140;
            e->vx = sgn(0x140 - e->x) * 5;
            e->vy = 0;
            e->aim = game_rand(g) % 0x240 + 0x20;
            e->w = 0x40;
            e->h = 0x20;
        }
        if (e->vx + e->x < -0x13f || e->vx + e->x > 0x37f) e->y = -0x20;
        if (e->y > -0x20) {
            if (p->quota <= p->kills) e->vx = sgn(e->vx) * 10;
            if ((e->x < e->aim) != (e->x + e->vx < e->aim)) {
                int n;
                for (n = 0; n < 5; n++) {
                    *slot = last_free_abomb(p, *slot);
                    k = *slot;
                    p->ab[k].y = e->y + 0xe;
                    p->ab[k].x = e->x + 0xe;
                    /* The original reads the ENEMY at the bomb's own slot
                     * index here, not this enemy - so the aim comes out of
                     * whatever is in that slot.  Kept as it is. */
                    t = p->px - p->e[k].x;
                    if (t < 0) t = -t;
                    if (t < 0x21) {
                        p->ab[k].vy = game_rand(g) % 3 + 2;
                        p->ab[k].vx = game_rand(g) % 3 - 1;
                    } else {
                        p->ab[k].vy = game_rand(g) % 3 + 2;
                        d = sgn(p->px - p->ab[k].x);
                        p->ab[k].vx = (game_rand(g) % 3 + 1) * d;
                    }
                    p->nab++;
                }
                e->state = 9;           /* and it goes up with them */
                e->vx = 0;
                e->vy = 0;
            }
        }
        break;

    case 9:
        if (game_rand(g) % 0x2d == 0 && e->y < -0x1f) {
            if (p->kills < p->quota) {
                e->aim = (game_rand(g) % 3 + 3) * 0x20;
                e->y = (game_rand(g) % 2) * 0x40 - 0x20 + e->aim;
                e->x = (game_rand(g) % 2) * 0x4e0 - 0x140;
                e->vx = (game_rand(g) % 3 + 3) * sgn(0x140 - e->x);
                e->vy = 0;
                e->w = 0x20;
                e->h = 0x20;
                if (p->quota <= p->kills) e->vx = sgn(e->vx) * 10;
            } else {
                e->vx = sgn(e->vx) * 10;
            }
        } else if (p->quota <= p->kills) {
            e->vx = sgn(e->vx) * 10;
        }
        if (e->vx + e->x < -0x13f || e->vx + e->x > 0x39f) {
            e->y = -0x20;
            e->vy = 0;
        }
        if (e->y > -0x20) e->vy += sgn(e->aim - e->y);
        break;
    }
}

/* ---- the frame -------------------------------------------------------- */

void air_frame(Game *g)
{
    Play *p = &g->p;
    Video *v = g->v;
    const DarPat *sky1, *sky2, *sky3, *sea;
    int i, j, k, x, n, slot = 0;

    if (g->hook_arg) {
        g->hook_arg = 0;
        play_field_build(g);            /* FUN_0040aa20 */
        play_clear_banners(g);          /* FUN_0040a9f0 */
        p->over = 0;
        p->banner = 0;
        game_scene(g, "depth1.dar", 9);
        g->flash = 0;
        p->nenemy = 10;
        for (i = 0; i < UPSHOTS; i++) p->up[i].y = -0x10;
        for (i = 0; i < BOMBS; i++) p->bomb[i].y = 0x160;
        for (i = 0; i < ABOMBS; i++) {
            p->ab[i].y = 0x160;
            p->ab[i].vx = 0;
            p->ab[i].vy = 0;
        }
        p->itemvy = 0;
        p->itemk = 0;
        p->itemy = 0;
        p->itemx = 0;
        p->item = 0;
        p->nab = 0;
        p->nbomb = 0;
        p->py = 0x120;
        p->onscreen = 0;
        p->kills = 0;
        if (p->loaded == p->stage) {
            p->speed = 2;
            p->charges = 4;
            p->powerB = 0;
            p->powerA = 0;
            p->life = 10;
            p->px = 0x120;
            p->banner = 0x96;           /* FUN_0040a8c0 */
        } else {
            p->announce = 0x96;         /* FUN_0040a810 */
            p->announce_stage = p->stage;
            p->loaded = p->stage;
        }
        p->inflight = 0;
        p->wob2 = 0;
        p->itemt = 0;
        p->quota = ((p->powerA & p->powerB) + (p->stage / 4)) * 10 + 0x2d;
        plat_bgm(3, "bgm04");
    }

    /* --- the ship ------------------------------------------------------ */
    p->pdx = 0;
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
        if (game_right(g)) p->pdx += p->speed;
        if (game_left(g)) p->pdx -= p->speed;
        if (game_left(g) && game_right(g)) p->pdx = 0;
        p->px += p->pdx;
        if (p->px < 0x20) p->px = 0x20;
        else if (p->px > 0x220) p->px = 0x220;
        if (game_any_key(g)) fire(g, p->px + 5, p->py + 0x10);
        if (game_btn2(g)) fire(g, p->px + 0x2c, p->py + 0x10);
    }

    /* --- the aircraft --------------------------------------------------- */
    p->onscreen = 0;
    for (i = 0; i < p->nenemy; i++) {
        Enemy *e = &p->e[i];

        if (e->y > -0x20) p->onscreen++;
        if (e->state < 10) {
            if (p->flip != 0) e->state--;
            if (e->state < 0) {
                e->y = -0x20;
                e->state = 10;
            }
        } else {
            air_enemy(g, i, &slot);
        }
    }

    /* chain reactions, with the air stage's own boxes */
    if (p->echain) {
        for (i = 0; i < p->nenemy; i++) {
            Enemy *a = &p->e[i];
            if (a->state <= 3 || a->state >= 9) continue;
            for (j = 0; j < p->nenemy; j++) {
                Enemy *b = &p->e[j];
                int right;
                if (b->y <= -0x20 || b->state != 10) continue;
                switch (b->kind) {
                case 1: case 5: right = b->x + 0x18; break;
                case 2: case 3: case 4: right = b->x + 0x38; break;
                case 9: right = b->x + 0x18; break;
                default: continue;
                }
                if (a->x < b->x - 0x36 || a->x > right) continue;
                if (a->y < b->y - 0x36 || a->y > b->y + 0x18) continue;
                play_enemy_hit(g, j, a->chain + 1);
                p->kills++;
                if (b->kind == 9 && p->item == 0 && b->x > 0x1f && b->x < 0x241) {
                    play_item_pick(g);
                    p->itemk = 0;
                    p->itemx = b->x + 8;
                    p->itemvy = 4;      /* it falls, unlike at sea */
                    p->itemy = b->y + 8;
                    p->itemt = -1;
                }
            }
        }
    }

    /* --- the ship's shots ----------------------------------------------- */
    for (k = 0; k < p->charges && k < UPSHOTS; k++) {
        UpShot *s = &p->up[k];
        int step;

        if (s->y <= -0x10) continue;
        step = p->powerB + 2;
        s->y -= step * 3;
        s->x += s->dx * step * 2;
        for (j = 0; j < p->nenemy; j++) {
            Enemy *e = &p->e[j];
            int left, right, top;
            if (e->state < 10 || e->y <= -0x20) continue;
            switch (e->kind) {
            case 1: case 5:
                left = e->x - 8; right = e->x + 0x18; top = e->y - 0xc; break;
            case 2: case 3: case 4:
                left = e->x - 8; right = e->x + 0x38; top = e->y - 0xc; break;
            case 9:
                left = e->x - 8; right = e->x + 0x18; top = e->y - 0xc; break;
            default:
                continue;
            }
            if (s->x < left || s->x > right) continue;
            if (s->y < top || s->y > e->y + 0x18) continue;
            s->y = -0x10;
            if (e->kind == 9 && p->item == 0 && e->x > 0x1f && e->x < 0x241) {
                play_item_pick(g);
                p->itemx = e->x + 8;
                p->itemy = e->y + 8;
                p->itemk = 0;
                p->itemvy = 4;
                p->itemt = -1;
            }
            play_enemy_hit(g, j, 1);
            p->kills++;
        }
        if (s->x < 0x10 || s->x > 0x260) s->y = -0x10;
        if (s->y < -0xf) p->inflight--;
    }

    /* --- what they drop ------------------------------------------------- */
    for (i = 0; i < BOMBS; i++) {
        Bomb *b = &p->bomb[i];

        if (b->y >= 0x160) continue;
        if (b->vy < 8 && p->flip != 0) b->vy++;
        b->y += b->vy;
        if (p->life == 10 && p->hit == 0 &&
            b->x >= p->px - 4 && b->x <= p->px + 0x34 &&
            b->y >= p->py && b->y <= p->py + 0x18) {
            p->life = 9;
            b->y = 0x160;
            plat_se("burn", 0);
        }
        if (b->y > 0x15f) p->nbomb--;
    }
    for (i = 0; i < ABOMBS; i++) {
        ABomb *b = &p->ab[i];

        if (b->y >= 0x160) continue;
        b->y += b->vy;
        b->x += b->vx;
        if (b->x < 0x1c || b->x > 0x260) {
            b->y = 0x160;
            p->nab--;
            continue;
        }
        if (p->life == 10 && p->hit == 0 &&
            b->x >= p->px - 2 && b->x <= p->px + 0x3a &&
            b->y >= p->py + 6 && b->y <= p->py + 0x1a) {
            p->life = 9;
            b->y = 0x160;
            plat_se("burn", 0);
        }
        if (b->y >= 0x160 || b->y < 0) p->nab--;
    }

    /* --- the item ------------------------------------------------------- */
    if (p->item != 0) {
        if (p->itemt < 0) p->itemy += p->itemvy;
        else p->itemt--;
        if (p->itemx >= p->px - 4 && p->itemx <= p->px + 0x34 &&
            p->itemy >= p->py + 8 && p->itemy <= p->py + 0x20 && p->life == 10) {
            if (p->item == 4) {
                for (j = 0; j < ENEMIES; j++) {
                    Enemy *e = &p->e[j];
                    if (e->state == 10 && e->y != -0x20 &&
                        e->x > -0x20 && e->x < 0x260) {
                        p->kills++;
                        play_enemy_hit(g, j, 1);
                    }
                }
                for (j = 0; j < UPSHOTS; j++) p->up[j].y = -0x10;
                p->inflight = 0;
                p->nbomb = 0;
                p->nab = 0;
                for (j = 0; j < BOMBS; j++) p->bomb[j].y = 0x160;
                for (j = 0; j < ABOMBS; j++) p->ab[j].y = 0x160;
                plat_se("burn", 0);
            } else {
                play_item_apply(p);
            }
            p->item = 0;
            plat_se("item", 0);
        }
        if (p->itemt < 0 && p->itemy > 0x12f) {
            p->itemy = 0x130;
            p->itemt = 0x78;
        } else if (p->itemt == 0) {
            p->item = 0;
        }
    }

    if (g->frame % 8 == 0) p->wob2 ^= 1;

    /* --- the sky and the sea -------------------------------------------- */
    sky1 = vid_pat_info(v, EXT_BASE + 5);
    sky2 = vid_pat_info(v, EXT_BASE + 6);
    sky3 = vid_pat_info(v, EXT_BASE + 7);
    sea = vid_pat_info(v, EXT_BASE);
    if (!sky1 || !sky2 || !sky3 || !sea) return;
    for (x = 0x20; x < 0x260; x += sky3->w)
        vid_pat(v, x, 0, EXT_BASE + 7);
    for (x = 0x20; x < 0x260; x += sky2->w)
        for (i = 0; i < 4; i++)
            vid_pat(v, x, sky2->h * i + sky3->h, EXT_BASE + 6);
    for (x = 0x20; x < 0x260; x += sky1->w)
        vid_pat(v, x, (p->py - sky1->h) + 0x1a, EXT_BASE + 5);
    for (x = 0x20; x < 0x260; x += sea->w)
        vid_pat(v, x, p->wob2 + 0x19 + p->py, EXT_BASE);

    /* --- everything moves and is drawn ---------------------------------- */
    for (i = 0; i < p->nenemy; i++)
        if (p->e[i].y > -0x20) {
            p->e[i].x += p->e[i].vx;
            p->e[i].y += p->e[i].vy;
        }
    for (i = 0; i < p->nenemy; i++) {
        Enemy *e = &p->e[i];
        int left;

        if (e->y < -0x1f) continue;
        left = e->kind != 2 ? 0 : -0x20;
        if (e->x < left || e->x > 0x25f) continue;
        if (e->state < 10) {
            switch (e->kind) {
            case 1: case 9:
                play_boom(v, e->x - 0x10, e->y - 0x10, 9 - e->state);
                break;
            case 2: case 3: case 4: case 5:
                play_boom(v, e->x, e->y - 0x10, 9 - e->state);
                break;
            }
            continue;
        }
        switch (e->kind) {
        case 1:
        case 5:
            /* The sprite banks with the speed, in five steps, and the two
             * frames alternate: the original writes it as four
             * `(cond - 1) & 0xfffffffe` terms, which are 0 or -2. */
            n = (e->kind == 1 ? 0x9e5 : 0xa03) - 4 + p->flip
                + (e->vx > 2 ? 0 : -2)
                + (e->vx > 7 ? 0 : -2)
                - (e->vx < -7 ? 0 : -2)
                - (e->vx < -2 ? 0 : -2);
            break;
        case 2: n = (e->vx < 1) + 0xa25; break;
        case 3: n = p->flip * 8 + 0xa21; break;
        case 4: n = 0xa34 - (e->vx > 0 ? 2 : 0); break;
        case 9: n = 0x9ca - (e->vx > 0 ? 1 : 0); break;
        default: continue;
        }
        vid_pat(v, e->x, e->y, n);
    }
    for (i = 0; i < p->charges && i < UPSHOTS; i++)
        if (p->up[i].y > -0x10)
            vid_pat(v, p->up[i].x, p->up[i].y,
                    p->up[i].dx + p->powerB * 9 + 0x9a6);
    for (i = 0; i < BOMBS; i++)
        if (p->bomb[i].y < 0x160)
            vid_pat(v, p->bomb[i].x, p->bomb[i].y,
                    (int)(g->frame % 4) + 0x993);
    for (i = 0; i < ABOMBS; i++)
        if (p->ab[i].y < 0x160 && p->ab[i].y >= 0)
            vid_pat(v, p->ab[i].x, p->ab[i].y, p->flip + 0xa41);
    if (p->life < 10) play_boom(v, p->px, p->py - 0x10, 9 - p->life);
    else vid_pat(v, p->px, p->wob2 + p->py, 0xa05);

    if (p->item != 0) {
        if (p->itemt > 0x2d || p->itemt < 1 || p->flip != 0) {
            int lift = p->itemy <= p->py + 0x11 ? p->wob2 : 0;
            vid_pat(v, p->itemx, p->itemy + lift, p->item + 0x989);
            play_item_name(g, p->itemx, p->itemy - 8 + lift, p->item);
        }
    }
    if (p->item == 0 && p->onscreen == 0 && p->quota <= p->kills &&
        p->life == 10) {
        g->hook = HOOK_AIRCLEAR;        /* LAB_004010d2 -> FUN_0040f490 */
        g->hook_arg = 1;
    }

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

/* FUN_0040f490: the air stage is over.  The whole sky slides down 16 pixels
 * a frame while the ship sails to the left edge and settles, rain streaks
 * fall past, and after 0x78 frames the next mode takes over. */
void air_clear_frame(Game *g)
{
    Play *p = &g->p;
    Video *v = g->v;
    const DarPat *spa, *sky1, *sky2, *sky3, *sea;
    int i, x, n;

    if (g->hook_arg) {
        g->hook_arg = 0;
        p->announce = 0;
        p->over = 0;
        p->banner = 0;
        p->py = 0x120;
        p->ac_scroll = 0;
        plat_bgm(0, "bgm10");
        plat_se("plane", 0);            /* the WAV the installer is missing */
        for (i = 0; i < CLOUDS; i++) {
            p->cloud[i].x = game_rand(g) % 0x280;
            p->cloud[i].y = game_rand(g) % 3 - 0x20;
            p->cloud[i].kind = game_rand(g) % 6;
        }
        p->ncloud = 0;
        p->ac_timer = 0;
    }

    spa = vid_pat_info(v, EXT_BASE + 8);        /* sky2spa, 96x960 */
    sky1 = vid_pat_info(v, EXT_BASE + 5);
    sky2 = vid_pat_info(v, EXT_BASE + 6);
    sky3 = vid_pat_info(v, EXT_BASE + 7);
    sea = vid_pat_info(v, EXT_BASE);
    if (!spa || !sky1 || !sky2 || !sky3 || !sea) return;

    for (x = 0x20; x < 0x260; x += spa->w)
        vid_pat(v, x, p->ac_scroll - spa->h, EXT_BASE + 8);
    for (i = 0; i < p->ncloud; i++)
        vid_pat(v, p->cloud[i].x, p->cloud[i].y, p->cloud[i].kind + 0xb30);
    for (x = 0x20; x < 0x260; x += sky3->w)
        vid_pat(v, x, p->ac_scroll, EXT_BASE + 7);
    for (x = 0x20; x < 0x260; x += sky2->w)
        for (i = 0; i < 4; i++)
            vid_pat(v, x, sky3->h + p->ac_scroll + sky2->h * i, EXT_BASE + 6);
    for (x = 0x20; x < 0x260; x += sky1->w)
        vid_pat(v, x, (p->ac_scroll - sky1->h) + 0x1a + p->py, EXT_BASE + 5);
    for (x = 0x20; x < 0x260; x += sea->w)
        vid_pat(v, x, p->ac_scroll + 0x19 + p->py, EXT_BASE);
    p->ac_scroll += 0x10;

    /* the ship works its way to x = 0x40 from either side */
    if (p->px < 0x40) {
        n = p->px + 5;
        if (n < 0) p->px = 0;
        else if (n > 0x40) p->px = 0x40;
        else p->px = n;
    } else if (p->px > 0x40) {
        p->px -= 5;
        if (p->px < 0x40) p->px = 0x40;
        else if (p->px > 999) p->px = 999;
    }
    if (p->py > 0xaa) {
        n = p->py - 1;
        p->py = n < 0xaa ? 0xaa : (n < 1000 ? n : 999);
    }

    if (p->ac_timer > 0x1e && p->ncloud < 0x40) p->ncloud += 2;
    if (p->ac_timer < 0x6e)
        for (i = 0; i < p->ncloud; i++) {
            p->cloud[i].y += 0x10 + p->cloud[i].kind;
            if (p->cloud[i].y > 0x160) {
                p->cloud[i].y -= 0x160;
                p->cloud[i].x = game_rand(g) % 0x280;
                p->cloud[i].kind = game_rand(g) % 6;
            }
        }

    vid_pat(v, p->flip + 8 + p->px, p->py + 0x16, p->flip + 0xb2b);
    vid_pat(v, p->flip + 0x28 + p->px, p->py + 0x16, p->flip + 0xb2b);
    vid_pat(v, p->px, p->py, 0xa05);
    vid_text(v, 0x22, 10, "Clear!", FNT_YELLOW);
    play_status_bar(g);
    p->ac_timer++;
    if (p->ac_timer > 0x78) {
        g->hook = HOOK_SPACE;           /* LAB_0040110e -> FUN_0040f970 */
        g->hook_arg = 1;
        p->stage++;
    }
    p->flip ^= 1;
}
