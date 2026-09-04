/* The Bio_100% logo state (FUN_00401500 case 0xf / 0x10), checked against
 * what the decompilation says it must do, and shot to PNGs so the render can
 * be looked at without opening a window.
 *
 *     tmp/logo_check.exe            checks, and writes tmp/logo_*.png
 */
#include <stdio.h>
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

static int bgm_calls;
static char bgm_last[64];
static int bgm_mode_last;

void plat_bgm(int mode, const char *name)
{
    bgm_calls++;
    bgm_mode_last = mode;
    strncpy(bgm_last, name, sizeof bgm_last - 1);
}

void plat_se(const char *name)
{
    (void)name;
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

/* How many pixels of the row the logo occupies are not colour 0. */
static int row_ink(int row)
{
    const DarPat *p = vid_pat_info(&vid, EXT_BASE);
    int left = 0x140 - p->w / 2, y = row - p->h / 2 + 0xf0, x, n = 0;

    for (x = left; x < left + p->w; x++)
        if (vid.px[y][x]) n++;
    return n;
}

int main(void)
{
    int i, t, seen, hidden_ink, shown;
    int cleared[LOGO_ROWS];

    if (dar_load(&dar, "disk/depth.dar") != 0) {
        printf("FAIL cannot read disk/depth.dar\n");
        return 1;
    }
    vid_init(&vid, &dar);
    game_init(&game, &vid);

    /* One frame of state 0x0a, which only arms state 0x10. */
    ok(game.state == ST_BOOT, "the game starts in state 0x0a");
    game_tick(&game);
    ok(game.state == ST_LOGO, "state 0x0a hands over to 0x10");

    /* The first logo frame loads staff.dar and starts bgm01. */
    game_tick(&game);
    ok(bgm_calls == 1 && !strcmp(bgm_last, "bgm01") && bgm_mode_last == 0,
       "the logo starts bgm01 with mode 0");
    ok(vid.ext && vid.ext->count == 0x123, "staff.dar is in the slots at 0xb47");
    ok(vid_pat_info(&vid, EXT_BASE)->h == LOGO_ROWS,
       "biologo_staff is exactly as tall as the row array is long");
    ok(game.logo_left == LOGO_ROWS - 4, "four rows appear on the first frame");
    ok(game.logo_phase == 0, "and the phase is still 0");

    /* The border frame is drawn whatever the state does. */
    {   /* pattern 0x9d9's own top two rows are colour 0, so count instead */
        int x, y, ink = 0;
        for (y = 0; y < 0x20; y++) for (x = 0; x < SCR_W; x++) if (vid.px[y][x]) ink++;
        ok(ink > 4000, "the border frame is tiled along the top");
        for (y = 0, ink = 0; y < SCR_H; y++) for (x = 0; x < 0x20; x++) if (vid.px[y][x]) ink++;
        ok(ink > 4000, "and down the left");
    }

    /* Every hidden row must be blank across the logo's width, every shown row
     * must have something on it. */
    hidden_ink = shown = 0;
    for (i = 0; i < LOGO_ROWS; i++) {
        if (game.logo_row[i]) hidden_ink += row_ink(i);
        else shown++;
    }
    ok(hidden_ink == 0, "the rows still hidden are painted out");
    ok(shown == 4, "and four rows are showing");
    shot("tmp/logo_01.png");

    /* 184 rows, four a frame: 46 frames to come up.  Each row must be
     * revealed exactly once - the original picks the r'th row that is still
     * hidden, so the order is a permutation. */
    memset(cleared, 0, sizeof cleared);
    for (t = 1; t < 46; t++) {
        game_tick(&game);
        for (i = 0; i < LOGO_ROWS; i++)
            if (!game.logo_row[i]) cleared[i]++;
        if (t == 22) shot("tmp/logo_23.png");
    }
    ok(game.logo_left == 0, "the logo is fully up after 46 frames");
    for (i = 0, seen = 0; i < LOGO_ROWS; i++) if (cleared[i]) seen++;
    ok(seen == LOGO_ROWS, "every one of the 184 rows was revealed");
    for (i = 0, hidden_ink = 0; i < LOGO_ROWS; i++) hidden_ink += row_ink(i);
    ok(hidden_ink > 20000, "and the whole picture is on the screen");
    shot("tmp/logo_46.png");

    /* It sits there until DAT_0045cb78 passes 0x77, then hides itself four
     * rows a frame, and the last row leaving is the cue for the title. */
    for (t = 46; t < 120; t++) {
        game_tick(&game);
        ok(game.logo_phase == 0 || t == 119, "the logo waits until frame 120");
    }
    ok(game.logo_phase == 2, "at frame 120 the phase turns to 2");
    for (t = 0; t < 46 && game.state == ST_LOGO; t++) game_tick(&game);
    ok(game.state == ST_TITLE, "the logo leaving hands over to the title");
    ok(game.hook == 0 && game.hook_arg == 1, "and sets the 0, 1 pair with it");

    /* The button skips the wait: thunk_FUN_00402de0 wants an edge on BTN1. */
    game_init(&game, &vid);
    game_tick(&game);                                   /* 0x0a */
    for (t = 0; t < 10; t++) game_tick(&game);
    ok(game.logo_phase == 0, "the phase is 0 before the button");
    game_set_pad(&game, PAD_BTN1);
    game_tick(&game);
    ok(game.logo_phase == 2, "BTN1 skips to the hide");
    game_tick(&game);
    ok(game.logo_phase == 2, "and it stays there");

    /* A button that was already down is not a press: the edge is against the
     * copy WinGL takes at the end of the frame before. */
    game_init(&game, &vid);
    game_set_pad(&game, PAD_BTN1);
    game_tick(&game);                                   /* 0x0a */
    game_tick(&game);                                   /* the logo's first */
    ok(game.logo_phase == 0, "a button held from before is not a press");

    if (fails) { printf("%d checks failed\n", fails); return 1; }
    printf("logo checks passed  (tmp/logo_01.png, logo_23.png, logo_46.png)\n");
    return 0;
}
