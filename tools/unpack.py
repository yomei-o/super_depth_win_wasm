"""Take the game out of depth-build115.exe.

    python tools/unpack.py            depth-build115.exe -> disk/

Three layers, and none of them needs the installer to be run:

1. `depth-build115.exe` is a PackageForTheWeb 2.04 stub (InstallShield).  Its
   payload is a plain **Microsoft cabinet** appended after the last PE section
   - the MSCF signature sits at file offset 0x20b07 - so it comes out with
   Windows' own expand.exe.
2. Inside is a DISK1 folder: an InstallShield 3 setup, whose files live in
   `_SETUP.1` (magic 0x8C655D13).
3. `_SETUP.1` is a table of members at the end of the file, each member a
   PKWare Data Compression Library stream (the two-byte header is 00 06 for
   every one: uncoded literals, a 4096-byte window).  tools/unlib.c explodes
   one with Mark Adler's blast.

THE TABLE.  A name record is

    +0x00 dword  its own length, 43 + the name's length
    +0x04 word   0x0100
    +0x06 byte   the name's length
    +0x07 char   the name
          13 zero bytes

and it is followed by a 23-byte block

    +0x00 3 bytes  flags, 01 01 00 / 01 03 00 / 01 00 00
    +0x03 dword    unpacked size
    +0x07 dword    packed size
    +0x0b dword    where the data starts in _SETUP.1
    +0x0f word     dos time, word dos date
    +0x13 dword    attributes

**The block belongs to the NEXT name, not the one it follows.**  The first
name's block sits before it, right after the Group table, which is the odd
23 bytes that made the header's table pointer look wrong.  Pairing each name
with the block after it gets every size right but hands out the wrong files -
a JPEG called staff.dar, a MIDI called index.html - which is what caught it.
"""
import io
import os
import struct
import subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = os.path.join(ROOT, 'depth-build115.exe')
TMP = os.path.join(ROOT, 'tmp')
DISK = os.path.join(ROOT, 'disk')
CAB_AT = 0x20b07                        # the MSCF signature in the stub
LIB = os.path.join(TMP, 'disk1', 'DISK1', '_SETUP.1')
UNLIB = os.path.join(TMP, 'unlib.exe')

# What each file should start with, so a wrong pairing cannot pass quietly.
SIGN = {'.exe': b'MZ', '.mid': b'MThd', '.wav': b'RIFF', '.jpg': b'\xff\xd8',
        '.gif': b'GIF8', '.dar': b'DAR:', '.html': b'<htm'}


def carve_cab():
    d = io.open(EXE, 'rb').read()
    if d[CAB_AT:CAB_AT + 4] != b'MSCF':
        raise SystemExit('no cabinet at %#x' % CAB_AT)
    out = os.path.join(TMP, 'disk1.cab')
    io.open(out, 'wb').write(d[CAB_AT:])
    print('cabinet  -> %s (%d bytes)' % (out, len(d) - CAB_AT))
    return out


def expand_cab(cab):
    dst = os.path.join(TMP, 'disk1')
    os.makedirs(dst, exist_ok=True)
    subprocess.run([os.path.join(os.environ.get('SystemRoot', r'C:\Windows'),
                                 'System32', 'expand.exe'),
                    cab, '-F:*', dst], check=True, stdout=subprocess.DEVNULL)
    print('DISK1    -> %s' % dst)


def members(lib):
    """(name, unpacked, packed, offset) in the order the table lists them."""
    d = io.open(lib, 'rb').read()
    if struct.unpack_from('<I', d, 0)[0] != 0x8C655D13:
        raise SystemExit('%s is not an InstallShield 3 library' % lib)

    at = d.find(b'Group1')
    if at < 0:
        raise SystemExit('no group table in %s' % lib)
    at -= 6

    names = []                       # (name, where its 23-byte block starts)
    while at < len(d) - 8:
        ln = struct.unpack_from('<I', d, at)[0]
        if 44 <= ln <= 200 and at + ln <= len(d):
            nl = d[at + 6]
            if ln == 43 + nl and struct.unpack_from('<H', d, at + 4)[0] == 0x0100:
                name = bytes(d[at + 7:at + 7 + nl])
                if all(32 <= c < 127 for c in name):
                    names.append((name.decode('latin1'), at - 23))
                    at += ln
                    continue
        at += 1

    out = []
    for name, block in names:
        usize, csize, off = struct.unpack_from('<III', d, block + 3)
        out.append((name, usize, csize, off))
    return out


def main():
    os.makedirs(TMP, exist_ok=True)
    os.makedirs(DISK, exist_ok=True)
    if not os.path.exists(LIB):
        expand_cab(carve_cab())
    if not os.path.exists(UNLIB):
        raise SystemExit('build tmp/unlib.exe first: '
                         'sh tools/cc.sh -O2 -Itools -o tmp/unlib.exe '
                         'tools/unlib.c tools/blast.c')

    ok = bad = 0
    for name, usize, csize, off in members(LIB):
        dst = os.path.join(DISK, name)
        rc = subprocess.run([UNLIB, LIB, str(off), str(csize), dst],
                            stdout=subprocess.DEVNULL)
        got = os.path.getsize(dst) if os.path.exists(dst) else -1
        head = io.open(dst, 'rb').read(4) if got > 4 else b''
        want = SIGN.get(os.path.splitext(name)[1].lower())
        note = []
        if rc.returncode:
            note.append('blast failed')
        if got != usize:
            note.append('size %d, wanted %d' % (got, usize))
        if want and not head.startswith(want):
            note.append('starts %r, wanted %r' % (head, want))
        if note:
            bad += 1
        else:
            ok += 1
        print('  %-16s %8d -> %8d  %s'
              % (name, csize, got, '; '.join(note) if note else 'ok'))
    print('%d files out, %d wrong' % (ok, bad))


if __name__ == '__main__':
    main()
