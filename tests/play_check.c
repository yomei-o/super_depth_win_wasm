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

/* Press a button for one frame, with a frame's gap first so the edge is
 * seen (WinGL copies the pad at the end of the frame). */
static void tap(unsigned bit)
{
    game_set_pad(&game, 0);
    game_tick(&game);
    game_set_pad(&game, bit);
    game_tick(&game);
    game_set_pad(&game, 0);
}

/* Hold a direction for one frame and let go, which moves the name entry's
 * cursor once: it reads the pad held, with a delay before it repeats. */
static void step(unsigned bit)
{
    game_set_pad(&game, bit);
    game_tick(&game);
    game_set_pad(&game, 0);
    game_tick(&game);
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

/* From a cold start to the first frame of play. */
static void start_game(void)
{
    int i;

    game_init(&game, &vid);
    game_set_date(&game, 1999, 2, 14);   /* the build's own date */
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
    ok(game.state == ST_PLAY, "Game Start reaches state 0x32");
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

    /* ESC pauses, and CONTINUE goes back in without re-arming the field. */
    start_game();
    p->hit = 1;
    game_set_pad(&game, PAD_RIGHT);
    for (t = 0; t < 10; t++) game_tick(&game);
    game_set_pad(&game, 0);
    x0 = p->px;
    game_set_pad(&game, PAD_ESC);
    game_tick(&game);
    game_set_pad(&game, 0);
    ok(game.hook == HOOK_PAUSE, "ESC opens the pause menu");
    game_tick(&game);
    {   /* PAUSE is drawn in the yellow font, which is colour 251 */
        int x, y, n = 0;
        for (y = 0x90 + 0x20; y < 0xa0 + 0x20; y++)
            for (x = 0x100; x < 0x180; x++)
                if (vid.px[y][x] == 251) n++;
        ok(n > 100, "and says so on the screen");
    }
    shot("tmp/play_pause.png");
    tap(PAD_BTN1);
    ok(game.hook == HOOK_PLAY, "CONTINUE goes back to the game");
    ok(p->px == x0, "with the ship where it was");
    game_set_pad(&game, PAD_ESC);
    game_tick(&game);
    game_set_pad(&game, 0);
    game_tick(&game);
    game_set_pad(&game, PAD_DOWN);
    game_tick(&game);
    game_set_pad(&game, 0);
    ok(game.p.pause_cur == 1, "DOWN picks EXIT");
    tap(PAD_BTN1);
    ok(game.state == ST_BOOT, "and EXIT drops out to the logo");

    /* Game over with a score worth putting in the table: the name entry.
     * The default table tops out at a thousand points, so five thousand
     * takes first place. */
    start_game();
    p->hit = 1;
    p->lives = 0;
    p->score = 5000;
    p->life = 1;
    for (t = 0; t < 400 && game.hook == HOOK_PLAY; t++) game_tick(&game);
    ok(game.hook == HOOK_OVER, "the last life ends the game");
    game_tick(&game);                                   /* its first frame */
    ok(p->rankin == 0, "and the score takes first place");
    ok(vid.ext && vid.ext->count == 50, "space.dar is in the slots at 0xb47");
    ok(!strcmp(bgm_last, "bgm08") && bgm_mode_last == 1, "bgm08 plays");
    ok(!strcmp(p->date, "99/02/14"), "the date is stamped YY/MM/DD");
    ok(p->nnames > 0, "the names already in the table are listed for DUP");
    shot("tmp/play_name.png");
    ok(p->rcurx == 0 && p->rcury == 0, "the cursor starts on the first cell");
    step(PAD_DOWN);
    ok(p->rcury == 1, "DOWN moves to the letters");
    step(PAD_RIGHT);
    ok(p->rcurx == 1, "RIGHT moves along them");
    tap(PAD_BTN1);
    ok(!strcmp(p->nm, "A"), "and the button types one");
    ok(p->namelen == 1, "which the length follows");
    step(PAD_DOWN);
    ok(p->rcury == 2, "DOWN again reaches DEL / DUP / END");
    ok(p->rcurx == 0, "on DEL");
    tap(PAD_BTN1);
    ok(p->namelen == 0 && p->nm[0] == 0, "which deletes the letter");
    /* UP from DEL lands on the far end of the letter row, at 'V'. */
    step(PAD_UP);
    ok(p->rcury == 1 && p->rcurx == 0x16, "UP from DEL goes to V");
    step(PAD_RIGHT);
    tap(PAD_BTN1);
    ok(!strcmp(p->nm, "W"), "another letter");
    step(PAD_DOWN);
    step(PAD_RIGHT);
    step(PAD_RIGHT);
    ok(p->rcury == 2 && p->rcurx == 2, "and along to END");
    shot("tmp/play_name2.png");
    tap(PAD_BTN1);
    ok(!strcmp(game.rank[0].name, "W"), "END writes the name into the table");
    ok(game.rank[0].score == 5000, "with the score");
    ok(!strcmp(game.rank[0].date, "99/02/14"), "and the date");
    ok(game.rank[0].stage == 1, "and the stage it got to");
    ok(game.rank[1].score == 100, "the old first place moves down");
    ok(game.state == ST_TITLE, "then it goes back to the title");

    /* A score that is not good enough goes straight back to the logo. */
    start_game();
    p->hit = 1;
    p->lives = 0;
    p->score = 1;
    p->life = 1;
    for (t = 0; t < 400 && game.hook == HOOK_PLAY; t++) game_tick(&game);
    game_tick(&game);
    ok(game.state == ST_LOGO, "too small a score skips the name entry");

    /* The attract demo plays DEMO1.DAT back through the pad. */
    game_init(&game, &vid);
    game_set_date(&game, 1999, 2, 14);   /* the build's own date */
    for (t = 0; t < 400 && game.state != ST_TITLE; t++) game_tick(&game);
    for (t = 0; t < 0x708 + 4 && game.state == ST_TITLE; t++) game_tick(&game);
    ok(game.state == ST_DEMO, "the title falls into the demo");
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
