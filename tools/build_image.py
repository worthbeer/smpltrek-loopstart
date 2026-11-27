#!/usr/bin/env python3
"""Build the smpltrek-loopstart patch image from the OFFICIAL SmplTrek 3.2.174 SmplTrek.bin.

  python3 -I build_image.py <official SmplTrek.bin> <payload.bin> <payload.nm> <out.bin> <recipe.json>

Changes made (and nothing else; verified byte-for-byte afterwards):
  * append one 'sect' (load address 0xC3F00000 + payload) to the SYSTEM image
  * container/descriptor/system lengths and section count 7 -> 8
  * three vtable words of AppLoopTrackView (vptr 0xC16C5978):
        0xC16C597C  close (slot 1) 0xC15608B4 -> loopstart_leave
        0xC16C5994  handleEncoder  0xC1560350 -> loopstart_enc
        0xC16C59B0  draw           0xC1561554 -> loopstart_draw
  * both CRCs resealed (vendor/sonicware_integrity.py, from github.com/seawaffle/smpltrek-re, MIT)
Refuses any other source image. Never overwrites an existing output file.
"""
import sys, struct, hashlib, json, re
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "vendor"))
import sonicware_integrity as I

SRC_SHA = "3b6848b0f041bcf23c73d4b0357d2f3d6dc9e7e03752b32ff7322e91e6814e53"
SRC_LEN = 0x1DCF91
LOAD = 0xC3F00000
HOOKS = {  # vtable word address -> (original target, payload symbol)
    0xC16C5994: (0xC1560350, "loopstart_enc"),    # AppLoopTrackView handleEncoder (vptr 0xC16C5978, +28)
    0xC16C59B0: (0xC1561554, "loopstart_draw"),   # AppLoopTrackView draw (+56)
    0xC16C597C: (0xC15608B4, "loopstart_leave"),  # AppLoopTrackView close (slot 1, +4): releases the scene hold
}
CAP = 0x400000  # SYSTEM flash area
GAP = (0xC16FFA98, 0xC4000000)

def sha(b): return hashlib.sha256(b).hexdigest()

def main(src_p, pay_p, nm_p, out_p, rec_p):
    src = Path(src_p).read_bytes(); pay = Path(pay_p).read_bytes()
    I.require(len(src) == SRC_LEN and sha(src) == SRC_SHA, "source must be the official SmplTrek 3.2.174 .bin")
    chk = I.verify(src, "smpltrek"); I.require(all(a == b for _, a, b in chk), "source CRC invalid")
    sysimg = chk[-1][0]
    I.require(sysimg.tag == "mmtd" and sysimg.offset == 0x30, "unexpected layout")
    I.require(len(pay) % 2 == 0 and LOAD % 32 == 0 and GAP[0] <= LOAD and LOAD + len(pay) <= GAP[1], "bad payload placement")
    I.require(len(pay) <= 0x2000, "payload must stay under 8 KiB (7,952 bytes is the largest run on a device)")
    syms = {name: int(addr, 16) for addr, name in re.findall(r"^([0-9a-f]+) T (\w+)$", Path(nm_p).read_text(), re.M)}
    pos = sysimg.offset + 36; secs = []          # (base, file offset of data, size)
    for _ in range(I.word(src, sysimg.offset + 32)):
        base, size = struct.unpack_from("<II", src, pos + 4); secs.append((base, pos + 12, size)); pos += 12 + size
    I.require(len(secs) == 7, "expected 7 sections")
    for b, _, s in secs: I.require(LOAD + len(pay) <= b or LOAD >= b + s, "overlaps a section")
    def foff(a):
        r = [o + a - b for b, o, s in secs if b <= a and a + 4 <= b + s]; I.require(len(r) == 1, "unmapped address"); return r[0]
    appended = struct.pack("<4sII", b"sect", LOAD, len(pay)) + pay
    I.require(sysimg.size + len(appended) <= CAP, "SYSTEM image would exceed its flash area")
    out = bytearray(src + appended)
    edits = {4: len(out), 0x2C: sysimg.size + len(appended), sysimg.offset + 4: sysimg.size + len(appended),
             sysimg.offset + 32: 8}
    hooks = []
    for addr, (orig, name) in HOOKS.items():
        o = foff(addr); I.require(I.word(src, o) == orig, f"vtable word {addr:#x} is not {orig:#x}")
        I.require(name in syms and LOAD <= syms[name] < LOAD + len(pay), f"payload symbol {name} missing")
        edits[o] = syms[name]
        hooks.append({"vtable_word": hex(addr), "file_offset": o, "old": hex(orig), "new": hex(syms[name]), "symbol": name})
    for o, v in edits.items(): struct.pack_into("<I", out, o, v)
    out = bytes(I.reseal(bytes(out), "smpltrek"))
    chk2 = I.verify(out, "smpltrek"); I.require(all(a == b for _, a, b in chk2), "output CRC invalid")
    allowed = set()                               # prove nothing else changed
    for o in list(edits) + [0x08, 0x38]: allowed.update(range(o, o + 4))
    diff = [i for i in range(len(src)) if src[i] != out[i] and i not in allowed]
    I.require(not diff, f"unexpected change at {diff[:4]}")
    I.require(out[len(src):] == appended, "appended section mismatch")
    if Path(out_p).exists() or Path(rec_p).exists(): sys.exit("refusing to overwrite output")
    Path(out_p).write_bytes(out)
    hook_offs = [h["file_offset"] for h in hooks]
    rec = {"source_sha256": SRC_SHA, "output_sha256": sha(out), "output_len": len(out), "payload_sha256": sha(pay),
           "payload_len": len(pay), "load": hex(LOAD), "hooks": hooks,
           "header_edits": {hex(k): hex(v) for k, v in edits.items() if k not in hook_offs}}
    Path(rec_p).write_text(json.dumps(rec, indent=1))
    print("OK", out_p, len(out), sha(out)); print(json.dumps(hooks, indent=1))

if __name__ == "__main__": main(*sys.argv[1:6])
