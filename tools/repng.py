"""Squeeze a PNG the checks wrote.

`src/png.c` writes stored (uncompressed) deflate blocks so that nothing has
to be linked into the tools - every 640x480 shot comes out at 308KB.  This
reads one back and writes it again with zlib, which is what the pictures in
docs/ are made with:

    python tools/repng.py tmp/demo_sea.png docs/shot_sea.png

Only what src/png.c writes is understood: IHDR, PLTE, one IDAT, IEND.
"""
import struct
import sys
import zlib


def chunks(d):
    at = 8
    while at < len(d):
        n, kind = struct.unpack_from('>I4s', d, at)
        yield kind.decode('latin1'), d[at + 8:at + 8 + n]
        at += 12 + n


def build(kind, data):
    out = struct.pack('>I', len(data)) + kind.encode('latin1') + data
    return out + struct.pack('>I', zlib.crc32(kind.encode('latin1') + data)
                             & 0xffffffff)


def main(src, dst):
    d = open(src, 'rb').read()
    if d[:8] != b'\x89PNG\r\n\x1a\n':
        raise SystemExit('%s is not a PNG' % src)
    ihdr = plte = None
    idat = b''
    for kind, data in chunks(d):
        if kind == 'IHDR':
            ihdr = data
        elif kind == 'PLTE':
            plte = data
        elif kind == 'IDAT':
            idat += data
    raw = zlib.decompress(idat)
    out = bytearray(b'\x89PNG\r\n\x1a\n')
    out += build('IHDR', ihdr)
    if plte is not None:
        out += build('PLTE', plte)
    out += build('IDAT', zlib.compress(raw, 9))
    out += build('IEND', b'')
    open(dst, 'wb').write(out)
    print('%s -> %s (%d -> %d bytes)' % (src, dst, len(d), len(out)))


if __name__ == '__main__':
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    main(sys.argv[1], sys.argv[2])
