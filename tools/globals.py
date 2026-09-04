"""Which globals a decompiled function touches that the port does not name.

Classifies the noise away: code labels, the pattern table (0x470bd8 + p*0x34),
and the 28-int object arrays whose base the port already has.

    python tools/globals.py FUN_0040f970 [FUN_...]
"""
import io
import re
import sys

dump = io.open('out/superdepth.c', encoding='utf-8', errors='replace').read()
src = ''
for p in ('src/game.h', 'src/game.c', 'src/play.h', 'src/play.c', 'src/air.c',
          'src/space.c', 'src/boss.c', 'src/ending.c', 'src/video.c',
          'src/video.h', 'src/dar.c', 'RESUME.md'):
    src += io.open(p, encoding='utf-8', errors='replace').read()

# the 28-int arrays the port has (base, how many)
ARRAYS = [
    (0x4621a8, 64, 'enemies'),
    (0x461a68, 16, '0x461a68 array'),
    (0x463dd0, 16, 'aimed bombs'),
    (0x461350, 16, '0x461350 array'),
    (0x4a5488, 16, 'charges'),
    (0x4a5b88, 16, 'shells'),
    (0x4a4d80, 16, 'torpedoes'),
    (0x4a5fa0, 64, 'splashes'),
    (0x4a8548, 76, 'popups'),
]


def classify(a):
    if 0x401000 <= a < 0x420000:
        return 'code label'
    if 0x470bd8 <= a <= 0x4b0000 and (a - 0x470bd8) % 0x34 in (0, 2):
        return 'pattern %#x w/h' % (0x0 + (a - 0x470bd8) // 0x34)
    for base, n, what in ARRAYS:
        if base <= a < base + n * 112 and (a - base) % 112 < 112:
            return '%s +%#x' % (what, (a - base) % 112)
    return None


def body(name):
    at = dump.index('/* ==== %s at' % name)
    at = dump.index('\n{\n', at)
    end = dump.index('\n}\n', at)
    return dump[at:end]


for name in sys.argv[1:]:
    b = body(name)
    seen = {}
    for m in re.finditer(r'(?:_?DAT|uRam|_?LAB)_00([0-9a-f]{6})', b):
        seen[m.group(1)] = seen.get(m.group(1), 0) + 1
    rows = []
    for a, n in sorted(seen.items()):
        if a in src:
            continue
        k = classify(int(a, 16))
        rows.append((a, n, k))
    unknown = [r for r in rows if r[2] is None]
    print('%s: %d globals, %d unnamed, %d unexplained' %
          (name, len(seen), len(rows), len(unknown)))
    for a, n, k in unknown:
        print('   0x%s  x%d' % (a, n))
    for a, n, k in rows:
        if k and not k.startswith('code'):
            print('   0x%s  x%-3d  %s' % (a, n, k))
