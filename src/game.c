/* FUN_00401500 - the game frame - and FUN_004209a0's bookkeeping around it.
 * See game.h.  Every function here names the original it comes from; nothing
 * is added that the original does not do.
 */
#include "game.h"

#include <stdio.h>
#include <string.h>

/* DAT_00441d68: three pointers, drawn at col 0x1c, rows 0x0c/0x0e/0x10. */
const char *const GAME_MENU[3] = {
    " Game Start ", "   Record   ", "    Exit    "
};

/* 0x43ed40: eight fixed 0x25 byte slots, 36 characters each.  The '@' in the
 * last one is the character the original has; the font draws it as a (c). */
const char *const GAME_STAFF[8] = {
    "     Game Design : alty & tacox     ",
    "   Character Design : tacox & alty  ",
    "  Music Composition : FIN & CLAUDE  ",
    "        Font Design : tacox         ",
    "         Programming : alty         ",
    "         Bio_100% Presents          ",
    "     Super Depth  version 1.00a     ",
    "   @ 1991 alty & tacox / Bio_100%   "
};

/* ---- the pieces the states are built out of --------------------------- */

/* FUN_0042691c: seed = seed * 0x343fd + 0x269ec3, take bits 16..30.  That is
 * the MSVC rand(); the seed starts at 1 and FUN_00426912(1) sets it back to 1
 * at every title state, so the sequence is the same on every run. */
int game_rand(Game *g)
{
    g->seed = g->seed * 0x343fdu + 0x269ec3u;
    return (int)((g->seed >> 16) & 0x7fff);
}

/* FUN_00421da0: the next state, and the flag that says the state has just
 * been entered.  The original also zeroes 0xe100 dwords of object area at
 * DAT_004c0070 (FUN_004223c0); here every state keeps its own fields and
 * clears them on entry instead. */
static void set_state(Game *g, int st)
{
    g->entered = 1;
    g->state = st;
}

static int same_name(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) {
        int ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca += 'a' - 'A';
        if (cb >= 'A' && cb <= 'Z') cb += 'a' - 'A';
        if (ca != cb) return 0;
    }
    return *a == *b;                    /* __strcmpi */
}

/* thunk_FUN_004033a0(name, count): put `name` in the slots from 0xb47 up,
 * unless it is already there.  The original reloads pic\depth.dar into slot 0
 * first (FUN_00419c50(0, 0xb47, ...)) because the archive it is replacing may
 * have been longer than the span; here depth.dar simply stays loaded.  It
 * ends by handing pattern 0xb48 to FUN_004178e0, which is where the screen
 * palette comes from - vid_scene does that part. */
static void scene(Game *g, const char *name, int count)
{
    if (same_name(g->scene_name, name)) return;
    dar_free(&g->scene);
    if (plat_dar(&g->scene, name) != 0 || g->scene.count < count) {
        /* the original puts up a MessageBox and gives up on the scene */
        g->scene_name[0] = 0;
        vid_scene(g->v, 0);
        return;
    }
    vid_scene(g->v, &g->scene);
    strncpy(g->scene_name, name, sizeof g->scene_name - 1);
    g->scene_name[sizeof g->scene_name - 1] = 0;
}

/* thunk_FUN_00402de0: "has someone pressed the button".  Two things to keep:
 * it burns a random number every time it is called, so the sequence advances
 * once a frame from here, and the second bit it looks at (DAT_004bf868) is
 * only reachable from a joystick - the key table has nothing on 0x400.
 * While a recorded demo is playing (demo 1 or 2) the original reads the
 * demo's own key at DAT_004bf868's place; demo playback is not ported. */
static int any_key(Game *g)
{
    game_rand(g);
    if (g->demo == 0)
        return (((g->pad & PAD_BTN1) && !(g->pad_prev & PAD_BTN1)) ||
                ((g->pad & PAD_JOY7) && !(g->pad_prev & PAD_JOY7))) ? 1 : 0;
    return 0;
}

/* FUN_00402ec0: the same thing for the start button (bit 0x1000, F2).  It
 * burns a random number too. */
static int start_key(Game *g)
{
    game_rand(g);
    if (g->demo == 0 || g->demo == 1)
        return ((g->pad & PAD_START) && !(g->pad_prev & PAD_START)) ? 1 : 0;
    return 0;
}

/* The pad edges the menu reads straight out of WinGL's spread block. */
static int edge(const Game *g, unsigned bit)
{
    return ((g->pad & bit) && !(g->pad_prev & bit)) ? 1 : 0;
}

/* ---- the states ------------------------------------------------------- */

/* case 10: one frame of setting up. */
static void st_boot(Game *g)
{
    if (g->entered) {
        g->entered = 0;
        g->sub = 0;
        g->flash = 0;
    }
    g->hook = 0;
    set_state(g, ST_LOGO);
}

/* case 0xf / case 0x10: the Bio_100% logo.
 *
 * staff.dar pattern 0 - `biologo_staff`, 300x184 - is drawn whole in the
 * middle of the screen, and then every row that DAT_0044653c marks with a 1
 * is painted over in colour 0.  All 184 rows start marked, so the picture
 * appears four rows a frame in a random order; after 0x78 frames (or as soon
 * as the button is pressed) DAT_004492c0 goes to 2 and the rows come back
 * four a frame until the logo has gone, which is the cue for the title.
 */
static void st_logo(Game *g)
{
    Video *v = g->v;
    const DarPat *p;
    int i, k, seen, r;

    if (g->entered) {
        g->entered = 0;
        g->sub = 0;
        scene(g, "staff.dar", 0x123);
        plat_bgm(0, "bgm01");
        for (i = 0; i < LOGO_ROWS; i++) g->logo_row[i] = 1;
        g->logo_timer = 0;
        g->logo_phase = 0;
        g->logo_left = LOGO_ROWS;
        g->demo = 0;                    /* thunk_FUN_00403350 */
    }

    vid_clear(v, 0);
    vid_pat_centre(v, 0x140, 0xf0, EXT_BASE);

    if (g->logo_phase == 0) {
        for (k = 0; k < 4; k++) {
            if (g->logo_left <= 0) continue;
            r = game_rand(g) % g->logo_left;
            seen = 0;
            for (i = 0; i < LOGO_ROWS; i++)
                if (g->logo_row[i] == 1) {
                    if (r == seen) g->logo_row[i] = 0;
                    seen++;
                }
            g->logo_left--;
        }
    } else if (g->logo_phase == 2) {
        for (k = 0; k < 4; k++) {
            if (g->logo_left >= LOGO_ROWS) continue;
            r = game_rand(g) % (LOGO_ROWS - g->logo_left);
            seen = 0;
            for (i = 0; i < LOGO_ROWS; i++)
                if (g->logo_row[i] == 0) {
                    if (r == seen) g->logo_row[i] = 1;
                    seen++;
                }
            g->logo_left++;
        }
        if (g->logo_left > LOGO_ROWS - 1) {
            set_state(g, ST_TITLE);
            g->hook = 0;                /* thunk_FUN_00402450(0, 1) */
            g->hook_arg = 1;
        }
    }

    p = vid_pat_info(v, EXT_BASE);
    if (p)
        for (i = 0; i < LOGO_ROWS; i++)
            if (g->logo_row[i] == 1) {
                int top = i - p->h / 2 + 0xf0;
                int left = 0x140 - p->w / 2;
                vid_fill(v, left, top, left + p->w, top + 1, 0);
            }

    g->logo_timer++;
    if (g->logo_timer > 0x77) g->logo_phase = 2;
    if (any_key(g) && g->logo_phase >= 0 && g->logo_phase < 2) g->logo_phase = 2;
}

/* FUN_00402800: the SUPER DEPTH logo, laid out of 16x16 tiles from depth.dar
 * rather than kept as one picture.  Three blocks of arithmetic, kept exactly
 * as they are written: the first walks a 16-wide grid of tiles from 0x205 (a
 * row of the grid is 0x10 patterns, which is why the pattern and the y move
 * by the same 0x10), the second adds four more columns on the right with
 * three further bands (+0x209, +0x211, +0x20d) and the third puts five tiles
 * on the two rows above. */
static void title_logo(Game *g)
{
    Video *v = g->v;
    int col, x, y, n, i;

    for (col = 0x16; col < 0x36; col += 2) {
        n = col / 2 + 0x205;
        for (y = 0x40; y < 0x90; y += 0x10, n += 0x10)
            vid_pat(v, col * 8, y, n);
    }
    for (y = 0x70; y < 0x90; y += 0x10) {
        int c = 0x36;
        for (x = 0x1b0; x < 0x1f0; x += 0x10, c += 2) {
            n = c / 2 + y - 0x30;
            vid_pat(v, x,        y - 0x30, n + 0x205);
            vid_pat(v, x,        y - 0x10, n + 0x209);
            vid_pat(v, x,        y,        n + 0x211);
            vid_pat(v, x - 0x80, y + 0x20, n + 0x20d);
        }
    }
    for (i = 0, x = 0x90; i < 5; i++, x += 0x10) {
        vid_pat(v, x, 0x20, 0x30b + i - 5);
        vid_pat(v, x, 0x30, 0x30b + i);
    }
}

/* FUN_00414920, armed as WinGL's overlay hook while the title is up: the
 * three item menu, the logo above it, and the attract timeout.
 *
 * Note the order the buttons are tested in - BTN1 first, and only if that is
 * not down the start button - because each of those calls burns a random
 * number, and the sequence has to come out the same. */
static void hook_menu(Game *g)
{
    Video *v = g->v;
    int i;

    if (g->draw_new) {
        g->menu_cur = 0;
        g->menu_idle = 0;
        g->draw_new = 0;
    }
    if (edge(g, PAD_DOWN)) {
        g->menu_idle = 0;
        if (++g->menu_cur > 2) g->menu_cur = 0;
    }
    if (edge(g, PAD_UP)) {
        g->menu_idle = 0;
        if (--g->menu_cur < 0) g->menu_cur = 2;
    }
    if (any_key(g) || start_key(g)) {
        g->menu_idle = 0;
        switch (g->menu_cur) {
        case 0:                         /* Game Start */
            set_state(g, ST_TITLE2);
            g->hook = HOOK_PLAY;
            g->hook_arg = 1;
            g->draw = DRAW_NONE;
            g->draw_new = 1;
            break;
        case 1:                         /* Record */
            plat_se("depth01");
            g->draw = DRAW_RECORD;
            g->draw_new = 1;
            break;
        case 2:                         /* Exit */
            set_state(g, ST_VERSION);
            break;
        }
    }

    title_logo(g);
    for (i = 0; i < 3; i++)
        vid_text(v, 0x1c, 0x0c + i * 2, GAME_MENU[i],
                 i == g->menu_cur ? FNT_YELLOW : FNT_BLACK);

    /* 0x708 frames of nobody touching anything - 59 seconds at 33ms - and
     * the demo takes over. */
    if (++g->menu_idle >= 0x708) {
        set_state(g, ST_TITLE3);
        g->hook = HOOK_PLAY;
        g->hook_arg = 1;
        g->draw = DRAW_NONE;
        g->draw_new = 1;
    }
}

/* FUN_0040bb60 + FUN_0040bbb0: one line of the score table.  The caller
 * passes the record's fields separately; here the record is to hand.
 *
 * The two patterns are glyphs out of the 16x16 white font: the rank's own
 * digit (0x30 + rank, so '1'..'9', and 0x14 for the tenth) and one of the
 * four markers at 0x10..0x13, which stop rising after the third place. */
static void rank_row(Game *g, int rank)
{
    Video *v = g->v;
    const Rank *r = &g->rank[rank - 1];
    char line[64];
    int row = rank + 5;
    int y = row * 0x10;
    int n = rank == 10 ? 0x14 : rank + 0x30;
    int k = rank - 1;

    if (k < 0) k = 0;
    else if (k > 3) k = 3;

    vid_pat(v, 0x50, y, FNT_WHITE + n);
    vid_pat(v, 0x60, y, FNT_WHITE + 0x10 + k);
    sprintf(line, "%05d0", r->score);
    vid_text(v, 0x0f, row, line, FNT_WHITE);
    sprintf(line, "%02d", r->stage);
    vid_text(v, 0x1e, row, line, FNT_WHITE);
    vid_text(v, 0x24, row, r->name, FNT_WHITE);
    vid_text(v, 0x36, row, r->date, FNT_WHITE);
}

/* FUN_00414b00, the other overlay: the score table, on top of whatever state
 * is running - which is the title, since that is where it is armed from.
 *
 * The header line is kept in the binary with asterisks where the decorated
 * glyphs go, and patched at 1, 2 and 10..13 before it is drawn.  Leaving is a
 * plain edge test on BTN1 rather than FUN_00402de0, so unlike everywhere else
 * it does not burn a random number.
 */
static void hook_record(Game *g)
{
    Video *v = g->v;
    char line[64];
    int i;

    if (g->draw_new) g->draw_new = 0;

    vid_text(v, 10, 1, "Super Depth  Top Score Ranking", FNT_RED);
    strcpy(line, " ** Score ****  Name     Date   ");
    line[1] = 0x15; line[2] = 0x16;
    line[10] = 0x17; line[11] = 0x18; line[12] = 0x19; line[13] = 0x1a;
    vid_text(v, 8, 4, line, FNT_WHITE);
    vid_text(v, 8, 5, "--------------------------------", FNT_WHITE);
    vid_text(v, 8, 0x10, "--------------------------------", FNT_WHITE);

    for (i = 1; i < 11; i++) rank_row(g, i);

    if ((int)(g->frame % 0x10) < 8)
        vid_text(v, 0x12, 0x12, "Hit any key to return.", FNT_YELLOW);

    if (edge(g, PAD_BTN1)) {
        plat_se("depth01");
        g->draw = DRAW_MENU;            /* FUN_004148f0(&LAB_0040118b, 0) - */
        g->draw_new = 0;                /* a 0, so the menu keeps its cursor */
    }
}

/* case 0x1e: the sea title.  depth1.dar goes into the slots at 0xb47, so
 * 0xb47..0xb4a are sea01..sea04 and 0xb4c is sky01; the ground tiles below
 * are depth.dar's.  Everything is placed in the game's own coordinates, so
 * video.c adds the 32 pixels of frame.
 */
static void st_title(Game *g)
{
    Video *v = g->v;
    const DarPat *p;
    int i, x, y, n, band, step;

    if (g->entered) {
        g->entered = 0;
        g->sub = 0;
        scene(g, "depth1.dar", 9);
        for (i = 0; i < FISH; i++) {
            g->fish_y[i] = 0;           /* DAT_00449260 */
            g->fish_vx[i] = 0;          /* DAT_00446514; x is left as it was */
        }
        g->tick5 = 0;
        g->wob = 0;
        g->staff_step = 0;
        g->staff_line = 0;
        g->draw = DRAW_MENU;            /* FUN_004148f0(&LAB_0040118b, 1) */
        g->draw_new = 1;
        plat_bgm(1, "bgm02");
        g->demo = 0;
        /* DAT_00449284 = 0x708 goes in here as well, and nothing in the
         * binary ever decrements it; the attract timeout that works is the
         * menu's own DAT_004bf16c. */
    }

    /* every fifth frame the water and the title picture move a pixel */
    if (++g->tick5 > 4) {
        g->tick5 = 0;
        g->wob ^= 1;
    }

    /* the sky: one row of sky01, hung so its bottom lands on the water */
    p = vid_pat_info(v, EXT_BASE + 5);
    step = p ? p->w : 0;
    if (step > 0)
        for (x = 0x20; x < 0x260; x += step)
            vid_pat(v, x, 0x2a - p->h, EXT_BASE + 5);

    /* the water: sea01 on the surface, then sea02, sea02, sea03 down to the
     * sea bed.  The band index picks the tile, and the step is the height of
     * whichever tile was just drawn. */
    p = vid_pat_info(v, EXT_BASE);
    step = p ? p->w : 0;
    if (step > 0)
        for (x = 0x20; x < 0x260; x += step) {
            vid_pat(v, x, g->wob + 0x29, EXT_BASE);
            band = 0;
            y = p->h + 0x29;
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

    /* the sea bed, out of depth.dar */
    for (x = 0x20; x < 0x260; x += 0x10) {
        vid_pat(v, x, 0x160, 0x9ba);
        vid_pat(v, x, 0x170, 0x9bf);
        vid_pat(v, x, 0x180, 0x9bf);
        vid_pat(v, x, 400, 0x9b9);
    }
    for (x = 0x20; x < 0x260; x += 0x20)
        vid_pat(v, x, 0x141, 0x9d3);

    /* nine slots of something swimming past.  One in ten frames a free slot
     * starts one off: a lane out of seven, an edge to come in from, and one
     * to eight pixels a frame away from that edge.  Note the first random
     * number is drawn for every slot whether it is free or not. */
    for (i = 0; i < FISH; i++) {
        if (game_rand(g) % 10 == 0 && g->fish_y[i] == 0) {
            int dir;
            g->fish_y[i] = (game_rand(g) % 7) * 0x18 + 0x40;
            x = (game_rand(g) % 2) * 0x280 - 0x20;
            g->fish_x[i] = x;
            dir = 0x140 - x > 0 ? 1 : (0x140 - x < 0 ? -1 : 0);
            g->fish_vx[i] = (game_rand(g) % 8 + 1) * dir;
        }
        n = g->fish_x[i] + g->fish_vx[i];
        if (n < -0x1f || n > 0x25f) g->fish_y[i] = 0;
    }
    /* only the lanes below 99 are drawn, so the top two of the seven never
     * show - they would be above the water */
    for (i = 0; i < FISH; i++)
        if (g->fish_y[i] > 99)
            vid_pat(v, g->fish_x[i], g->fish_y[i],
                    g->fish_vx[i] < 0 ? 0xa1e : 0xa1d);
    for (i = 0; i < FISH; i++)
        g->fish_x[i] += g->fish_vx[i];

    vid_pat(v, 0x120, g->wob + 0x10, 0xa05);

    /* the staff line scrolls in 16 pixels a frame, waits 0x32 frames, and
     * scrolls out; then the next of the eight. */
    switch (g->staff_step) {
    case 0:
        g->staff_step++;
        g->staff_col = 0x50;
        break;
    case 1:
        g->staff_col -= 2;
        if (g->staff_col == 4) {
            g->staff_step++;
            g->staff_wait = 0x32;
        }
        break;
    case 2:
        if (--g->staff_wait < 1) g->staff_step++;
        break;
    case 3:
        g->staff_col -= 2;
        if (g->staff_col < -0x4a) {
            g->staff_step = 0;
            if (++g->staff_line > 7) g->staff_line = 0;
        }
        break;
    }
    vid_text_at(v, g->staff_col, 0x178, GAME_STAFF[g->staff_line], FNT_WHITE);

    /* (*DAT_004bf164)() - the overlay hook: the menu, or the score table
     * when the menu has handed over to it. */
    if (g->draw == DRAW_MENU) hook_menu(g);
    else if (g->draw == DRAW_RECORD) hook_record(g);
}

/* case 0x5a: two lines of small print and PostQuitMessage(0).  There is
 * nothing to quit in a browser, so the flag is all the port does. */
static void st_version(Game *g)
{
    vid_text8_at(g->v, 5, 4, "Super Depth ver1.00a Copyright(c)1991 "
                             "Hideki Mori and Yasuo Futatsugi");
    vid_text8_at(g->v, 5, 5, "alty & tacox / Bio_100%");
    g->quit = 1;
}

/* Not the original: a note on the screen for the states that are still to be
 * ported, so the page says which one it is sitting in. */
static void st_todo(Game *g)
{
    char line[48];

    sprintf(line, "state %02x: not ported yet", g->state);
    vid_text8_at(g->v, 0x1a, 0x1c, line);
}

/* ---- the frame -------------------------------------------------------- */

/* FUN_00401500. */
static void frame(Game *g)
{
    Video *v = g->v;
    const DarPat *step;
    char line[32];
    int x, y, tw, th;

    vid_clear(v, 0);                    /* FUN_004180c0(param_1, 0) */

    switch (g->state) {
    case ST_BOOT:  st_boot(g); break;
    case ST_LOGO0:
    case ST_LOGO:  st_logo(g); break;
    case ST_TITLE: st_title(g); break;
    case ST_VERSION: st_version(g); break;
    default:       st_todo(g); break;
    }

    /* DAT_0046217c: while it is running down, every odd frame is white. */
    if (g->flash != 0) {
        if (g->flash % 2 != 0) vid_clear(v, 0xff);
        g->flash--;
    }
    /* DAT_004492cc would clear the surface here as well, but nothing in the
     * binary ever sets it. */

    /* The frame the game runs inside, which is what the 32 pixel offset in
     * video.h leaves room for: pattern 0x9d9 tiled along all four edges,
     * stepping by the size of pattern 0x9c9 (DAT_004908ac / DAT_004908ae,
     * which are that slot's width and height).  Both are 32x32 members of
     * depth.dar's `swd_3232` group.  Fullscreen gets plain black edges
     * instead; DAT_004bf8b8 is the registry's FullScreen setting and the
     * windowed default is 0. */
    step = vid_pat_info(v, 0x9c9);
    tw = step ? step->w : 0;
    th = step ? step->h : 0;
    if (!g->fullscreen && tw > 0 && th > 0) {
        for (x = 0; x < 0x280; x += tw) {
            vid_pat_raw(v, x, 0, 0x9d9);
            vid_pat_raw(v, x, 0x1c0, 0x9d9);
        }
        for (y = 0; y < 0x1e0; y += th) {
            vid_pat_raw(v, 0, y, 0x9d9);
            vid_pat_raw(v, 0x260, y, 0x9d9);
        }
    } else {
        vid_fill(v, 0, 0, 0x280, 0x20, 0);
        vid_fill(v, 0, 0x1c0, 0x280, 0x1e0, 0);
        vid_fill(v, 0, 0x20, 0x20, 0x1c0, 0);
        vid_fill(v, 0x260, 0x20, 0x280, 0x1c0, 0);
    }

    g->sub++;
    sprintf(line, "%2dFPS", g->fps);    /* the beta's own corner display */
    vid_text8_at(v, 0x4b, 0, line);
}

void game_init(Game *g, Video *v)
{
    int i;

    memset(g, 0, sizeof *g);
    g->v = v;
    /* FUN_00402610's defaults for the score table.  The original then reads
     * the registry over the top of them (FUN_004026f0); a browser has no
     * registry, and nothing can score yet, so the defaults are all there is
     * for now. */
    for (i = 0; i < RANKS; i++) {
        g->rank[i].score = 100 - i * 10;
        strcpy(g->rank[i].name, "Bio_100%");
        strcpy(g->rank[i].date, "--/--/--");
        g->rank[i].stage = 1;
    }
    g->seed = 1;                        /* FUN_00426912(1) in the app's init */
    g->state = ST_BOOT;                 /* FUN_00421da0(10), same place */
    g->entered = 1;
}

void game_set_pad(Game *g, unsigned pad) { g->pad = pad; }
void game_set_second(Game *g, int second) { g->second = second; }

void game_tick(Game *g)
{
    int was = g->fps_count;

    frame(g);

    /* FUN_004209a0's own lines, in its order: the frame count published as
     * the FPS is the one from before this frame was counted. */
    if (g->second == g->second_prev) g->fps_count++;
    else { g->fps_count = 0; g->fps = was; }

    g->frame = (g->frame + 1) & 0x800000ffu;
    g->pad_prev = g->pad;               /* the copy to DAT_004bfc18 */
    g->second_prev = g->second;
}
