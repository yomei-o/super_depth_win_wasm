/* FUN_00401500 - the game frame - and FUN_004209a0's bookkeeping around it.
 * See game.h.  Every function here names the original it comes from; nothing
 * is added that the original does not do.
 */
#include "game.h"

#include <stdio.h>
#include <string.h>

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
    memset(g, 0, sizeof *g);
    g->v = v;
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
