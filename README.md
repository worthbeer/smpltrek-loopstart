# smpltrek-loopstart

> ## 🚧 Work in progress
> This is an early, experimental project, published as it stands. Expect rough edges, changes, and things that are untested (see **Status** below).
> It has been run on **one** SmplTrek only. Nothing here is finished or guaranteed.

A small, unofficial patch for the **Sonicware SmplTrek** (system **3.2.174**) that makes the loop-clip **START POS** page work in
beats and bars, plus a scene-wide shift for loop clips. It does **not** include any Sonicware firmware: you build the patched update
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

## What it does

On the loop clip's **START POS** page (clip EDIT screen, then push the **B** knob):

| | Stock 3.2.174 | With this patch |
|---|---|---|
| Knob A | moves START by a fixed fraction of the file (about 1/128), shown as a raw frame count | moves START **one beat** per click (one **bar** per click on long loops) so it always lands on the wave's own beat grid |
| Display | `START:132110` (frames) | the same, plus a readout such as `B2.2` (bar.beat) |
| **FUNC + knob A** (v0.2) | - | **shift**: every other loop clip in the same scene moves by the same number of beats (their offsets are kept) |
| **FUNC + knob B** (v0.2) | - | **set all**: this clip and every other loop clip in the scene go to the same bar, each on its own grid |

To reset a scene to zero: bring one clip to 0 with knob A, then FUNC + knob B.

Playback rules are the stock firmware's (we only change where START is set): START is where playback begins inside the wave, the scene's
BAR length is the playback window, a clip that runs out of wave before the scene ends goes silent, and each new pass restarts at START.

## Status (read this)

| | v0.1 | v0.2 |
|---|---|---|
| Beat/bar snap + readout | verified on hardware | verified on hardware |
| FUNC + A shift | - | verified on hardware |
| FUNC + B set all | - | **emulator only** |
| Values persist after save + power cycle | not yet checked | not yet checked |
| Scenes that are not 4/4 | bar grouping assumes 4 beats (beat steps stay correct) | same |
| Loops shorter than their scene | untested | untested |

The beat grid is each clip's own: beat = file length / (whole bars x 4), falling back to the project tempo when the file is not (within 2%)
a whole number of bars.

## Build the patched update file

You need your own official `SmplTrek.bin` for **3.2.174** (sha256 `3b6848b0f041bcf23c73d4b0357d2f3d6dc9e7e03752b32ff7322e91e6814e53`, 1,953,681 bytes)
from Sonicware, and Python 3. Prebuilt payloads are in `releases/`, so no compiler is needed:

```sh
python3 -I tools/build_image.py /path/to/official/SmplTrek.bin releases/v0.2/payload.bin releases/v0.2/symbols.txt out/SmplTrek.bin out/recipe.json
python3 -I vendor/sonicware_integrity.py verify --profile smpltrek out/SmplTrek.bin      # must say 2/2 integrity fields match
shasum -a 256 out/SmplTrek.bin    # v0.2: 7bef98e8fe9f66ec33de9b30e96de901e072be25a97db72ead6df924e3c75f58 ; v0.1: e1e5aabc72047bbe238756ff8ba2641b61ab80272b042e9a58cd75a416c97f0b
```

`build_image.py` checks the source hash, appends one small code section (about 3 KB at `0xC3F00000`), redirects two function pointers of the
clip-edit page, reseals the update file's CRCs, and verifies that nothing else changed (23 header/pointer bytes differ from the official file).
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
