/* FUN_00405c10 - the game itself - and the handful of routines it works
 * through.  See play.h for the objects.
 *
 * The frame runs in this order, which is the original's:
 *
 *     re-arm the field if DAT_004492ac is set (a new stage or a new life)
 *     count the dying player down, or read the pad and move
 *     the enemies: spawn, run away when the quota is met, shoot
 *     chain reactions between enemies
 *     the depth charges: sink, hit, land
 *     the torpedoes and the shells: rise, hit the ship
 *     the item
 *     the sea, the enemies, the shots, the splashes, the ship, the status bar
 *
 * THE RANDOM SEQUENCE IS PART OF THE GAME.  rand() is called for every enemy
 * slot every frame whether or not it is free, the pad tests burn one each,
 * and the button tests short-circuit - so the order of the calls here is the
 * order over there.
 */
#include "game.h"

#include <stdio.h>
#include <string.h>

/* The original's `if (x < 1) r = (-1 < x) - 1; else r = 1;` - which is the
 * sign, with zero mapping to zero. */
static int sgn(int v) { return v > 0 ? 1 : (v < 0 ? -1 : 0); }

/* 0x43fe48 indexed by the shell's own vx, which is clamped to +-8: the
 * sprite tilts with the speed.  The table is read with negative indices,
 * which is why it is written out here from -8. */
static const int SHELL_TILT[17] = {
    -3, -3, -2, -2, -2, -1, -1, 0, 0, 0, 1, 1, 2, 2, 2, 3, 3
};

/* 0x43f7d4: kind 4's five sprites, picked by its aim counter. */
static const int SHIP_FRAME[5] = { 5, 13, 21, 29, 37 };

/* 0x4400c8: the item the sixteen-sided die gives, before all the conditions
 * in FUN_0040aed0 are applied. */
static const int ITEM_ROLL[16] = { 1,1,1,1, 2,2,2,2, 3,3, 4,4, 5,5, 6, 7 };

/* 0x44010c, 0x20 bytes apiece: what the pickup calls itself. */
static const char *const ITEM_NAME[8] = {
    "EMPTY", "Speed Up", "Shot Max Up", "Shot Power Up",
    "Flash Bomb", "Shot Special", "Full Power", "Ship 1up"
};

/* 0x43fae8: 0x40 bytes a stage, and the loader reads 64 of them from there -
 * so the rows overlap and a stage's own ten are the first ten of its row.
 * Kinds 8 and 12 have no code, which is what makes those stages unplayable.
 */
static const int STAGE_KIND[16][16] = {
    { 0 },
    { 1, 2, 3, 1, 5, 5, 1, 1, 9, 9, 1, 1, 1, 1, 1, 1 },
    { 2, 2, 2, 3, 1, 1, 1, 5, 5, 9, 1, 1, 1, 1, 1, 1 },
    { 3, 2, 2, 3, 1, 1, 1, 1, 1, 9, 1, 1, 1, 1, 1, 1 },
    { 4, 0 },
    { 5, 4, 3, 2, 2, 1, 1, 1, 9, 9, 1, 1, 1, 1, 1, 1 },
    { 6, 3, 3, 2, 2, 2, 1, 1, 1, 9, 1, 1, 1, 1, 1, 1 },
    { 7, 4, 3, 2, 2, 2, 1, 1, 1, 9, 1, 1, 1, 1, 1, 1 },
    { 8, 0 },
    { 9, 4, 4, 3, 3, 2, 2, 1, 9, 9, 1, 1, 1, 1, 1, 1 },
    { 10, 4, 3, 3, 3, 2, 2, 1, 9, 9, 1, 1, 1, 1, 1, 1 },
    { 11, 4, 4, 4, 3, 3, 2, 1, 9, 9, 1, 1, 1, 1, 1, 1 },
    { 12, 0 },
    { -3, -3, -2, -2, -2, -1, -1, 0, 0, 0, 1, 1, 2, 2, 2, 3 },
    { 3, 5, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 },
    { 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29 }
};

/* 0x43fe70, 0x1e ints a row, indexed by the four-stage cycle and the kind. */
static const int SCORE[5][12] = {
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 },
    { 1, 5, 30, 20, 50, 30, 0, 0, 0, 10, 0, 0 },
    { 2, 10, 20, 30, 50, 10, 0, 0, 0, 10, 0, 0 },
    { 3, 10, 20, 30, 200, 10, 11, 12, 13, 10, 20, 0 },
    { 0 }                               /* the fourth row is not read yet */
};

/* One kind's slot in the stage table, the way FUN_0040aa20 reads it: 64
 * dwords from the stage's own row, so it runs on into the rows below. */
static int stage_kind(int stage, int slot)
{
    int at = stage * 16 + slot;         /* dwords from the top of the table */

    if (at < 0 || at >= 16 * 16) return 0;
    return STAGE_KIND[at / 16][at % 16];
}

/* FUN_0041fdf0: an effect panned by where it happened.  The original hands
 * DirectSound (x - 0x140) * 0x1f, clamped, which is hundredths of a decibel
 * from the middle. */
static void se_at(const char *name, int x)
{
    int pan = (x - 0x140) * 0x1f;

    if (pan < -10000) pan = -10000;
    else if (pan > 10000) pan = 10000;
    plat_se(name, pan);
}

/* FUN_00405ba0: a splash or a puff of smoke, four frames from 0x997. */
static void splash(Play *p, int x, int y)
{
    int i;

    for (i = 0; i < SPLASHES; i++)
        if (p->sp[i].frame == 4) {
            p->sp[i].frame = 0;
            p->sp[i].x = x;
            p->sp[i].y = y;
            return;
        }
}

/* FUN_00405980 / FUN_00405ac0: a charge's slot is free when its y is 0x134,
 * which is where the sea bed is. */
static void charge_free(Play *p, int i)
{
    p->c[i].y = 0x134;
    p->inflight--;
}

/* FUN_004059e0: drop one, if the ship still has one to drop. */
static void charge_drop(Play *p, int x, int y)
{
    int i;

    if (p->charges == p->inflight || p->charges - p->inflight < 0) return;
    if (p->charges <= 0) return;
    for (i = 0; i < p->charges; i++)
        if (p->c[i].y == 0x134) break;
    if (i >= p->charges) return;

    p->c[i].x = x;
    p->c[i].y = y;
    p->c[i].vx = p->powerA * -7;
    p->c[i].vy = 2;
    p->c[i].tx = x;
    p->inflight++;
    se_at("drop", p->px + 0x20);
}

/* FUN_0040ae50: eight frames of explosion from 0xa35. */
static void boom(Video *v, int x, int y, int frame)
{
    if (frame < 8) vid_pat(v, x, y, frame + 0xa35);
}

/* FUN_0040aed0: which item to drop.  The die is loaded by everything the
 * player already has - there is no point dropping a speed-up on a ship that
 * is already at full speed. */
static void item_pick(Game *g)
{
    Play *p = &g->p;

    p->item = ITEM_ROLL[game_rand(g) % 16];
    if (p->item == 6 && game_rand(g) % 3 == 0) p->item = 4;
    if (p->powerB == 0 && p->powerA == 0 && p->item == 4)
        p->item = (game_rand(g) % 2) * 2 + 3;
    if ((p->item == 3 || p->item == 5) && p->powerB == 1 && p->powerA == 1) {
        p->item = 4;
    } else {
        if (p->item == 3) {
            if (p->powerB == 1) p->item = 5;
        } else if (p->item != 5) goto done;
        if (p->powerA == 1) p->item = 3;
    }
done:
    if (p->lives > 2 && p->item == 7) p->item = 4;
    if (p->speed < 8 && p->item != 6 && p->item != 3 && p->item != 5) p->item = 1;
    if (p->charges < 6 && p->item != 6) p->item = 2;
    if (p->speed < 6 && p->item != 6) p->item = 1;
}

/* FUN_0040aab0: what picking one up does. */
static void item_apply(Play *p)
{
    switch (p->item) {
    case 1:
        if (p->speed < 8) p->speed++;
        break;
    case 2:
        p->charges += 2 + (p->cycle != 1 ? p->powerA : 0);
        if (p->charges > 0xf) p->charges = 0x10;
        break;
    case 3:
        p->powerB = 1;
        break;
    case 5:
        p->powerA = 1;
        if (p->charges < 0xf && p->cycle != 1) p->charges += 2;
        break;
    case 6:
        p->charges = 0x10;
        p->powerA = 1;
        p->powerB = 1;
        p->speed = 8;
        break;
    case 7:
        p->lives++;
        break;
    }
}

/* FUN_0040b6c0: put the points up over whatever was hit. */
static void popup_add(Play *p, int x, int y, int value, int chain)
{
    int i;

    for (i = 0; i < POPUPS; i++)
        if (p->pop[i].t == 0) {
            p->pop[i].t = 0x3c;
            p->pop[i].value = value;
            p->pop[i].x = x;
            p->pop[i].y = y;
            p->pop[i].chain = chain;
            return;
        }
}

/* FUN_0040b740: draw them, blinking out over the last sixteen frames. */
static void popups_draw(Game *g)
{
    Play *p = &g->p;
    char line[24];
    int i;

    for (i = 0; i < POPUPS; i++) {
        Popup *q = &p->pop[i];
        int len, x;

        if (q->t <= 0) continue;
        q->t--;
        if (q->chain == 1) sprintf(line, "%d", q->value);
        else sprintf(line, "%dx%d", q->value, q->chain);
        len = (int)strlen(line);
        x = q->x - len * 4;
        if (x < 0x20) {
            x = 0x20;
        } else {
            int cap = (0x4c - len) * 8;
            if (cap < x) x = cap;
        }
        if (q->t < 0x10 && q->t % 2 == 0) continue;
        vid_text8(g->v, x, q->y + 0x1c, line);
    }
}

/* FUN_0040acf0: an enemy has been hit.  Its own speed is halved (the wreck
 * drifts), the death animation starts, the score goes up by the kind's worth
 * times the chain, and a popup says so.  The popup itself is not ported yet;
 * the score is. */
static void enemy_hit(Game *g, int i, int chain)
{
    Play *p = &g->p;
    Enemy *e = &p->e[i];
    int worth;

    e->vx /= 2;
    e->vy /= 2;
    e->state = 9;
    e->chain = chain;
    worth = SCORE[p->cycle][e->kind];
    p->score += worth * chain;
    se_at("burn", e->x + 0x18);
    if (worth * chain > 0)
        popup_add(p, e->w / 2 + e->x, e->h / 2 + e->y, worth * 10, chain);
}

/* FUN_0040aa20: build the field for a stage out of the kind table. */
static void field_build(Game *g)
{
    Play *p = &g->p;
    int i;

    for (i = 0; i < ENEMIES; i++) {
        Enemy *e = &p->e[i];
        e->vx = 0;
        e->vy = 0;
        e->x = 0x3c0;                   /* off to the right, out of the way */
        e->y = 0;
        e->w = 0;                       /* [6] is not touched here */
        e->state = 10;                  /* [11] */
        e->aim = 0;                     /* [12] */
        e->kind = stage_kind(p->stage, i);
        e->chain = 0;
        p->e[i].anim = 0;
        p->e[i].animt = 0;
        p->e[i].face = 0;
        /* [7] = 10 as well: the original writes puVar3[7] and puVar3[8] */
        e->h = 10;
    }
}

/* The re-arm at the top of FUN_00405c10, which runs whenever the hook's
 * second argument was set: a new stage, or a new life on the same one. */
static void field_reinit(Game *g)
{
    Play *p = &g->p;
    int i;

    p->announce = 0;                    /* FUN_0040a9f0 */
    p->over = 0;
    p->banner = 0;
    for (i = 0; i < ENEMIES; i++) {     /* FUN_0040ac70 -> FUN_0040abe0 */
        p->e[i].kind = 0;
        p->e[i].x = 0;
        p->e[i].y = 0;
    }
    for (i = 0; i < CHARGES; i++) p->c[i].y = 0x134;     /* FUN_004059b0 */
    g->flash = 0;
    game_scene(g, "depth1.dar", 9);
    p->nenemy = 10;
    if (p->lives < 0) {                 /* nothing left: back to the logo */
        game_set_state(g, ST_BOOT);
        return;
    }
    for (i = 0; i < TORPS; i++) p->t[i].y = 0x20;
    for (i = 0; i < ESHOTS; i++) { p->s[i].y = -0x10; p->s[i].vx = 0; }
    for (i = 0; i < SPLASHES; i++) p->sp[i].frame = 4;
    field_build(g);
    p->px = 0x120;
    p->py = 0x10;
    p->inflight = 0;
    p->onscreen = 0;
    p->kills = 0;
    p->sunk = 0;
    p->ntorp = 0;
    p->neshot = 0;
    p->flip = 0;
    if (p->loaded == p->stage) {
        /* the same stage again, so the power-ups go */
        p->speed = 2;
        p->charges = 4;
        p->powerB = 0;
        p->powerA = 0;
        p->life = 10;
        p->banner = 0x96;               /* FUN_0040a8c0 */
    } else {
        p->life = 10;
        p->announce = 0x96;             /* FUN_0040a810 */
        p->announce_stage = p->stage;
        p->loaded = p->stage;
    }
    p->swellt = 0;
    p->swell = 0;
    p->item = 0;
    p->itemt = 0;
    p->quota = ((p->stage / 4) + 5 + (p->powerA & p->powerB) * 2) * 5;
    plat_bgm(3, "bgm03");
}

/* ---- the enemies ------------------------------------------------------ */

/* Every kind comes in the same way: a lane, an edge to come in from, and a
 * speed away from that edge.  The three random numbers are drawn in that
 * order - which is why each kind writes them out rather than calling a
 * helper with rand() in its arguments, where C does not fix the order.
 * `span` is how far out the right-hand edge is; the left is always -0x140. */
static void spawn_side(Game *g, Enemy *e, int span)
{
    e->x = (game_rand(g) % 2) * span - 0x140;
}

/* When the quota is met the survivors head for the edge at eight (ten for
 * kind 9) and the stage can end. */
static void flee(Enemy *e, int speed)
{
    e->vx = sgn(e->vx) * speed;
}

/* The last free torpedo slot, which is what the original's scan leaves in
 * its register - it keeps looking after it has found one.  `slot` carries
 * the previous value in, as the original's does. */
static int last_free_torp(Play *p, int slot)
{
    int i;

    for (i = 0; i < TORPS; i++)
        if (p->t[i].y < 0x21) slot = i;
    return slot;
}

static int last_free_shell(Play *p, int slot)
{
    int i;

    for (i = 0; i < ESHOTS; i++)
        if (p->s[i].y < -0xf) slot = i;
    return slot;
}

static void enemy_update(Game *g, int i, int *slot)
{
    Play *p = &g->p;
    Enemy *e = &p->e[i];
    int r, d;

    switch (e->kind) {
    case EK_SUB:
        if (game_rand(g) % 0x32 == 0 && e->y == 0) {
            if (p->kills < p->quota) {
                e->y = (game_rand(g) % 7 + 2) * 0x20;
                spawn_side(g, e, 0x4c0);
                e->vx = (game_rand(g) % 4) * sgn(0x140 - e->x);
                e->vy = 0;
                e->face = game_rand(g) % 2;
                e->w = 0x40;
                e->h = 0x20;
                if (p->quota <= p->kills) flee(e, 8);
            } else {
                flee(e, 8);
            }
        } else if (p->quota <= p->kills) {
            flee(e, 8);
        }
        if (e->vx + e->x < -0x13f || e->vx + e->x > 0x37f) e->y = 0;
        if (e->y != 0 && game_rand(g) % (p->powerB * -0x28 + 0xfa) == 0 &&
            p->ntorp < 0x10 && e->x > 0 && e->x < 0x240) {
            *slot = last_free_torp(p, *slot);
            p->t[*slot].y = e->y + 0xc;
            p->t[*slot].x = e->x + 0x18;
            p->ntorp++;
        }
        break;

    case EK_SUB2:
        if (game_rand(g) % 0x50 == 0 && e->y == 0) {
            if (p->kills < p->quota) {
                e->y = (game_rand(g) % 2 + 7) * 0x20;
                spawn_side(g, e, 0x4c0);
                e->vx = (game_rand(g) % 3 + 1) * sgn(0x140 - e->x);
                e->vy = 0;
                e->w = 0x40;
                e->h = 0x20;
                if (p->quota <= p->kills) flee(e, 8);
            } else {
                flee(e, 8);
            }
        } else if (p->quota <= p->kills) {
            flee(e, 8);
        }
        if (e->vx + e->x < -0x13f || e->vx + e->x > 0x37f) e->y = 0;
        if (e->y != 0 &&
            game_rand(g) % (((p->powerB * -2 + 0xf) - p->powerA) * 10) == 0 &&
            p->neshot < 8 && e->x > 0 && e->x < 0x241) {
            *slot = last_free_shell(p, *slot);
            p->s[*slot].y = e->y + 0xc;
            p->s[*slot].x = e->x + 0x18;
            p->s[*slot].vx = 0;
            p->neshot++;
            se_at("eneshot", p->s[*slot].x + 8);
        }
        break;

    case EK_MINI:
        if (game_rand(g) % (((p->powerB * -2 + 9) - p->powerA) * 10) == 0 &&
            e->y == 0) {
            if (p->kills < p->quota) {
                e->y = (game_rand(g) % 2 + 2) * 0x20;
                spawn_side(g, e, 0x4e0);
                e->vx = (game_rand(g) % 4 + 2) * sgn(0x140 - e->x);
                e->vy = 0;
                e->w = 0x20;
                e->h = 0x20;
                if (p->quota <= p->kills) flee(e, 8);
            } else {
                flee(e, 8);
            }
        } else if (p->quota <= p->kills) {
            flee(e, 8);
        }
        if (e->vx + e->x < -0x13f || e->vx + e->x > 0x37f) e->y = 0;
        if (e->y != 0 &&
            game_rand(g) % (((10 - p->powerA) - p->powerB) * 5) == 0 &&
            p->ntorp < 0x10 && e->x > 0 && e->x < 0x240) {
            *slot = last_free_torp(p, *slot);
            p->t[*slot].y = e->y + 0x10;
            p->t[*slot].x = e->x + 0x10;
            p->ntorp++;
        }
        break;

    case EK_SHIP:
        if (game_rand(g) % 0x3c == 0 && e->y == 0 && p->kills < p->quota) {
            e->y = 0x121;
            spawn_side(g, e, 0x4c0);
            e->vx = (game_rand(g) % 4 + 3) * sgn(0x140 - e->x);
            e->vy = 0;
            e->aim = 0;
            e->w = 0x40;
            e->h = 0x20;
        }
        if (e->vx == 0) {
            if (e->aim < 4) e->aim++;
        } else {
            if (e->aim > 0) e->aim--;
        }
        if (e->x + e->vx > 0x1f && e->x + e->vx < 0x221 &&
            game_rand(g) % (((3 - p->powerB) * 2 - p->powerA) * 10) == 0)
            e->vx = 0;                  /* stop, and start aiming */
        if (e->x + e->vx < -0x13f || e->x + e->vx > 0x37f) e->y = 0;
        if (e->y != 0 && e->aim == 4) {
            if (p->neshot < 4 && e->x > 0 && e->x < 0x241) {
                int n, x = e->x, k = *slot;
                for (n = 0; n < 4; n++) {
                    int j;
                    for (j = 0; j < ESHOTS; j++)
                        if (p->s[j].y < -0xf) break;
                    k = j < ESHOTS ? j : k;
                    p->s[k].y = e->y + 8;
                    p->s[k].x = x;
                    p->s[k].vx = 0;
                    x += 0x10;
                }
                *slot = k;
                p->neshot += 4;
                se_at("eneshot", p->s[k].x + 8);
            }
            e->vx = (game_rand(g) % 4 + 5) * sgn(0x140 - e->x);
        }
        break;

    case EK_SUB3:
        if (game_rand(g) % (((p->powerB * -2 + 9) - p->powerA) * 10) == 0 &&
            e->y == 0) {
            if (p->kills < p->quota) {
                e->y = (game_rand(g) % 5) * 0x10 + 200;
                spawn_side(g, e, 0x4e0);
                e->vx = (game_rand(g) % 2 + 2) * sgn(0x140 - e->x);
                e->vy = 0;
                e->anim = 0;
                e->animt = 0;
                e->w = 0x40;
                e->h = 0x20;
                if (p->quota <= p->kills) flee(e, 8);
            } else {
                flee(e, 8);
            }
        } else if (p->quota <= p->kills) {
            flee(e, 8);
        }
        if (e->vx + e->x < -0x13f || e->vx + e->x > 0x37f) e->y = 0;
        if (e->y != 0) {
            if (++e->animt > 2) {
                e->anim = (e->anim + 1) % 3;
                e->animt = 0;
            }
            if (game_rand(g) % (((0x14 - p->powerA) - p->powerB) * 5) == 0 &&
                p->ntorp < 0x10 && e->x > 0 && e->x < 0x240) {
                *slot = last_free_torp(p, *slot);
                p->t[*slot].y = e->y + 0x10;
                p->t[*slot].x = e->x + 0x10;
                p->ntorp++;
            }
        }
        break;

    case EK_HOMING:
        if (game_rand(g) % 0x1e == 0 && e->y == 0) {
            if (p->kills < p->quota) {
                e->aim = (game_rand(g) % 5 + 3) * 0x20;
                e->y = (game_rand(g) % 2) * 0x40 - 0x20 + e->aim;
                spawn_side(g, e, 0x4e0);
                e->vx = (game_rand(g) % 4 + 1) * sgn(0x140 - e->x);
                e->vy = 0;
                e->w = 0x20;
                e->h = 0x20;
                if (p->quota <= p->kills) flee(e, 10);
            } else {
                flee(e, 10);
            }
        } else if (p->quota <= p->kills) {
            flee(e, 10);
        }
        if (e->vx + e->x < -0x13f || e->vx + e->x > 0x39f) {
            e->y = 0;
            e->vy = 0;
        }
        if (e->y != 0) {
            d = e->aim - e->y;
            e->vy += sgn(d);
        }
        break;

    default:
        r = 0;
        (void)r;                        /* kinds 6, 7, 8 and up do nothing */
        break;
    }
}

/* FUN_0040bbb0: one line of the score table.  The caller hands over the
 * fields, because the name entry draws a row that is not in the table yet.
 *
 * The two patterns are glyphs out of the 16x16 font: the rank's own digit
 * (0x30 + rank, so '1'..'9', and 0x14 for the tenth) and one of the four
 * markers at 0x10..0x13, which stop rising after the third place. */
void play_rank_row_of(Game *g, int rank, int score, int stage,
                      const char *name, const char *date, int bank)
{
    Video *v = g->v;
    char line[64];
    int row = rank + 5;
    int y = row * 0x10;
    int n = rank == 10 ? 0x14 : rank + 0x30;
    int k = rank - 1;

    if (k < 0) k = 0;
    else if (k > 3) k = 3;

    vid_pat(v, 0x50, y, bank + n);
    vid_pat(v, 0x60, y, bank + 0x10 + k);
    sprintf(line, "%05d0", score);
    vid_text(v, 0x0f, row, line, bank);
    sprintf(line, "%02d", stage);
    vid_text(v, 0x1e, row, line, bank);
    vid_text(v, 0x24, row, name, bank);
    vid_text(v, 0x36, row, date, bank);
}

/* FUN_0040bb60: a row straight out of the table, in white. */
void play_rank_row(Game *g, int rank)
{
    const Rank *r = &g->rank[rank - 1];

    play_rank_row_of(g, rank, r->score, r->stage, r->name, r->date, FNT_WHITE);
}

/* FUN_004096e0: the ship's own two blips on the sonar panel. */
static void sonar_player(Game *g)
{
    Play *p = &g->p;

    if (p->life > 9 && p->py >= 0) {
        int sx = p->px / 8 + 0x118, sy = p->py / 8 + 0x16a;
        vid_pat(g->v, sx, sy, 0xb40);
        vid_pat(g->v, sx + 4, sy, 0xb40);
    }
}

/* FUN_004097a0: one blip a kind, two of them for the wide ones.  Kind 9
 * flickers - it is only drawn on the odd frames. */
static void sonar_enemies(Game *g)
{
    Play *p = &g->p;
    int i;

    for (i = 0; i < p->nenemy; i++) {
        const Enemy *e = &p->e[i];
        int sx, sy, n, twice = 1;

        if (e->y <= 0xf || e->state <= 9) continue;
        if ((unsigned)(e->kind - 1) > 8) continue;
        switch (e->kind) {
        case 1: n = 0xb41; break;
        case 2: n = 0xb42; break;
        case 3: n = 0xb43; twice = 0; break;
        case 4: n = 0xb44; break;
        case 5: n = 0xb45; break;
        case 9:
            if (g->frame % 2 == 0) continue;
            n = 0xb40;
            twice = 0;
            break;
        default: continue;
        }
        sx = e->x / 8 + 0x118;
        sy = e->y / 8 + 0x16a;
        vid_pat(g->v, sx, sy, n);
        if (twice) vid_pat(g->v, sx + 4, sy, n);
    }
}

/* FUN_004093d0: the panel along the bottom - the sonar in the middle with
 * the sea bed either side of it, Score on the left and Left on the right.
 * The three "strings" at 0x43fac4 are box-drawing glyphs out of the 16x16
 * font, which is how the panel's frame is drawn. */
static void play_status(Game *g)
{
    Play *p = &g->p;
    Video *v = g->v;
    char line[64];
    int x;

    vid_fill(v, 0x20, 0x180, 0x260, 0x184, 0);
    vid_fill(v, 0x20, 0x1bc, 0x260, 0x1c0, 0);
    vid_pat(v, 0xf0, 0x168, 0xb46);
    sonar_enemies(g);
    sonar_player(g);
    vid_text_at(v, 0x1e, 0x168, "\x01\x05\x0b\x09\x09\x09\x09\x0c\x05\x02",
                FNT_WHITE);
    vid_text_at(v, 0x1e, 0x178, "\x07        \x08", FNT_WHITE);
    vid_text_at(v, 0x1e, 0x188, "\x03\x06\x0e\x0a\x0a\x0a\x0a\x0d\x06\x04",
                FNT_WHITE);
    for (x = 0x20; x < 0xf0; x += 0x10) {
        vid_pat(v, x, 0x160, 0x9ba);
        vid_pat(v, x, 0x170, 0x9bf);
        vid_pat(v, x, 0x180, 0x9bf);
        vid_pat(v, x, 400, 0x9b9);
    }
    for (x = 400; x < 0x260; x += 0x10) {
        vid_pat(v, x, 0x160, 0x9ba);
        vid_pat(v, x, 0x170, 0x9bf);
        vid_pat(v, x, 0x180, 0x9bf);
        vid_pat(v, x, 400, 0x9b9);
    }
    for (x = 0xf0; x < 400; x += 0x10) {
        vid_pat(v, x, 0x160, 0xb3e);
        vid_pat(v, x, 0x198, 0xb3f);
    }
    vid_text_at(v, 10, 0x168, "Score", FNT_RED);
    vid_text_at(v, 0x3c, 0x168, "Left", FNT_RED);
    sprintf(line, "%05d0", p->score);
    vid_text_at(v, 10, 0x188, line, FNT_WHITE);
    sprintf(line, "%02d", p->lives);
    vid_text_at(v, 0x3e, 0x188, line, FNT_WHITE);

    /* FUN_0040a980: the banners.  The score popups (FUN_0040b740) are not
     * ported yet. */
    if (p->demo == 1) {
        if (g->frame % 0x10 < 8)
            vid_text(v, 0x1c, 10, "DEMONSTRATION", FNT_RED);
        popups_draw(g);
    } else {
        popups_draw(g);
        if (p->banner != 0) {           /* FUN_0040a8e0 */
            vid_text(v, 0x24, 10, "Ready", FNT_RED);
            p->banner--;
        }
        if (p->announce != 0) {         /* FUN_0040a840 */
            vid_text(v, 0x20, 10, "Stage", FNT_RED);
            sprintf(line, "%02d", p->announce_stage);
            vid_text(v, 0x2c, 10, line, FNT_WHITE);
            p->announce--;
        }
        if (p->over != 0) {             /* FUN_0040a940 */
            vid_text(v, 0x1e, 10, "Game Over", FNT_RED);
            p->over--;
        }
    }
}

/* ---- the frame -------------------------------------------------------- */

void play_frame(Game *g)
{
    Play *p = &g->p;
    Video *v = g->v;
    const DarPat *pat;
    int i, j, k, x, y, n, step;
    int slot = 0;                       /* the register the scans reuse */

    if (g->hook_arg) {
        g->hook_arg = 0;
        field_reinit(g);
        if (g->state == ST_BOOT) return;
    }

    /* --- the player ---------------------------------------------------- */
    p->pdx = 0;
    if (p->life < 10) {
        if (g->frame % 2 != 0) p->life--;
        if (p->life < 1) {
            if (p->lives == 0) {
                if (p->life == 0) {
                    p->announce = 0;
                    p->over = 300;      /* FUN_0040a920 */
                    plat_bgm(0, "bgm07");
                }
                if (p->life < -0x59) {
                    g->hook = HOOK_OVER;
                    g->hook_arg = 1;
                }
            } else {
                p->lives--;
                g->hook_arg = 1;        /* re-arm: a new life */
            }
        }
    } else {
        if (game_right(g) && p->speed + p->px < 0x210) p->pdx += p->speed;
        if (game_left(g) && p->px - p->speed > 0x2f) p->pdx -= p->speed;
        if (game_right(g) && game_left(g)) p->pdx = 0;
        p->px += p->pdx;
        if (p->px < 0x31) p->px = 0x30;
        else if (p->px > 0x20f) p->px = 0x20f;
        if (game_any_key(g)) charge_drop(p, p->px - 0x10, p->py + 2);
        if (game_btn2(g)) charge_drop(p, p->px + 0x40, p->py + 2);
    }

    /* --- the enemies --------------------------------------------------- */
    p->onscreen = 0;
    for (i = 0; i < p->nenemy; i++) {
        Enemy *e = &p->e[i];

        if (e->y != 0) p->onscreen++;
        if (e->state < 10) {
            if (g->frame % 2 != 0) e->state--;
            if (e->state < 0) {
                e->x = 0;
                e->y = 0;
                e->vx = 0;
                e->vy = 0;
                e->state = 10;
            }
        } else {
            enemy_update(g, i, &slot);
        }
    }

    /* Chain reactions: an enemy in the middle of blowing up takes anything
     * it touches with it, and the chain multiplies the score. */
    if (p->echain) {
        for (i = 0; i < p->nenemy; i++) {
            Enemy *a = &p->e[i];
            if (a->state <= 3 || a->state >= 9) continue;
            for (j = 0; j < p->nenemy; j++) {
                Enemy *b = &p->e[j];
                int right, top;
                if (b->y <= -0x20 || b->state != 10) continue;
                switch (b->kind) {
                case 1: case 2: case 4: case 5: right = b->x + 0x38; break;
                case 3: case 9: right = b->x + 0x18; break;
                default: continue;
                }
                top = b->y - 0x36;
                if (a->x < b->x - 0x36 || a->x > right) continue;
                if (a->y < top || a->y > b->y + 0x18) continue;
                if (b->kind == EK_HOMING && p->item == 0 &&
                    b->x > 0x1f && b->x < 0x241) {
                    item_pick(g);
                    p->itemx = b->x + 8;
                    p->itemy = b->y + 8;
                    p->itemk = 0;
                    p->itemvy = -2;
                    p->itemt = -1;
                }
                enemy_hit(g, j, a->chain + 1);
                p->kills++;
            }
        }
    }

    /* --- the depth charges --------------------------------------------- */
    for (k = 0; k < p->charges; k++) {
        Charge *c = &p->c[k];

        if (c->y == 0x134) continue;
        y = c->y + 2 + p->powerB * 2;
        c->y = y;
        if (y >= 0x134) {
            splash(p, c->x, y);
            charge_free(p, k);
            continue;
        }
        if (p->powerA == 1) {           /* the homing charge steers back */
            int nx = c->x + c->vx;
            c->x = nx;
            c->vx += sgn(c->tx - nx);
        }
        for (j = 0; j < p->nenemy; j++) {
            Enemy *e = &p->e[j];
            int left, right, top;
            if (e->state < 10 || e->y == 0) continue;
            switch (e->kind) {
            case 1: case 2: case 4: case 5:
                left = e->x - 4; right = e->x + 0x38; top = e->y - 0xc; break;
            case 3:
                left = e->x - 8; right = e->x + 0x18; top = e->y - 6; break;
            case 9:
                left = e->x - 8; right = e->x + 0x18; top = e->y - 6; break;
            default:
                continue;
            }
            if (c->x < left || c->x > right) continue;
            if (c->y < top || c->y > e->y + 0x18) continue;
            if (e->kind == EK_HOMING && p->item == 0 &&
                e->x > 0x1f && e->x < 0x241) {
                item_pick(g);
                p->itemk = 0;
                p->itemx = e->x + 8;
                p->itemvy = -2;
                p->itemy = e->y + 8;
                p->itemt = -1;
            }
            enemy_hit(g, j, 1);
            p->kills++;
            charge_free(p, k);
        }
        if (c->y > 0x133) p->sunk--;
    }

    /* --- the torpedoes ------------------------------------------------- */
    for (i = 0; i < TORPS; i++) {
        Torp *t = &p->t[i];
        int stall = 0;

        if (t->y <= 0x20) continue;
        t->y -= 2;
        if (t->y < 0x21) splash(p, t->x, t->y);
        if (p->px - 6 < t->x) {
            if (t->x >= p->px + 0x36) {
                stall = 1;
                if (t->y > 0x30 && game_rand(g) % 4 != 0) stall = 0;
            }
        } else {
            stall = 1;
        }
        if (stall) t->y++;
        t->x += p->flip * -2 + 1;
        if (p->life == 10 && p->hit == 0 &&
            t->x >= p->px - 6 && t->x <= p->px + 0x36 &&
            t->y >= p->py - 0xc && t->y <= p->py + 0x18) {
            splash(p, t->x, t->y);
            p->life = 9;
            t->y = 0;
            se_at("burn", p->px + 0x20);
        }
        if (t->y < 0x21) p->ntorp--;
    }

    /* --- the shells ---------------------------------------------------- */
    for (i = 0; i < ESHOTS; i++) {
        EShot *s = &p->s[i];
        int lim;

        if (s->y <= -0x10) continue;
        lim = (p->powerA + p->powerB) * -0x1e + 0xa0;
        s->y -= 3;
        if (s->y > lim) {               /* high up it still steers */
            int d = (p->px - s->x) + 0x18;
            s->vx += (game_rand(g) * sgn(d)) % 2;
        } else {
            s->vx -= sgn(s->vx) * p->flip;
        }
        if (s->vx > 8 || s->vx < -8) s->vx = sgn(s->vx) * 8;
        s->x += s->vx;
        if (p->life == 10 && p->hit == 0 &&
            s->x >= p->px - 4 && s->x <= p->px + 0x34 &&
            s->y >= p->py - 0xc && s->y <= p->py + 0x18) {
            p->life = 9;
            s->y = -0x10;
            se_at("burn", p->px + 0x20);
        }
        if (s->x < 0x10 || s->x > 0x26f) s->y = -0x10;
        if (s->y < -0xf) p->neshot--;
    }

    /* --- the item ------------------------------------------------------ */
    if (p->item != 0) {
        if (p->itemt < 0) p->itemy += p->itemvy;
        else p->itemt--;
        if (p->itemx >= p->px - 4 && p->itemx <= p->px + 0x34 &&
            p->itemy >= p->py && p->itemy <= p->py + 0x1a && p->life == 10) {
            if (p->item == 4) {         /* the flash bomb takes the screen */
                for (j = 0; j < ENEMIES; j++) {
                    Enemy *e = &p->e[j];
                    if (e->state == 10 && e->y != 0 && e->x > 0 && e->x < 0x240) {
                        p->kills++;
                        enemy_hit(g, j, 1);
                    }
                }
                for (j = 0; j < CHARGES; j++) {
                    if (p->c[j].y < 0x134) splash(p, p->c[j].x, p->c[j].y);
                    charge_free(p, j);
                }
                p->inflight = 0;
                p->sunk = 0;
                for (j = 0; j < TORPS; j++) {
                    if (p->t[j].y > 0x20) splash(p, p->t[j].x, p->t[j].y);
                    p->t[j].y = 0x20;
                }
                p->ntorp = 0;
                for (j = 0; j < ESHOTS; j++) {
                    if (p->s[j].y > -0x10) splash(p, p->s[j].x, p->s[j].y);
                    p->s[j].y = -0x10;
                }
                p->neshot = 0;
                plat_se("burn", 0);
            } else {
                item_apply(p);
            }
            p->item = 0;
            plat_se("item", 0);
        }
        if (p->itemt < 0 && p->itemy < 0x21) {
            p->itemy = 0x20;
            p->itemt = 0x78;
        } else if (p->itemt == 0) {
            p->item = 0;
        }
    }

    /* --- the swell and the charge sprite -------------------------------- */
    if (++p->swellt > 4) {
        p->swellt = 0;
        p->swell ^= 1;
    }
    if (g->frame % 4 == 0) p->canim = (p->canim + 1) % 4;
    switch (p->canim) {
    case 0: case 2: p->cframe = 0; break;
    case 1: p->cframe = 2; break;
    case 3: p->cframe = 1; break;
    }

    /* --- the sea -------------------------------------------------------- */
    pat = vid_pat_info(v, EXT_BASE + 5);
    step = pat ? pat->w : 0;
    if (step > 0)
        for (x = 0x20; x < 0x260; x += step)
            vid_pat(v, x, 0x2a - pat->h, EXT_BASE + 5);
    pat = vid_pat_info(v, EXT_BASE);
    step = pat ? pat->w : 0;
    if (step > 0)
        for (x = 0x20; x < 0x260; x += step) {
            int band = 0;
            vid_pat(v, x, p->swell + 0x29, EXT_BASE);
            y = pat->h + 0x29;
            while (y < 0x160) {
                const DarPat *q;
                n = EXT_BASE + 1;
                if (band == 2) n = EXT_BASE + 2;
                else if (band == 3 || band == 4) n = EXT_BASE + 3;
                vid_pat(v, x, y, n);
                q = vid_pat_info(v, n);
                if (!q || q->h <= 0) break;
                y += q->h;
                band++;
            }
        }
    for (x = 0x20; x < 0x260; x += 0x20) {
        if (p->stage == 1 || p->stage == 9) vid_pat(v, x, 0x141, 0x9d3);
        else if (p->stage == 5) vid_pat(v, x, 0x141, 0x9d4);
    }

    /* --- everything moves and is drawn ---------------------------------- */
    for (i = 0; i < p->nenemy; i++) {
        p->e[i].x += p->e[i].vx;
        p->e[i].y += p->e[i].vy;
    }
    for (i = 0; i < p->nenemy; i++) {
        Enemy *e = &p->e[i];
        int wide = (e->kind == 3 || e->kind == 9);
        int left = wide ? 0 : -0x20;

        if (e->y <= 0xf || e->state < 0) continue;
        if (e->x < left || e->x >= 0x260) continue;
        if (e->state < 10) {
            switch (e->kind) {
            case 1: case 2: case 4: case 5:
                boom(v, e->x, e->y - 0x10, 9 - e->state);
                break;
            case 3: case 9:
                boom(v, e->x - 0x10, e->y - 0x10, 9 - e->state);
                break;
            }
        } else {
            switch (e->kind) {
            case 1:
                n = (e->face == 0 ? 0xa1d : 0xa15) + (e->vx < 1);
                break;
            case 2: n = 0xa0d + (e->vx < 1); break;
            case 3: n = 0x9cd + (e->vx > 0); break;
            case 4: n = 0xa05 + SHIP_FRAME[e->aim < 0 ? 0 :
                                           (e->aim > 4 ? 4 : e->aim)]; break;
            case 5: n = 0xa0b + (e->vx < 1) + e->anim * 8; break;
            case 9: n = 0x9c9 + !(e->vx > 0); break;
            default: continue;
            }
            vid_pat(v, e->x, e->y, n);
        }
    }
    for (i = 0; i < ESHOTS; i++)
        if (p->s[i].y > -0x10) {
            int vx = p->s[i].vx;
            if (vx < -8) vx = -8;
            else if (vx > 8) vx = 8;
            vid_pat(v, p->s[i].x, p->s[i].y, SHELL_TILT[vx + 8] + 0x99f);
        }
    for (i = 0; i < TORPS; i++)
        if (p->t[i].y > 0x20)
            vid_pat(v, p->t[i].x, p->t[i].y, p->t[i].y % 3 + 0x987);
    for (i = 0; i < SPLASHES; i++)
        if (p->sp[i].frame < 4) {
            vid_pat(v, p->sp[i].x, p->sp[i].y, p->sp[i].frame + 0x997);
            if (g->frame % 2 != 0) p->sp[i].frame++;
        }
    for (i = 0; i < p->charges; i++)    /* FUN_00405af0 */
        if (p->c[i].y != 0x134)
            vid_pat(v, p->c[i].x, p->c[i].y, p->cframe + 0x981);
    if (p->life < 10) boom(v, p->px, p->py - 0x10, 9 - p->life);
    else vid_pat(v, p->px, p->swell + p->py, 0xa05);
    for (i = 0, n = 0x28; i < p->charges - p->inflight; i++, n += 2)
        vid_pat(v, (n - p->charges) * 8, 4, p->cframe + 0x981);

    /* --- the item, and the end of the stage ------------------------------ */
    if (p->item != 0) {
        if (p->itemt > 0x2d || p->itemt < 1 || p->flip != 0) {
            /* the swell only lifts it once it has reached the surface */
            int lift = p->itemy < 0x21 ? p->swell : 0;
            const char *name = ITEM_NAME[p->item & 7];
            int len = (int)strlen(name);
            int at = p->itemx + (2 - len) * 4;

            vid_pat(v, p->itemx, p->itemy + lift, p->item + 0x989);
            /* FUN_0040b1c0: the name in the small font, kept on the screen.
             * Its y is already in surface coordinates - the caller adds the
             * 0x20 itself. */
            if (at < 0x20) {
                at = 0x20;
            } else {
                int cap = (0x4c - len) * 8;
                if (cap < at) at = cap;
            }
            vid_text8(v, at, p->itemy - 8 + lift + 0x20, name);
        }
    }
    if (p->item == 0 && p->onscreen == 0 && p->quota <= p->kills &&
        p->life == 10) {
        g->hook = HOOK_CLEAR;
        g->hook_arg = 1;
    }

    play_status(g);
    p->flip ^= 1;

    /* ESC or START opens the pause menu (FUN_0040b930), except while a demo
     * is running or being recorded, when they cut back to the title. */
    if (game_edge(g, PAD_ESC) || game_start_key(g)) {
        if (g->demo == 0) {
            p->saved_hook = g->hook;
            g->hook = HOOK_PAUSE;
            g->hook_arg = 1;
        } else if (g->demo == 1) {
            game_set_state(g, ST_TITLE5);
            g->hook = HOOK_NONE;
            g->hook_arg = 1;
        } else if (g->demo == 2) {
            game_set_state(g, ST_LOGO);
            g->hook = HOOK_NONE;
            g->hook_arg = 1;
        }
    }
}

/* FUN_00408210: the stage is over.  The camera follows the ship up to the
 * surface - the sky tiles and the sea bed slide down by eight a frame while
 * the ship's own y rises - and then hands over to whatever comes next.
 *
 * depth1.dar's own patterns do the sky here: 0xb4c is sky01 on the water
 * line, 0xb4d (sky02) is tiled above it and 0xb4e (sky03) sits at the top.
 */
void play_clear_frame(Game *g)
{
    Play *p = &g->p;
    Video *v = g->v;
    const DarPat *sky, *up, *top, *sea;
    char line[16];
    int x, y, n, band;

    if (g->hook_arg) {
        g->hook_arg = 0;
        p->announce = 0;                /* FUN_0040a9f0 */
        p->over = 0;
        p->banner = 0;
        plat_bgm(0, "bgm09");
        p->cl_step = 0;
    }

    sky = vid_pat_info(v, EXT_BASE + 5);
    up = vid_pat_info(v, EXT_BASE + 6);
    top = vid_pat_info(v, EXT_BASE + 7);
    sea = vid_pat_info(v, EXT_BASE);
    if (!sky || !up || !top || !sea) return;

    switch (p->cl_step) {
    case 0:
        p->cl_timer = 0;
        p->cl_sky = 0x2a - sky->h;
        p->cl_step++;
        p->cl_ground = 0;
        p->cl_row = ((0x120 - p->py) / 8) * -8;
        if (++p->cl_timer > 0x1d) { p->cl_step++; p->cl_timer = 0; }
        break;
    case 1:
        if (++p->cl_timer > 0x1d) { p->cl_step++; p->cl_timer = 0; }
        break;
    case 2:
        p->py += 8;
        p->cl_ground += 8;
        p->cl_sky += 8;
        p->cl_row += 8;
        if (p->py > 0x11f) {
            p->cl_step++;
            p->py = 0x120;
        }
        /* falls through, as the original does */
    case 3:
        if (++p->cl_timer > 0x3b) {
            g->hook = HOOK_AIR;         /* FUN_0040c9e0 */
            g->hook_arg = 1;
            p->stage++;
        }
        break;
    }

    for (x = 0x20; x < 0x260; x += sky->w)
        vid_pat(v, x, p->cl_sky, EXT_BASE + 5);
    for (x = 0x20; x < 0x260; x += up->w)
        for (y = p->cl_sky - up->h; y >= -up->h; y -= up->h)
            vid_pat(v, x, y, EXT_BASE + 6);
    for (x = 0x20; x < 0x260; x += top->w)
        if (p->cl_row >= 0x20 - top->h)
            vid_pat(v, x, p->cl_row, EXT_BASE + 7);
    for (x = 0x20; x < 0x260; x += sea->w) {
        vid_pat(v, x, p->py + 0x19, EXT_BASE);
        band = 0;
        y = sea->h + 0x19 + p->py;
        while (y < 0x160) {
            const DarPat *q;
            n = EXT_BASE + 1;
            if (band == 2) n = EXT_BASE + 2;
            else if (band == 3 || band == 4) n = EXT_BASE + 3;
            vid_pat(v, x, y, n);
            q = vid_pat_info(v, n);
            if (!q || q->h <= 0) break;
            y += q->h;
            band++;
        }
    }
    vid_pat(v, p->px, p->py, 0xa05);
    if (p->cl_ground < 0x28)
        for (x = 0x20; x < 0x260; x += 0x20) {
            if (p->stage == 1 || p->stage == 9)
                vid_pat(v, x, p->cl_ground + 0x141, 0x9d3);
            else if (p->stage == 5)
                vid_pat(v, x, p->cl_ground + 0x141, 0x9d4);
        }
    vid_text(v, 0x22, 10, "Clear!", FNT_YELLOW);
    (void)line;
    play_status(g);
}

/* FUN_0040b960: PAUSE, with CONTINUE and EXIT.  The frame is cleared before
 * this runs, so the game does not show through - which is what the original
 * does as well.  UP and DOWN are read held rather than edged, so the cursor
 * runs from one to the other in two frames.
 */
void play_pause_frame(Game *g)
{
    Play *p = &g->p;
    Video *v = g->v;
    int n;

    if (g->hook_arg) {
        g->hook_arg = 0;
        p->pause_cur = 0;
    }

    vid_text(v, 0x28 - (int)strlen("PAUSE"), 9, "PAUSE", FNT_YELLOW);
    vid_text(v, 0x22, 0xc, "CONTINUE", FNT_WHITE);
    vid_text(v, 0x22, 0xe, "EXIT", FNT_WHITE);
    vid_text(v, 0x1e, p->pause_cur * 2 + 0xc, ">", FNT_WHITE);

    if (game_up(g)) {
        n = p->pause_cur - 1;
        p->pause_cur = n < 0 ? 0 : (n < 2 ? n : 1);
    }
    if (game_down(g)) {
        n = p->pause_cur + 1;
        p->pause_cur = n < 0 ? 0 : (n < 2 ? n : 1);
    }
    if (game_any_key(g)) {
        if (p->pause_cur == 0) {
            g->hook = p->saved_hook;    /* back in, without re-arming */
            g->hook_arg = 0;
            return;
        }
        if (p->pause_cur == 1) {
            game_set_state(g, ST_BOOT);
            return;
        }
    }
    if (game_edge(g, PAD_ESC) || game_start_key(g)) {
        g->hook = p->saved_hook;
        g->hook_arg = 0;
    }
}

/* 0x44055c: three rows of 0x21 bytes - 32 cells and a terminator.  The
 * original keeps a space where the quote goes and patches 0x22 in at
 * runtime (and then hardcodes the same pattern for that cell anyway); it is
 * written out here.  Row 2 is mostly blank: only DEL, DUP and END. */
static const char NAME_GRID[3][33] = {
    " !\"#$%&'()*+,-./0123456789:;<=>?",
    "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_",
    "                     DEL DUP END"
};

/* FUN_0040bdb0: the game is over, and if the score is good enough this is
 * where the name goes in.  space.dar's two big pictures wave behind a
 * starfield, the table is drawn with the new row in yellow, and a 3x32 grid
 * of characters is walked with the pad.
 */
void play_over_frame(Game *g)
{
    Play *p = &g->p;
    Video *v = g->v;
    const DarPat *a, *b;
    char line[64];
    int i, j, x, y, n;

    if (g->hook_arg) {
        g->hook_arg = 0;
        /* Where does it rank?  Off the bottom of the table and there is
         * nothing to type: straight back to the logo. */
        for (i = 0; i < RANKS && p->score <= g->rank[i].score; i++) ;
        if (i >= RANKS) {
            game_set_state(g, ST_LOGO);
            g->hook = HOOK_NONE;
            g->hook_arg = 1;
            return;
        }
        p->rcurx = 0;
        p->rcury = 0;
        p->repeat = 0;
        p->nm[0] = 0;
        p->namelen = 0;
        p->rankin = i;
        game_scene(g, "space.dar", 0x32);
        plat_bgm(1, "bgm08");
        sprintf(p->date, "%02d/%02d/%02d", g->year % 100, g->month, g->day);
        memset(p->names, 0, sizeof p->names);
        p->nnames = 0;
        p->pickname = 0;
        for (i = 0; i < RANKS; i++) {   /* the distinct names, for DUP */
            int dup = 0;
            for (j = 0; j < p->nnames; j++)
                if (!strcmp(p->names[j], g->rank[i].name)) { dup = 1; break; }
            if (!dup && p->nnames < 16) {
                strncpy(p->names[p->nnames], g->rank[i].name, 15);
                p->nnames++;
            }
        }
        for (i = 0; i < STARS; i++) {
            p->star[i].x = game_rand(g) % 0x240;
            p->star[i].y = game_rand(g) % 0x1a0;
            p->star[i].vx = -2 - game_rand(g) % 6;
            p->star[i].kind = game_rand(g) % 16;
        }
    }

    a = vid_pat_info(v, EXT_BASE + 48);         /* SPACE1 */
    b = vid_pat_info(v, EXT_BASE + 49);         /* SPACE2 */
    if (a && b) {
        vid_pat_wave(v, 0x2c0 - a->w, 0x1f8 - b->h, EXT_BASE + 48,
                     -0x100, 0x10, (int)g->frame);
        vid_pat_wave(v, -0x60, -0x60, EXT_BASE + 49,
                     -0x100, 0x10, (int)g->frame);
    }
    for (i = 0; i < STARS; i++) {
        Star *st = &p->star[i];
        st->x += st->vx;
        if (st->x < 0) {
            st->x += 0x280;
            st->y = game_rand(g) % 0x240;       /* 0x240, not 0x1a0 */
        }
        vid_pat(v, st->x, st->y, 0xb2e + st->kind);
    }

    vid_text(v, 10, 2, "Super Depth  Top Score Ranking", FNT_RED);
    strcpy(line, " ** Score ****  Name     Date   ");
    line[1] = 0x15; line[2] = 0x16;
    line[10] = 0x17; line[11] = 0x18; line[12] = 0x19; line[13] = 0x1a;
    vid_text(v, 8, 4, line, FNT_WHITE);
    vid_text(v, 8, 5, "--------------------------------", FNT_WHITE);
    vid_text(v, 8, 0x10, "--------------------------------", FNT_WHITE);
    for (i = 0; i < RANKS; i++)
        if (i != p->rankin) play_rank_row(g, i + 1);
    sprintf(p->date, "%02d/%02d/%02d", g->year % 100, g->month, g->day);
    play_rank_row_of(g, p->rankin + 1, p->score, p->stage, p->nm, p->date,
                     FNT_YELLOW);
    x = p->namelen < 8 ? (p->namelen + 0x12) * 0x10 : 400;
    if (g->frame % 4 < 2)
        vid_pat_raw(v, x, (p->rankin + 8) * 0x10, 0x880);

    for (y = 0; y < 3; y++)
        for (x = 0; x < 0x20; x++)
            vid_pat(v, (x + 4) * 0x10, y * 0x20 + 0x130,
                    (unsigned char)NAME_GRID[y][x] + FNT_WHITE);

    /* The pad is read held, with a delay before it repeats. */
    if (p->repeat == 0 || p->repeat > 10) {
        if (g->pad & PAD_RIGHT) {
            p->rcurx++;
            if (p->rcury >= 0) {
                if (p->rcury < 2) { if (p->rcurx > 0x1f) p->rcurx = 0; }
                else if (p->rcury == 2 && p->rcurx > 2) p->rcurx = 0;
            }
        }
        if (g->pad & PAD_LEFT) {
            p->rcurx--;
            if (p->rcurx < 0 && p->rcury >= 0) {
                if (p->rcury < 2) p->rcurx = 0x1f;
                else if (p->rcury == 2) p->rcurx = 2;
            }
        }
        if (g->pad & PAD_UP) {
            if (p->rcury == 0) {
                if (p->rcurx < 0 || p->rcurx > 0x17)
                    n = (p->rcurx >= 0x18 && p->rcurx <= 0x1b) ? 1 : 2;
                else n = 0;
                p->rcury = 2;
                p->rcurx = n;
            } else if (p->rcury == 1) {
                p->rcury = 0;
            } else if (p->rcury == 2) {
                if (p->rcurx == 0) p->rcurx = 0x16;
                else if (p->rcurx == 1) p->rcurx = 0x1a;
                else if (p->rcurx == 2) p->rcurx = 0x1e;
                p->rcury = 1;
            }
        }
        n = p->rcurx;
        if (g->pad & PAD_DOWN) {
            if (p->rcury == 0) {
                p->rcury = 1;
            } else if (p->rcury == 1) {
                if (n < 0 || n > 0x17) p->rcurx = (n >= 0x18 && n <= 0x1b) ? 1 : 2;
                else p->rcurx = 0;
                p->rcury = 2;
            } else if (p->rcury == 2) {
                if (p->rcurx == 0) p->rcurx = 0x16;
                else if (p->rcurx == 1) p->rcurx = 0x1a;
                else if (p->rcurx == 2) p->rcurx = 0x1e;
                p->rcury = 0;
            }
        }
    }
    if (!(g->pad & (PAD_LEFT | PAD_RIGHT | PAD_UP | PAD_DOWN))) {
        p->repeat = 0;
    } else {
        n = p->repeat + 1;
        p->repeat = n < 0 ? 0 : (n < 0xd ? n : 0xc);
    }

    if (game_any_key(g) && p->rcury >= 0) {
        plat_se("depth01", 0);
        if (p->rcury < 2) {
            n = p->namelen > 7 ? 7 : p->namelen;
            p->namelen = n + 1;
            p->nm[n] = NAME_GRID[p->rcury][p->rcurx & 0x1f];
            p->nm[n + 1] = 0;
            if (n + 1 > 7) {            /* eight characters is the lot */
                p->rcurx = 2;
                p->rcury = 2;
            }
        } else if (p->rcury == 2) {
            if (p->rcurx == 0) {                        /* DEL */
                if (p->namelen > 0) {
                    p->nm[p->namelen - 1] = 0;
                    p->namelen--;
                }
            } else if (p->rcurx == 1) {                 /* DUP */
                if (p->nnames > 0) {
                    strncpy(p->nm, p->names[p->pickname], sizeof p->nm - 1);
                    p->nm[sizeof p->nm - 1] = 0;
                    p->pickname = (p->pickname + 1) % p->nnames;
                    p->namelen = (int)strlen(p->nm);
                }
            } else if (p->rcurx == 2) {                 /* END */
                for (i = RANKS - 1; i > p->rankin; i--)
                    g->rank[i] = g->rank[i - 1];
                memset(&g->rank[p->rankin], 0, sizeof g->rank[p->rankin]);
                strncpy(g->rank[p->rankin].name, p->nm,
                        sizeof g->rank[p->rankin].name - 1);
                strncpy(g->rank[p->rankin].date, p->date,
                        sizeof g->rank[p->rankin].date - 1);
                g->rank[p->rankin].score = p->score;
                g->rank[p->rankin].stage = p->stage;
                g->clear_next = 1;      /* DAT_004492cc */
                game_set_state(g, ST_TITLE);
                g->hook = HOOK_NONE;
                g->hook_arg = 1;
                return;
            }
        }
    }

    /* the block cursor on the grid, on for two frames out of four */
    if (p->rcury < 0) return;
    x = p->rcury < 2 ? (p->rcurx + 4) * 0x10 : p->rcurx * 0x40 + 400;
    if (g->frame % 4 < 2) {
        if (p->rcury == 2) {
            vid_pat_raw(v, x, 400, 0x880);
            vid_pat_raw(v, x + 0x10, 400, 0x880);
            x += 0x20;
        }
        vid_pat_raw(v, x, p->rcury * 0x20 + 0x150, 0x880);
    }
    /* The original formats "rankin = %d rcurX = %02d rcurY = %02d" here and
     * then never draws it. */
}
