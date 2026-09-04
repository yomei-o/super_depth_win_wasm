/* The sea title (FUN_00401500 case 0x1e) and its menu (FUN_00414920),
 * checked against the decompilation and shot to PNGs.
 *
 *     tmp/title_check.exe           checks, and writes tmp/title_*.png
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

/* Run until the state changes, or give up after `limit` frames. */
static int run_to(int state, int limit)
{
    int i;

    for (i = 0; i < limit; i++) {
        if (game.state == state) return i;
        game_tick(&game);
    }
    return -1;
}

/* Press a button for exactly one frame, so the edge is seen.  A frame with
 * nothing down comes first: WinGL takes its copy at the end of the frame, so
 * two taps in a row with no gap would be one long press. */
static void tap(unsigned bit)
{
    game_set_pad(&game, 0);
    game_tick(&game);
    game_set_pad(&game, bit);
    game_tick(&game);
    game_set_pad(&game, 0);
}

static int ink(int x0, int y0, int x1, int y1)
{
    int x, y, n = 0;

    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++)
            if (vid.px[y][x]) n++;
    return n;
}

int main(void)
{
    int i, t, alive, col0;

    if (dar_load(&dar, "disk/depth.dar") != 0) {
        printf("FAIL cannot read disk/depth.dar\n");
        return 1;
    }
    vid_init(&vid, &dar);
    game_init(&game, &vid);

    ok(run_to(ST_TITLE, 400) > 0, "the logo reaches the title");
    game_tick(&game);                                   /* the entry frame */
    ok(vid.ext && vid.ext->count == 9, "depth1.dar is in the slots at 0xb47");
    ok(!strcmp(bgm_last, "bgm02") && bgm_mode_last == 1,
       "the title starts bgm02 with mode 1");
    ok(game.draw == DRAW_MENU, "and arms the menu overlay");
    ok(game.menu_cur == 0 && game.menu_idle == 1,
       "the cursor starts on Game Start");

    /* The sea, the sky and the sea bed all have to be on the screen: the
     * water fills the middle, the bed the band at 0x180 + 32. */
    ok(ink(0x20, 0x30, 0x260, 0x60) > 20000, "the sky is drawn");
    ok(ink(0x20, 0x80, 0x260, 0x150) > 100000, "the water is drawn");
    ok(ink(0x20, 0x1a0, 0x260, 0x1b0) > 8000, "the sea bed is drawn");
    /* the tiled SUPER DEPTH logo, which is FUN_00402800's three blocks */
    ok(ink(0xb0, 0x60, 0x1e0, 0xb0) > 4000, "the title logo is drawn");

    /* The nine swimmers: a lane out of seven, one edge or the other, and a
     * speed of one to eight pixels a frame. */
    for (t = 0; t < 200; t++) {
        game_tick(&game);
        for (i = 0; i < FISH; i++) {
            int y = game.fish_y[i];
            if (!y) continue;
            ok(y >= 0x40 && y <= 0x40 + 6 * 0x18 && (y - 0x40) % 0x18 == 0,
               "a swimmer is in one of the seven lanes");
            ok(game.fish_vx[i] >= -8 && game.fish_vx[i] <= 8 &&
               game.fish_vx[i] != 0, "and moves 1..8 pixels a frame");
        }
    }
    for (i = 0, alive = 0; i < FISH; i++) if (game.fish_y[i]) alive++;
    ok(alive > 0, "some slots have started by frame 200");
    shot("tmp/title_menu.png");

    /* The staff crawl: in from col 0x50 two columns a frame, 0x32 frames of
     * standing still at col 4, out to the left, then the next of the eight. */
    col0 = game.staff_line;
    for (t = 0; t < 400 && game.staff_line == col0; t++) game_tick(&game);
    ok(game.staff_line != col0, "the crawl moves on to the next line");
    ok(game.staff_step == 0, "and starts that line's walk-in over");
    game_tick(&game);
    ok(game.staff_col == 0x50, "which begins at col 0x50");
    game_tick(&game);
    ok(game.staff_col == 0x50 - 2, "and moves two columns a frame");
    for (t = 0; t < 100 && game.staff_step == 1; t++) game_tick(&game);
    ok(game.staff_col == 4 && game.staff_wait == 0x32,
       "it stops at col 4 for 0x32 frames");

    /* The menu: DOWN and UP walk the three items and wrap. */
    tap(PAD_DOWN);
    ok(game.menu_cur == 1, "DOWN moves to Record");
    tap(PAD_DOWN);
    ok(game.menu_cur == 2, "DOWN again to Exit");
    tap(PAD_DOWN);
    ok(game.menu_cur == 0, "and wraps round to Game Start");
    tap(PAD_UP);
    ok(game.menu_cur == 2, "UP wraps the other way");
    tap(PAD_UP);
    ok(game.menu_cur == 1, "UP moves to Record");

    /* Record plays depth01.wav and swaps the overlay for the record screen. */
    tap(PAD_BTN1);
    ok(!strcmp(se_last, "depth01"), "Record plays depth01");
    ok(game.draw == DRAW_RECORD, "and arms the record overlay");
    ok(game.state == ST_TITLE, "while the title stays up");

    /* The score table overlay: the header, ten rows of defaults, and the
     * blinking line at the bottom. */
    game_tick(&game);
    ok(ink(0x50, 0x20, 0x260, 0x40) > 1000, "the ranking header is drawn");
    ok(ink(0x50, 0x60, 0x260, 0x110) > 8000, "and the ten rows under it");
    shot("tmp/title_record.png");
    {   /* "Hit any key to return." shows for eight frames out of sixteen.
         * The yellow font draws in colour 251 and nothing else on the screen
         * uses it, so counting that one index finds the line. */
        int on = 0, off = 0, k, x, y, n;
        for (k = 0; k < 16; k++) {
            game_tick(&game);
            for (y = 0x140, n = 0; y < 0x150; y++)
                for (x = 0x90; x < 0x1f0; x++)
                    if (vid.px[y][x] == 251) n++;
            if (n > 200) on++; else off++;
        }
        ok(on == 8 && off == 8, "the return line blinks eight frames in sixteen");
    }
    se_last[0] = 0;
    tap(PAD_BTN1);
    ok(game.draw == DRAW_MENU, "BTN1 goes back to the menu");
    ok(!strcmp(se_last, "depth01"), "with the same sound");
    ok(game.menu_cur == 1, "and the cursor is still on Record");

    /* Game Start hands over to state 0x32 with the play routine armed. */
    game_init(&game, &vid);
    ok(run_to(ST_TITLE, 400) > 0, "the logo reaches the title again");
    game_tick(&game);
    tap(PAD_BTN1);
    ok(game.state == ST_PLAY, "BTN1 on Game Start goes to state 0x32");
    ok(game.hook == HOOK_PLAY && game.hook_arg == 1, "with the play hook armed");
    ok(game.draw == DRAW_NONE, "and the menu overlay gone");

    /* The start button does the same thing. */
    game_init(&game, &vid);
    run_to(ST_TITLE, 400);
    game_tick(&game);
    tap(PAD_START);
    ok(game.state == ST_PLAY, "START also starts the game");

    /* Exit: state 0x5a, which is two lines and PostQuitMessage. */
    game_init(&game, &vid);
    run_to(ST_TITLE, 400);
    game_tick(&game);
    tap(PAD_UP);
    ok(game.menu_cur == 2, "UP once picks Exit");
    tap(PAD_BTN1);
    ok(game.state == ST_VERSION, "Exit goes to state 0x5a");
    game_tick(&game);
    ok(game.quit == 1, "which asks to quit");
    shot("tmp/title_exit.png");

    /* Nobody touching anything for 0x708 frames: the demo takes over. */
    game_init(&game, &vid);
    run_to(ST_TITLE, 400);
    for (t = 0; t < 0x708 + 2 && game.state == ST_TITLE; t++) game_tick(&game);
    ok(game.state == ST_DEMO, "0x708 idle frames hand over to the demo");
    ok(t >= 0x708 && t <= 0x709, "which is 1800 frames, not sooner");

    /* While it plays, the pad belongs to the recording: the original reads
     * only Esc (FUN_00405c10's tail - the START key answers while a
     * recording is being made, not while one is played back). */
    for (t = 0; t < 100; t++) game_tick(&game);
    tap(PAD_BTN1);
    ok(game.state == ST_DEMO, "Z does not stop the demo");
    tap(PAD_START);
    ok(game.state == ST_DEMO, "and neither does the start button");
    tap(PAD_ESC);
    ok(game.state == ST_LOGO, "Esc does, back to the logo");
    game_tick(&game);                   /* the logo's first frame clears it */
    ok(game.demo == 0, "and the pad belongs to the player again");

    /* Which is the whole point: the keys have to work after a demo. */
    run_to(ST_TITLE, 400);
    ok(game.state == ST_TITLE, "the logo hands back to the title");
    ok(game.menu_cur == 0, "with the menu on Game Start");
    tap(PAD_DOWN);
    ok(game.menu_cur == 1, "Down moves the menu after a demo");
    tap(PAD_UP);
    ok(game.menu_cur == 0, "and Up moves it back");
    tap(PAD_BTN1);
    game_tick(&game);
    ok(game.state == ST_PLAY, "and Z starts a game");

    /* The recording runs out on its own too - four and a half minutes of
     * it - and that has to leave the keys working as well. */
    game_init(&game, &vid);
    run_to(ST_TITLE, 400);
    for (t = 0; t < 0x70a && game.state == ST_TITLE; t++) game_tick(&game);
    for (t = 0; t < 12000 && game.state == ST_DEMO; t++) game_tick(&game);
    ok(game.recat == game.reclen, "the demo plays the recording out");
    ok(game.state == ST_LOGO, "and that ends it at the logo");
    game_tick(&game);
    ok(game.demo == 0, "with the pad back");

    if (fails) { printf("%d checks failed\n", fails); return 1; }
    printf("title checks passed  (tmp/title_menu.png, tmp/title_exit.png)\n");
    return 0;
}
