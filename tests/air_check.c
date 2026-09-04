/* The stage after a sea stage (FUN_0040c9e0) and its clear (FUN_0040f490).
 *
 *     tmp/air_check.exe             checks, and writes tmp/air_*.png
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
    (void)name;
    (void)buf;
    (void)max;
    return -1;
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

static void tap(unsigned bit)
{
    game_set_pad(&game, 0);
    game_tick(&game);
    game_set_pad(&game, bit);
    game_tick(&game);
    game_set_pad(&game, 0);
}

/* Play the sea stage, clear it, and let the pan run into the air stage. */
static void reach_air(void)
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
    game_tick(&game);                                   /* state 0x32 */
    p->hit = 1;
    for (i = 0; i < p->nenemy; i++) { p->e[i].y = 0; p->e[i].state = 10; }
    p->kills = p->quota;
    p->onscreen = 0;
    for (i = 0; i < 300 && game.hook != HOOK_AIR; i++) game_tick(&game);
}

int main(void)
{
    Play *p = &game.p;
    int i, t, x0;

    if (dar_load(&dar, "disk/depth.dar") != 0) {
        printf("FAIL cannot read disk/depth.dar\n");
        return 1;
    }
    vid_init(&vid, &dar);

    reach_air();
    ok(game.hook == HOOK_AIR, "the sea stage's clear hands over to the air");
    game_tick(&game);                                   /* its first frame */
    ok(!strcmp(bgm_last, "bgm04") && bgm_mode_last == 3, "bgm04 plays");
    ok(p->stage == 2, "on stage 2");
    ok(p->py == 0x120, "the ship is at the bottom of the screen");
    ok(p->quota == 45, "and 45 kills clear it");
    ok(p->charges == 4 && p->speed == 2, "with four shots at a time");
    {
        int free_slots = 0;
        for (i = 0; i < p->nenemy; i++) if (p->e[i].y <= -0x20) free_slots++;
        ok(free_slots > 0, "the aircraft slots start free (y at -0x20)");
    }
    shot("tmp/air_first.png");

    /* The ship moves further than at sea: 0x20 to 0x220. */
    p->hit = 1;
    x0 = p->px;
    game_set_pad(&game, PAD_LEFT);
    for (t = 0; t < 300; t++) game_tick(&game);
    ok(p->px == 0x20, "LEFT takes it to the very edge");
    game_set_pad(&game, PAD_RIGHT);
    for (t = 0; t < 300; t++) game_tick(&game);
    ok(p->px == 0x220, "RIGHT to the other one");
    game_set_pad(&game, 0);
    (void)x0;

    /* BTN1 sends a shot up the screen. */
    for (i = 0; i < UPSHOTS; i++) p->up[i].y = -0x10;
    p->inflight = 0;
    tap(PAD_BTN1);
    ok(p->inflight == 1, "BTN1 fires");
    ok(!strcmp(se_last, "depth05"), "with its own sound");
    for (i = 0; i < UPSHOTS; i++) if (p->up[i].y > -0x10) break;
    ok(i < UPSHOTS, "and there is a shot on the screen");
    if (i < UPSHOTS) {
        int y = p->up[i].y;
        game_tick(&game);
        ok(p->up[i].y == y - 6, "which climbs six pixels a frame");
    }
    for (t = 0; t < 200 && p->inflight; t++) game_tick(&game);
    ok(p->inflight == 0, "the top of the screen takes it back");

    /* A shot into an aircraft: the kill counts and the score goes up. */
    p->nenemy = 1;
    p->e[0].kind = 2;
    p->e[0].x = p->px + 4;
    p->e[0].y = 0x80;
    p->e[0].vx = 0;
    p->e[0].vy = 0;
    p->e[0].state = 10;
    p->e[0].w = 0x40;
    p->e[0].h = 0x20;
    p->kills = 0;
    p->score = 0;
    tap(PAD_BTN1);
    for (t = 0; t < 100 && p->e[0].state == 10; t++) game_tick(&game);
    ok(p->e[0].state == 9, "a shot into an aircraft blows it up");
    ok(p->kills == 1, "and counts");
    ok(p->score > 0, "for points");
    shot("tmp/air_hit.png");

    /* Meeting the quota runs the clear, which hands on to the next mode. */
    p->nenemy = 10;
    for (i = 0; i < p->nenemy; i++) { p->e[i].y = -0x20; p->e[i].state = 10; }
    p->kills = p->quota;
    p->onscreen = 0;
    p->item = 0;
    game_tick(&game);
    ok(game.hook == HOOK_AIRCLEAR, "the quota ends the air stage");
    game_tick(&game);
    ok(!strcmp(bgm_last, "bgm10") && bgm_mode_last == 0, "and plays bgm10");
    x0 = p->px;
    for (t = 0; t < 40; t++) game_tick(&game);
    ok(p->px < x0, "the ship works its way toward the left edge");
    shot("tmp/air_clear.png");
    {
        int stage0 = p->stage;
        for (t = 0; t < 200 && game.hook == HOOK_AIRCLEAR; t++) game_tick(&game);
        ok(game.hook == HOOK_SPACE, "then the next mode takes over");
        ok(p->stage == stage0 + 1, "with the stage number up one");
        ok(p->px == 0x40, "and the ship has got there");
    }

    if (fails) { printf("%d checks failed\n", fails); return 1; }
    printf("air checks passed  (tmp/air_first.png, air_hit.png, air_clear.png)\n");
    return 0;
}
