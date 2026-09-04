/* The game itself (FUN_00405c10): the field a stage is built with, the ship,
 * the depth charges and what they hit.
 *
 *     tmp/play_check.exe            checks, and writes tmp/play_*.png
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
static int se_pan_last;

void plat_bgm(int mode, const char *name)
{
    bgm_mode_last = mode;
    strncpy(bgm_last, name, sizeof bgm_last - 1);
}

void plat_se(const char *name, int pan)
{
    se_pan_last = pan;
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

/* From a cold start to the first frame of play. */
static void start_game(void)
{
    int i;

    game_init(&game, &vid);
    for (i = 0; i < 400 && game.state != ST_TITLE; i++) game_tick(&game);
    game_tick(&game);                                   /* the title's first */
    game_set_pad(&game, PAD_BTN1);
    game_tick(&game);
    game_set_pad(&game, 0);
    game_tick(&game);                                   /* state 0x32 arrives */
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

    start_game();
    ok(game.state == ST_TITLE2, "Game Start reaches state 0x32");
    ok(game.hook == HOOK_PLAY, "with the play routine hooked up");
    ok(!strcmp(bgm_last, "bgm03") && bgm_mode_last == 3,
       "and bgm03 playing in the mode that does not restart it");

    /* The field a stage is built with: ten slots, the kinds out of the table
     * at 0x43fae8, and the stage's own quota. */
    ok(p->nenemy == 10, "the stage runs ten enemy slots");
    {
        static const int want[10] = { 1, 2, 3, 1, 5, 5, 1, 1, 9, 9 };
        int bad = 0;
        for (i = 0; i < 10; i++) if (p->e[i].kind != want[i]) bad++;
        ok(bad == 0, "stage 1's kinds are the table's row");
    }
    ok(p->quota == 25, "and 25 kills clear it");
    ok(p->charges == 4 && p->speed == 2 && p->lives == 2,
       "the ship starts with four charges, speed two and two spare lives");
    ok(p->px == 0x120 && p->py == 0x10, "in the middle of the surface");
    ok(p->banner > 0, "and Ready is on the screen");

    /* The ship moves at its speed and stops at the edges. */
    p->hit = 1;                         /* DAT_00463dc4: no collisions */
    x0 = p->px;
    game_set_pad(&game, PAD_RIGHT);
    game_tick(&game);
    ok(p->px == x0 + p->speed, "RIGHT moves the ship by its speed");
    game_set_pad(&game, PAD_LEFT);
    game_tick(&game);
    ok(p->px == x0, "LEFT moves it back");
    game_set_pad(&game, PAD_LEFT | PAD_RIGHT);
    game_tick(&game);
    ok(p->px == x0, "both at once cancel out");
    game_set_pad(&game, PAD_RIGHT);
    for (t = 0; t < 300; t++) game_tick(&game);
    /* the gate is `px + speed < 0x210`, so it stops one step short of
     * the 0x20f clamp rather than on it */
    ok(p->px >= 0x20c && p->px <= 0x20f, "and it stops at the right edge");
    game_set_pad(&game, PAD_LEFT);
    for (t = 0; t < 300; t++) game_tick(&game);
    ok(p->px == 0x30, "and at the left edge");
    game_set_pad(&game, 0);

    /* A charge sinks two pixels a frame and frees its slot at the sea bed. */
    for (i = 0; i < CHARGES; i++) p->c[i].y = 0x134;
    p->inflight = 0;
    game_set_pad(&game, PAD_BTN1);
    game_tick(&game);
    game_set_pad(&game, 0);
    ok(p->inflight == 1, "BTN1 drops a charge");
    ok(!strcmp(se_last, "drop"), "with the drop sound");
    for (i = 0; i < CHARGES; i++) if (p->c[i].y != 0x134) break;
    ok(i < CHARGES && p->c[i].x == p->px - 0x10,
       "off the bow, sixteen pixels ahead of the ship");
    {
        int y = p->c[i].y;
        game_tick(&game);
        ok(p->c[i].y == y + 2, "and it sinks two pixels a frame");
    }
    for (t = 0; t < 200 && p->inflight; t++) game_tick(&game);
    ok(p->inflight == 0, "the sea bed takes it back");

    /* A charge onto a submarine: the kind's score times the chain, and the
     * enemy starts blowing up. */
    start_game();
    p->hit = 1;
    p->nenemy = 1;                      /* only slot 0 runs */
    p->px = 0x100;
    p->e[0].kind = 1;
    p->e[0].x = 0xf0;
    p->e[0].y = 0x100;
    p->e[0].vx = 0;
    p->e[0].vy = 0;
    p->e[0].state = 10;
    p->e[0].w = 0x40;
    p->e[0].h = 0x20;
    p->score = 0;
    p->kills = 0;
    game_set_pad(&game, PAD_BTN1);
    game_tick(&game);
    game_set_pad(&game, 0);
    for (t = 0; t < 200 && p->e[0].state == 10; t++) game_tick(&game);
    ok(p->e[0].state == 9, "a charge on a submarine starts it blowing up");
    ok(p->kills == 1, "and counts as a kill");
    ok(p->score == 5, "worth five in stage one");
    ok(!strcmp(se_last, "burn"), "with the burn sound");
    shot("tmp/play_hit.png");
    for (t = 0; t < 40; t++) game_tick(&game);
    ok(p->e[0].y == 0 && p->e[0].state == 10,
       "and the slot comes back when the explosion ends");

    /* The points go up over whatever was hit: the kind's worth times ten,
     * and the chain after an x when there was one. */
    {
        int found = -1;
        for (i = 0; i < POPUPS; i++)
            if (p->pop[i].t > 0) { found = i; break; }
        ok(found >= 0, "a popup goes up with the points");
        if (found >= 0) {
            ok(p->pop[found].value == 50, "worth fifty, which is five times ten");
            ok(p->pop[found].chain == 1, "and no chain");
        }
    }

    /* Meeting the quota with the screen clear ends the stage: the camera
     * follows the ship up to the surface and the stage number goes up. */
    start_game();
    p->hit = 1;
    for (i = 0; i < p->nenemy; i++) { p->e[i].y = 0; p->e[i].state = 10; }
    p->kills = p->quota;
    p->onscreen = 0;
    game_tick(&game);
    ok(game.hook == HOOK_CLEAR, "the quota ends the stage");
    game_tick(&game);                   /* the clear's own first frame */
    ok(!strcmp(bgm_last, "bgm09") && bgm_mode_last == 0, "and plays bgm09 once");
    {
        int y0 = p->py, stage0 = p->stage;
        for (t = 0; t < 25; t++) game_tick(&game);
        ok(p->py == y0, "the ship holds still for the first 0x1e frames");
        for (t = 0; t < 40; t++) game_tick(&game);
        ok(p->py > y0, "then the camera follows it up");
        shot("tmp/play_clear.png");
        for (t = 0; t < 200 && game.hook == HOOK_CLEAR; t++) game_tick(&game);
        ok(p->py == 0x120, "it stops at the surface");
        ok(game.hook == HOOK_AIR, "and hands over to what comes next");
        ok(p->stage == stage0 + 1, "with the stage number up one");
    }

    /* Left alone, the ship gets sunk and loses a life; with no lives left
     * the game is over. */
    start_game();
    for (t = 0; t < 4000 && p->lives == 2; t++) game_tick(&game);
    ok(p->lives == 1, "sitting still costs a life");
    for (t = 0; t < 20000 && game.hook == HOOK_PLAY; t++) game_tick(&game);
    ok(game.hook == HOOK_OVER, "and running out of lives ends the game");

    /* The attract demo plays DEMO1.DAT back through the pad. */
    game_init(&game, &vid);
    for (t = 0; t < 400 && game.state != ST_TITLE; t++) game_tick(&game);
    for (t = 0; t < 0x708 + 4 && game.state == ST_TITLE; t++) game_tick(&game);
    ok(game.state == ST_TITLE3, "the title falls into the demo");
    game_tick(&game);
    ok(game.demo == 2, "which plays a recording back");
    ok(game.reclen > 0, "demo1.dat has something in it");
    ok(p->demo == 1, "and the screen says DEMONSTRATION");
    for (t = 0; t < 300; t++) game_tick(&game);
    ok(game.recat > 0, "the recording is being read");
    shot("tmp/play_demo.png");

    if (fails) { printf("%d checks failed\n", fails); return 1; }
    printf("play checks passed  (tmp/play_hit.png, tmp/play_demo.png)\n");
    return 0;
}
