#!/usr/bin/env python3
"""Build a per-character skin folder for CTR 3D from a character-pack disc image.

    python3 tools/make_skins.py VANILLA.bin PATCHED.bin OUT_DIR

VANILLA.bin is the NTSC-U disc the game runs from; PATCHED.bin is the same disc with a character
pack applied (for example the 30th Anniversary Character Pack). For every character whose
high-LOD model differs, OUT_DIR gets skin_NN.ctr (the pack's model file, unchanged) and
skins.dat gets that character's names and HUD icon (pixels and CLUT, original and pack), so the
game can switch each character on its own. Copy OUT_DIR to sdmc:/3ds/ctr_native/skins/.

Only character models, their HUD icons and their names are taken: the pack's code, tracks and
voices are ignored. No game data is stored in this repository; the output stays on your SD card.
"""
import os
import struct
import sys

BI_LANG_EN = 235   # NTSC-U English language file
BI_RACER_HI = 242  # 16 high-LOD driver models
BI_SHARED_VRM = 258
BI_1P_PACK = 260   # any 1P arcade pack: its icon table names every character icon

# MetaDataCharacters iconID per character (game/zGlobal_DATA.c)
ICON_IDS = [32, 33, 34, 35, 36, 37, 38, 39, 43, 41, 40, 42, 44, 53, 54, 55]
# MetaDataCharacters name_LNG_long / name_LNG_short per character
LONG_NAME_IDS = [44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 58, 59, 57]
SHORT_NAME_IDS = [60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 74, 75, 73]


def sector(f, lba):
    f.seek(lba * 2352 + 24)
    return f.read(2048)


def iso_file(path, wanted):
    f = open(path, 'rb')
    pvd = sector(f, 16)
    root = pvd[156:156 + 34]

    def walk(lba, size):
        data = b''.join(sector(f, lba + i) for i in range((size + 2047) // 2048))
        i = 0
        while i < len(data):
            n = data[i]
            if n == 0:
                i = (i // 2048 + 1) * 2048
                continue
            rec = data[i:i + n]
            elba, esz = struct.unpack_from('<I', rec, 2)[0], struct.unpack_from('<I', rec, 10)[0]
            name = rec[33:33 + rec[32]].split(b';')[0].decode('latin1')
            if name == wanted:
                return b''.join(sector(f, elba + k) for k in range((esz + 2047) // 2048))[:esz]
            i += n
        return None

    return walk(struct.unpack_from('<I', root, 2)[0], struct.unpack_from('<I', root, 10)[0])


def big_entry(big, index):
    off, size = struct.unpack_from('<II', big, 8 + 8 * index)
    return big[off * 2048:off * 2048 + size]


def vram_tims(vrm):
    out = []
    p = 4
    while True:
        size = struct.unpack_from('<I', vrm, p)[0]
        if size == 0:
            return out
        tim = vrm[p + 4:p + 4 + size]
        _, x, y, w, h = struct.unpack_from('<IHHHH', tim, 8)
        out.append((x, y, w, h, tim[20:20 + w * h * 2]))
        p += 4 + size


def read_rect(tims, rx, ry, rw, rh):
    out = bytearray()
    for row in range(rh):
        for col in range(rw):
            px, py = rx + col, ry + row
            for x, y, w, h, pix in tims:
                if x <= px < x + w and y <= py < y + h:
                    o = ((py - y) * w + (px - x)) * 2
                    out += pix[o:o + 2]
                    break
            else:
                raise SystemExit('icon pixel (%d,%d) is outside the shared VRAM file' % (px, py))
    return bytes(out)


def icon_rects(pack):
    data = pack[4:]
    ltl = struct.unpack_from('<I', data, 0)[0]
    count, first = struct.unpack_from('<II', data, ltl)
    rects = {}
    for k in range(count):
        o = first + 32 * k
        gid = struct.unpack_from('<i', data, o + 16)[0]
        u0, v0, clut, u1, v1, tpage, u2, v2, u3, v3 = struct.unpack_from('<BBHBBHBBBB', data, o + 20)
        mode = (tpage >> 7) & 3
        div = {0: 4, 1: 2}.get(mode, 1)
        us, vs = (u0, u1, u2, u3), (v0, v1, v2, v3)
        tx, ty = (tpage & 0xf) * 64, ((tpage >> 4) & 1) * 256
        rect = (tx + min(us) // div, ty + min(vs), max(us) // div - min(us) // div + 1, max(vs) - min(vs) + 1)
        crect = ((clut & 0x3f) * 16, clut >> 6, 16 if mode == 0 else 256, 1) if mode < 2 else (0, 0, 0, 0)
        rects[gid] = (rect, crect)
    return rects


def lang_strings(lng):
    count, table = struct.unpack_from('<II', lng, 0)
    out = []
    for i in range(count):
        off = struct.unpack_from('<I', lng, table + 4 * i)[0]
        out.append(lng[off:lng.index(b'\0', off)].decode('latin1'))
    return out


def main():
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    vanilla, patched, out_dir = sys.argv[1:]
    big_a, big_b = iso_file(vanilla, 'BIGFILE.BIG'), iso_file(patched, 'BIGFILE.BIG')
    if big_a is None or big_b is None:
        raise SystemExit('BIGFILE.BIG not found')
    os.makedirs(out_dir, exist_ok=True)

    tims_a, tims_b = vram_tims(big_entry(big_a, BI_SHARED_VRM)), vram_tims(big_entry(big_b, BI_SHARED_VRM))
    rects = icon_rects(big_entry(big_a, BI_1P_PACK))
    names_a, names_b = lang_strings(big_entry(big_a, BI_LANG_EN)), lang_strings(big_entry(big_b, BI_LANG_EN))
    long_idx = LONG_NAME_IDS
    short_idx = SHORT_NAME_IDS

    dat = bytearray(b'SKN1')
    dat += struct.pack('<I', 16)
    for c in range(16):
        model_a, model_b = big_entry(big_a, BI_RACER_HI + c), big_entry(big_b, BI_RACER_HI + c)
        has_model = model_a != model_b
        if has_model:
            open(os.path.join(out_dir, 'skin_%02d.ctr' % c), 'wb').write(model_b)
        (ix, iy, iw, ih), (cx, cy, cw, ch) = rects[ICON_IDS[c]]
        long_name = names_b[long_idx[c]]
        short_name = names_b[short_idx[c]]
        dat += struct.pack('<I', 1 if has_model else 0)
        dat += long_name.encode('latin1')[:31].ljust(32, b'\0')
        dat += short_name.encode('latin1')[:15].ljust(16, b'\0')
        dat += struct.pack('<8H', ix, iy, iw, ih, cx, cy, cw, ch)
        dat += read_rect(tims_a, ix, iy, iw, ih) + read_rect(tims_b, ix, iy, iw, ih)
        dat += read_rect(tims_a, cx, cy, cw, ch) + read_rect(tims_b, cx, cy, cw, ch)
        print('%2d %-18s -> %-24s %s' % (c, names_a[long_idx[c]], long_name, 'model' if has_model else 'no model'))
    open(os.path.join(out_dir, 'skins.dat'), 'wb').write(dat)
    print('wrote', out_dir)


if __name__ == '__main__':
    main()
