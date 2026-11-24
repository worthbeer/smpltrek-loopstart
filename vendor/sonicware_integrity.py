#!/usr/bin/env python3
"""Reproduce Sonicware update CRCs; offline files only, no device access.

The constants below use zlib's API convention, not a raw CRC register value.
They are equivalent parameters inferred from published files, not recovered
bootloader source. Only families validated across several releases are supported
(SmplTrek 8, Lofi-12 XT 3, ELZ_1 play 3, Liven 8bit warps 3, Liven Lofi-12 2, Liven XFM 2, CyDrums 2). Liven updates
(.syx) are unwrapped from their SysEx framing first.
"""
import argparse
from dataclasses import dataclass
from functools import lru_cache
from pathlib import Path
import struct
import sys
import zipfile
import zlib


PROFILES = {
    "smpltrek": {"cmtd": 0x6D2DCD30, "pmtd": 0x05C3029C, "mmtd": 0x1FC67D2F},
    "lofi12": {"cmtd": 0x8061B03E, "mmtd": 0x2929E2B7},
    "elz1play": {"cmtd": 0xF21764BC, "pmtd": 0x09528EF7, "mmtd": 0x2F06AE24},
    "liven8bit": {"cmtd": 0xF79228D4, "pmtd": 0xB46329DD, "mmtd": 0x743A0A9F},
    "livenlofi12": {"cmtd": 0x042ECEE9, "mmtd": 0x617E460F},
    "livenxfm": {"cmtd": 0xD6D0F0EB, "mmtd": 0xB7B66668},
    "cydrums": {"cmtd": 0x9763BE13, "mmtd": 0x2C577CBB},
}
# CyDrums shares family word 8 with ELZ_1 play but has different seeds (profile added 2026-11-18, when
# 2.0.97 beta recovered the same two seeds as 1.1.84).
# The family word equals the SysEx device byte of the Liven updates (2026-11-12).
FAMILY_WORD = {"smpltrek": 5, "lofi12": 12, "elz1play": 8, "liven8bit": 2, "livenlofi12": 7, "livenxfm": 3,
               "cydrums": 8}
SYSEX_HEADER = b"\xf0\x00\x48\x04\x00\x00"  # manufacturer 00 48 04, then device byte, 7F, command


@dataclass(frozen=True)
class Image:
    tag: str
    offset: int
    size: int
    version: tuple = ()


def require(condition, message):
    if not condition:
        raise ValueError(message)


def word(data, offset):
    return struct.unpack_from("<I", data, offset)[0]


def parse(data):
    """Parse the observed modern layout using descriptors, never magic scanning.

    The legacy ELZ_1 layout is deliberately rejected. Exact bounds/contiguity
    here describe this corpus; they are not assertions about every future file.
    """
    require(len(data) >= 24 and data[:4] == b"cmtd", "not a cmtd container")
    require(word(data, 4) == len(data), "size mismatch or unsupported legacy layout")
    count = word(data, 20)
    require(0 < count <= (len(data) - 24) // 24, "invalid descriptor count")
    end = 24 + count * 24
    images = [Image("cmtd", 0, len(data))]
    for index in range(count):
        kind, major, minor, build, offset, size = struct.unpack_from("<6I", data, 24 + 24 * index)
        tag = {2: "pmtd", 3: "mmtd"}.get(kind)
        require(tag is not None, f"unsupported image kind {kind}")
        require(offset == end and size >= 32 and offset + size <= len(data), "invalid image bounds")
        b = memoryview(data)[offset:offset + size]
        require(b[:4] == tag.encode() and word(b, 4) == size, "descriptor/image mismatch")
        version = (major, minor, build)
        require(struct.unpack_from("<3I", b, 16) == version, "descriptor/version mismatch")
        if tag == "mmtd":
            require(size >= 36, "truncated mmtd")
            pos = 36
            for _ in range(word(b, 32)):
                require(pos + 12 <= size and b[pos:pos + 4] == b"sect", "invalid section header")
                pos += 12 + word(b, pos + 8)
                require(pos <= size, "section exceeds image")
        else:
            partitions = word(b, 28)
            pos = 32 + 12 * partitions
            require(pos <= size, "invalid partition table")
            pos += sum(word(b, 32 + 12 * k + 8) for k in range(partitions))
        require(pos == size, "payload sizes do not exhaust image")
        images.append(Image(tag, offset, size, version))
        end = offset + size
    require(end == len(data), "unaccounted container bytes")
    return images


def checksum(data, image, seed):
    """CRC of the full declared image with only its own +8..+11 zeroed."""
    b = memoryview(data)[image.offset:image.offset + image.size]
    crc = zlib.crc32(b[:8], seed)
    crc = zlib.crc32(b"\0" * 4, crc)
    return zlib.crc32(b[12:], crc)


def seed_for(profile, tag):
    """The profile's seed for an image type; refuses types the profile has no seed for (e.g. a pmtd under
    the cydrums profile, which shares family word 8 with ELZ_1 play)."""
    require(tag in PROFILES[profile], f"profile {profile} has no seed for a {tag} image")
    return PROFILES[profile][tag]


def verify(data, profile, allow_family_mismatch=False):
    images = parse(data)
    if not allow_family_mismatch:
        require(word(data, 16) == FAMILY_WORD[profile], "family word differs from selected profile")
    return [
        (image, word(data, image.offset + 8), checksum(data, image, seed_for(profile, image.tag)))
        for image in images
    ]


def reseal(data, profile, allow_family_mismatch=False):
    images = parse(data)
    if not allow_family_mismatch:
        require(word(data, 16) == FAMILY_WORD[profile], "family word differs from selected profile")
    result = bytearray(data)
    # The parent's CRC includes the children's stored CRC fields.
    for image in images[1:] + images[:1]:
        value = checksum(result, image, seed_for(profile, image.tag))
        struct.pack_into("<I", result, image.offset + 8, value)
    return bytes(result)


@lru_cache(maxsize=128)
def seed_influence(length):
    """Columns of the linear map from a zlib seed difference to CRC difference."""
    zeros = bytes(length)
    base = zlib.crc32(zeros)
    return tuple(zlib.crc32(zeros, 1 << k) ^ base for k in range(32))


def recover_seed(data, image):
    """Fit ONE sample with final XOR fixed at zero. Requires independent validation.

    Every message/check pair admits such a seed; one fit proves nothing about
    the actual algorithm. Stability across different releases is the evidence.
    """
    target = word(data, image.offset + 8) ^ checksum(data, image, 0)
    pivots = {}
    for k, column in enumerate(seed_influence(image.size)):
        mask = 1 << k
        while column:
            pivot = column.bit_length() - 1
            if pivot not in pivots:
                pivots[pivot] = (column, mask)
                break
            other, bits = pivots[pivot]
            column ^= other
            mask ^= bits
    result = 0
    while target:
        other, bits = pivots[target.bit_length() - 1]
        target ^= other
        result ^= bits
    return result


def unpack7(encoded):
    """Seven payload bytes follow one byte holding their top bits, bit 6 for the first byte."""
    out = bytearray()
    for start in range(0, len(encoded), 8):
        group = encoded[start:start + 8]
        for n, byte in enumerate(group[1:]):
            out.append(byte | (group[0] >> (6 - n) & 1) << 7)
    return bytes(out)


def unwrap_sysex(data):
    """Return the cmtd container packed in a Liven update (.syx).

    Messages are F0 00 48 04 00 00 <device> 7F <command> <payload> F7: command 0 opens with the
    container length, 1 carries data (210 bytes per message), 2 closes. Observed on the 8bit warps
    and Lofi-12 updates (2026-11-12); the device byte equals the container's family word.
    """
    messages = data.split(b"\xf7")
    require(len(messages) >= 4 and messages.pop() == b"", "not a SysEx update")
    device = messages[0][6:7]
    expected = [0] + [1] * (len(messages) - 2) + [2]
    payloads = []
    for message, command in zip(messages, expected):
        require(message[:6] == SYSEX_HEADER and message[6:8] == device + b"\x7f" and message[8] == command,
                "unexpected SysEx message")
        payloads.append(unpack7(message[9:]))
    require(len(payloads[0]) == 4 and payloads[-1] == b"", "unexpected SysEx open/close payload")
    container = b"".join(payloads[1:-1])
    require(len(container) == word(payloads[0], 0), "SysEx length mismatch")
    return container


def inputs(paths, skip_non_updates=False):
    """Yield (name, cmtd bytes) per update file. A .zip with no cmtd .bin (the CyDrums card-contents folder zip, for
    one) is an error, or with skip_non_updates a notice on stderr."""
    for path in paths:
        if path.suffix.lower() == ".syx":
            yield str(path), unwrap_sysex(path.read_bytes())
        elif path.suffix.lower() == ".zip":
            with zipfile.ZipFile(path) as archive:
                damaged = archive.testzip()   # check every member's CRC first: corruption is never "not an update"
                require(damaged is None, f"{path} is damaged: {damaged} fails its zip CRC")
                found = False
                for name in archive.namelist():
                    if name.lower().endswith(".bin") and not name.startswith("__MACOSX/"):
                        with archive.open(name) as source:
                            prefix = source.read(4)
                            if prefix != b"cmtd":
                                continue
                            found = True
                            yield f"{path}:{name}", prefix + source.read()
                if not found and skip_non_updates:
                    print(f"skipped {path}: no cmtd update inside (not an update file)", file=sys.stderr)
                    continue
                require(found, f"no cmtd update in {path}")
        else:
            yield str(path), path.read_bytes()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    for command in ("verify", "derive"):
        sub = commands.add_parser(command)
        sub.add_argument("paths", nargs="+", type=Path)
        if command == "verify":
            sub.add_argument("--profile", choices=PROFILES, required=True)
            sub.add_argument("--allow-family-mismatch", action="store_true",
                             help="use the profile's seeds even if the cmtd family word disagrees (probe files)")
    sub = commands.add_parser("reseal", help="write a new local file with recalculated CRC fields")
    sub.add_argument("--profile", choices=PROFILES, required=True)
    sub.add_argument("--allow-family-mismatch", action="store_true",
                     help="use the profile's seeds even if the cmtd family word disagrees (probe files)")
    sub.add_argument("input", type=Path, help=".bin, official .zip, or Liven .syx (unwrapped first)")
    sub.add_argument("output", type=Path, help="new file (never overwritten); always a bare cmtd container")
    args = parser.parse_args()
    try:
        if args.command == "reseal":
            found = list(inputs([args.input]))
            require(len(found) == 1, f"{args.input} holds {len(found)} update files; reseal takes one")
            result = reseal(found[0][1], args.profile, args.allow_family_mismatch)
            with args.output.open("xb") as output:
                output.write(result)
            note = (" (the bare cmtd container: .syx inputs are unwrapped and not re-packed as SysEx)"
                    if args.input.suffix.lower() == ".syx" else "")
            print(f"Wrote {args.output}{note}; offline CRC repair only, device acceptance untested.")
            return 0
        failures = total = 0
        files = 0
        for name, data in inputs(args.paths, skip_non_updates=True):
            files += 1
            print(name)
            if args.command == "derive":
                for image in parse(data):
                    print(f"  {image.tag} seed={recover_seed(data, image):08x} (single-sample fit)")
            else:
                for image, stored, calculated in verify(data, args.profile, args.allow_family_mismatch):
                    ok = stored == calculated
                    total += 1
                    failures += not ok
                    print(f"  {image.tag} @{image.offset:#x} stored={stored:08x} calculated={calculated:08x} {'OK' if ok else 'FAIL'}")
        require(files, "no update files among the inputs")
        if args.command == "verify":
            print(f"{total - failures}/{total} integrity fields match")
        return int(bool(failures))
    except (ValueError, OSError, struct.error, KeyError, zipfile.BadZipFile) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
