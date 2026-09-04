/* The ending, CAST and the staff roll (FUN_00408650 / FUN_00408a80 /
 * FUN_00414210), which only the debug menu's commands can reach.
 *
 *     tmp/ending_check.exe          checks, and writes tmp/end_*.png
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
static int bgm_calls;

void plat_bgm(int mode, const char *name)
{
    bgm_mode_last = mode;
    bgm_calls++;
    strncpy(bgm_last, name, sizeof bgm_last - 1);
}

void plat_se(const char *name, int pan)
{
    (void)name;
    (void)pan;
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

static int ink(void)
{
    int i, n = 0;

    for (i = 0; i < SCR_W * SCR_H; i++) if ((&vid.px[0][0])[i]) n++;
    return n;
}

/* Up to the title, which is where the debug menu would be used from. */
static void to_title(void)
{
    int i;

    game_init(&game, &vid);
    game_set_date(&game, 1999, 2, 14);
    for (i = 0; i < 400 && game.state != ST_TITLE; i++) game_tick(&game);
    game_tick(&game);
}

int main(void)
{
    Play *p = &game.p;
    Enemy *e = &game.p.e[0];
    const DarPat *pic;
    int i, t, target, was;

    if (dar_load(&dar, "disk/depth.dar") != 0) {
        printf("FAIL cannot read disk/depth.dar\n");
        return 1;
    }
    vid_init(&vid, &dar);

    /* ---- the stage select, which is the same handler family ---------- */
    to_title();
    ok(game_debug(&game, DBG_STAGE07) == 1, "the stage select answers");
    ok(p->stage == 0, "but not from the title");
    game_debug(&game, DBG_ENDING);
    game_tick(&game);
    ok(game.state == ST_PLAY && game.hook == HOOK_END, "the ending starts");
    /* The handler writes stage 2 and the ship's position, and then the
     * state's own entry block (FUN_0040b250 and the lines after it) wipes
     * the lot: stage 1, the ship at 0,0 and the starfield it just seeded
     * cleared.  That is what the original does, so it is what happens. */
    ok(p->stage == 1 && p->loaded == 1, "on stage 1 - the entry block wins");
    ok(p->lives == 2, "with two lives on the panel");
    ok(p->cloud[0].x == 0 && p->cloud[0].kind == 0,
       "and the stars the command seeded are gone again");
    ok(!strcmp(bgm_last, "bgm14") && bgm_mode_last == 0, "bgm14 plays once");
    pic = vid_pat_info(&vid, EXT_BASE);
    ok(pic != 0 && pic->w == 131, "ending.dar is loaded (earth192, 131 wide)");
    target = 0x200 - 131;
    ok(e->x == 900 - 131 - 1, "the earth starts off to the right");
    shot("tmp/end_first.png");

    /* It comes in a pixel a frame and the ship walks to the top left. */
    was = ink();
    for (t = 0; t < 60; t++) game_tick(&game);
    ok(e->x == 900 - 131 - 61, "the earth drifts in a pixel a frame");
    ok(p->px == 61 && p->py == 61, "and the ship walks up out of the corner");
    for (t = 0; t < 500 && e->x != target; t++) game_tick(&game);
    ok(e->x == target, "the earth stops at 0x200 - its width");
    ok(p->px == 0x80 && p->py == 0xc4, "the ship has reached its corner");
    ok(p->end_hold == 1, "and the two lines of text are up");
    ok(ink() > was, "with more on the screen than at the start");
    shot("tmp/end_earth.png");

    /* 150 frames of that, then the creatures. */
    for (t = 0; t < 200 && game.hook == HOOK_END; t++) game_tick(&game);
    ok(game.hook == HOOK_CAST, "then it hands over to CAST");
    ok(p->end_hold == 0x96, "after 150 frames of Congratulation");
    game_tick(&game);
    ok(p->cast_i == 0 && p->cast_x == -0x100, "the first one starts offscreen");
    for (t = 0; t < 30; t++) game_tick(&game);
    ok(p->cast_x == 0xc0, "it slides in to 0xc0 in 16-pixel steps");
    ok(p->cast_state == 10, "and waits there");
    shot("tmp/end_cast.png");
    for (t = 0; t < 200 && p->cast_i == 0; t++) game_tick(&game);
    ok(p->cast_i == 1, "the second one follows");

    /* Twenty of them, then the staff roll. */
    for (t = 0; t < 4000 && game.hook == HOOK_CAST; t++) game_tick(&game);
    ok(game.hook == HOOK_STAFF, "after the twentieth it is the staff roll");
    ok(p->cast_i == 0x14, "which is what the twentieth leaves behind");
    game_tick(&game);
    ok(!strcmp(bgm_last, "finst1") && bgm_mode_last == 0, "finst1 plays");
    ok(p->staff_y >= 0x1bf && p->staff_y <= 0x1c0,
       "the roll starts below the screen");
    pic = vid_pat_info(&vid, EXT_BASE);
    ok(pic != 0 && pic->w == 300, "staff.dar is loaded (biologo_staff)");
    was = p->staff_y;
    for (t = 0; t < 400; t++) game_tick(&game);
    ok(p->staff_y == was - 200, "and creeps up every other frame");
    ok(p->staff_t == 401, "the frame counter in the corner counts");
    shot("tmp/end_staff.png");
    for (t = 0; t < 700; t++) game_tick(&game);
    ok(ink() > 1000, "and the credits come past");
    shot("tmp/end_credit.png");

    /* All the way to the end, and out to the name entry. */
    for (t = 0; t < 12000 && game.hook == HOOK_STAFF; t++) game_tick(&game);
    ok(game.hook == HOOK_OVER, "the roll ends at the name entry");
    ok(p->staff_hold == 0x3d, "after a second of sitting still");
    shot("tmp/end_done.png");

    /* ---- and the stage select proper, from a stage ------------------- */
    game_debug(&game, DBG_STAGE01);
    game_tick(&game);
    ok(game.hook == HOOK_PLAY && p->stage == 1, "STAGE 01 is the sea");
    game_debug(&game, DBG_STAGE02);
    ok(game.hook == HOOK_AIR && p->stage == 2, "STAGE 02 the air");
    game_debug(&game, DBG_STAGE03);
    ok(game.hook == HOOK_SPACE && p->stage == 3, "STAGE 03 space");
    game_debug(&game, DBG_STAGE04);
    ok(game.hook == HOOK_BOSS && p->stage == 4, "STAGE 04 the big one");
    game_debug(&game, DBG_STAGE07);
    ok(game.hook == HOOK_SPACE && p->stage == 7, "STAGE 07 space again");
    game_debug(&game, DBG_STAGE12);
    ok(game.hook == HOOK_BOSS && p->stage == 12, "STAGE 12 the big one again");
    ok(game_debug(&game, 0x123) == 0, "anything else is not ours");

    game_debug(&game, DBG_STAGE01);
    game_tick(&game);
    p->speed = 0;
    p->charges = 0;
    p->powerA = 0;
    p->powerB = 0;
    game_debug(&game, DBG_FULLPOWER);
    ok(p->speed > 0 && p->charges > 0, "full power fills the ship up");

    /* The staff roll on its own, which is the other mode-select entry. */
    to_title();
    ok(game_debug(&game, DBG_STAFF) == 1, "the staff roll can be picked");
    game_tick(&game);
    ok(game.state == ST_PLAY && game.hook == HOOK_STAFF, "and it starts");
    for (i = 0; i < 60; i++) game_tick(&game);
    ok(ink() > 1000, "with something on the screen");

    if (fails) { printf("%d checks failed\n", fails); return 1; }
    printf("ending checks passed  (tmp/end_first.png, end_earth.png, "
           "end_cast.png, end_staff.png, end_done.png)\n");
    return 0;
}
