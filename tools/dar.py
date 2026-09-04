"""Read a .dar - Bio_100%'s "pattern" archive - the way the game reads it.

    python tools/dar.py disk/depth1.dar                  list the patterns
    python tools/dar.py disk/depth1.dar out.png 3        one pattern to a PNG
    python tools/dar.py disk/depth1.dar sheet.png        all of them on a sheet

The format comes out of FUN_00419700 (`PatEntryDAR`) and FUN_004199c0:

    +0x00  char[5]  "DAR:8"        FUN_004199c0 compares five bytes
    +0x05  byte     version, 0 or 1 (over 1 is refused)
    +0x06  word     how many patterns          <- what that function returns
    +0x08  dword    4
    +0x0c  4 bytes  0
    +0x10  RGBQUAD[256]            the palette, 1024 bytes
    +0x410 ...      the patterns

and a pattern, for version 1 (`param_4[5] != 0`):

    word   headerLen        counted from AFTER this word
    word   width
    word   height
    word   stride           bytes a row; the pixels are one byte each
    word   (signed)         -1 in the files here
    [headerLen > 13]
    word   three more
    word
    word
    [headerLen > 15]
    byte   nameLen          at +14 from the width word
    char   name[nameLen]    at +15, padded
    byte   four flags       after the name; one of them is >> 3 and picks
                            something the game keeps per pattern
    pixels height * stride bytes

    the next pattern is at  width word + headerLen + height * stride
"""
import io
import os
import struct
import sys


def load(path):
    d = io.open(path, 'rb').read()
    if d[:5] != b'DAR:8':
        raise SystemExit('%s does not start with DAR:8' % path)
    ver = d[5]
    if ver > 1:
        raise SystemExit('%s is version %d' % (path, ver))
    count = struct.unpack_from('<H', d, 6)[0]
    pal = [tuple(d[0x0c + i * 4 + k] for k in (2, 1, 0)) for i in range(256)]

    pats = []
    at = 0x40c
    for _ in range(count):
        if ver:
            hlen = struct.unpack_from('<H', d, at)[0]
            head = at + 2
        else:
            hlen = 8
            head = at
        w, h, stride = struct.unpack_from('<HHH', d, head)
        name = ''
        if hlen > 15:
            nl = d[head + 14]
            # the length byte counts itself, so the string is nl - 1
            raw = bytes(d[head + 15:head + 14 + nl]).rstrip(b'\0')
            name = raw.decode('cp932', 'replace')
        px = head + hlen
        pats.append({'name': name, 'w': w, 'h': h, 'stride': stride,
                     'at': at, 'hlen': hlen, 'px': px})
        at = head + hlen + h * stride
    return d, pal, pats, at


def rows(d, pat):
    """The pattern's pixels, one list a row, None where it is transparent.

    A row is a list of runs, each of them

        dword   the transparent count in the LOW word,
                the opaque count in the HIGH word
        byte    the opaque pixels, padded to a multiple of four

    which is exactly what FUN_0041a590 writes when it builds a run list out of
    a bitmap: `*param_2 = opaque << 0x10 | transparent & 0xffff`, and the next
    run lands at `local_18 + (opaque + 7 & 0xfffffffc)` - so a run costs
    4 + align4(opaque) bytes.  Miss that padding and everything after the
    first run whose length is not a multiple of four slides sideways.

    A row's runs together cover the width; `stride` is how many bytes the file
    gives each row.
    """
    w, h, stride, at = pat['w'], pat['h'], pat['stride'], pat['px']
    out = []
    for y in range(h):
        px = [None] * w
        # bottom-up, like a Windows DIB: stored row 0 is the last line
        q = at + (h - 1 - y) * stride
        end = q + stride
        x = 0
        while x < w and q + 4 <= end:
            pair = struct.unpack_from('<I', d, q)[0]
            q += 4
            x += pair & 0xffff                    # the transparent run
            run = pair >> 16
            if run == 0:
                break                             # a zero run ends the row
            for i in range(run):
                if 0 <= x < w and q + i < len(d):
                    px[x] = d[q + i]
                x += 1
            q += (run + 3) & ~3                   # padded to four
        out.append(px)
    return out


def draw(d, pal, pat, back=(255, 0, 255)):
    from PIL import Image
    im = Image.new('RGB', (pat['w'], pat['h']), back)
    for y, row in enumerate(rows(d, pat)):
        for x, v in enumerate(row):
            if v is not None:
                im.putpixel((x, y), pal[v])
    return im


def main():
    path = sys.argv[1]
    d, pal, pats, end = load(path)
    if len(sys.argv) < 3:
        print('%s: %d patterns, walk ends at %d of %d %s'
              % (path, len(pats), end, len(d),
                 '(exact)' if end == len(d) else '<-- MISMATCH'))
        for i, p in enumerate(pats):
            print('  %4d  %-12s %4dx%-4d stride %4d hlen %3d  at %8d'
                  % (i, p['name'], p['w'], p['h'], p['stride'], p['hlen'], p['at']))
        return

    out = sys.argv[2]
    if len(sys.argv) > 3:
        p = pats[int(sys.argv[3])]
        draw(d, pal, p).save(out)
        print('%s -> %s (%dx%d %s)' % (path, out, p['w'], p['h'], p['name']))
        return

    # everything on one sheet, in rows of ten
    cols = 10
    cw = max(p['w'] for p in pats) + 2
    ch = max(p['h'] for p in pats) + 2
    nrows = (len(pats) + cols - 1) // cols
    from PIL import Image
    im = Image.new('RGB', (cols * cw, nrows * ch), (40, 40, 40))
    for i, p in enumerate(pats):
        ox, oy = (i % cols) * cw + 1, (i // cols) * ch + 1
        im.paste(draw(d, pal, p, (40, 40, 40)), (ox, oy))
    im.save(out)
    print('%s -> %s (%d patterns)' % (path, out, len(pats)))


if __name__ == '__main__':
    main()
