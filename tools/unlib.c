/* Explode one member out of an InstallShield 3 library (_SETUP.1).
 *
 *     tmp/unlib.exe _SETUP.1 <offset> <packed> <out>
 *
 * The members are PKWare Data Compression Library streams - "implode", the
 * two-byte header being 00 (uncoded literals) and 06 (a 4096-byte window) for
 * every file in this library - so Mark Adler's blast does the work.  The
 * library's own file table is read by tools/unpack.py, which calls this.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blast.h"

static unsigned char *in;
static unsigned long inLeft;
static FILE *out;

static unsigned inf(void *how, unsigned char **buf)
{
    unsigned n = inLeft > 16384 ? 16384 : (unsigned)inLeft;
    (void)how;
    *buf = in;
    in += n;
    inLeft -= n;
    return n;
}

static int outf(void *how, unsigned char *buf, unsigned len)
{
    (void)how;
    return fwrite(buf, 1, len, out) != len;
}

int main(int argc, char **argv)
{
    FILE *f;
    long off, packed;
    unsigned char *blob;
    int rc;

    if (argc < 5) {
        fprintf(stderr, "usage: unlib <lib> <offset> <packed> <out>\n");
        return 2;
    }
    off = strtol(argv[2], NULL, 0);
    packed = strtol(argv[3], NULL, 0);

    f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }
    blob = (unsigned char *)malloc((size_t)packed);
    if (!blob) { fprintf(stderr, "out of memory\n"); return 1; }
    if (fseek(f, off, SEEK_SET) ||
        fread(blob, 1, (size_t)packed, f) != (size_t)packed) {
        fprintf(stderr, "cannot read %ld bytes at %ld\n", packed, off);
        return 1;
    }
    fclose(f);

    out = fopen(argv[4], "wb");
    if (!out) { fprintf(stderr, "cannot write %s\n", argv[4]); return 1; }
    in = blob;
    inLeft = (unsigned long)packed;
    rc = blast(inf, NULL, outf, NULL, NULL, NULL);
    fclose(out);
    if (rc) fprintf(stderr, "blast: %d\n", rc);
    return rc ? 1 : 0;
}
