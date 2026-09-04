/* Run the whole thing for a long time with a pretend player at the keys, and
 * check the invariants the state machine is supposed to keep.  This is what
 * catches a counter that goes negative or a slot index that walks off the end
 * of an array - the sort of thing a port gets wrong once and never notices,
 * because the screen still looks about right.
 *
 *     tmp/soak_check.exe [frames]        default 200000
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dar.h"
#include "game.h"
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

void plat_bgm(int mode, const char *name) { (void)mode; (void)name; }
void plat_se(const char *name, int pan) { (void)name; (void)pan; }

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

static void bad(const char *what, int frame, int v)
{
    if (fails < 20) printf("FAIL %s at frame %d (%d)\n", what, frame, v);
    fails++;
}

/* A player who wanders about and leans on the buttons. */
static unsigned fake_pad(unsigned *seed, int t)
{
    unsigned pad = 0;

    *seed = *seed * 1103515245u + 12345u;
    if ((*seed >> 16 & 7) < 3) pad |= PAD_LEFT;
    if ((*seed >> 19 & 7) < 3) pad |= PAD_RIGHT;
    if ((*seed >> 22 & 7) < 2) pad |= PAD_UP;
    if ((*seed >> 25 & 7) < 2) pad |= PAD_DOWN;
    if ((t & 7) == 0) pad |= PAD_BTN1;
    if ((t & 15) == 0) pad |= PAD_BTN2;
    return pad;
}

int main(int argc, char **argv)
{
    Play *p = &game.p;
    long frames = argc > 1 ? atol(argv[1]) : 200000;
    unsigned seed = 12345;
    long t;
    int i;
    int seen_state[0x100];
    int seen_hook[8];

    if (dar_load(&dar, "disk/depth.dar") != 0) {
        printf("FAIL cannot read disk/depth.dar\n");
        return 1;
    }
    memset(seen_state, 0, sizeof seen_state);
    memset(seen_hook, 0, sizeof seen_hook);
    vid_init(&vid, &dar);
    game_init(&game, &vid);
    game_set_date(&game, 1999, 2, 14);

    for (t = 0; t < frames; t++) {
        game_set_pad(&game, fake_pad(&seed, (int)t));
        game_set_second(&game, (int)((t / 30) % 60));
        game_tick(&game);

        /* Exit really does quit in the original, so the port sits on the
         * credits for ever; start over so the soak keeps moving. */
        if (game.state == ST_VERSION || game.hook == HOOK_BOSS) {
            /* HOOK_BOSS is the stage that is not ported: nothing there
             * moves, so start over rather than sit in it. */
            game_init(&game, &vid);
            game_set_date(&game, 1999, 2, 14);
            continue;
        }
        /* And push a stage over the line now and then, so the clears and the
         * air stage get a turn as well. */
        if (t % 5000 == 4999 && (game.hook == HOOK_PLAY || game.hook == HOOK_AIR)) {
            p->kills = p->quota;
            for (i = 0; i < p->nenemy; i++)
                p->e[i].y = game.hook == HOOK_PLAY ? 0 : -0x20;
            p->onscreen = 0;
            p->item = 0;
        }
        /* the space stage ends when its script does, so wind that on too */
        if (t % 5000 == 4999 && game.hook == HOOK_SPACE) {
            p->sc_at = p->nscript;
            p->sc_wait = 0;
            for (i = 0; i < p->nenemy; i++) p->e[i].kind = 0;
            p->onscreen = 0;
            p->item = 0;
        }

        if (game.state >= 0 && game.state < 0x100) seen_state[game.state]++;
        else bad("the state is out of range", (int)t, game.state);
        if (game.hook >= 0 && game.hook < 8) seen_hook[game.hook]++;
        else bad("the hook is out of range", (int)t, game.hook);

        /* the counters the slot scans lean on */
        if (p->inflight < 0 || p->inflight > CHARGES)
            bad("charges in flight", (int)t, p->inflight);
        if (p->ntorp < 0 || p->ntorp > TORPS)
            bad("torpedoes in flight", (int)t, p->ntorp);
        if (p->neshot < 0 || p->neshot > ESHOTS + 4)
            bad("shells in flight", (int)t, p->neshot);
        if (p->nbomb < 0 || p->nbomb > BOMBS)
            bad("bombs in flight", (int)t, p->nbomb);
        if (p->nab < 0 || p->nab > ABOMBS + 5)
            bad("aimed bombs in flight", (int)t, p->nab);
        if (p->charges < 0 || p->charges > 0x10)
            bad("charges carried", (int)t, p->charges);
        if (p->lives < -1 || p->lives > 99)
            bad("lives", (int)t, p->lives);
        if (p->nenemy < 0 || p->nenemy > ENEMIES)
            bad("enemy slots in use", (int)t, p->nenemy);
        if (p->rankin < 0 || p->rankin >= RANKS)
            bad("the ranking row", (int)t, p->rankin);
        if (p->namelen < 0 || p->namelen > 8)
            bad("the typed name's length", (int)t, p->namelen);
        if (p->rcury < 0 || p->rcury > 2)
            bad("the name cursor's row", (int)t, p->rcury);
        if (p->pause_cur < 0 || p->pause_cur > 1)
            bad("the pause cursor", (int)t, p->pause_cur);
        if (game.menu_cur < 0 || game.menu_cur > 2)
            bad("the menu cursor", (int)t, game.menu_cur);
        if (p->item < 0 || p->item > 7)
            bad("the item kind", (int)t, p->item);
        /* 0 until the first play state works it out */
        if (p->cycle < 0 || p->cycle > 4)
            bad("the four-stage cycle", (int)t, p->cycle);
        if (p->stage < 0 || p->stage > 99)
            bad("the stage number", (int)t, p->stage);
        if (game.staff_line < 0 || game.staff_line > 7)
            bad("the staff line", (int)t, game.staff_line);
        if (game.logo_left < 0 || game.logo_left > LOGO_ROWS)
            bad("the logo's hidden rows", (int)t, game.logo_left);
        if (p->nnames < 0 || p->nnames > 16)
            bad("the DUP name list", (int)t, p->nnames);
        if (p->nnames > 0 && (p->pickname < 0 || p->pickname >= p->nnames))
            bad("which name DUP would paste", (int)t, p->pickname);
        for (i = 0; i < p->nenemy; i++) {
            if (p->e[i].state < 0 || p->e[i].state > 10)
                bad("an enemy's state", (int)t, p->e[i].state);
            if (p->e[i].kind < 0 || p->e[i].kind > 0x20)
                bad("an enemy's kind", (int)t, p->e[i].kind);
            if (p->e[i].aim < -0x400 || p->e[i].aim > 0x400)
                bad("an enemy's aim", (int)t, p->e[i].aim);
        }
        if (fails > 40) break;
    }

    printf("soak: %ld frames\n", t);
    printf("  states:");
    for (i = 0; i < 0x100; i++)
        if (seen_state[i]) printf(" %02x(%d)", i, seen_state[i]);
    printf("\n  hooks:");
    for (i = 0; i < 8; i++)
        if (seen_hook[i]) printf(" %d(%d)", i, seen_hook[i]);
    printf("\n  score %d, stage %d, top %d\n", p->score, p->stage,
           game.rank[0].score);

    /* The point of the soak is that it gets around: the logo, the title, the
     * game, and at least one of the two stage kinds. */
    if (!seen_state[ST_LOGO]) { printf("FAIL never saw the logo\n"); fails++; }
    if (!seen_state[ST_TITLE]) { printf("FAIL never saw the title\n"); fails++; }
    if (!seen_state[ST_TITLE2] && !seen_state[ST_TITLE3]) {
        printf("FAIL never played\n");
        fails++;
    }
    if (!seen_hook[HOOK_PLAY]) { printf("FAIL never ran the sea stage\n"); fails++; }
    if (frames >= 60000) {
        if (!seen_hook[HOOK_CLEAR])
            { printf("FAIL never cleared a stage\n"); fails++; }
        if (!seen_hook[HOOK_AIR])
            { printf("FAIL never reached the air stage\n"); fails++; }
    }

    if (fails) { printf("%d checks failed\n", fails); return 1; }
    printf("soak checks passed\n");
    return 0;
}
