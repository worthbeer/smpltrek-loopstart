# v0.3.1 - loop-length fix and WARP switch (work in progress)
- Base: official SmplTrek 3.2.174 only. Expected output image sha256 `151963c8bd33e4003ed21b9ea479ff2202fc463aa20cae411cb18151c5e170c5`.
- **Fix (v0.3 bug):** v0.3 stored the right loop lengths but the unit played the new LEN with the clip's OLD note length until the scene was reloaded.
  v0.3.1 also updates the firmware's note-length cache.
- **WARP** (clip list: FUNC + turn knob B, right = on, left = off; off after every power-on): on group moves, clips the global start has passed
  (the longest clip's start is at or beyond their end) keep their full length: they play from the wrapped start to their end, stay silent, and repeat on
  their own length. Other clips keep looping what is left.
- The bottom hint line of the list now reads `OK=GO` (`WARP ON` while the switch is on).
- Payload 8,168 bytes (limit 8,192).

## What has been checked
Installed on the one test unit and tried by the author, with a general positive report; the individual features were not checked off one by one.
Emulator, through the controls and measured on playback position: normal loop of the remainder, WARP play-then-silence, start inside a clip, full length
restored at 1.1, a change made while playing.
Not confirmed on a unit: loop lengths surviving save + power cycle; time signatures other than 4/4.

## Known behaviour
- Loops behave predictably while the scene is held in EDIT. On the timeline each scene end restarts the loop count, so some loops leave gaps.
- A START change through the patch replaces that clip's SEQ loop length (also one set by hand); SAVE stores it.
