"""Dump the MENU resources of disk/superdepth.exe, ids and all."""
import struct

raw = open('disk/superdepth.exe', 'rb').read()
pe = struct.unpack_from('<I', raw, 0x3c)[0]
nsec = struct.unpack_from('<H', raw, pe + 6)[0]
optsz = struct.unpack_from('<H', raw, pe + 20)[0]
base = struct.unpack_from('<I', raw, pe + 24 + 28)[0]
secs = []
for i in range(nsec):
    o = pe + 24 + optsz + i * 40
    name = raw[o:o + 8].rstrip(b'\0').decode('latin1')
    vsz, vad, rsz, rad = struct.unpack_from('<IIII', raw, o + 8)
    secs.append((name, vad, vsz, rad, rsz))


def foff(rva):
    for name, vad, vsz, rad, rsz in secs:
        if vad <= rva < vad + max(vsz, rsz):
            return rad + (rva - vad)
    return None


# the resource directory
ndir = struct.unpack_from('<I', raw, pe + 24 + 96 + 16)[0]  # DataDirectory[2]
root = foff(ndir)


def entries(off):
    nname, nid = struct.unpack_from('<HH', raw, off + 12)
    out = []
    for i in range(nname + nid):
        name, data = struct.unpack_from('<II', raw, off + 16 + i * 8)
        out.append((name, data))
    return out


def walk(off, level, path):
    for name, data in entries(off):
        if name & 0x80000000:
            no = root + (name & 0x7fffffff)
            n = struct.unpack_from('<H', raw, no)[0]
            label = raw[no + 2:no + 2 + n * 2].decode('utf-16-le')
        else:
            label = name
        if data & 0x80000000:
            walk(root + (data & 0x7fffffff), level + 1, path + [label])
        else:
            rva, size = struct.unpack_from('<II', raw, root + data)
            yield_list.append((path + [label], rva, size))


yield_list = []
walk(root, 0, [])
for path, rva, size in yield_list:
    print(path, 'rva %08x size %d' % (rva, size))


def menu(off, size, depth=0):
    # MENUHEADER: version, offset
    ver, hoff = struct.unpack_from('<HH', raw, off)
    at = off + 4 + hoff
    end = off + size

    def items(at, depth):
        while at < end:
            flags = struct.unpack_from('<H', raw, at)[0]
            if flags & 0x10:                # POPUP
                at += 2
                text = ''
                while True:
                    c = struct.unpack_from('<H', raw, at)[0]
                    at += 2
                    if c == 0:
                        break
                    text += chr(c)
                print('%s+ %s' % ('  ' * depth, text.encode('cp932', 'replace').decode('cp932')))
                at = items(at, depth + 1)
            else:
                mid = struct.unpack_from('<H', raw, at + 2)[0]
                at += 4
                text = ''
                while True:
                    c = struct.unpack_from('<H', raw, at)[0]
                    at += 2
                    if c == 0:
                        break
                    text += chr(c)
                print('%s  %-30s id 0x%03x (%d)' % ('  ' * depth, text, mid, mid))
            if flags & 0x80:                # last item at this level
                return at
        return at

    items(at, depth)


for path, rva, size in yield_list:
    if path[0] == 4:
        print('==== MENU %s' % path)
        menu(foff(rva), size)
