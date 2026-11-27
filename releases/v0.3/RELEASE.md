# v0.3 - clip list and modulo looping (work in progress)
> **Do not use: known bug, fixed in [v0.3.1](../v0.3.1/RELEASE.md).** New loop lengths are stored correctly but play with the clip's old note length until the scene is reloaded.
- Base: official SmplTrek 3.2.174 only. Expected output image: 1,961,645 bytes, sha256 `7142ad6d124ef3951ef35c08e7f3fdef637947884b6322743dd7635982548cff`.
- New since v0.2: a clip list page (FUNC + turn knob D on a loop clip's EDIT page); modulo looping (a moved START loops what is left, wraps past the end of a
  shorter clip, and START at 1.1 restores the whole clip); SET ALL now wraps short clips instead of skipping them; leaving the EDIT page releases the scene hold.
- **It writes loop lengths.** Moving a clip's START through the patch replaces that clip's SEQ LEN/NOTE, including a value you set by hand; SAVE stores it.
  Try it on a project you can afford to lose.
- Payload is 7,952 bytes (v0.2: 3,200), compiled with -Os and linked with libgcc's two register save/restore helpers.

## What has been checked
Seen on the one test unit: the clip list opens and shows its columns; shifting all clips from the list and playing the timeline; moving START while the song plays,
down to a one-beat loop; a START past a shorter clip's end wraps and the SEQ column shows it; leaving EDIT lets the timeline carry on.
Emulator only: restoring the full loop at 1.1; SET ALL with wrapping; that SAVE writes the new loop lengths; the OK jump.
Not checked on a unit: loop lengths surviving save + power cycle; time signatures other than 4/4.

## Known behaviour
- On the timeline some leftover loops go silent until a scene ends (the stock firmware restarts the loop count at each scene end). Inside EDIT they loop cleanly.
- Loops longer than 128 beats are written in half notes or whole bars; an odd remainder is rounded up.
