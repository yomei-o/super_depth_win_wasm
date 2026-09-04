/* The fourth stage (FUN_00403dc0): the big one, and the loop back to the
 * sea stage that closes the game.
 *
 *     tmp/boss_check.exe            checks, and writes tmp/boss_*.png
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dar.h"
#include "game.h"
#include "png.h"
#include "video.h"

static Dar dar;
static Video vid;
static Game game;
static int fails;

int plat_dar(Dar *d, const char *name)
{
    char path[96];

    sprintf(path, "disk/%s", name);
    return dar_load(d, path);
}

static char bgm_last[64];
static int bgm_mode_last = -1;
static char se_last[64];

void plat_bgm(int mode, const char *name)
{
    bgm_mode_last = mode;
    strncpy(bgm_last, name, sizeof bgm_last - 1);
}

void plat_se(const char *name, int pan)
{
    (void)pan;
    strncpy(se_last, name, sizeof se_last - 1);
}

int plat_read(const char *name, unsigned char *buf, int max)
{
    char path[96];
    FILE *f;
    int n;

    sprintf(path, "disk/%s", name);
    f = fopen(path, "rb");
    if (!f) return -1;
    n = (int)fread(buf, 1, (size_t)max, f);
    fclose(f);
    return n;
}

/* The checks never put a recording on the disk - disk/demo1.dat is the
 * original's own - so this only remembers that it was asked. */
int demo_written = -1;

void plat_write(const char *name, const unsigned char *buf, int n)
{
    (void)name;
    (void)buf;
    demo_written = n;
}

static void ok(int cond, const char *what)
{
    if (!cond) { printf("FAIL %s\n", what); fails++; }
}

static void shot(const char *path)
{
    if (!png_write_indexed(path, SCR_W, SCR_H, &vid.px[0][0], SCR_W,
                           vid_palette(&vid), 256))
        printf("FAIL cannot write %s\n", path), fails++;
}

/* Sea, air, space, and out the other side. */
static void reach_boss(void)
{
    Play *p = &game.p;
    int i;

    game_init(&game, &vid);
    game_set_date(&game, 1999, 2, 14);
    for (i = 0; i < 400 && game.state != ST_TITLE; i++) game_tick(&game);
    game_tick(&game);
    game_set_pad(&game, PAD_BTN1);
    game_tick(&game);
    game_set_pad(&game, 0);
    game_tick(&game);
    p->hit = 1;
    for (i = 0; i < p->nenemy; i++) { p->e[i].y = 0; p->e[i].state = 10; }
    p->kills = p->quota;
    p->onscreen = 0;
    for (i = 0; i < 300 && game.hook != HOOK_AIR; i++) game_tick(&game);
    game_tick(&game);
    p->hit = 1;
    for (i = 0; i < p->nenemy; i++) { p->e[i].y = -0x20; p->e[i].state = 10; }
    p->kills = p->quota;
    p->onscreen = 0;
    p->item = 0;
    for (i = 0; i < 400 && game.hook != HOOK_SPACE; i++) game_tick(&game);
    game_tick(&game);
    p->hit = 1;
    p->sc_at = p->nscript;              /* wind the script to its end */
    p->sc_wait = 0;
    for (i = 0; i < p->nenemy; i++) p->e[i].kind = 0;
    p->onscreen = 0;
    p->item = 0;
    for (i = 0; i < 300 && game.hook != HOOK_BOSS; i++) {
        game_tick(&game);
        p->hit = 1;
    }
}

int main(void)
{
    Play *p = &game.p;
    Enemy *b = &game.p.e[0];
    int i, t, x0;

    if (dar_load(&dar, "disk/depth.dar") != 0) {
        printf("FAIL cannot read disk/depth.dar\n");
        return 1;
    }
    vid_init(&vid, &dar);

    reach_boss();
    ok(game.hook == HOOK_BOSS, "the space stage hands over to the boss");
    game_tick(&game);                                   /* its first frame */
    ok(p->stage == 4, "on stage 4");
    ok(!strcmp(bgm_last, "bgm06") && bgm_mode_last == 3, "bgm06 plays");
    ok(p->boss_hits == 0 && p->boss_phase == 0, "the boss is fresh");
    ok(p->boss_live == 1, "and shows on the sonar");
    ok(p->px == 0x40 && p->py == 0xaa, "the ship starts at the left");
    ok(p->quota == 999, "the quota is out of the way");
    shot("tmp/boss_first.png");

    /* It comes in from the right. */
    p->hit = 1;
    x0 = b->x;
    for (t = 0; t < 60; t++) game_tick(&game);
    ok(b->x < x0, "the boss works its way in");
    shot("tmp/boss_in.png");

    /* A shot into the middle of it counts; one into the rest does not. */
    p->boss_hits = 0;
    p->score = 0;
    b->x = 0x100;
    b->y = 0x40;
    b->vx = 0;
    b->vy = 0;
    for (i = 0; i < UPSHOTS; i++) p->up[i].y = -0x10;
    p->inflight = 0;
    p->up[0].x = b->x + 0x10;
    p->up[0].y = b->y + 0x24;           /* inside 0x1c..0x34: the weak spot */
    p->up[0].vx = 0;
    p->up[0].dx = 0;
    p->inflight = 1;
    game_tick(&game);
    ok(p->boss_hits == 1, "a shot in the middle tells");
    ok(p->score > 0, "and scores");
    ok(!strcmp(se_last, "depth06"), "with the hit sound");
    p->up[0].x = b->x + 0x10;
    p->up[0].y = b->y + 0x50;           /* outside it: no hit */
    p->up[0].vx = 0;
    p->up[0].dx = 0;
    p->inflight = 1;
    game_tick(&game);
    ok(p->boss_hits == 1, "a shot anywhere else does not");

    /* Thirty hits and it comes apart, then the game goes back to stage 1. */
    p->boss_hits = 0x1e;
    game_tick(&game);
    ok(p->boss_phase > 0, "thirty hits start it blowing up");
    for (t = 0; t < 0x78 - 2; t++) game_tick(&game);
    shot("tmp/boss_dying.png");
    {
        int on = 0;
        for (i = 0; i < BOOMS; i++) if (p->boom[i].on) on++;
        ok(on > 0, "with explosions all over it");
    }
    for (t = 0; t < 8; t++) game_tick(&game);
    ok(p->boss_live == 0, "it stops showing on the sonar");
    {
        int pieces = 0;
        for (i = 1; i < 8; i++) if (p->e[i].state < 10) pieces++;
        ok(pieces > 0, "and breaks into pieces");
    }
    for (t = 0; t < 130 && !strcmp(bgm_last, "bgm06"); t++) game_tick(&game);
    ok(!strcmp(bgm_last, "bgm11"), "bgm11 plays over the wreck");
    shot("tmp/boss_dead.png");
    for (t = 0; t < 500 && game.hook == HOOK_BOSS; t++) game_tick(&game);
    ok(game.hook == HOOK_PLAY, "then it is back to the sea");
    ok(p->stage == 1, "at stage 1 - the four stages go round");

    if (fails) { printf("%d checks failed\n", fails); return 1; }
    printf("boss checks passed  (tmp/boss_first.png, boss_in.png, "
           "boss_dying.png, boss_dead.png)\n");
    return 0;
}
