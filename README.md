# smpltrek-loopstart

> ## 🚧 Work in progress
> This is an early, experimental project, published as it stands. Expect rough edges, changes, and things that are untested (see **Status** below).
> It has been run on **one** SmplTrek only. Nothing here is finished or guaranteed.

A small, unofficial patch for the **Sonicware SmplTrek** (system **3.2.174**) for loop clips: START in beats and bars, scene-wide moves, a clip list
page, and "modulo" looping (a moved start loops what is left of each clip). It does **not** include any Sonicware firmware: you build the patched update
file yourself from your own official `SmplTrek.bin`.

> **Unofficial. Use at your own risk.** Not affiliated with or endorsed by Sonicware. Modifying firmware can leave a unit unusable until
> an official update is flashed again (it is recoverable, see [docs/flashing-and-recovery.md](docs/flashing-and-recovery.md)), and may void
> your warranty. Tested on **one** SmplTrek only (BOOT 1.0.17, SYSTEM 3.2.174, MCU 1.0.6). The builder refuses any other firmware version.

## Why this exists

This is a hobby exploration, not a product: a place to try out ideas for how a loop-based song-writing workflow on the SmplTrek could feel, based on
the author's own observations of the unit. It exists because **Sonicware** built a remarkable little instrument, and because
[seawaffle](https://github.com/seawaffle)'s open reverse-engineering work (the update format, the emulator, the safety notes) made it possible to look
inside safely. Thank you to both. Nothing here is meant to compete with, replace, or take anything away from Sonicware's work. If you enjoy the SmplTrek,
please buy and support Sonicware's products.

If Sonicware or any other rights holder has a concern about anything in this repository, please open an issue and it will be addressed promptly.

## What it does (v0.3.1)

> **If you built v0.3, rebuild with v0.3.1:** v0.3 played changed loop lengths wrongly until the scene was reloaded.

**On a loop clip's START POS page** (EDIT on the track, then push the **B** knob):

| Do this | Result |
|---|---|
| knob A | START moves in beats (a faster turn moves bars), shown as bar.beat such as `B2.2` |
| FUNC + knob A | **shift**: every loop clip in the scene moves by the same number of beats |
| FUNC + knob B | **set all**: every loop clip goes to this clip's bar (shorter clips wrap) |

**Clip list** (anywhere on a loop clip's EDIT page: hold FUNC and *turn* knob D): title `SCENE n PLAY/STOP`; columns T (track), STRT (start), NOW (play
position), LN (audio length, bars), SEQ (loop length, bars). UP/DOWN or the VALUE knob selects; knob A moves the selected start; FUNC + knob A moves the others
with it; OK opens that clip; CLR closes.

**Modulo looping:** after a START change the clip loops what is left (`clip length - start`); a start past a shorter clip's end wraps around it; START at 1.1
restores the whole clip. This replaces the clip's SEQ loop length (also one you set by hand), and SAVE stores it.

**WARP** (in the clip list: FUNC + turn knob B, right on, left off; off after power-on): on group moves, clips the global start has passed keep their full
length, so they play to their end, stay silent, then repeat. The others keep looping what is left.

**EDIT page:** while the song plays, EDIT still holds the scene; leaving EDIT now lets the timeline carry on.

## Status (read this)

| v0.3.1 feature | On the test unit | Emulator |
|---|---|---|
| Beat snap + readout (v0.1), shift (v0.2) | seen | yes |
| Clip list page; timeline continues after leaving EDIT | seen | yes |
| Modulo looping with the loop-length fix, WARP switch | tried, general positive report | yes, on playback position |
| Full loop restored at 1.1; set all with wrapping; OK jump | not individually confirmed | yes (OK jump: earlier build) |
| Loop starts and lengths survive Save | seen (author's save test) | yes |
| Time signatures other than 4/4 | untested (4 beats per bar assumed) | untested |

Loops behave predictably while the scene is held in EDIT; on the timeline some leave gaps. See [releases/v0.3.1/RELEASE.md](releases/v0.3.1/RELEASE.md).

The beat grid is each clip's own: beat = file length / (whole bars x 4), falling back to the project tempo when the file is not (within 2%)
a whole number of bars.

## Build the patched update file

You need your own official `SmplTrek.bin` for **3.2.174** (sha256 `3b6848b0f041bcf23c73d4b0357d2f3d6dc9e7e03752b32ff7322e91e6814e53`, 1,953,681 bytes)
from Sonicware, and Python 3. Prebuilt payloads are in `releases/`, so no compiler is needed:

```sh
python3 -I tools/build_image.py /path/to/official/SmplTrek.bin releases/v0.3.1/payload.bin releases/v0.3.1/symbols.txt out/SmplTrek.bin out/recipe.json
python3 -I vendor/sonicware_integrity.py verify --profile smpltrek out/SmplTrek.bin      # must say 2/2 integrity fields match
shasum -a 256 out/SmplTrek.bin    # v0.3.1: 151963c8bd33e4003ed21b9ea479ff2202fc463aa20cae411cb18151c5e170c5
```

`build_image.py` checks the source hash, appends one small code section (about 8 KB at `0xC3F00000`), redirects three function pointers of the
clip-edit page, reseals the update file's CRCs, and verifies that nothing else changed
(older releases v0.1 and v0.2 were built with the builder of their own commit).
To rebuild the payload from source you need a `tic6x-elf` GCC (13.3.0 tested): `scripts/build_payload.sh`.

Then flash it like any official update, but **read [docs/flashing-and-recovery.md](docs/flashing-and-recovery.md) first**.

## Repository layout

- `src/payload.c`, `src/link.ld`: the patch (C, no libc, runs inside the stock firmware's page handlers).
- `tools/build_image.py`: makes the update file from your official one. `vendor/`: the container/CRC tool from smpltrek-re (MIT).
- `releases/`: prebuilt payloads, symbol lists, recipes and the expected output hashes. No firmware.
- `docs/knowledge-base.md`: what we learned about the firmware (addresses, data layouts, behaviours, how it was verified).
- `docs/flashing-and-recovery.md`: backup, flashing, recovery, and the hardware test list.

## Credits and licence

This project stands on the reverse-engineering done in [seawaffle/smpltrek-re](https://github.com/seawaffle/smpltrek-re)
(update-container and CRC scheme, symbol files, emulator, safety notes; MIT) and was checked with its emulator.
[seawaffle/enaga](https://github.com/seawaffle/enaga) (MIT) was used as reference documentation. See [NOTICE.md](NOTICE.md).
Code in this repository is released under the **MIT** licence ([LICENSE](LICENSE)). "Sonicware" and "SmplTrek" belong to their owners.
This README is not legal advice; if you are unsure about the rules where you live, check before modifying your device.
