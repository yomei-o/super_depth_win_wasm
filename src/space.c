/* FUN_0040f970 - the third kind of stage, in space, and the one place the
 * game reads `stage3.bin`.
 *
 * The ship flies freely (x 0x20..0x220, y 0..0x140) and shoots both ways:
 * BTN1 to the left, BTN2 to the right.  What comes at it is not spawned by
 * chance but by a script: 275 entries out of stage3.bin, walked one at a
 * time - spawn this kind, wait so many frames, wait until the screen is
 * clear.  When the script runs out "EMERGENCY" flashes and the next mode
 * takes over.
 *
 * THE ENEMY ARRAY HOLDS THE BULLETS TOO.  FUN_00414060 puts kind 0x14 or
 * 0x15 into a free slot, and here a slot is free when its **kind is 0** -
 * not by its y as in the sea and the air.  That is why the stage runs all
 * 64 slots.
 */
#include "game.h"

#include <stdio.h>
#include <string.h>

static int sgn(int v) { return v > 0 ? 1 : (v < 0 ? -1 : 0); }

/* stage3.bin is little endian, like the machine it was written on. */
static int rd32(const unsigned char *q)
{
    return (int)((unsigned)q[0] | ((unsigned)q[1] << 8) |
                 ((unsigned)q[2] << 16) | ((unsigned)q[3] << 24));
}

/* 0x44072c, read with kind 0xc's own vx as the index (it runs -6..6). */
static const int SPACE_VXPAT[13] = {
    0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4
};

/* Kind 0x15's sprite: sixteen directions out of (vx, vy).  The order the
 * tests are written in matters - a later one wins - so they are kept as
 * they are in FUN_0040f970. */
static int space_dir16(int vx, int vy)
{
    int n = 8;

    if (vx == 0) {
        if (vy > 0) n = 0xc;
        if (vy < 0) n = 4;
        if (vx == vy) n = 8;
        return n;
    }
    if (vy == 0) {
        if (vx > 0) n = 0;
        if (vx < 0) n = 8;
        return n;                       /* the vy tests below do nothing */
    }
    if (vx > 0 && vy < 0) {
        int a = -vy;
        if (vx == a) n = 3;
        if (a < vx) n = 1;
        if (vx < a) n = 2;
    }
    if (vx < 0) {
        if (vy < 0) {
            if (vy == vx) n = 6;
            if (vy != vx && -vy <= -vx) n = 7;
            if (-vx < -vy) n = 5;
        }
        if (vy > 0) {
            if (-vy == vx) n = 0xb;
            if (-vy != vx && vy <= -vx) n = 9;
            if (-vx < vy) n = 10;
        }
    }
    if (vx > 0 && vy > 0) {
        if (vx == vy) n = 0xe;
        if (vy < vx) n = 0xf;
        if (vx < vy) n = 0xd;
    }
    return n;
}
static int absi(int v) { return v < 0 ? -v : v; }
static int clamp6(int v) { return v < -6 ? -6 : (v > 6 ? 6 : v); }

/* FUN_0040b850, the same box test the air stage uses. */
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

/* A slot goes back to the pool. */
static void enemy_gone(Enemy *e)
{
    e->y = -0x20;
    e->vy = 0;
    e->kind = 0;
    e->layer = 1;
    e->tick = 0;
}

/* FUN_00414060: a bullet, into whatever slot is free. */
static void bullet(Play *p, int kind, int x, int y, int vx, int vy)
{
    int i;

    for (i = 0; i < ENEMIES; i++) {
        Enemy *e = &p->e[i];
        if (e->kind != 0) continue;
        e->kind = kind;
        e->x = x;
        e->y = y;
        e->vx = vx;
        e->vy = vy;
        e->w = 0x10;
        e->h = 0x10;
        e->c1 = 0;
        e->hp = 1;
        e->phase = 0;
        e->c2 = 0;
        e->anim = 0;
        e->animt = 0;
        e->tick = 0;
        e->mirror = 0;
        return;
    }
}

/* FUN_00413ae0: the dust, which is a second layer of stars. */
static void dust_init(Game *g)
{
    Play *p = &g->p;
    int i;

    for (i = 0; i < DUSTS; i++) {
        p->dust[i].x = game_rand(g) % 0x280;
        p->dust[i].y = game_rand(g) % 0x160;
        p->dust[i].vy = -0x1c - game_rand(g) % 8;
        p->dust[i].a = 0;
        p->dust[i].k1 = game_rand(g) % 3;
        p->dust[i].k2 = game_rand(g) % 0xf;
        p->dust[i].k3 = game_rand(g) % 2;
        p->dust[i].k4 = game_rand(g) % 2;
    }
}

/* FUN_00413df0: read stage3.bin and turn it into the runtime script.
 * Returns 0 when it is good, 1 when it is not - and the stage gives up and
 * goes back to the title. */
static int script_load(Game *g)
{
    Play *p = &g->p;
    unsigned char buf[0x20 + SCRIPTS * 0x18];
    int n, count, i;

    p->nscript = 0;
    n = plat_read("stage3.bin", buf, (int)sizeof buf);
    if (n < 0x20) return 1;
    if (memcmp(buf, "SDEPTH", 7) != 0) return 1;
    if (rd32(buf + 0x10) != 2) return 1;            /* the version */
    if (rd32(buf + 0x18) != 0x32a) return 1;
    count = rd32(buf + 0x1c);
    if (count == 0) return 1;
    if (count > SCRIPTS - 1) count = SCRIPTS - 1;
    if (n < 0x20 + count * 0x18) return 1;

    for (i = 0; i < count; i++) {
        const unsigned char *r = buf + 0x20 + i * 0x18;
        Script *sc = &p->script[i];

        sc->type = 0;
        sc->v = 0;
        sc->a = 0;
        sc->b = 0;
        sc->c = 0;
        sc->d = 0;
        switch (r[4]) {
        case 1:
            if (r[5] == 1) {
                sc->type = 3;                       /* spawn, place at random */
                sc->v = rd32(r + 8);
            } else if (r[5] == 2) {
                sc->type = 2;                       /* spawn where it says */
                sc->v = rd32(r + 8);
                sc->a = rd32(r + 0x0c);
                sc->b = rd32(r + 0x10);
            }
            break;
        case 2:
            sc->type = 1;                           /* wait */
            sc->c = rd32(r + 0x14);
            break;
        case 3:
            sc->type = 0x32;                        /* wait for a clear screen */
            break;
        }
    }
    p->script[count].type = 0xff;                   /* the end */
    p->nscript = count;
    return 0;
}

/* The per-kind setup the script's two spawn commands share.  `at_y` is -1
 * when the kind picks its own lane (script type 3) and the y otherwise. */
static void spawn(Game *g, int i, int kind, int at_y)
{
    Play *p = &g->p;
    Enemy *e = &p->e[i];

    e->kind = kind;
    switch (kind) {
    case 1:
        e->y = at_y >= 0 ? at_y : game_rand(g) % 0xc0 + 0x40;
        e->x = 0x220 - game_rand(g) % 0x40;
        e->vx = 0;
        e->vy = 0;
        e->w = 0x20;
        e->h = 0x20;
        e->c1 = 0;
        e->hp = 1;
        e->anim = 0;
        e->c2 = -6 - game_rand(g) % 6;
        e->animt = 0;
        break;
    case 2:
        e->y = at_y >= 0 ? at_y : game_rand(g) % 0xc0 + 0x40;
        e->x = 0x380;
        e->vx = -2 - game_rand(g) % 6;
        e->vy = 0;
        e->w = 0x40;
        e->h = 0x20;
        break;
    case 3:
        e->y = at_y >= 0 ? at_y : game_rand(g) % 0xc0 + 0x40;
        e->x = -0x140;
        e->vx = game_rand(g) % 6 + 2;
        e->vy = 0;
        e->w = 0x40;
        e->h = 0x20;
        break;
    case 4:
        e->y = 0x9a;
        e->x = 0x1c0;
        e->vx = 0;
        e->vy = 0;
        e->w = 0x80;
        e->h = 0x40;
        e->phase = 0;
        e->hp = 0x14;
        e->c1 = -0x7f;
        e->anim = 0;
        e->animt = game_rand(g) % 0xb4 + 600;
        break;
    case 5:
        e->y = at_y >= 0 ? at_y : game_rand(g) % 0xb0 + 0x60;
        e->x = 0x220 - game_rand(g) % 0x40;
        e->vx = 0;
        e->vy = 0;
        e->w = 0x40;
        e->h = 0x40;
        e->c1 = 0;
        e->hp = 3;
        break;
    case 6:
        e->y = at_y >= 0 ? at_y : game_rand(g) % 200 + 0x40;
        e->x = -0x140;
        e->vx = 0;
        e->vy = 0;
        e->w = 0x20;
        e->h = 0x20;
        e->c1 = 0;
        e->hp = 1;
        e->phase = 0;
        e->anim = 0;
        e->animt = 3;
        e->mirror = 0;
        e->layer = 0;
        break;
    case 7:
    case 8:
        e->y = at_y >= 0 ? at_y : 0x60;
        e->x = 0x3c0;
        e->vx = -4;
        e->vy = -3;
        e->w = 0x20;
        e->h = 0x10;
        e->hp = 1;
        e->phase = 0;
        e->animt = 3;
        break;
    case 9:
        e->aim = 200;
        e->y = at_y >= 0 ? at_y : game_rand(g) % 0xf0 + 0x60;
        e->x = 0x39f;
        e->vx = -3 - game_rand(g) % 5;
        e->vy = 0;
        e->w = 0x20;
        e->h = 0x20;
        e->c1 = 0;
        break;
    case 10:
        e->x = 0x3c0;
        e->y = at_y >= 0 ? at_y : game_rand(g) % 0xf0 + 0x60;
        e->vx = -6;
        e->vy = -6;
        e->w = 0x20;
        e->h = 0x20;
        e->phase = 0;
        e->hp = 2;
        e->anim = 0;
        break;
    case 0xc:
        e->x = 0x3c0;
        e->y = at_y >= 0 ? at_y : game_rand(g) % 0x130 + 0x20;
        e->vx = -6;
        e->vy = 0;
        e->w = 0x20;
        e->h = 0x20;
        e->phase = 0;
        e->hp = 2;
        e->anim = 0;
        break;
    default:
        return;                         /* the kinds with no code */
    }
    if (kind != 6 && kind != 9) e->mirror = 0;
}

/* The script, walked as far as it goes this frame. */
static void script_run(Game *g)
{
    Play *p = &g->p;
    int i;

    for (;;) {
        Script *sc;
        int type;

        if (p->sc_wait > 0) {
            p->sc_wait--;
            if (p->sc_wait != 0) return;
        }
        if (p->sc_at < 0 || p->sc_at > p->nscript) return;
        sc = &p->script[p->sc_at];
        type = sc->type;
        if (type == 0xff) {             /* the end: park here */
            p->sc_wait = 999;
            return;
        }
        if (type > 3) {
            if (type != 0x32) return;
            if (p->onscreen == 0) p->sc_at++;
            return;
        }
        if (type == 0) {
            p->sc_at++;
            continue;
        }
        if (type == 1) {
            p->sc_wait = sc->c;
            p->sc_at++;
            continue;
        }
        /* type 2 and 3 both put an enemy in the first free slot */
        for (i = 0; i < p->nenemy; i++)
            if (p->e[i].kind == 0) break;
        if (i >= p->nenemy) return;     /* no room: try again next frame */
        spawn(g, i, sc->v, type == 2 ? sc->b : -1);
        p->sc_at++;
    }
}

/* ---- the enemies ------------------------------------------------------ */

/* The tail several kinds share: stay while the value is inside the range,
 * otherwise the slot goes back. */
static int inside(int v, int low, int high)
{
    return v > low && v < high;
}

static void space_enemy(Game *g, int i)
{
    Play *p = &g->p;
    Enemy *e = &p->e[i];
    int t, d, s;

    switch (e->kind) {
    case 1:
        if (!inside(e->x + e->vx, -0x140, 0x3c0)) { enemy_gone(e); break; }
        if (e->y < -0x1f) {
            p->onscreen--;
        } else if (e->anim < 5) {
            if (e->animt < 2) e->animt++;
            else { e->animt = 0; e->anim++; }
        } else {
            if (e->vx == 0) {
                if (p->py < e->y) e->vy = -1;
                if (e->y < p->py) e->vy = 1;
            }
            t = e->vx - 1;
            e->vx = e->c2 <= t ? (t > 0 ? 0 : t) : e->c2;
        }
        break;

    case 2:
    case 3:
        if (e->x > 0 && e->x < 0x260 && game_rand(g) % 300 == 0) {
            int bx, by;
            d = p->px - e->x;
            if (absi(d + 0x20) < 0x21) {
                bx = game_rand(g) % 2 - 2;
            } else {
                s = sgn(d);
                bx = (game_rand(g) % 2 + 2) * s;
            }
            d = p->py - e->y;
            if (absi(d + 0x20) < 0x21) {
                by = game_rand(g) % 2 - 2;
            } else {
                s = sgn(d);
                by = (game_rand(g) % 2 + 2) * s;
            }
            bullet(p, 0x14, e->x + 0x10, e->y + 8, bx, by);
        }
        if (!inside(e->x + e->vx, -0x140, 0x3c0)) { enemy_gone(e); break; }
        if (e->y < -0x1f) p->onscreen--;
        break;

    case 4:                             /* the big one, in six phases */
        switch (e->phase) {
        case 0:
            if (++e->c1 > -0x40) e->phase++;
            break;
        case 1:
            if (++e->c1 > -1) { e->phase++; e->vy = -0xe; }
            break;
        case 2:
            e->animt--;
            if (++e->vy > 0xd) e->phase++;
            if (e->animt < 1) e->phase = 4;
            break;
        case 3:
            e->animt--;
            if (--e->vy < -0xd) e->phase--;
            if (e->animt < 1) e->phase = 4;
            break;
        case 4:
            e->vy = 0;
            if (--e->c1 < -0x3f) e->phase++;
            break;
        case 5:
            if (--e->c1 < -0x7f) enemy_gone(e);
            break;
        }
        if ((e->phase == 2 || e->phase == 3) && e->animt % 0x19 == 0)
            bullet(p, 0x15, e->x + 0x40, e->y + 0x20, -4, -4);
        e->anim = (e->anim + 4) & 0xff;
        break;

    case 5:
        if (!inside(e->x + e->vx, -0x140, 0x3c0)) { enemy_gone(e); break; }
        if (e->y < -0x1f) {
            p->onscreen--;
        } else if (e->c1 < 0x100) {
            e->c1 += 0x10;
            if (e->c1 > 0xff) {
                e->vx = -8 - game_rand(g) % 4;
                e->vy = game_rand(g) % 3 - 1;
                e->c1 = 0x100;
            }
        }
        break;

    case 6:
        e->vx = 0;
        if (e->phase == 0) {
            e->vx = 4;
            if (e->x + 4 > 0x3bf) { e->phase++; e->layer = 1; }
        } else if (e->phase == 1) {
            e->vx = -8;
            if (e->x - 8 < -0x13f) { enemy_gone(e); break; }
        }
        if (e->animt == 0) { e->animt = 3; e->anim ^= 1; }
        else e->animt--;
        break;

    case 7:
        switch (e->phase) {
        case 0:
            e->vy++;
            if (e->x > 0x167) { if (e->vy > 2) e->phase++; }
            else e->phase = 4;
            break;
        case 1:
            e->vy--;
            if (e->vy < -2) e->phase--;
            else if (e->x < 0x168) e->phase = 4;
            break;
        case 2:
            if (++e->vy > 2) e->phase++;
            break;
        case 3:
            if (--e->vy < -2) e->phase--;
            break;
        case 4:
            e->anim = e->y < 0xa1 ? 1 : -1;
            e->phase++;
            /* falls through */
        case 5:
            e->vx = 0;
            e->vy = e->anim << 1;
            if (++e->animt > 0x3b) {
                e->vx = -4;
                e->vy = -3;
                e->phase = 2;
            }
            break;
        }
        if (e->x + e->vx < -0x13f) enemy_gone(e);
        break;

    case 8:
        switch (e->phase) {
        case 0:
            e->vy++;
            if (e->x < 0x168) e->phase = 4;
            else if (e->vy > 2) e->phase++;
            break;
        case 1:
            e->vy--;
            if (e->vy < -2) e->phase--;
            else if (e->x < 0x168) e->phase = 4;
            break;
        case 4:
            e->anim = e->y < 0xa1 ? 1 : -1;
            e->phase++;
            /* falls through */
        case 5:
            e->vy = e->anim << 1;
            break;
        }
        if (inside(e->x + e->vx, -0x140, 0x3c0) &&
            inside(e->vy + e->y, 0, 0x160)) break;
        enemy_gone(e);
        break;

    case 9:
        if (!inside(e->x + e->vx, -0x140, 0x3a0)) { enemy_gone(e); break; }
        if (e->y < -0x1f) {
            p->onscreen--;
        } else {
            if (e->c1 == 0) {
                if (g->frame % 8 == 0 && e->x < 300) e->vx++;
                if (e->vx >= 0) { e->vx = 0; e->c1 = 1; }
            } else if (e->c1 == 1) {
                if (g->frame % 8 == 0) e->vx++;
                if (e->vx > 4) { e->vx = 5; e->c1 = 2; }
            }
            e->vy += sgn(e->aim - e->y);
        }
        break;

    case 10:
        e->anim ^= 1;
        if (e->phase == 0) {
            if (e->anim) e->vy++;
            if (e->vy > 5) e->phase++;
        } else if (e->phase == 1) {
            if (e->anim) e->vy--;
            if (e->vy < -5) e->phase--;
        }
        if (e->x + e->vx < -0x13f) enemy_gone(e);
        break;

    case 0xc:
        switch (e->phase) {
        case 0:
            if (e->x < 0x280) {
                e->phase = 1;
                t = game_rand(g) % 3;
                e->vy = e->y > 0xb0 ? -t : t;
            }
            break;
        case 1:
            if (e->x < 400) e->phase = 2;
            break;
        case 2:
            if (++e->vx > 5) e->phase++;
            break;
        case 3:
            if (e->x + e->vx < 0x3c0 && inside(e->vy + e->y, 0, 0x160)) break;
            enemy_gone(e);
            break;
        }
        break;

    case 0x14:
        e->anim = (e->anim + 1) % 3;
        if (inside(e->x + e->vx, 0x10, 0x260) &&
            inside(e->vy + e->y, -0x10, 0x160)) break;
        enemy_gone(e);
        break;

    case 0x15:
        e->tick++;
        if (e->x < -0x1f || e->x > 0x29f || e->y < -0x1f || e->y > 0x17f) {
            enemy_gone(e);
            break;
        }
        if (e->y > -0x20 &&
            ((e->tick < 0x79 ||
              (e->vx > -3 && e->vx < 3 && e->vy > -3 && e->vy < 3)) &&
             e->tick % 3 == 0)) {
            if (p->px + 0x28 < e->x) e->vx = clamp6(e->vx - 1);
            if (e->x < p->px + 8) e->vx = clamp6(e->vx + 1);
            if (p->py + 0x1c < e->y) e->vy = clamp6(e->vy - 1);
            if (e->y < p->py + 4) e->vy = clamp6(e->vy + 1);
        }
        break;
    }
}

/* The box each kind is hit in.  `w`/`h` come back through the pointers; the
 * return is 0 for the kinds nothing can touch. */
static int hit_box(const Enemy *e, int *w, int *h)
{
    switch (e->kind) {
    case 1: *w = 0x20; *h = 0x20; return 1;
    case 2: case 3: *w = 0x40; *h = 0x20; return 1;
    case 4: *w = 0x80; *h = 0x40; return 1;
    case 5: *w = 0x40; *h = 0x40; return 1;
    case 6: *w = 0x20; *h = 0x20; return 1;
    case 7: case 8: *w = 0x20; *h = 0x10; return 1;
    case 9: *w = 0x20; *h = 0x20; return 1;
    case 10: case 0xc: *w = 0x20; *h = 0x20; return 1;
    case 0x14: case 0x15: *w = 0x10; *h = 0x10; return 1;
    default: return 0;
    }
}

/* Whether this kind can be hit at all right now, and what a hit does: the
 * kinds with more than one hit point lose one and flash. */
static int hittable(const Enemy *e)
{
    switch (e->kind) {
    case 1: return e->vx != 0;          /* only once it is moving */
    case 4: return e->phase == 2 || e->phase == 3;
    case 5: return e->c1 > 0xfe;
    case 6: return e->phase == 1;
    default: return 1;
    }
}

static int tough(const Enemy *e)
{
    return e->kind == 4 || e->kind == 5 || e->kind == 10 || e->kind == 0xc;
}

/* One hit, from anything.  Returns 1 when the thing that hit it is used up. */
static int enemy_take(Game *g, int i, int x)
{
    Play *p = &g->p;
    Enemy *e = &p->e[i];

    if (tough(e)) {
        if (--e->hp >= 1) {
            plat_se("depth06", (x - 0x140) * 0x1f);
            e->mirror = 1;              /* one frame of white */
            return 1;
        }
    }
    if (e->kind == 9 && p->item == 0) { /* kind 9 is the one that drops one */
        play_item_pick(g);
        p->itemx = e->x + 8;
        p->itemy = e->y + 8;
        p->itemk = 8;
        p->itemvy = 0;
        p->itemt = 0;
    }
    play_enemy_hit(g, i, 1);
    p->kills++;
    return 1;
}

/* ---- the frame -------------------------------------------------------- */

void space_frame(Game *g)
{
    Play *p = &g->p;
    Video *v = g->v;
    const DarPat *pa, *pb;
    int i, j, k, n, pass;

    if (g->hook_arg) {
        g->hook_arg = 0;
        game_scene(g, "space.dar", 0x32);
        play_field_build(g);
        p->announce = 0;
        p->over = 0;
        p->banner = 0;
        g->flash = 0;
        dust_init(g);
        if (script_load(g) != 0) {      /* no stage3.bin: give up */
            game_set_state(g, ST_TITLE);
            return;
        }
        p->emerg = 0;
        p->sc_wait = 0;
        p->sc_at = 0;
        p->nenemy = ENEMIES;
        p->itemvy = 0;
        p->itemk = 0;
        p->itemy = 0;
        p->itemx = 0;
        p->item = 0;
        p->py = 0xaa;
        p->px = 0x40;
        p->big_timer = 0;
        p->scroll_n = 1;
        p->onscreen = 0;
        p->kills = 0;
        if (p->loaded == p->stage) {
            p->speed = 2;
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
            p->banner = 0x96;
        } else {
            p->announce = 0x96;
            p->announce_stage = p->stage;
        }
        if (p->speed < 5) p->speed = 4;
        p->loaded = p->stage;
        plat_bgm(3, "bgm05");
        p->inflight = 0;
        for (i = 0; i < UPSHOTS; i++) { p->up[i].y = -0x10; p->up[i].dx = 0; }
        for (i = 0; i < ENEMIES; i++) {
            p->e[i].y = -0x20;
            p->e[i].vy = 0;
            p->e[i].kind = 0;
            p->e[i].layer = 1;
            p->e[i].tick = 0;
        }
        p->itemt = 0;
        p->drift_x = 0;
        p->drift_y = 0;
        p->quota = ((p->powerA & p->powerB) + (p->stage / 4)) * 10 + 0x2d;
        pa = vid_pat_info(v, EXT_BASE + 48);
        p->spacex[0] = 0x280;
        p->spacevx[0] = -1;
        p->spacey[0] = game_rand(g) % 100 + 0x40;
        p->spacex[1] = (pa ? pa->w : 0) + 0x140 + 0x280;
        p->spacevx[1] = -1;
        p->spacey[1] = game_rand(g) % 100 + 0x40;
        for (i = 0; i < BIGS; i++) p->big[i].on = 0;
        p->flash2 = 0;
    }

    if (p->kills > 100) p->kills -= 100;

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
        p->drift_y = 0;
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
            p->drift_y++;               /* FUN_0040b080(0) */
        }
        if (game_down(g)) {
            n = p->py + p->speed;
            p->py = n < 0 ? 0 : (n < 0x141 ? n : 0x140);
            p->drift_y--;               /* FUN_0040b080(1) */
        }
        if (game_any_key(g) && p->inflight < p->charges + p->powerA * -2) {
            int vx = (-2 - p->powerB) * 6;
            for (i = 0; i < p->charges && i < UPSHOTS; i++)
                if (p->up[i].y < -0xf) break;
            if (i < UPSHOTS) {
                p->up[i].x = p->px + 5;
                p->up[i].y = p->py + 0xc;
                p->up[i].vx = vx;
                p->up[i].dx = 0;
                p->inflight++;
                if (p->powerA == 1) {
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
                }
                plat_se("depth05", (p->up[i].x - 0x140) * 0x1f);
            }
        }
        if (game_btn2(g) && p->inflight < p->charges + p->powerA * -2) {
            int vx = (p->powerB * 3 + 6) * 2;
            for (i = 0; i < p->charges && i < UPSHOTS; i++)
                if (p->up[i].y < -0xf) break;
            if (i < UPSHOTS) {
                p->up[i].x = p->px + 0x2c;
                p->up[i].y = p->py + 0xc;
                p->up[i].vx = vx;
                p->up[i].dx = 0;
                p->inflight++;
                if (p->powerA == 1) {
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
                }
                plat_se("depth05", (p->up[i].x - 0x140) * 0x1f);
            }
        }
    }

    script_run(g);

    /* --- the enemies ---------------------------------------------------- */
    p->onscreen = 0;
    for (i = 0; i < p->nenemy; i++) {
        Enemy *e = &p->e[i];

        if (e->kind != 0) p->onscreen++;
        if (e->state < 10) {
            if (p->flip != 0) e->state--;
            if (e->state < 0) {
                e->kind = 0;
                e->y = -0x20;
                e->state = 10;
            }
            continue;
        }
        space_enemy(g, i);
    }

    /* --- the big shot the Flash Bomb turns into ------------------------- */
    p->big_timer--;
    if (p->big_timer < 0) {
        p->big_timer = 0;
    } else {
        if (p->big_timer > 999) p->big_timer = 999;
        if (p->big_timer >= 1 && p->big_timer % 3 == 0) {
            for (i = 0; i < BIGS; i++) {
                if (p->big[i].on == 1) continue;
                p->big[i].on = 1;
                p->big[i].x = -0x40;
                p->big[i].y = game_rand(g) % 0x160 - 0x10;
                p->flash2 = 1;
                p->big[i].vx = (game_rand(g) % 4) + 0x18;
                plat_se("finalt09", (game_rand(g) % 0x280 - 0x140) * 0x1f);
                break;
            }
        }
    }
    for (i = 0; i < BIGS; i++)
        if (p->big[i].on != 0) {
            p->big[i].x += p->big[i].vx;
            if (p->big[i].x > 0x25f) p->big[i].on = 0;
        }
    for (i = 0; i < BIGS; i++) {
        if (!p->big[i].on) continue;
        for (j = 0; j < p->nenemy; j++) {
            Enemy *e = &p->e[j];
            int w, h;
            if (e->state < 10 || e->y < -0x1f || e->y > 0x17f) continue;
            if (!hit_box(e, &w, &h)) continue;
            if (!hittable(e)) continue;
            if (!overlap(e->x, e->y, p->big[i].x, p->big[i].y, w, h, 0x40, 0x20))
                continue;
            enemy_take(g, j, p->big[i].x);
        }
    }

    /* --- the ship's shots ----------------------------------------------- */
    for (k = 0; k < p->charges && k < UPSHOTS; k++) {
        UpShot *s = &p->up[k];

        if (s->y <= -0x10) continue;
        s->x += s->vx;
        s->y += (p->powerB + 2) * s->dx * 2;
        for (j = 0; j < p->nenemy; j++) {
            Enemy *e = &p->e[j];
            int w, h;
            if (e->state < 10 || e->y < -0x1f || e->y > 0x17f) continue;
            if (!hit_box(e, &w, &h)) continue;
            if (!hittable(e)) continue;
            if (!overlap(e->x, e->y, s->x, s->y + 2, w, h, 0x10, 0xc)) continue;
            enemy_take(g, j, s->x);
            s->y = -0x10;
            break;
        }
        if (s->x < 0 || s->x > 0x26f || s->y > 0x15f) s->y = -0x10;
        if (s->y < -0xf) p->inflight--;
    }

    /* --- everything moves ----------------------------------------------- */
    for (i = 0; i < p->nenemy; i++)
        if (p->e[i].y > -0x20 && p->e[i].y < 0x180) {
            p->e[i].x += p->e[i].vx;
            p->e[i].y += p->e[i].vy;
        }

    /* --- and can run into the ship -------------------------------------- */
    for (i = 0; i < p->nenemy; i++) {
        Enemy *e = &p->e[i];
        int w, h;

        if (p->life < 10 || e->y < -0x1f || e->y > 0x15f || e->state < 10)
            continue;
        if (p->hit == 1 || p->big_timer > 0) break;
        switch (e->kind) {
        case 1: case 9: case 10: case 0xc: w = 0x20; h = 0x20; break;
        case 2: case 3: w = 0x40; h = 0x20; break;
        case 4: w = 0x80; h = 0x40; break;
        case 5: w = 0x40; h = 0x40; break;
        case 6: w = 0x40; h = 0x20; break;
        case 7: case 8: w = 0x20; h = 0x10; break;
        case 0x14: case 0x15:
            if (overlap(e->x + 4, e->y + 4, p->px + 2, p->py + 8, 8, 8,
                        0x3c, 0x18) && p->life > 9) {
                p->life = 9;
                plat_se("burn", (p->px + 0x20 - 0x140) * 0x1f);
            }
            continue;
        default:
            continue;
        }
        if (e->kind == 4 && e->phase != 2 && e->phase != 3) continue;
        if (e->kind == 6 && e->phase != 1) continue;
        if (overlap(e->x, e->y, p->px, p->py + 8, w, h, 0x20, 0x14) &&
            p->life > 9) {
            p->life = 9;
            plat_se("burn", (p->px + 0x20 - 0x140) * 0x1f);
        }
    }

    /* --- the item drifts left ------------------------------------------- */
    if (p->item != 0) {
        p->itemx += p->itemk;
        n = p->itemk - 1;
        p->itemk = n < -6 ? -6 : (n < 1000 ? n : 999);
        if (p->itemx >= p->px - 4 && p->itemx <= p->px + 0x34 &&
            p->itemy >= p->py - 4 && p->itemy <= p->py + 0x20 &&
            p->life == 10) {
            if (p->item == 4) p->big_timer += 0x3c;     /* more big shots */
            else play_item_apply(p);
            p->item = 0;
            plat_se("item", 0);
        }
        if (p->itemx < 0) p->item = 0;
    }

    /* --- the stars drift ------------------------------------------------ */
    if (g->frame % 8 == 0 && p->drift_x > -8) p->drift_x--;
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

    /* --- the two pictures drifting past --------------------------------- */
    pa = vid_pat_info(v, EXT_BASE + 48);
    pb = vid_pat_info(v, EXT_BASE + 49);
    for (n = 0; n < p->scroll_n; n++)
        for (i = 0; i < 2; i++) {
            const DarPat *q = i == 0 ? pa : pb;
            if (!q) continue;
            if (g->frame % 2 == 0) p->spacex[i] += p->spacevx[i];
            if (p->spacex[i] < -q->w) {
                p->spacex[i] += q->w + 0x3c0;
                p->spacey[i] = game_rand(g) % 100 + 0x60;
            }
        }
    for (i = 0; i < 2; i++)
        vid_pat_wave(v, p->spacex[i], p->spacey[i], EXT_BASE + 48 + i,
                     -0x100, 0x10, (int)g->frame);
    for (i = 0; i < STARS2; i++)
        vid_pat(v, p->cloud[i].x, p->cloud[i].y, p->cloud[i].kind + 0xb30);

    /* --- the enemies, in two passes so the big things layer -------------- */
    for (pass = 0; pass < 2; pass++)
        for (i = 0; i < p->nenemy; i++) {
            Enemy *e = &p->e[i];

            if (e->y < -0x1f || e->layer != pass) continue;
            if (e->state < 10) {
                switch (e->kind) {
                case 1: case 6: case 7: case 8: case 9: case 10: case 0xc:
                    play_boom(v, e->x - 0x10, e->y - 0x10, 9 - e->state);
                    break;
                case 2: case 3:
                    play_boom(v, e->x, e->y - 0x10, 9 - e->state);
                    break;
                case 4:
                    play_boom(v, e->x, e->y, 9 - e->state);
                    play_boom(v, e->x + 0x40, e->y, 9 - e->state);
                    break;
                case 5:
                    play_boom(v, e->x, e->y, 9 - e->state);
                    break;
                case 0x14: case 0x15:
                    if (e->state > 4) e->state = 3;
                    /* FUN_0040ae90: the small four-frame one */
                    if (3 - e->state < 4)
                        vid_pat(v, e->x, e->y, (3 - e->state) + 0x997);
                    break;
                }
                continue;
            }
            switch (e->kind) {
            case 1:
                vid_pat(v, e->x, e->y, e->anim + 0xac0);
                break;
            case 2:
            case 3:
                vid_pat(v, e->x, e->y, (e->vx < 1) + 0xa23);
                break;
            case 4:
                /* four pieces, and they wave */
                switch (e->phase) {
                case 0: case 5:
                    vid_pat_wave(v, e->x, e->y, 0xa84, -0x60,
                                 (signed char)e->c1, e->anim);
                    vid_pat_wave(v, e->x + 0x40, e->y, 0xa85, -0x60,
                                 (signed char)e->c1, e->anim);
                    break;
                case 1: case 4:
                    vid_pat_wave(v, e->x, e->y, 0xa86, -0x60,
                                 (signed char)e->c1, e->anim);
                    vid_pat_wave(v, e->x + 0x40, e->y, 0xa87, -0x60,
                                 (signed char)e->c1, e->anim);
                    break;
                case 2: case 3:
                    if (e->mirror == 0) {
                        vid_pat(v, e->x, e->y, 0xa86);
                        vid_pat(v, e->x + 0x40, e->y, 0xa87);
                    } else {
                        vid_pat_flash(v, e->x, e->y, 0xa86);
                        vid_pat_flash(v, e->x + 0x40, e->y, 0xa87);
                    }
                    e->mirror = 0;
                    break;
                }
                break;
            case 5:
                if (e->c1 < 0x100) {
                    vid_pat_scale(v, e->x, e->y, 0xa81, e->c1, 0x100);
                    e->mirror = 0;
                } else if (e->mirror == 0) {
                    vid_pat(v, e->x, e->y, 0xa81);
                } else {
                    vid_pat_flash(v, e->x, e->y, 0xa81);
                    e->mirror = 0;
                }
                break;
            case 6:
                vid_pat(v, e->x, e->y, e->anim + 0xaf2 + e->phase * 2);
                break;
            case 7:
                n = -1;
                if (e->phase == 0 || e->phase == 2) {
                    if (e->vy == -3 || e->vy == -2) n = 0xa4a;
                    else n = 0xa48;
                } else if (e->phase == 1 || e->phase == 3) {
                    if (e->vy == 2 || e->vy == 3) n = 0xa49;
                    else n = 0xa48;
                } else if (e->phase == 4 || e->phase == 5) {
                    n = e->anim == 1 ? 0xa49 : 0xa4a;
                }
                if (n > 0) vid_pat(v, e->x, e->y, n);
                break;
            case 8:
                n = -1;
                if (e->phase == 0 || e->phase == 2) {
                    if (e->vy == -3 || e->vy == -2) n = 0xa4b + 2;
                    else n = 0xa4b;
                } else if (e->phase == 1 || e->phase == 3) {
                    if (e->vy == 2 || e->vy == 3) n = 0xa4b + 1;
                    else n = 0xa4b;
                } else if (e->phase == 4 || e->phase == 5) {
                    n = e->anim == 1 ? 0xa4c : 0xa4d;
                }
                if (n > 0) vid_pat(v, e->x, e->y, n);
                break;
            case 9:
                vid_pat(v, e->x, e->y, 0x9ca - (e->vx > 0));
                break;
            case 10:
                if (e->mirror == 0) vid_pat(v, e->x, e->y, 0xac6);
                else vid_pat_flash(v, e->x, e->y, 0xac6);
                e->mirror = 0;
                break;
            case 0xc:
                n = SPACE_VXPAT[(e->vx < -6 ? -6 : (e->vx > 6 ? 6 : e->vx)) + 6] + 0xb06;
                if (e->mirror == 0) vid_pat(v, e->x, e->y, n);
                else vid_pat_flash(v, e->x, e->y, n);
                e->mirror = 0;
                break;
            case 0x14:
                vid_pat(v, e->x, e->y, e->anim + 0x987);
                break;
            case 0x15:
                vid_pat(v, e->x, e->y, space_dir16(e->vx, e->vy) + 0xb1b);
                break;
            }
        }

    /* --- the big shots, the ship's own shots, the ship ------------------- */
    for (i = 0; i < BIGS; i++)
        if (p->big[i].on != 0) {
            vid_pat(v, p->big[i].x, p->big[i].y, 0xaa0);
            vid_pat(v, p->big[i].x + 0x20, p->big[i].y, 0xaa1);
        }
    for (i = 0; i < p->charges && i < UPSHOTS; i++)
        if (p->up[i].y > -0x10) {
            n = p->up[i].dx + 0x9a9 + p->powerB * 9;
            if (p->up[i].vx > 0) n += 3;
            vid_pat(v, p->up[i].x, p->up[i].y, n);
        }
    if (p->life < 10) {
        play_boom(v, p->px, p->py - 0x10, 9 - p->life);
    } else {
        if (p->big_timer != 0) {
            n = (int)(p->big_timer % 4);
            vid_pat(v, p->px + 8, p->py + 8, n * 2 + 0xa90);
            vid_pat(v, p->px + 0x28, p->py + 8, n * 2 + 0xa91);
        }
        vid_pat(v, p->px, p->py, 0xa05);
    }
    if (p->item != 0 && p->itemx > 0) {
        vid_pat(v, p->itemx, p->itemy, p->item + 0x989);
        play_item_name(g, p->itemx, p->itemy - 8, p->item);
    }
    if (p->flash2 != 0) {
        if (p->flash2 % 2 != 0) vid_clear(v, 0xff);
        p->flash2--;
    }

    /* --- the script has run out ----------------------------------------- */
    if (p->item == 0 && p->onscreen == 0 &&
        p->script[p->sc_at].type == 0xff && p->life == 10) {
        vid_text(v, 0x1f, 10, "EMERGENCY", FNT_RED);
        if (p->emerg % 2 != 0) p->drift_x--;
        n = p->scroll_n + 1;
        p->scroll_n = n < 0 ? 0 : (n < 0x21 ? n : 0x20);
        if (p->emerg == 0) plat_bgm(0, "bgm15");
        if (p->emerg % 0xe == 0) {
            g->flash = 6;
            if (p->emerg > 0x29) g->flash = 0x1e;
        }
        if (p->emerg > 0x45) {
            plat_se("burn", 0);
            p->stage++;
            g->hook = HOOK_BOSS;        /* LAB_004011c7 -> FUN_00403dc0 */
            g->hook_arg = 1;
        }
        p->emerg++;
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
