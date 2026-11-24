# Flashing, testing and recovery

**Read all of this before flashing.** Everything here was done on one SmplTrek (BOOT 1.0.17, SYSTEM 3.2.174, MCU 1.0.6). The recovery path relies on
the bootloader, which this patch does not touch. Background: [smpltrek-re SAFETY.md](https://github.com/seawaffle/smpltrek-re/blob/main/SAFETY.md).

## What can go wrong and the way out
| Situation | Meaning | Fix |
|---|---|---|
| "NG / This file is invalid" at *Checking* | the file was rejected, nothing was written | check the file and its hash |
| Power or card lost mid-update, or *Verifying* fails | the old system may be erased; no app | HOLD HOME + power -> BOOT MODE -> SYSTEM UPDATE with the **official** file |
| Frozen on the SONICWARE logo, power key dead | the app hangs at boot | remove all power (adapter out, batteries out), then HOME + power -> official file |
| Boots, but the START POS page misbehaves | a bug in the patch, only that page | power off the same way, HOME + power -> official file |

The bootloader reads HOME **before** it loads the system image, so HOME + power reaches BOOT MODE even when the app is broken. The patch changes nothing in BOOT,
the panel MCU or the presets area.

## Before you flash
1. Check SYSTEM INFO shows SYSTEM **3.2.174** (photograph BOOT / PRESET / SYSTEM / MCU). Other versions are not supported.
2. Back up the SD card (copy it to your computer).
3. Keep the official `SmplTrek.bin` ready, ideally on a second card, so recovery takes two minutes.
4. Use the AC adapter with the **batteries out** (a hung app keeps the unit on; removing the supply is the only way off). The updater also refuses to start on low batteries.
5. Test on a **copy** of a project (PROJECT MENU > SAVE AS). START values are written when you save.

## Flash
1. Build the update file (see the README), verify its CRCs and hash, copy it to the card root named exactly `SmplTrek.bin`.
2. Card in, unit off. **Hold HOME, then press and hold power.** Choose SYSTEM UPDATE, OK. Watch *Checking... 100%*; NG means stop.
3. Choose YES, OK, and do not touch anything until it finishes. Power off, power on.

## First checks, in order (stop at the first failure)
1. Reaches the home screen and loads a project.
2. SYSTEM INFO still shows 3.2.174.
3. Playback, pattern changes and the mixer behave as before.
4. Open a loop clip: EDIT, then push the **B** knob (START POS page). Turn knob A one click right: START jumps to the next beat line and a `B..` readout appears.
5. Push B again: the page shows `END:` read-only for a moment, with no `B..` text; turning A returns to START and snaps again.
6. OK, then play the scene: the clip starts at the chosen bar. Drums and other tracks are unchanged.
7. v0.2 only: hold FUNC and turn A (shift: all loop clips in the scene move together, offsets kept); hold FUNC and turn B one click (set all). Scenes other than the one you opened are untouched.
8. Save the project, power cycle, reopen: **is START still where you set it?** (not yet confirmed by us; please report).

## Return to stock
Put the official `SmplTrek.bin` on the card root (replacing the patched one) and repeat the flash steps. Then delete the file from the card.

## Please report
Anything that differs, in particular: FUNC + knob doing nothing on your unit, persistence after save, BOOT versions other than 1.0.17, and loops shorter than their scene.
