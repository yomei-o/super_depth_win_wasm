/* The third stage (FUN_0040f970), the one that reads stage3.bin.
 *
 *     tmp/space_check.exe            checks, and writes tmp/space_*.png
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

/* Play through the sea and the air stage to get here. */
static void reach_space(void)
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
    game.nodie = 1;
    for (i = 0; i < p->nenemy; i++) { p->e[i].y = 0; p->e[i].state = 10; }
    p->kills = p->quota;
    p->onscreen = 0;
    for (i = 0; i < 300 && game.hook != HOOK_AIR; i++) game_tick(&game);
    game_tick(&game);
    game.nodie = 1;
    for (i = 0; i < p->nenemy; i++) { p->e[i].y = -0x20; p->e[i].state = 10; }
    p->kills = p->quota;
    p->onscreen = 0;
    p->item = 0;
    for (i = 0; i < 400 && game.hook != HOOK_SPACE; i++) game_tick(&game);
}

int main(void)
{
    Play *p = &game.p;
    int i, t, alive, kinds[0x20];

    if (dar_load(&dar, "disk/depth.dar") != 0) {
        printf("FAIL cannot read disk/depth.dar\n");
        return 1;
    }
    vid_init(&vid, &dar);

    reach_space();
    ok(game.hook == HOOK_SPACE, "the air stage's clear hands over to space");
    game_tick(&game);                                   /* its first frame */
    ok(p->stage == 3, "on stage 3");
    ok(vid.ext && vid.ext->count == 50, "space.dar is in the slots at 0xb47");
    ok(!strcmp(bgm_last, "bgm05") && bgm_mode_last == 3, "bgm05 plays");
    ok(p->nscript == 275, "stage3.bin's 275 entries are loaded");
    ok(p->script[275].type == 0xff, "with the end marker after them");
    ok(p->nenemy == ENEMIES, "all 64 slots are in play (the bullets share)");
    ok(p->px == 0x40 && p->py == 0xaa, "the ship starts at the left");
    ok(p->speed >= 4, "and moves at four or better");
    ok(p->quota == 45, "the quota is the same 45");

    /* The script has to be doing something: entries get used up and things
     * turn up on the screen. */
    game.nodie = 1;
    for (t = 0; t < 600; t++) game_tick(&game);
    ok(p->sc_at > 0, "the script is being walked");
    for (i = 0, alive = 0; i < p->nenemy; i++)
        if (p->e[i].kind != 0) alive++;
    ok(alive > 0, "and it has put something on the screen");
    shot("tmp/space_first.png");

    /* Which kinds turn up over a longer run, and nothing out of range. */
    memset(kinds, 0, sizeof kinds);
    for (t = 0; t < 20000; t++) {
        game_set_pad(&game, (t & 31) < 8 ? PAD_BTN1 :
                            ((t & 31) < 16 ? PAD_BTN2 : 0));
        game_tick(&game);
        game.nodie = 1;                     /* the checks are about the stage */
        for (i = 0; i < p->nenemy; i++) {
            int k = p->e[i].kind;
            if (k < 0 || k >= 0x20) { ok(0, "an enemy kind out of range"); break; }
            kinds[k]++;
            if (p->e[i].state < 0 || p->e[i].state > 10) {
                ok(0, "an enemy state out of range");
                break;
            }
        }
        if (p->inflight < 0 || p->inflight > UPSHOTS)
            { ok(0, "shots in flight"); break; }
        if (t == 2000) shot("tmp/space_2000.png");
    }
    printf("  kinds seen:");
    for (i = 0; i < 0x20; i++) if (kinds[i]) printf(" %x(%d)", i, kinds[i]);
    printf("\n  script at %d of %d, kills %d, score %d\n",
           p->sc_at, p->nscript, p->kills, p->score);
    ok(kinds[1] + kinds[2] + kinds[3] > 0, "the early kinds all appear");
    ok(kinds[0x14] + kinds[0x15] > 0, "and they shoot back");
    shot("tmp/space_late.png");

    /* Shooting things has to score. */
    ok(p->score > 0, "shooting them scores");

    if (fails) { printf("%d checks failed\n", fails); return 1; }
    printf("space checks passed  (tmp/space_first.png, space_2000.png, "
           "space_late.png)\n");
    return 0;
}
